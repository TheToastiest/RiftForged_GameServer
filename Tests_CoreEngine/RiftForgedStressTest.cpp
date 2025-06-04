// File: RiftForgedStressTest.cpp
// (Corrected join request/response logic for stress test clients)

// Standard C++ Includes
//#include <iostream>
//#include <vector>
//#include <string>
//#include <thread>
//#include <chrono>
//#include <random>
//#include <cstdint>
//#include <cstring>
//#include <stdexcept>
//#include <atomic>
//#include <memory>
//#include <algorithm>
//#include <cmath> // For std::abs in random movement
//
// Winsock
//#ifndef WIN32_LEAN_AND_MEAN
//#define WIN32_LEAN_AND_MEAN
//#endif
//#ifndef NOMINMAX
//#define NOMINMAX
//#endif
//#include <winsock2.h>
//#include <ws2tcpip.h>
//#pragma comment(lib, "Ws2_32.lib")
//#include <Windows.h> // For GetAsyncKeyState (though not used in spam_loop)

// FlatBuffers & Project Headers
//#include "flatbuffers/flatbuffers.h"
//#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h"
//#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"
//#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h"
//#include "../NetworkEngine/GamePacketHeader.h"
//#include "../NetworkEngine/UDPReliabilityProtocol.h"
//#include "../Utils/Logger.h"

// Namespaces for convenience
//namespace RF_C2S = RiftForged::Networking::UDP::C2S;
//namespace RF_S2C = RiftForged::Networking::UDP::S2C;
//namespace RF_Shared = RiftForged::Networking::Shared;
//namespace RF_Net = RiftForged::Networking;

//class SimulatedPlayer {
//public:
//    uint64_t clientId_; // Client-side self-assigned ID for stress test tracking
//    std::string serverIp_;
//    int serverPort_;
//
//    SOCKET clientSocket_ = INVALID_SOCKET;
//    sockaddr_in serverAddr_{};
//
//    std::unique_ptr<RF_Net::ReliableConnectionState> connectionState_;
//    flatbuffers::FlatBufferBuilder builder_;
//    std::atomic_bool stop_running_ = { false };
//
//    char recvBuffer_[2048];
//
//    enum class PlayerJoinState {
//        NotConnected,
//        AttemptingJoin,
//        Joined,
//        FailedToJoin
//    };
//    PlayerJoinState joinState_ = PlayerJoinState::NotConnected;
//    uint64_t serverAssignedPlayerId_ = 0;
//    std::string characterIdForJoin_ = "StressChar_";
//
//    static const int MAX_CLIENT_PACKET_RETRIES = 3;
//    static const long CLIENT_RELIABILITY_CHECK_INTERVAL_MS = 100;
//    static const long CLIENT_EXPLICIT_ACK_DELAY_MS = 50;
//    static const long JOIN_RESEND_INTERVAL_MS = 2000;
//
//    explicit SimulatedPlayer(uint64_t id, const std::string& ip, int port)
//        : clientId_(id),
//        serverIp_(ip),
//        serverPort_(port),
//        builder_(512),
//        connectionState_(std::make_unique<RF_Net::ReliableConnectionState>()) {
//
//        characterIdForJoin_ += std::to_string(id);
//
//        clientSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
//        if (clientSocket_ == INVALID_SOCKET) {
//            throw std::runtime_error("Client " + std::to_string(clientId_) +
//                ": socket() failed: " + std::to_string(WSAGetLastError()));
//        }
//
//        sockaddr_in clientLocalAddr;
//        clientLocalAddr.sin_family = AF_INET;
//        clientLocalAddr.sin_addr.s_addr = INADDR_ANY;
//        clientLocalAddr.sin_port = 0;
//        if (bind(clientSocket_, (sockaddr*)&clientLocalAddr, sizeof(clientLocalAddr)) == SOCKET_ERROR) {
//            std::cerr << "[Client " << clientId_ << "] bind() failed: " << WSAGetLastError() << ". May affect receiving." << std::endl;
//        }
//
//        DWORD timeout = 1;
//        if (setsockopt(clientSocket_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
//            std::cerr << "[Client " << clientId_ << "] setsockopt SO_RCVTIMEO failed: " << WSAGetLastError() << std::endl;
//        }
//
//        serverAddr_.sin_family = AF_INET;
//        serverAddr_.sin_port = htons(static_cast<u_short>(serverPort_));
//        if (inet_pton(AF_INET, serverIp_.c_str(), &serverAddr_.sin_addr) <= 0) {
//            closesocket(clientSocket_);
//            clientSocket_ = INVALID_SOCKET;
//            throw std::runtime_error("Client " + std::to_string(clientId_) +
//                ": inet_pton() failed for IP " + serverIp_ +
//                " with error: " + std::to_string(WSAGetLastError()));
//        }
//    }
//
//    ~SimulatedPlayer() {
//        stop_running_ = true;
//        if (clientSocket_ != INVALID_SOCKET) {
//            closesocket(clientSocket_);
//            clientSocket_ = INVALID_SOCKET;
//        }
//    }
//
//    SimulatedPlayer(const SimulatedPlayer&) = delete;
//    SimulatedPlayer& operator=(const SimulatedPlayer&) = delete;
//    SimulatedPlayer(SimulatedPlayer&&) = default;
//    SimulatedPlayer& operator=(SimulatedPlayer&&) = default;
//
//    bool isValid() const noexcept {
//        return clientSocket_ != INVALID_SOCKET;
//    }
//
//    static uint64_t current_timestamp_ms() {
//        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
//            std::chrono::system_clock::now().time_since_epoch()).count());
//    }

    ////void send_packet_internal(RF_Net::MessageType msgType,
    //    const uint8_t* flatbuffer_data,
    //    size_t flatbuffer_size,
    //    bool isReliable) {
    //    if (!isValid() || !connectionState_) return;
    //    uint8_t packetFlags = static_cast<uint8_t>(RF_Net::GamePacketFlag::NONE);
    //    if (isReliable) {
    //        packetFlags |= static_cast<uint8_t>(RF_Net::GamePacketFlag::IS_RELIABLE);
    //    }
    //    std::vector<uint8_t> packet_buffer_uint8 = RF_Net::PrepareOutgoingPacket(
    //        *connectionState_, msgType, flatbuffer_data, static_cast<uint16_t>(flatbuffer_size), packetFlags);

    //    if (packet_buffer_uint8.empty()) {
            // This can happen if reliability protocol decides not to send (e.g. max retries for an ACK_ONLY)
            // For a new message, it's less common but possible if flatbuffer_size is 0 for a reliable message needing payload.
            // std::cerr << "[Client " << clientId_ << "] PrepareOutgoingPacket returned empty buffer for MsgType: "
            //           << static_cast<int>(msgType) << ". Not sending." << std::endl;
           //return;
        //}
        //sendto(clientSocket_, reinterpret_cast<const char*>(packet_buffer_uint8.data()), static_cast<int>(packet_buffer_uint8.size()),
    //        0, (const sockaddr*)&serverAddr_, sizeof(serverAddr_));
    //}

    //void send_join_request() {
    //    builder_.Clear();

    //    // Create the character_id_to_load string for the FlatBuffer
    //    auto charIdOffset = characterIdForJoin_.empty() ? 0 : builder_.CreateString(characterIdForJoin_);

    //    // Create the C2S_JoinRequestMsg payload using the correct FlatBuffer type
    //    auto join_request_payload = RF_C2S::CreateC2S_JoinRequestMsg(
    //        builder_,
    //        current_timestamp_ms(),
    //        charIdOffset
    //    );

    //    RF_C2S::Root_C2S_UDP_MessageBuilder root_builder(builder_);
    //    // Set the payload type in the Root_C2S_UDP_Message to indicate it's a JoinRequest
    //    root_builder.add_payload_type(RF_C2S::C2S_UDP_Payload_JoinRequest);
    //    root_builder.add_payload(join_request_payload.Union()); // Add the actual join request payload

    //    auto root_message_offset = root_builder.Finish();
    //    builder_.Finish(root_message_offset);

    //    // Log before sending (optional, but good for debugging)
    //    // std::cout << "[Client " << clientId_ << "] Sending actual Join Request (Char: " << characterIdForJoin_ << ")" << std::endl;

    //    send_packet_internal(RF_Net::MessageType::C2S_JoinRequest, builder_.GetBufferPointer(), builder_.GetSize(), true);
    //}

    //void send_movement_input(float local_x, float local_y, float local_z, bool is_sprinting) {
    //    builder_.Clear(); RF_Shared::Vec3 ld(local_x, local_y, local_z);
    //    auto p = RF_C2S::CreateC2S_MovementInputMsg(builder_, current_timestamp_ms(), &ld, is_sprinting);
    //    RF_C2S::Root_C2S_UDP_MessageBuilder rb(builder_); rb.add_payload_type(RF_C2S::C2S_UDP_Payload_MovementInput); rb.add_payload(p.Union());
    //    builder_.Finish(rb.Finish()); send_packet_internal(RF_Net::MessageType::C2S_MovementInput, builder_.GetBufferPointer(), builder_.GetSize(), false);
    //}
    //void send_basic_attack(float ax, float ay, float az, uint64_t tid = 0) {
        //builder_.Clear(); RF_Shared::Vec3 ad(ax, ay, az);
        //auto p = RF_C2S::CreateC2S_BasicAttackIntentMsg(builder_, current_timestamp_ms(), &ad, tid);
        //RF_C2S::Root_C2S_UDP_MessageBuilder rb(builder_); rb.add_payload_type(RF_C2S::C2S_UDP_Payload_BasicAttackIntent); rb.add_payload(p.Union());
        //builder_.Finish(rb.Finish()); send_packet_internal(RF_Net::MessageType::C2S_BasicAttackIntent, builder_.GetBufferPointer(), builder_.GetSize(), true);
    //}
    //void send_rift_step_dodge(RF_C2S::RiftStepDirectionalIntent intent) {
        //builder_.Clear();
        //auto p = RF_C2S::CreateC2S_RiftStepActivationMsg(builder_, current_timestamp_ms(), intent);
        //RF_C2S::Root_C2S_UDP_MessageBuilder rb(builder_); rb.add_payload_type(RF_C2S::C2S_UDP_Payload_RiftStepActivation); rb.add_payload(p.Union());
        //builder_.Finish(rb.Finish()); send_packet_internal(RF_Net::MessageType::C2S_RiftStepActivation, builder_.GetBufferPointer(), builder_.GetSize(), true);
    //}
    //void send_ping() {
        //builder_.Clear();
        //auto p = RF_C2S::CreateC2S_PingMsg(builder_, current_timestamp_ms());
        //RF_C2S::Root_C2S_UDP_MessageBuilder rb(builder_); rb.add_payload_type(RF_C2S::C2S_UDP_Payload_Ping); rb.add_payload(p.Union());
        //builder_.Finish(rb.Finish()); send_packet_internal(RF_Net::MessageType::C2S_Ping, builder_.GetBufferPointer(), builder_.GetSize(), true);
    //}


    //void receive_server_packets() {
    //    if (!isValid() || !connectionState_) return;
    //    int bytes_received;
    //    sockaddr_in sender_addr; int sender_addr_len = sizeof(sender_addr);

    //    while (true) { // Loop to process all immediately available packets
    //        bytes_received = recvfrom(clientSocket_, recvBuffer_, sizeof(recvBuffer_), 0, (sockaddr*)&sender_addr, &sender_addr_len);
    //        if (bytes_received == SOCKET_ERROR) {
    //            if (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAETIMEDOUT) break; // No more data
    //            std::cerr << "[Client " << clientId_ << "] recvfrom error: " << WSAGetLastError() << std::endl;
    //            joinState_ = PlayerJoinState::FailedToJoin; // Assume connection lost on other errors
    //            stop_running_ = true;
    //            break;
    //        }
    //        if (bytes_received < static_cast<int>(RF_Net::GetGamePacketHeaderSize())) {
    //            // std::cerr << "[Client " << clientId_ << "] Received packet too small." << std::endl;
    //            continue; // Too small to be valid
    //        }

    //        RF_Net::GamePacketHeader server_header;
    //        memcpy(&server_header, recvBuffer_, RF_Net::GetGamePacketHeaderSize());
    //        if (server_header.protocolId != RF_Net::CURRENT_PROTOCOL_ID_VERSION) {
    //            // std::cerr << "[Client " << clientId_ << "] Mismatched protocol ID." << std::endl;
    //            continue; // Mismatched protocol
    //        }

    //        const uint8_t* s2c_payload_after_header_ptr = reinterpret_cast<const uint8_t*>(recvBuffer_ + RF_Net::GetGamePacketHeaderSize());
    //        uint16_t s2c_payload_after_header_len = static_cast<uint16_t>(bytes_received - RF_Net::GetGamePacketHeaderSize());
    //        const uint8_t* app_payload_to_process_ptr = nullptr;
    //        uint16_t app_payload_size = 0;

    //        bool should_process_app_payload = RF_Net::ProcessIncomingPacketHeader(
    //            *connectionState_, server_header, s2c_payload_after_header_ptr, s2c_payload_after_header_len,
    //            &app_payload_to_process_ptr, &app_payload_size);

    //        if (should_process_app_payload && app_payload_to_process_ptr && app_payload_size > 0) {
    //            flatbuffers::Verifier verifier(app_payload_to_process_ptr, app_payload_size);
    //            if (!RF_S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) {
    //                std::cerr << "[Client " << clientId_ << "] S2C FlatBuffer verification failed for MsgType: " << static_cast<int>(server_header.messageType) << std::endl;
    //                continue;
    //            }
    //            auto root = RF_S2C::GetRoot_S2C_UDP_Message(app_payload_to_process_ptr);
    //            if (!root) {
    //                std::cerr << "[Client " << clientId_ << "] GetRoot_S2C_UDP_Message failed for MsgType: " << static_cast<int>(server_header.messageType) << std::endl;
    //                continue;
    //            }

    //            switch (server_header.messageType) {
    //            case RF_Net::MessageType::S2C_JoinSuccess: {
    //                if (root->payload_type() == RF_S2C::S2C_UDP_Payload_S2C_JoinSuccessMsg) {
    //                    auto join_success_msg = root->payload_as_S2C_JoinSuccessMsg();
    //                    if (join_success_msg) {
    //                        serverAssignedPlayerId_ = join_success_msg->assigned_player_id();
    //                        if (joinState_ != PlayerJoinState::Joined) {
    //                            std::cout << "[Client " << clientId_ << " SID: " << serverAssignedPlayerId_
    //                                << "] JOIN SUCCESSFUL. Server Tick: " << join_success_msg->server_tick_rate_hz() << "Hz. Msg: "
    //                                << (join_success_msg->welcome_message() ? join_success_msg->welcome_message()->c_str() : "")
    //                                << std::endl;
    //                        }
    //                        joinState_ = PlayerJoinState::Joined;
    //                    }
    //                    else {
    //                        std::cerr << "[Client " << clientId_ << "] Failed to cast S2C_JoinSuccess payload." << std::endl;
    //                    }
    //                }
    //                else {
    //                    std::cerr << "[Client " << clientId_ << "] Received S2C_JoinSuccess header but wrong payload type: " << static_cast<int>(root->payload_type()) << std::endl;
    //                }
    //                break;
    //            }
    //            case RF_Net::MessageType::S2C_JoinFailed: {
    //                if (root->payload_type() == RF_S2C::S2C_UDP_Payload_S2C_JoinFailedMsg) {
    //                    auto join_failed_msg = root->payload_as_S2C_JoinFailedMsg();
    //                    std::string reason = "Unknown";
    //                    int16_t code = 0;
    //                    if (join_failed_msg) {
    //                        if (join_failed_msg->reason_message()) reason = join_failed_msg->reason_message()->str();
    //                        code = join_failed_msg->reason_code();
    //                    }
    //                    std::cerr << "[Client " << clientId_ << "] JOIN FAILED. Reason: " << reason << " (Code: " << code << "). Stopping." << std::endl;
    //                }
    //                else {
    //                    std::cerr << "[Client " << clientId_ << "] Received S2C_JoinFailed header but wrong payload type: " << static_cast<int>(root->payload_type()) << std::endl;
    //                }
    //                joinState_ = PlayerJoinState::FailedToJoin;
    //                stop_running_ = true;
    //                break;
    //            }
    //            case RF_Net::MessageType::S2C_EntityStateUpdate:
    //                // Minimal processing for stress test
    //                break;
    //            case RF_Net::MessageType::S2C_Pong:
    //                // Minimal processing for stress test
    //                break;
    //            default:
    //                // std::cout << "[Client " << clientId_ << "] Received unhandled S2C message type: " << static_cast<int>(server_header.messageType) << std::endl;
    //                break;
    //            }
    //        }
    //    }
    //}

    //void ManageReliability() {
    //    if (!isValid() || !connectionState_) return;
    //    auto currentTime = std::chrono::steady_clock::now();
    //    std::vector<std::vector<uint8_t>> packetsToResend = RF_Net::GetPacketsForRetransmission(
    //        *connectionState_, currentTime, MAX_CLIENT_PACKET_RETRIES);

    //    for (const auto& packetData : packetsToResend) {
    //        // std::cout << "[Client " << clientId_ << "] Retransmitting packet..." << std::endl;
    //        sendto(clientSocket_, reinterpret_cast<const char*>(packetData.data()), static_cast<int>(packetData.size()),
    //            0, (const sockaddr*)&serverAddr_, sizeof(serverAddr_));
    //    }
    //    if (connectionState_->connectionDroppedByMaxRetries) {
    //        std::cerr << "[Client " << clientId_ << "] Connection dropped by max C2S retries. Stopping." << std::endl;
    //        joinState_ = PlayerJoinState::FailedToJoin;
    //        stop_running_ = true;
    //        return;
    //    }
    //    if (connectionState_->hasPendingAckToSend) {
    //        auto timeSinceLastClientSend = std::chrono::duration_cast<std::chrono::milliseconds>(
    //            currentTime - connectionState_->lastPacketSentTimeToRemote);

    //        if (connectionState_->lastPacketSentTimeToRemote == std::chrono::steady_clock::time_point::min() ||
    //            timeSinceLastClientSend.count() > CLIENT_EXPLICIT_ACK_DELAY_MS) {
    //            // std::cout << "[Client " << clientId_ << "] Sending explicit ACK." << std::endl;
    //            std::vector<uint8_t> ack_packet_uint8 = RF_Net::PrepareOutgoingPacket(
    //                *connectionState_, RF_Net::MessageType::Unknown, nullptr, 0,
    //                static_cast<uint8_t>(RF_Net::GamePacketFlag::IS_RELIABLE) | static_cast<uint8_t>(RF_Net::GamePacketFlag::IS_ACK_ONLY));
    //            if (!ack_packet_uint8.empty()) {
    //                sendto(clientSocket_, reinterpret_cast<const char*>(ack_packet_uint8.data()), static_cast<int>(ack_packet_uint8.size()),
    //                    0, (const sockaddr*)&serverAddr_, sizeof(serverAddr_));
    //            }
    //        }
    //    }
    //}

//    void run_spam_loop(long long test_duration_seconds, int actions_per_second_target) {
//        if (!isValid()) { std::cerr << "[Client " << clientId_ << "] Invalid socket, cannot run." << std::endl; return; }
//
//        send_join_request();
//        joinState_ = PlayerJoinState::AttemptingJoin;
//        std::cout << "[Client " << clientId_ << "] Attempting to join server (Char: " << characterIdForJoin_ << ")..." << std::endl;
//
//        std::random_device rd;
//        std::mt19937 gen(rd() ^ (static_cast<unsigned int>(clientId_)) ^ static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
//        std::uniform_int_distribution<> action_dist(0, 3);
//        std::uniform_real_distribution<float> coord_dist(-1.0f, 1.0f);
//        std::uniform_real_distribution<float> small_coord_dist(-0.1f, 0.1f); // For smaller, more frequent movements
//
//
//        auto test_start_time = std::chrono::steady_clock::now();
//        auto last_action_time = test_start_time;
//        auto last_reliability_check_time = test_start_time;
//        auto last_join_resend_time = test_start_time;
//
//        long long action_interval_ms = (actions_per_second_target > 0) ? (1000 / actions_per_second_target) : 1000;
//        if (action_interval_ms <= 0) action_interval_ms = 100;
//
//        while (!stop_running_.load()) {
//            auto current_loop_time = std::chrono::steady_clock::now();
//            if (std::chrono::duration_cast<std::chrono::seconds>(current_loop_time - test_start_time).count() >= test_duration_seconds) {
//                std::cout << "[Client " << clientId_ << "] Test duration " << test_duration_seconds << "s ended." << std::endl;
//                break;
//            }
//
//            receive_server_packets();
//
//            if (joinState_ == PlayerJoinState::FailedToJoin) {
//                break;
//            }
//
//            // Manage reliability protocol (resends, explicit ACKs)
//            if (std::chrono::duration_cast<std::chrono::milliseconds>(current_loop_time - last_reliability_check_time).count() >= CLIENT_RELIABILITY_CHECK_INTERVAL_MS) {
//                ManageReliability();
//                last_reliability_check_time = current_loop_time;
//            }
//
//
//            if (joinState_ == PlayerJoinState::AttemptingJoin) {
//                if (std::chrono::duration_cast<std::chrono::milliseconds>(current_loop_time - last_join_resend_time).count() >= JOIN_RESEND_INTERVAL_MS) {
//                    std::cout << "[Client " << clientId_ << "] Resending Join Request (Char: " << characterIdForJoin_ << ")..." << std::endl;
//                    send_join_request();
//                    last_join_resend_time = current_loop_time;
//                }
//            }
//            else if (joinState_ == PlayerJoinState::Joined) {
//                if (std::chrono::duration_cast<std::chrono::milliseconds>(current_loop_time - last_action_time).count() >= action_interval_ms) {
//                    last_action_time = current_loop_time;
//                    int action_type = action_dist(gen);
//                    try {
//                        switch (action_type) {
//                        case 0: { // More frequent, smaller movements
//                            float dx = small_coord_dist(gen); float dy = small_coord_dist(gen);
//                            if (std::abs(dx) + std::abs(dy) > 0.01f) { // Only send if there's some movement
//                                send_movement_input(dx, dy, 0.0f, (gen() % 10 == 0)); // 10% chance sprint
//                            }
//                            break;
//                        }
//                        case 1: send_basic_attack(coord_dist(gen), coord_dist(gen), 0.0f); break;
//                        case 2: send_rift_step_dodge(static_cast<RF_C2S::RiftStepDirectionalIntent>(gen() % 5)); break;
//                        case 3: send_ping(); break;
//                        }
//                    }
//                    catch (const std::exception& e) {
//                        std::cerr << "[Client " << clientId_ << "] Exception during action: " << e.what() << std::endl;
//                    }
//                }
//            }
//            std::this_thread::sleep_for(std::chrono::milliseconds(10));
//        }
//        std::cout << "[Client " << clientId_ << "] Spam loop finished (State: " << static_cast<int>(joinState_) << "). Assigned ID by Server: " << serverAssignedPlayerId_ << std::endl;
//    }
//};

//int main() {
//    const int NUM_CONCURRENT_CLIENTS = 50; // Start with a smaller number for testing
//    const std::string SERVER_IP = "192.168.50.186";
//    const int SERVER_PORT = 12345;
//    const long long TEST_DURATION_SECONDS = 60;
//    const int ACTIONS_PER_SECOND_PER_CLIENT = 5; // Reduced for initial testing clarity
//
//    RiftForged::Utilities::Logger::Init(spdlog::level::info, spdlog::level::trace, "logs/stress_client.log"); // Client-specific log
//    RF_CORE_INFO("[StressTest] WSAStartup...");
//    WSADATA wsaData;
//    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
//        RF_CORE_CRITICAL("[StressTest] WSAStartup failed: {}", WSAGetLastError());
//        return 1;
//    }
//    RF_CORE_INFO("[StressTest] WSAStartup successful.");
//
//
//    std::vector<std::thread> client_threads;
//    std::vector<std::unique_ptr<SimulatedPlayer>> players_list;
//    players_list.reserve(NUM_CONCURRENT_CLIENTS);
//
//    RF_CORE_INFO("[StressTest] Launching {} clients for {} seconds, targeting ~{} actions/sec/client.",
//        NUM_CONCURRENT_CLIENTS, TEST_DURATION_SECONDS, ACTIONS_PER_SECOND_PER_CLIENT);
//
//    for (int i = 0; i < NUM_CONCURRENT_CLIENTS; ++i) {
//        try {
//            players_list.push_back(std::make_unique<SimulatedPlayer>(static_cast<uint64_t>(i + 1001), SERVER_IP, SERVER_PORT));
//        }
//        catch (const std::exception& e) {
//            RF_CORE_CRITICAL("[StressTest] Failed to construct client {}: {}", (i + 1001), e.what());
//        }
//        if ((i + 1) % 10 == 0) {
//            RF_CORE_INFO("[StressTest] Constructed {} client objects...", (i + 1));
//            std::this_thread::sleep_for(std::chrono::milliseconds(50));
//        }
//    }
//
//    RF_CORE_INFO("[StressTest] {} clients constructed. Launching threads...", players_list.size());
//
//    for (auto& player_ptr : players_list) {
//        if (player_ptr && player_ptr->isValid()) {
//            client_threads.emplace_back([p = player_ptr.get(), TEST_DURATION_SECONDS, ACTIONS_PER_SECOND_PER_CLIENT]() {
//                p->run_spam_loop(TEST_DURATION_SECONDS, ACTIONS_PER_SECOND_PER_CLIENT);
//                });
//        }
//    }
//
//    RF_CORE_INFO("[StressTest] All {} valid client threads launched. Waiting for test completion...", client_threads.size());
//
//    for (std::thread& t : client_threads) {
//        if (t.joinable()) {
//            t.join();
//        }
//    }
//
//    RF_CORE_INFO("[StressTest] All clients finished.");
//    WSACleanup();
//    RiftForged::Utilities::Logger::FlushAll();
//    RiftForged::Utilities::Logger::Shutdown(); // Ensure logs are written
//
//    std::cout << "\n[StressTest] Application shut down. Log file: logs/stress_client.log" << std::endl;
//    // Removed _getch() for easier non-interactive testing if needed. Add back if you want to pause console.
//    return 0;
//}