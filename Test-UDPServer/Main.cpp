#include <iostream>
#include <vector>
#include <string>
#include <thread>	
#include <chrono>       // For std::chrono to get timestamps
#include <sstream>      // For parsing float inputs for RiftStep
#include <cmath>        // For sqrt, if doing vector normalization client-side (optional)
#include <conio.h>      // For _kbhit() and _getch() on Windows

// FlatBuffers main header
#include "flatbuffers/flatbuffers.h" // Ensure this path is correct for your FB installation

// Generated FlatBuffer headers (adjust path to your SharedProtocols/Generated/ folder)
#include "../FlatBuffers/V0.0.1/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.1/riftforged_s2c_udp_messages_generated.h" // For S2C_RiftStepExecutedMsg
#include "../FlatBuffers/V0.0.1/riftforged_common_types_generated.h" // For Vec3, Quaternion, etc.

// Your custom packet header
#include "../NetworkEngine/GamePacketHeader.h" // Ensure this path is correct


// Winsock
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>   // For inet_pton
#pragma comment(lib, "Ws2_32.lib")


// Constants for the client
const int CLIENT_RECEIVE_BUFFER_SIZE = 4096;
const float CLIENT_RIFTSTEP_DISTANCE = 5.0f; // How far RiftStep goes in current facing direction


// Client's current state (simple global variables for this test client)
RiftForged::Networking::Shared::Vec3 g_client_position(0.0f, 0.0f, 0.0f);
RiftForged::Networking::Shared::Vec3 g_client_orientation_vector(0.0f, 1.0f, 0.0f); // Default: Facing positive Y (North)
uint64_t g_client_player_id = 0; // Will be set by server or assumed for now
uint32_t g_sequenceNumber = 0;
std::string g_last_server_message = "None";

// --- Helper function to build a C2S_PingMsg packet ---
std::vector<char> BuildPingPacket(uint64_t client_timestamp_ms, uint32_t sequence_number) {
    flatbuffers::FlatBufferBuilder builder(256);
    auto ping_payload_offset = RiftForged::Networking::UDP::C2S::CreateC2S_PingMsg(builder, client_timestamp_ms);
    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_Ping);
    root_builder.add_payload(ping_payload_offset.Union());
    auto root_message_offset = root_builder.Finish();
    builder.Finish(root_message_offset);
    RiftForged::Networking::GamePacketHeader header(RiftForged::Networking::MessageType::C2S_Ping, sequence_number);
    header.protocolId = RiftForged::Networking::CURRENT_PROTOCOL_ID_VERSION;
    std::vector<char> packet_buffer(RiftForged::Networking::GetGamePacketHeaderSize() + builder.GetSize());
    memcpy(packet_buffer.data(), &header, RiftForged::Networking::GetGamePacketHeaderSize());
    memcpy(packet_buffer.data() + RiftForged::Networking::GetGamePacketHeaderSize(), builder.GetBufferPointer(), builder.GetSize());
    return packet_buffer;
}

// --- Helper function to build a C2S_MovementInputMsg packet ---
std::vector<char> BuildMovementInputPacket(const RiftForged::Networking::Shared::Vec3& world_space_direction, bool is_sprinting, uint32_t sequence_number) {
    flatbuffers::FlatBufferBuilder builder(256);
    uint64_t client_ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    auto movement_payload_offset = RiftForged::Networking::UDP::C2S::CreateC2S_MovementInputMsg(builder, client_ts, &world_space_direction, is_sprinting);
    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_MovementInput);
    root_builder.add_payload(movement_payload_offset.Union());
    auto root_message_offset = root_builder.Finish();
    builder.Finish(root_message_offset);
    RiftForged::Networking::GamePacketHeader header(RiftForged::Networking::MessageType::C2S_MovementInput, sequence_number);
    header.protocolId = RiftForged::Networking::CURRENT_PROTOCOL_ID_VERSION;
    std::vector<char> packet_buffer(RiftForged::Networking::GetGamePacketHeaderSize() + builder.GetSize());
    memcpy(packet_buffer.data(), &header, RiftForged::Networking::GetGamePacketHeaderSize());
    memcpy(packet_buffer.data() + RiftForged::Networking::GetGamePacketHeaderSize(), builder.GetBufferPointer(), builder.GetSize());
    return packet_buffer;
}

// --- Helper function to build C2S_RiftStepActivationMsg packet ---
std::vector<char> BuildRiftStepActivationPacket(uint64_t client_timestamp_ms,
    RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent intent,
    uint32_t sequence_number) {
    flatbuffers::FlatBufferBuilder builder(256);
    auto riftstep_payload_offset = RiftForged::Networking::UDP::C2S::CreateC2S_RiftStepActivationMsg(builder, client_timestamp_ms, intent);
    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_RiftStepActivation);
    root_builder.add_payload(riftstep_payload_offset.Union());
    auto root_message_offset = root_builder.Finish();
    builder.Finish(root_message_offset);
    RiftForged::Networking::GamePacketHeader header(RiftForged::Networking::MessageType::C2S_RiftStepActivation, sequence_number);
    header.protocolId = RiftForged::Networking::CURRENT_PROTOCOL_ID_VERSION;
    std::vector<char> packet_buffer(RiftForged::Networking::GetGamePacketHeaderSize() + builder.GetSize());
    memcpy(packet_buffer.data(), &header, RiftForged::Networking::GetGamePacketHeaderSize());
    memcpy(packet_buffer.data() + RiftForged::Networking::GetGamePacketHeaderSize(), builder.GetBufferPointer(), builder.GetSize());
    return packet_buffer;
}

// --- Helper function to parse an S2C_PongMsg packet ---
void ParsePongPacket(const uint8_t* fb_payload_ptr, int fb_payload_size) {
    flatbuffers::Verifier verifier(fb_payload_ptr, static_cast<size_t>(fb_payload_size));
    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) {
        g_last_server_message = "Pong Verification Failed"; return;
    }
    auto root_s2c_message = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(fb_payload_ptr);
    if (!root_s2c_message || root_s2c_message->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_Pong) {
        g_last_server_message = "Not a Pong Payload"; return;
    }
    auto pong_msg = root_s2c_message->payload_as_Pong();
    if (!pong_msg) { g_last_server_message = "Failed to get Pong msg"; return; }

    uint64_t original_client_ts = pong_msg->client_timestamp_ms();
    uint64_t now_client_ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    g_last_server_message = "PONG! RTT: " + std::to_string(now_client_ts - original_client_ts) + "ms";
}

// --- Helper function to parse an S2C_EntityStateUpdateMsg packet ---
void ParseEntityStateUpdatePacket(const uint8_t* fb_payload_ptr, int fb_payload_size) {
    flatbuffers::Verifier verifier(fb_payload_ptr, static_cast<size_t>(fb_payload_size));
    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) {
        g_last_server_message = "EntityStateUpdate Verification Failed"; return;
    }
    auto root = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(fb_payload_ptr);
    if (!root || root->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_EntityStateUpdate) {
        g_last_server_message = "Not an EntityStateUpdate Payload"; return;
    }
    auto update = root->payload_as_EntityStateUpdate();
    if (!update) { g_last_server_message = "Failed to get EntityStateUpdate msg"; return; }

    g_last_server_message = "EntityStateUpdate for ID: " + std::to_string(update->entity_id());
    if (g_client_player_id == 0 && update->entity_id() != 0) {
        g_client_player_id = update->entity_id();
        g_last_server_message += " (Assigned as my PlayerID)";
    }

    if (update->entity_id() == g_client_player_id || g_client_player_id == 0) {
        if (update->position()) {
            g_client_position = RiftForged::Networking::Shared::Vec3(update->position()->x(), update->position()->y(), update->position()->z());
        }
        if (update->orientation()) {
            // For simplicity, client doesn't store full quaternion, just updates forward vector if needed
            // This part would need more logic if client fully used quaternions for orientation
        }
    }
    // You can add more details from the update to g_last_server_message if desired
}

// --- Helper function to parse S2C_RiftStepInitiatedMsg packet ---
void ParseRiftStepInitiatedPacket(const uint8_t* fb_payload_ptr, int fb_payload_size) {
    flatbuffers::Verifier verifier(fb_payload_ptr, static_cast<size_t>(fb_payload_size));
    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) {
        g_last_server_message = "RiftStepInitiated Verification Failed"; return;
    }
    auto root_msg = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(fb_payload_ptr);
    if (!root_msg || root_msg->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_RiftStepInitiated) {
        g_last_server_message = "Not a RiftStepInitiated Payload"; return;
    }
    auto rs_msg = root_msg->payload_as_RiftStepInitiated();
    if (!rs_msg) { g_last_server_message = "Failed to get RiftStepInitiated msg"; return; }

    g_last_server_message = "RIFTSTEP INITIATED! Instigator: " + std::to_string(rs_msg->instigator_entity_id());
    if (rs_msg->instigator_entity_id() == g_client_player_id || g_client_player_id == 0) {
        if (rs_msg->actual_final_position()) { // Server tells us where we will land
            g_client_position = RiftForged::Networking::Shared::Vec3(
                rs_msg->actual_final_position()->x(),
                rs_msg->actual_final_position()->y(),
                rs_msg->actual_final_position()->z()
            );
            g_last_server_message += " My new pos: (" + std::to_string(g_client_position.x()) + ", ...)";
        }
    }
    // TODO: Parse and display effects
}

// --- DisplayClientState function ---
void DisplayClientState() {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[H\033[J"); // ANSI escape for clear screen
#endif
    std::cout << "RiftForged Test Client" << std::endl;
    std::cout << "Controls: W/A/S/D (move/turn), SPACE (RiftStep), P (Ping), Q (quit)" << std::endl;
    std::cout << "-----------------------------------------------------------------------" << std::endl;
    std::cout << "Client Pos: (" << g_client_position.x()
        << ", " << g_client_position.y()
        << ", " << g_client_position.z()
        << ") Facing (XY): (" << g_client_orientation_vector.x()
        << ", " << g_client_orientation_vector.y() << ")" << std::endl;
    std::cout << "Last Server Msg: " << g_last_server_message << std::endl;
    std::cout << "-----------------------------------------------------------------------" << std::endl;
    std::cout << "Enter command: " << std::flush;
}


int main() {
    std::cout << "RiftForged Test Client Starting..." << std::endl;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { std::cerr << "Client: WSAStartup failed: " << WSAGetLastError() << std::endl; return 1; }

    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) { std::cerr << "Client: Socket creation failed: " << WSAGetLastError() << std::endl; WSACleanup(); return 1; }
    std::cout << "Client: Socket created." << std::endl;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    const char* serverIp = "127.0.0.1";
    if (inet_pton(AF_INET, serverIp, &serverAddr.sin_addr) != 1) {
        std::cerr << "Client: inet_pton failed for IP " << serverIp << std::endl;
        closesocket(clientSocket); WSACleanup(); return 1;
    }
    std::cout << "Client: Server address configured for " << serverIp << ":" << 12345 << std::endl;

    DWORD timeout_ms = 10; // Short timeout for recvfrom
    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms)) == SOCKET_ERROR) {
        std::cerr << "Client: setsockopt for SO_RCVTIMEO failed: " << WSAGetLastError() << std::endl;
    }

    // --- SEND INITIAL PING TO "PRIME" THE SOCKET & GET PLAYER ID ---
    uint64_t initial_ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::vector<char> initial_ping_packet = BuildPingPacket(initial_ts, ++g_sequenceNumber);
    std::cout << "Client: Sending initial Ping to prime socket..." << std::endl;
    if (sendto(clientSocket, initial_ping_packet.data(), static_cast<int>(initial_ping_packet.size()), 0,
        (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Client: sendto for initial Ping failed: " << WSAGetLastError() << std::endl;
    }
    else {
        std::cout << "Client: Initial Ping sent." << std::endl;
    }
    // Attempt to receive the first pong/state update to get player ID
    char tempRecvBuffer[CLIENT_RECEIVE_BUFFER_SIZE];
    sockaddr_in tempFromAddr;
    int tempFromAddrLen = sizeof(tempFromAddr);
    int tempBytesReceived = recvfrom(clientSocket, tempRecvBuffer, sizeof(tempRecvBuffer), 0, (sockaddr*)&tempFromAddr, &tempFromAddrLen);
    if (tempBytesReceived > 0) {
        RiftForged::Networking::GamePacketHeader temp_s2c_header;
        if (tempBytesReceived >= static_cast<int>(RiftForged::Networking::GetGamePacketHeaderSize())) {
            memcpy(&temp_s2c_header, tempRecvBuffer, RiftForged::Networking::GetGamePacketHeaderSize());
            const uint8_t* temp_s2c_fb_payload = reinterpret_cast<const uint8_t*>(tempRecvBuffer + RiftForged::Networking::GetGamePacketHeaderSize());
            int temp_s2c_fb_length = tempBytesReceived - static_cast<int>(RiftForged::Networking::GetGamePacketHeaderSize());
            if (temp_s2c_header.messageType == RiftForged::Networking::MessageType::S2C_Pong) {
                ParsePongPacket(temp_s2c_fb_payload, temp_s2c_fb_length);
            }
            else if (temp_s2c_header.messageType == RiftForged::Networking::MessageType::S2C_EntityStateUpdate) {
                ParseEntityStateUpdatePacket(temp_s2c_fb_payload, temp_s2c_fb_length);
            }
        }
    } // We don't critically fail if this first receive times out.

    bool running = true;
    DisplayClientState();

    while (running) {
        std::vector<char> packet_to_send;
        bool wants_to_send = false;

        if (_kbhit()) {
            char input_char = static_cast<char>(_getch());
            // DisplayClientState(); // No need to redraw here, will redraw after processing input

            RiftForged::Networking::Shared::Vec3 move_dir_intent(0.f, 0.f, 0.f);
            RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent rift_intent =
                RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Default_Backward;

            switch (toupper(input_char)) {
            case 'W':
                move_dir_intent = g_client_orientation_vector;
                packet_to_send = BuildMovementInputPacket(move_dir_intent, false, ++g_sequenceNumber);
                wants_to_send = true; break;
            case 'S':
                move_dir_intent = RiftForged::Networking::Shared::Vec3(-g_client_orientation_vector.x(), -g_client_orientation_vector.y(), -g_client_orientation_vector.z());
                packet_to_send = BuildMovementInputPacket(move_dir_intent, false, ++g_sequenceNumber);
                wants_to_send = true; break;
            case 'A':
            {
                float cx = g_client_orientation_vector.x(); float cy = g_client_orientation_vector.y();
                g_client_orientation_vector = RiftForged::Networking::Shared::Vec3(cy, -cx, g_client_orientation_vector.z());
            }
            break;
            case 'D':
            {
                float cx = g_client_orientation_vector.x(); float cy = g_client_orientation_vector.y();
                g_client_orientation_vector = RiftForged::Networking::Shared::Vec3(-cy, cx, g_client_orientation_vector.z());
            }
            break;
            case ' ': {
                uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                // Simple: Spacebar defaults to Forward if W was last intent, else Backward
                // More robust: client maintains current movement input vector. If (0,0,0), then Default_Backward.
                // For now, to test different intents:
                std::cout << "RiftStep: Fwd(W), Back(S), Left(A), Right(D), Default(Space again): ";
                char intent_key = static_cast<char>(_getch());
                std::cout << intent_key << std::endl;
                switch (toupper(intent_key)) {
                case 'W': rift_intent = RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Forward; break;
                case 'S': rift_intent = RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Backward; break;
                case 'A': rift_intent = RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Left; break;
                case 'D': rift_intent = RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Right; break;
                default:  rift_intent = RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Default_Backward; break;
                }
                packet_to_send = BuildRiftStepActivationPacket(ts, rift_intent, ++g_sequenceNumber);
                wants_to_send = true; break;
            }
            case 'P': {
                uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                packet_to_send = BuildPingPacket(ts, ++g_sequenceNumber);
                wants_to_send = true; break;
            }
            case 'Q': running = false; break;
            }

            if (wants_to_send && !packet_to_send.empty()) {
                if (sendto(clientSocket, packet_to_send.data(), static_cast<int>(packet_to_send.size()), 0,
                    (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                    std::cerr << "Client: sendto failed: " << WSAGetLastError() << std::endl;
                }
            }
            if (running) DisplayClientState();
        }

        char recvBuffer[CLIENT_RECEIVE_BUFFER_SIZE];
        sockaddr_in fromAddr;
        int fromAddrLen = sizeof(fromAddr);
        int bytesReceived = recvfrom(clientSocket, recvBuffer, sizeof(recvBuffer), 0, (sockaddr*)&fromAddr, &fromAddrLen);

        bool state_changed_by_receive = false;
        if (bytesReceived > 0) {
            RiftForged::Networking::GamePacketHeader s2c_header;
            if (bytesReceived >= static_cast<int>(RiftForged::Networking::GetGamePacketHeaderSize())) {
                memcpy(&s2c_header, recvBuffer, RiftForged::Networking::GetGamePacketHeaderSize());
                const uint8_t* s2c_fb_payload = reinterpret_cast<const uint8_t*>(recvBuffer + RiftForged::Networking::GetGamePacketHeaderSize());
                int s2c_fb_length = bytesReceived - static_cast<int>(RiftForged::Networking::GetGamePacketHeaderSize());

                if (s2c_header.messageType == RiftForged::Networking::MessageType::S2C_Pong) {
                    ParsePongPacket(s2c_fb_payload, s2c_fb_length); state_changed_by_receive = true;
                }
                else if (s2c_header.messageType == RiftForged::Networking::MessageType::S2C_EntityStateUpdate) {
                    ParseEntityStateUpdatePacket(s2c_fb_payload, s2c_fb_length); state_changed_by_receive = true;
                }
                else if (s2c_header.messageType == RiftForged::Networking::MessageType::S2C_RiftStepInitiated) {
                    ParseRiftStepInitiatedPacket(s2c_fb_payload, s2c_fb_length); state_changed_by_receive = true;
                }
                else { /* unhandled type log */ }
            }
            else { /* packet too small log */ }
        }
        else if (bytesReceived == SOCKET_ERROR) { /* error handling as before */ }

        if (state_changed_by_receive && running) { DisplayClientState(); }

        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    std::cout << "Client: Shutting down..." << std::endl;
    closesocket(clientSocket);
    WSACleanup();
    std::cout << "Press Enter to exit client." << std::endl;
    // Clear potential leftover input from _getch()
    while (_kbhit()) _getch();
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
    return 0;
}