// File: RiftForgedStressTest.cpp
// (Refactored to use FlatBuffers payload_type dispatch)

// Standard C++ Includes
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <atomic>
#include <memory>
#include <algorithm>
#include <cmath> // For std::abs in random movement

// Winsock
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
// #include <Windows.h> // For GetAsyncKeyState (not used in spam_loop)

// FlatBuffers & Project Headers
#include "flatbuffers/flatbuffers.h"
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h"
#include "../NetworkEngine/GamePacketHeader.h" // Still needed for struct def, flags, protocol ID
#include "../NetworkEngine/UDPReliabilityProtocol.h"
#include "../Utils/Logger.h"
#include <fmt/core.h> // For FMT_STRING

// Namespaces for convenience
namespace RF_C2S = RiftForged::Networking::UDP::C2S;
namespace RF_S2C = RiftForged::Networking::UDP::S2C;
namespace RF_Shared = RiftForged::Networking::Shared;
namespace RF_Net = RiftForged::Networking;

class SimulatedPlayer {
public:
    uint64_t clientId_; // Client-side self-assigned ID for stress test tracking
    std::string serverIp_;
    int serverPort_;

    SOCKET clientSocket_ = INVALID_SOCKET;
    sockaddr_in serverAddr_{};

    std::unique_ptr<RF_Net::ReliableConnectionState> connectionState_;
    flatbuffers::FlatBufferBuilder builder_;
    std::atomic_bool stop_running_ = { false };

    char recvBuffer_[4096]; // Increased buffer size just in case

    enum class PlayerJoinState {
        NotConnected,
        AttemptingJoin,
        Joined,
        FailedToJoin
    };
    PlayerJoinState joinState_ = PlayerJoinState::NotConnected;
    uint64_t serverAssignedPlayerId_ = 0;
    std::string characterIdForJoin_ = "StressChar_";

    // Constants for reliability management within the simulated client
    // MAX_PACKET_RETRIES is defined in UDPReliabilityProtocol.h, used by GetPacketsForRetransmission
    static const long CLIENT_RELIABILITY_CHECK_INTERVAL_MS = 100; // How often ManageReliability is called
    static const long CLIENT_EXPLICIT_ACK_DELAY_MS = 50;      // Delay before sending an ACK-only packet
    static const long JOIN_RESEND_INTERVAL_MS = 2000;         // How often to resend join if no S2C_JoinSuccess

    explicit SimulatedPlayer(uint64_t id, const std::string& ip, int port)
        : clientId_(id),
        serverIp_(ip),
        serverPort_(port),
        builder_(512), // Default builder size
        connectionState_(std::make_unique<RF_Net::ReliableConnectionState>()) {

        characterIdForJoin_ += std::to_string(id);

        clientSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (clientSocket_ == INVALID_SOCKET) {
            throw std::runtime_error("Client " + std::to_string(clientId_) +
                ": socket() failed: " + std::to_string(WSAGetLastError()));
        }

        sockaddr_in clientLocalAddr;
        clientLocalAddr.sin_family = AF_INET;
        clientLocalAddr.sin_addr.s_addr = INADDR_ANY;
        clientLocalAddr.sin_port = 0; // Bind to any available port
        if (bind(clientSocket_, (sockaddr*)&clientLocalAddr, sizeof(clientLocalAddr)) == SOCKET_ERROR) {
            RF_CORE_ERROR(FMT_STRING("[Client {}] bind() failed: {}. May affect receiving."), clientId_, WSAGetLastError());
            // Not throwing, as it might still work for sending in some OS configurations
        }

        DWORD timeout = 1; // 1 ms timeout for non-blocking recvfrom
        if (setsockopt(clientSocket_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
            RF_CORE_WARN(FMT_STRING("[Client {}] setsockopt SO_RCVTIMEO failed: {}"), clientId_, WSAGetLastError());
        }

        serverAddr_.sin_family = AF_INET;
        serverAddr_.sin_port = htons(static_cast<u_short>(serverPort_));
        if (inet_pton(AF_INET, serverIp_.c_str(), &serverAddr_.sin_addr) <= 0) {
            closesocket(clientSocket_);
            clientSocket_ = INVALID_SOCKET;
            throw std::runtime_error("Client " + std::to_string(clientId_) +
                ": inet_pton() failed for IP " + serverIp_ +
                " with error: " + std::to_string(WSAGetLastError()));
        }
    }

    ~SimulatedPlayer() {
        stop_running_ = true; // Should signal any running loops to stop
        if (clientSocket_ != INVALID_SOCKET) {
            closesocket(clientSocket_);
            clientSocket_ = INVALID_SOCKET;
        }
    }

    SimulatedPlayer(const SimulatedPlayer&) = delete;
    SimulatedPlayer& operator=(const SimulatedPlayer&) = delete;
    SimulatedPlayer(SimulatedPlayer&&) = default;
    SimulatedPlayer& operator=(SimulatedPlayer&&) = default;

    bool isValid() const noexcept {
        return clientSocket_ != INVALID_SOCKET;
    }

    static uint64_t current_timestamp_ms() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }

    // UPDATED: Removed RF_Net::MessageType msgType parameter
    void send_packet_internal(const uint8_t* flatbuffer_data,
        size_t flatbuffer_size,
        bool isReliable) {
        if (!isValid() || !connectionState_) return;

        uint8_t packetFlags = static_cast<uint8_t>(RF_Net::GamePacketFlag::NONE);
        if (isReliable) {
            packetFlags |= static_cast<uint8_t>(RF_Net::GamePacketFlag::IS_RELIABLE);
        }

        // Assuming UDPReliabilityProtocol::PrepareOutgoingPacket signature is now:
        // std::vector<uint8_t> PrepareOutgoingPacket(ReliableConnectionState&, const uint8_t*, uint16_t, uint8_t);
        std::vector<uint8_t> packet_buffer_uint8 = RF_Net::PrepareOutgoingPacket(
            *connectionState_, flatbuffer_data, static_cast<uint16_t>(flatbuffer_size), packetFlags);

        if (packet_buffer_uint8.empty()) {
            RF_CORE_WARN(FMT_STRING("[Client {}] PrepareOutgoingPacket returned empty buffer. Not sending."), clientId_);
            return;
        }
        sendto(clientSocket_, reinterpret_cast<const char*>(packet_buffer_uint8.data()), static_cast<int>(packet_buffer_uint8.size()),
            0, (const sockaddr*)&serverAddr_, sizeof(serverAddr_));
    }

    void send_join_request() {
        builder_.Clear();
        auto charIdOffset = characterIdForJoin_.empty() ? 0 : builder_.CreateString(characterIdForJoin_);
        auto join_request_payload = RF_C2S::CreateC2S_JoinRequestMsg(builder_, current_timestamp_ms(), charIdOffset);

        RF_C2S::Root_C2S_UDP_MessageBuilder root_builder(builder_);
        root_builder.add_payload_type(RF_C2S::C2S_UDP_Payload_JoinRequest);
        root_builder.add_payload(join_request_payload.Union());
        auto root_message_offset = root_builder.Finish();
        builder_.Finish(root_message_offset);

        RF_CORE_INFO(FMT_STRING("[Client {}] Sending Join Request (Char: {})"), clientId_, characterIdForJoin_);
        send_packet_internal(builder_.GetBufferPointer(), builder_.GetSize(), true); // MessageType removed
    }

    void send_movement_input(float local_x, float local_y, float local_z, bool is_sprinting) {
        builder_.Clear(); RF_Shared::Vec3 ld(local_x, local_y, local_z);
        auto p = RF_C2S::CreateC2S_MovementInputMsg(builder_, current_timestamp_ms(), &ld, is_sprinting);
        RF_C2S::Root_C2S_UDP_MessageBuilder rb(builder_); rb.add_payload_type(RF_C2S::C2S_UDP_Payload_MovementInput); rb.add_payload(p.Union());
        builder_.Finish(rb.Finish());
        send_packet_internal(builder_.GetBufferPointer(), builder_.GetSize(), false); // MessageType removed
    }

    void send_basic_attack(float ax, float ay, float az, uint64_t tid = 0) {
        builder_.Clear(); RF_Shared::Vec3 ad(ax, ay, az);
        auto p = RF_C2S::CreateC2S_BasicAttackIntentMsg(builder_, current_timestamp_ms(), &ad, tid);
        RF_C2S::Root_C2S_UDP_MessageBuilder rb(builder_); rb.add_payload_type(RF_C2S::C2S_UDP_Payload_BasicAttackIntent); rb.add_payload(p.Union());
        builder_.Finish(rb.Finish());
        send_packet_internal(builder_.GetBufferPointer(), builder_.GetSize(), true); // MessageType removed
    }

    void send_rift_step_dodge(RF_C2S::RiftStepDirectionalIntent intent) {
        builder_.Clear();
        auto p = RF_C2S::CreateC2S_RiftStepActivationMsg(builder_, current_timestamp_ms(), intent);
        RF_C2S::Root_C2S_UDP_MessageBuilder rb(builder_); rb.add_payload_type(RF_C2S::C2S_UDP_Payload_RiftStepActivation); rb.add_payload(p.Union());
        builder_.Finish(rb.Finish());
        send_packet_internal(builder_.GetBufferPointer(), builder_.GetSize(), true); // MessageType removed
    }

    void send_ping() {
        builder_.Clear();
        auto p = RF_C2S::CreateC2S_PingMsg(builder_, current_timestamp_ms());
        RF_C2S::Root_C2S_UDP_MessageBuilder rb(builder_); rb.add_payload_type(RF_C2S::C2S_UDP_Payload_Ping); rb.add_payload(p.Union());
        builder_.Finish(rb.Finish());
        send_packet_internal(builder_.GetBufferPointer(), builder_.GetSize(), true); // MessageType removed
    }

    void receive_server_packets() {
        if (!isValid() || !connectionState_) return;
        int bytes_received;
        sockaddr_in sender_addr; int sender_addr_len = sizeof(sender_addr);

        while (true) { // Loop to process all immediately available packets
            bytes_received = recvfrom(clientSocket_, recvBuffer_, sizeof(recvBuffer_), 0, (sockaddr*)&sender_addr, &sender_addr_len);
            if (bytes_received == SOCKET_ERROR) {
                if (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAETIMEDOUT) break; // No more data
                RF_CORE_ERROR(FMT_STRING("[Client {}] recvfrom error: {}"), clientId_, WSAGetLastError());
                joinState_ = PlayerJoinState::FailedToJoin;
                stop_running_ = true;
                break;
            }
            if (bytes_received < static_cast<int>(RF_Net::GetGamePacketHeaderSize())) {
                RF_CORE_WARN(FMT_STRING("[Client {}] Received packet too small ({} bytes)."), clientId_, bytes_received);
                continue;
            }

            RF_Net::GamePacketHeader server_header;
            memcpy(&server_header, recvBuffer_, RF_Net::GetGamePacketHeaderSize());
            if (server_header.protocolId != RF_Net::CURRENT_PROTOCOL_ID_VERSION) {
                RF_CORE_WARN(FMT_STRING("[Client {}] Mismatched protocol ID. Expected: 0x{:X}, Got: 0x{:X}."),
                    clientId_, RF_Net::CURRENT_PROTOCOL_ID_VERSION, server_header.protocolId);
                continue;
            }

            const uint8_t* s2c_payload_after_header_ptr = reinterpret_cast<const uint8_t*>(recvBuffer_ + RF_Net::GetGamePacketHeaderSize());
            uint16_t s2c_payload_after_header_len = static_cast<uint16_t>(bytes_received - RF_Net::GetGamePacketHeaderSize());
            const uint8_t* app_payload_to_process_ptr = nullptr;
            uint16_t app_payload_size = 0;

            bool should_process_app_payload = RF_Net::ProcessIncomingPacketHeader(
                *connectionState_, server_header, s2c_payload_after_header_ptr, s2c_payload_after_header_len,
                &app_payload_to_process_ptr, &app_payload_size);

            if (should_process_app_payload && app_payload_to_process_ptr && app_payload_size > 0) {
                flatbuffers::Verifier verifier(app_payload_to_process_ptr, app_payload_size);
                if (!RF_S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) {
                    RF_CORE_WARN(FMT_STRING("[Client {}] S2C FlatBuffer verification failed. Discarding payload of size {}."), clientId_, app_payload_size);
                    continue;
                }
                auto root = RF_S2C::GetRoot_S2C_UDP_Message(app_payload_to_process_ptr);
                if (!root || !root->payload()) { // Also check if payload exists
                    RF_CORE_WARN(FMT_STRING("[Client {}] GetRoot_S2C_UDP_Message failed or payload is missing."), clientId_);
                    continue;
                }

                // Dispatch based on FlatBuffer payload type
                switch (root->payload_type()) {
                case RF_S2C::S2C_UDP_Payload_S2C_JoinSuccessMsg: {
                    auto join_success_msg = root->payload_as_S2C_JoinSuccessMsg();
                    if (join_success_msg) {
                        serverAssignedPlayerId_ = join_success_msg->assigned_player_id();
                        if (joinState_ != PlayerJoinState::Joined) { // Log only on first successful join
                            RF_CORE_INFO(FMT_STRING("[Client {} SID: {}] JOIN SUCCESSFUL. Server Tick: {}Hz. Msg: '{}'"),
                                clientId_, serverAssignedPlayerId_,
                                join_success_msg->server_tick_rate_hz(),
                                (join_success_msg->welcome_message() ? join_success_msg->welcome_message()->c_str() : ""));
                        }
                        joinState_ = PlayerJoinState::Joined;
                    }
                    else {
                        RF_CORE_ERROR(FMT_STRING("[Client {}] Failed to cast S2C_JoinSuccess payload."), clientId_);
                    }
                    break;
                }
                case RF_S2C::S2C_UDP_Payload_S2C_JoinFailedMsg: {
                    auto join_failed_msg = root->payload_as_S2C_JoinFailedMsg();
                    std::string reason = "Unknown";
                    int16_t code = 0;
                    if (join_failed_msg) {
                        if (join_failed_msg->reason_message()) reason = join_failed_msg->reason_message()->str();
                        code = join_failed_msg->reason_code();
                    }
                    RF_CORE_ERROR(FMT_STRING("[Client {}] JOIN FAILED. Reason: {} (Code: {}). Stopping."), clientId_, reason, code);
                    joinState_ = PlayerJoinState::FailedToJoin;
                    stop_running_ = true;
                    break;
                }
                case RF_S2C::S2C_UDP_Payload_Pong: {
                    auto pong_msg = root->payload_as_Pong();
                    if (pong_msg) {
                        uint64_t rtt = current_timestamp_ms() - pong_msg->client_timestamp_ms();
                        RF_CORE_INFO(FMT_STRING("[Client {} SID: {}] PONG! RTT: {}ms. ServerTS: {}"), clientId_, serverAssignedPlayerId_, rtt, pong_msg->server_timestamp_ms());
                    }
                    break;
                }
                case RF_S2C::S2C_UDP_Payload_EntityStateUpdate:
                    // RF_CORE_INFO(FMT_STRING("[Client {}] Received EntityStateUpdate."), clientId_);
                    break;
                case RF_S2C::S2C_UDP_Payload_RiftStepInitiated:
                    // RF_CORE_INFO(FMT_STRING("[Client {}] Received RiftStepInitiated."), clientId_);
                    break;
                    // Add other S2C message types as needed
                default:
                    RF_CORE_WARN(FMT_STRING("[Client {}] Received unhandled S2C FlatBuffer payload type: {}"),
                        clientId_, RF_S2C::EnumNameS2C_UDP_Payload(root->payload_type()));
                    break;
                }
            }
            else if (RiftForged::Networking::HasFlag(server_header.flags, RiftForged::Networking::GamePacketFlag::IS_RELIABLE) && !should_process_app_payload) {
                RF_CORE_TRACE(FMT_STRING("[Client {}] S2C Reliability packet processed (Seq {}). No app payload for client logic (e.g. duplicate or pure ACK)."),
                    clientId_, server_header.sequenceNumber);
            }
        }
    }

    void ManageReliability() {
        if (!isValid() || !connectionState_) return;
        auto currentTime = std::chrono::steady_clock::now();

        // GetPacketsForRetransmission signature was (ReliableConnectionState&, time_point)
        // MAX_CLIENT_PACKET_RETRIES is now an internal constant in UDPReliabilityProtocol or checked via ShouldDropPacket.
        std::vector<std::vector<uint8_t>> packetsToResend = RF_Net::GetPacketsForRetransmission(
            *connectionState_, currentTime);

        for (const auto& packetData : packetsToResend) {
            RF_CORE_WARN(FMT_STRING("[Client {}] Retransmitting packet of size {}..."), clientId_, packetData.size());
            sendto(clientSocket_, reinterpret_cast<const char*>(packetData.data()), static_cast<int>(packetData.size()),
                0, (const sockaddr*)&serverAddr_, sizeof(serverAddr_));
        }

        if (connectionState_->connectionDroppedByMaxRetries) {
            RF_CORE_ERROR(FMT_STRING("[Client {}] Connection dropped by max C2S retries. Stopping."), clientId_);
            joinState_ = PlayerJoinState::FailedToJoin;
            stop_running_ = true;
            return;
        }

        // TrySendAckOnlyPacket now takes a lambda for sending.
        RF_Net::TrySendAckOnlyPacket(*connectionState_, currentTime,
            [this](const std::vector<uint8_t>& packetDataToSend) {
                if (!packetDataToSend.empty()) {
                    // RF_CORE_DEBUG(FMT_STRING("[Client {}] Sending explicit ACK-only packet (Size: {})."), clientId_, packetDataToSend.size());
                    sendto(clientSocket_, reinterpret_cast<const char*>(packetDataToSend.data()), static_cast<int>(packetDataToSend.size()),
                        0, (const sockaddr*)&serverAddr_, sizeof(serverAddr_));
                }
            }
        );
    }

    void run_spam_loop(long long test_duration_seconds, int actions_per_second_target) {
        if (!isValid()) {
            RF_CORE_ERROR(FMT_STRING("[Client {}] Invalid socket, cannot run spam loop."), clientId_);
            return;
        }

        send_join_request();
        joinState_ = PlayerJoinState::AttemptingJoin;
        RF_CORE_INFO(FMT_STRING("[Client {}] Attempting to join server (Char: {})..."), clientId_, characterIdForJoin_);

        std::random_device rd;
        std::mt19937 gen(rd() ^ (static_cast<unsigned int>(clientId_)) ^ static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        std::uniform_int_distribution<> action_dist(0, 3); // Ping, Movement, Attack, RiftStep
        std::uniform_real_distribution<float> coord_dist(-1.0f, 1.0f);
        std::uniform_real_distribution<float> small_coord_dist(-0.5f, 0.5f);


        auto test_start_time = std::chrono::steady_clock::now();
        auto last_action_time = test_start_time;
        auto last_reliability_check_time = test_start_time;
        auto last_join_resend_time = test_start_time;

        long long action_interval_ms = (actions_per_second_target > 0) ? (1000 / actions_per_second_target) : 1000;
        if (action_interval_ms <= 0) action_interval_ms = 100; // Min 100ms interval

        while (!stop_running_.load(std::memory_order_relaxed)) {
            auto current_loop_time = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(current_loop_time - test_start_time).count() >= test_duration_seconds) {
                RF_CORE_INFO(FMT_STRING("[Client {}] Test duration {}s ended."), clientId_, test_duration_seconds);
                break;
            }

            receive_server_packets();

            if (joinState_ == PlayerJoinState::FailedToJoin) {
                RF_CORE_INFO(FMT_STRING("[Client {}] Join failed, terminating spam loop."), clientId_);
                break;
            }

            if (std::chrono::duration_cast<std::chrono::milliseconds>(current_loop_time - last_reliability_check_time).count() >= CLIENT_RELIABILITY_CHECK_INTERVAL_MS) {
                ManageReliability();
                last_reliability_check_time = current_loop_time;
            }

            if (joinState_ == PlayerJoinState::AttemptingJoin) {
                if (std::chrono::duration_cast<std::chrono::milliseconds>(current_loop_time - last_join_resend_time).count() >= JOIN_RESEND_INTERVAL_MS) {
                    RF_CORE_INFO(FMT_STRING("[Client {}] Resending Join Request (Char: {})..."), clientId_, characterIdForJoin_);
                    send_join_request();
                    last_join_resend_time = current_loop_time;
                }
            }
            else if (joinState_ == PlayerJoinState::Joined) {
                if (std::chrono::duration_cast<std::chrono::milliseconds>(current_loop_time - last_action_time).count() >= action_interval_ms) {
                    last_action_time = current_loop_time;
                    int action_type = action_dist(gen);
                    try {
                        switch (action_type) {
                        case 0: {
                            float dx = small_coord_dist(gen); float dy = small_coord_dist(gen);
                            if (std::abs(dx) > 0.01f || std::abs(dy) > 0.01f) {
                                send_movement_input(dx, dy, 0.0f, (gen() % 10 < 2)); // 20% chance sprint
                            }
                            break;
                        }
                        case 1: send_basic_attack(coord_dist(gen), coord_dist(gen), 0.0f); break;
                        case 2: send_rift_step_dodge(static_cast<RF_C2S::RiftStepDirectionalIntent>(gen() % RF_C2S::RiftStepDirectionalIntent_MAX + RF_C2S::RiftStepDirectionalIntent_MIN)); break;
                        case 3: send_ping(); break;
                        }
                    }
                    catch (const std::exception& e) {
                        RF_CORE_ERROR(FMT_STRING("[Client {}] Exception during action: {}"), clientId_, e.what());
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Loop delay
        }
        RF_CORE_INFO(FMT_STRING("[Client {}] Spam loop finished (State: {}). Assigned ID by Server: {}"),
            clientId_, static_cast<int>(joinState_), serverAssignedPlayerId_);
    }
}; // End of SimulatedPlayer class

int main() {
    const int NUM_CONCURRENT_CLIENTS = 50; // Keep low for initial testing after changes
    const std::string SERVER_IP = "192.168.50.186"; // Ensure this matches server config
    const int SERVER_PORT = 12345;             // Ensure this matches server config
    const long long TEST_DURATION_SECONDS = 60;
    const int ACTIONS_PER_SECOND_PER_CLIENT = 5; // Moderate action rate

    RiftForged::Utilities::Logger::Init(spdlog::level::info, spdlog::level::trace, "logs/stress_client.log");
    RF_CORE_INFO(FMT_STRING("[StressTest] WSAStartup..."));
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        RF_CORE_CRITICAL(FMT_STRING("[StressTest] WSAStartup failed: {}"), WSAGetLastError());
        return 1;
    }
    RF_CORE_INFO(FMT_STRING("[StressTest] WSAStartup successful."));

    std::vector<std::thread> client_threads;
    std::vector<std::unique_ptr<SimulatedPlayer>> players_list;
    players_list.reserve(NUM_CONCURRENT_CLIENTS);

    RF_CORE_INFO(FMT_STRING("[StressTest] Launching {} clients for {} seconds, targeting ~{} actions/sec/client."),
        NUM_CONCURRENT_CLIENTS, TEST_DURATION_SECONDS, ACTIONS_PER_SECOND_PER_CLIENT);

    for (int i = 0; i < NUM_CONCURRENT_CLIENTS; ++i) {
        try {
            players_list.push_back(std::make_unique<SimulatedPlayer>(static_cast<uint64_t>(i + 1001), SERVER_IP, SERVER_PORT));
        }
        catch (const std::exception& e) {
            RF_CORE_CRITICAL(FMT_STRING("[StressTest] Failed to construct client {}: {}"), (i + 1001), e.what());
        }
        if ((i + 1) % 5 == 0 || (i + 1) == NUM_CONCURRENT_CLIENTS) { // Log more frequently for smaller numbers
            RF_CORE_INFO(FMT_STRING("[StressTest] Constructed {} client objects..."), players_list.size());
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Small delay for staggered start
        }
    }

    RF_CORE_INFO(FMT_STRING("[StressTest] {} clients constructed. Launching threads..."), players_list.size());

    for (auto& player_ptr : players_list) {
        if (player_ptr && player_ptr->isValid()) {
            client_threads.emplace_back([p = player_ptr.get(), TEST_DURATION_SECONDS, ACTIONS_PER_SECOND_PER_CLIENT]() {
                try {
                    p->run_spam_loop(TEST_DURATION_SECONDS, ACTIONS_PER_SECOND_PER_CLIENT);
                }
                catch (const std::exception& e) {
                    RF_CORE_ERROR(FMT_STRING("[Client {} Thread] Unhandled exception: {}"), p->clientId_, e.what());
                }
                catch (...) {
                    RF_CORE_ERROR(FMT_STRING("[Client {} Thread] Unknown unhandled exception."), p->clientId_);
                }
                });
        }
        else {
            RF_CORE_WARN(FMT_STRING("[StressTest] Skipping invalid or null player unique_ptr for thread launch."));
        }
    }

    RF_CORE_INFO(FMT_STRING("[StressTest] All {} valid client threads launched. Waiting for test completion..."), client_threads.size());

    for (std::thread& t : client_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    RF_CORE_INFO(FMT_STRING("[StressTest] All clients finished."));
    WSACleanup();
    RiftForged::Utilities::Logger::FlushAll();
    RiftForged::Utilities::Logger::Shutdown();

    std::cout << "\n[StressTest] Application shut down. Log file: logs/stress_client.log" << std::endl;
    return 0;
}