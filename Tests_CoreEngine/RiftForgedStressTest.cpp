// RiftForgedStressTest.cpp
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h> // For inet_pton
#include <windows.h>

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <cstdint>
#include <cstring>   // For memcpy
#include <stdexcept> // For std::runtime_error
#include <atomic>    // For std::atomic_bool


// Link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

// --- Your Game-Specific Headers ---
#include "../NetworkEngine/GamePacketHeader.h" 
#include "flatbuffers/flatbuffers.h" 
#include "../SharedProtocols/Generated/riftforged_common_types_generated.h" 
#include "../SharedProtocols/Generated/riftforged_c2s_udp_messages_generated.h" 

// Namespaces for convenience
namespace RF_C2S = RiftForged::Networking::UDP::C2S;
namespace RF_Shared = RiftForged::Networking::Shared;
namespace RF_Net = RiftForged::Networking;

// Helper for sequence number comparison (handles wrap-around)
inline bool is_sequence_greater(uint32_t s1, uint32_t s2) {
    return ((s1 > s2) && (s1 - s2 <= (UINT32_MAX / 2))) ||
        ((s1 < s2) && (s2 - s1 > (UINT32_MAX / 2)));
}


class SimulatedPlayer {
public:
    uint64_t clientId_;
    std::string serverIp_;
    int serverPort_;

    SOCKET clientSocket_ = INVALID_SOCKET;
    sockaddr_in serverAddr_{};

    uint32_t clientSequenceNumber_ = 0; // Sequence number for packets sent BY THIS CLIENT

    // --- ACK State for Server Packets ---
    uint32_t serverHighestAckedSeq_ = 0;   // Highest sequence number received from server that we will ACK
    uint32_t serverAckBitfield_ = 0;     // Bitfield for packets received prior to serverHighestAckedSeq_

    flatbuffers::FlatBufferBuilder builder_;
    std::atomic_bool stop_running_ = false;

    char recvBuffer_[2048]; // Buffer for receiving packets

    explicit SimulatedPlayer(uint64_t id, const std::string& ip, int port)
        : clientId_(id), serverIp_(ip), serverPort_(port), builder_(1024) {

        clientSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (clientSocket_ == INVALID_SOCKET) {
            throw std::runtime_error("Client " + std::to_string(clientId_) +
                ": socket() failed with error: " + std::to_string(WSAGetLastError()));
        }

        // Set socket to non-blocking mode
        u_long mode = 1; // 1 to enable non-blocking
        if (ioctlsocket(clientSocket_, FIONBIO, &mode) == SOCKET_ERROR) {
            closesocket(clientSocket_);
            clientSocket_ = INVALID_SOCKET;
            throw std::runtime_error("Client " + std::to_string(clientId_) +
                ": ioctlsocket() FIONBIO failed with error: " + std::to_string(WSAGetLastError()));
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
        stop_running_ = true;
        if (clientSocket_ != INVALID_SOCKET) {
            closesocket(clientSocket_);
            clientSocket_ = INVALID_SOCKET;
        }
    }

    SimulatedPlayer(const SimulatedPlayer&) = delete;
    SimulatedPlayer& operator=(const SimulatedPlayer&) = delete;

    // Basic move constructor and assignment for vector emplace_back
    SimulatedPlayer(SimulatedPlayer&& other) noexcept
        : clientId_(other.clientId_),
        serverIp_(std::move(other.serverIp_)),
        serverPort_(other.serverPort_),
        clientSocket_(other.clientSocket_),
        serverAddr_(other.serverAddr_),
        clientSequenceNumber_(other.clientSequenceNumber_),
        serverHighestAckedSeq_(other.serverHighestAckedSeq_),
        serverAckBitfield_(other.serverAckBitfield_),
        builder_(1024), // Each player needs its own builder
        stop_running_(false) // Initial value
    {
        // Steal resources
        other.clientSocket_ = INVALID_SOCKET; // Prevent double close
        // No need to move recvBuffer_ as it's stack allocated in this version
    }


    SimulatedPlayer& operator=(SimulatedPlayer&& other) noexcept {
        if (this != &other) {
            if (clientSocket_ != INVALID_SOCKET) {
                closesocket(clientSocket_);
            }
            clientId_ = other.clientId_;
            serverIp_ = std::move(other.serverIp_);
            serverPort_ = other.serverPort_;
            clientSocket_ = other.clientSocket_;
            serverAddr_ = other.serverAddr_;
            clientSequenceNumber_ = other.clientSequenceNumber_;
            serverHighestAckedSeq_ = other.serverHighestAckedSeq_;
            serverAckBitfield_ = other.serverAckBitfield_;
            builder_.Clear(); // Reset own builder
            stop_running_ = other.stop_running_.load();

            other.clientSocket_ = INVALID_SOCKET;
        }
        return *this;
    }


    bool isValid() const noexcept {
        return clientSocket_ != INVALID_SOCKET;
    }

    // Processes a received server sequence number to update our ACK state
    void update_ack_state(uint32_t received_server_seq) {
        if (is_sequence_greater(received_server_seq, serverHighestAckedSeq_)) {
            // New highest sequence number received
            uint32_t diff = received_server_seq - serverHighestAckedSeq_; // Handles wrap implicitly due to uint32_t math if is_sequence_greater is correct

            if (diff >= 32) { // If the jump is too large, old bitfield info is lost
                serverAckBitfield_ = 0;
            }
            else {
                // Shift the existing bitfield to make room for the new history.
                // The bit for the old 'serverHighestAckedSeq_' needs to be set.
                serverAckBitfield_ = (serverAckBitfield_ << diff) | (1U << (diff - 1));
            }
            serverHighestAckedSeq_ = received_server_seq;
        }
        else if (received_server_seq < serverHighestAckedSeq_) {
            // Older packet received (possibly out of order)
            uint32_t diff = serverHighestAckedSeq_ - received_server_seq;
            if (diff > 0 && diff <= 32) { // Check if it falls within our current bitfield window
                serverAckBitfield_ |= (1U << (diff - 1)); // Bit 0 is for Seq-1, Bit 1 for Seq-2 etc.
            }
        }
        // If received_server_seq == serverHighestAckedSeq_, it's a duplicate, no change to ACK state needed.
    }


    void receive_server_packets() {
        if (!isValid()) return;

        int bytes_received;
        sockaddr_in sender_addr;
        int sender_addr_len = sizeof(sender_addr);

        while (true) { // Loop to drain the socket
            bytes_received = recvfrom(clientSocket_,
                recvBuffer_,
                sizeof(recvBuffer_),
                0,
                (sockaddr*)&sender_addr,
                &sender_addr_len);

            if (bytes_received == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    break; // No more data to read for now
                }
                else {
                    // Real error
                    // std::cerr << "[Client " << clientId_ << "] recvfrom failed with error: " << error << std::endl;
                    // Consider if this client should be marked invalid or stop
                    break;
                }
            }

            if (bytes_received < static_cast<int>(RF_Net::GetGamePacketHeaderSize())) {
                // Packet too small to be valid
                continue;
            }

            RF_Net::GamePacketHeader server_header;
            memcpy(&server_header, recvBuffer_, RF_Net::GetGamePacketHeaderSize());

            // TODO: Validate server_header.protocolId if necessary

            // Update our ACK state based on the server's sequence number
            update_ack_state(server_header.sequenceNumber);

            // For a stress client, we might not fully process the payload,
            // but if server sends reliable messages that client needs to respond to
            // (other than just ACKs), that logic would go here.
            // For now, just focusing on ACKing.
        }
    }


    void send_packet_internal(RF_Net::MessageType msgType, const uint8_t* flatbuffer_data, size_t flatbuffer_size) {
        if (!isValid()) return;

        RF_Net::GamePacketHeader header;
        header.protocolId = RF_Net::CURRENT_PROTOCOL_ID_VERSION;
        header.messageType = msgType;
        header.sequenceNumber = ++clientSequenceNumber_; // Client's own outgoing sequence

        // Populate with ACK info for server packets
        header.ackNumber = serverHighestAckedSeq_;
        header.ackBitfield = serverAckBitfield_;

        header.flags = 0; // Set appropriate flags if this packet itself is reliable etc.
        // For this client, all sends are unreliable from its perspective.

        std::vector<uint8_t> packet_buffer;
        // Ensure enough capacity upfront
        packet_buffer.reserve(RF_Net::GetGamePacketHeaderSize() + flatbuffer_size);
        packet_buffer.resize(RF_Net::GetGamePacketHeaderSize());
        memcpy(packet_buffer.data(), &header, RF_Net::GetGamePacketHeaderSize());

        if (flatbuffer_data && flatbuffer_size > 0) {
            packet_buffer.insert(packet_buffer.end(), flatbuffer_data, flatbuffer_data + flatbuffer_size);
        }

        sendto(clientSocket_,
            reinterpret_cast<const char*>(packet_buffer.data()),
            static_cast<int>(packet_buffer.size()),
            0,
            (const sockaddr*)&serverAddr_,
            sizeof(serverAddr_));
        // Error checking for sendto omitted for brevity as in original
    }

    static uint64_t current_timestamp_ms() { /* ... as before ... */
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }

    void send_movement_input(float local_x, float local_y, float local_z, bool is_sprinting) { /* ... as before ... */
        builder_.Clear();
        RF_Shared::Vec3 local_dir_struct(local_x, local_y, local_z);
        auto movement_msg_offset = RF_C2S::CreateC2S_MovementInputMsg(builder_, current_timestamp_ms(), &local_dir_struct, is_sprinting);
        RF_C2S::Root_C2S_UDP_MessageBuilder root_builder(builder_);
        root_builder.add_payload_type(RF_C2S::C2S_UDP_Payload_MovementInput);
        root_builder.add_payload(movement_msg_offset.Union());
        auto root_finished_offset = root_builder.Finish();
        builder_.Finish(root_finished_offset);
        send_packet_internal(RF_Net::MessageType::C2S_MovementInput, builder_.GetBufferPointer(), builder_.GetSize());
    }
    void send_basic_attack(float aim_x, float aim_y, float aim_z, uint64_t target_id = 0) { /* ... as before ... */
        builder_.Clear();
        RF_Shared::Vec3 aim_dir_struct(aim_x, aim_y, aim_z);
        auto attack_msg_offset = RF_C2S::CreateC2S_BasicAttackIntentMsg(builder_, current_timestamp_ms(), &aim_dir_struct, target_id);
        RF_C2S::Root_C2S_UDP_MessageBuilder root_builder(builder_);
        root_builder.add_payload_type(RF_C2S::C2S_UDP_Payload_BasicAttackIntent);
        root_builder.add_payload(attack_msg_offset.Union());
        auto root_finished_offset = root_builder.Finish();
        builder_.Finish(root_finished_offset);
        send_packet_internal(RF_Net::MessageType::C2S_BasicAttackIntent, builder_.GetBufferPointer(), builder_.GetSize());
    }
    void send_rift_step_dodge(RF_C2S::RiftStepDirectionalIntent intent) { /* ... as before ... */
        builder_.Clear();
        auto dodge_msg_offset = RF_C2S::CreateC2S_RiftStepActivationMsg(builder_, current_timestamp_ms(), intent);
        RF_C2S::Root_C2S_UDP_MessageBuilder root_builder(builder_);
        root_builder.add_payload_type(RF_C2S::C2S_UDP_Payload_RiftStepActivation);
        root_builder.add_payload(dodge_msg_offset.Union());
        auto root_finished_offset = root_builder.Finish();
        builder_.Finish(root_finished_offset);
        send_packet_internal(RF_Net::MessageType::C2S_RiftStepActivation, builder_.GetBufferPointer(), builder_.GetSize());
    }

    void run_spam_loop(long long test_duration_seconds, int actions_per_second_target) {
        if (!isValid()) {
            std::cerr << "[Client " << clientId_ << "] Not running spam loop due to invalid/uninitialized state." << std::endl;
            return;
        }

        std::random_device rd;
        std::mt19937 gen(rd() ^ (static_cast<unsigned int>(clientId_) << 16) ^ static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        std::uniform_int_distribution<> action_dist(0, 2);
        std::uniform_real_distribution<float> coord_dist(-1.0f, 1.0f);

        auto test_start_time = std::chrono::steady_clock::now();
        auto last_action_time = test_start_time; // For timing actions

        long long action_interval_ms = (actions_per_second_target > 0) ? (1000 / actions_per_second_target) : 1000;
        if (action_interval_ms <= 0) action_interval_ms = 1;


        while (!stop_running_.load()) {
            auto current_loop_time = std::chrono::steady_clock::now();
            auto elapsed_seconds_total = std::chrono::duration_cast<std::chrono::seconds>(current_loop_time - test_start_time).count();
            if (elapsed_seconds_total >= test_duration_seconds) {
                break;
            }

            // Attempt to receive packets
            receive_server_packets();

            // Check if it's time to send an action
            auto time_since_last_action = std::chrono::duration_cast<std::chrono::milliseconds>(current_loop_time - last_action_time).count();
            if (time_since_last_action >= action_interval_ms) {
                last_action_time = current_loop_time; // Reset for next action time
                int action_type = action_dist(gen);
                try {
                    switch (action_type) {
                    case 0:
                        send_movement_input(coord_dist(gen), coord_dist(gen), 0.0f, (gen() % 3 == 0));
                        break;
                    case 1:
                        send_basic_attack(coord_dist(gen), coord_dist(gen), 0.0f);
                        break;
                    case 2:
                        send_rift_step_dodge(static_cast<RF_C2S::RiftStepDirectionalIntent>(gen() % 5));
                        break;
                    }
                }
                catch (const std::exception& e) {
                    std::cerr << "[Client " << clientId_ << "] Exception during send action: " << e.what() << std::endl;
                }
            }

            // Small sleep to prevent busy-waiting if actions are infrequent or no receives
            // Adjust this sleep based on desired responsiveness vs CPU usage.
            // If actions_per_second_target is high, this sleep might be very short or skipped.
            // A more sophisticated approach might use select() or other event mechanisms for receives,
            // but for a spam loop, periodic checks are simpler.
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Check for packets/send actions frequently
        }
        std::cout << "[Client " << clientId_ << "] Spam loop finished." << std::endl;
    }
};


int main() {
    const int NUM_CONCURRENT_CLIENTS = 5;    // Start with a smaller number for testing ACKs
    const std::string SERVER_IP = "127.0.0.1";
    const int SERVER_PORT = 12345;            // Your game server's port
    const long long TEST_DURATION_SECONDS = 60;
    const int ACTIONS_PER_SECOND_PER_CLIENT = 2;

    WSADATA wsaData;
    int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsa_result != 0) {
        std::cerr << "WSAStartup failed with error: " << wsa_result << std::endl;
        return 1;
    }

    std::vector<std::thread> client_threads;
    // Use a vector of SimulatedPlayer objects to keep them in scope until threads join
    std::vector<SimulatedPlayer> players;
    players.reserve(NUM_CONCURRENT_CLIENTS);


    std::cout << "[StressTest] Launching " << NUM_CONCURRENT_CLIENTS << " clients for "
        << TEST_DURATION_SECONDS << " seconds, targeting ~"
        << ACTIONS_PER_SECOND_PER_CLIENT << " actions/sec/client." << std::endl;

    for (int i = 0; i < NUM_CONCURRENT_CLIENTS; ++i) {
        try {
            // Create player and move it into the vector first
            players.emplace_back(static_cast<uint64_t>(i + 1), SERVER_IP, SERVER_PORT);
        }
        catch (const std::exception& e) {
            std::cerr << "[StressTest] Failed to construct client " << (i + 1) << ": " << e.what() << std::endl;
            continue; // Skip this client if construction failed
        }
    }

    // Launch threads after all players that could be constructed are in the vector
    for (size_t i = 0; i < players.size(); ++i) {
        // Pass a pointer or reference to the player object in the vector
        client_threads.emplace_back([player_ptr = &players[i], TEST_DURATION_SECONDS, ACTIONS_PER_SECOND_PER_CLIENT]() {
            if (player_ptr->isValid()) {
                player_ptr->run_spam_loop(TEST_DURATION_SECONDS, ACTIONS_PER_SECOND_PER_CLIENT);
            }
            else {
                std::cerr << "[StressTest] Client " << player_ptr->clientId_ << " was not valid for run_spam_loop." << std::endl;
            }
            });
        if (players.size() > 20 && (i + 1) % 20 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }


    std::cout << "[StressTest] All " << client_threads.size() << " client threads launched. Waiting for test completion..." << std::endl;

    for (std::thread& t : client_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    std::cout << "[StressTest] All clients finished." << std::endl;
    // players vector will go out of scope here, calling destructors

    WSACleanup();
    return 0;
}