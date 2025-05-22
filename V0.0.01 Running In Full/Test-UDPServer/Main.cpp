// Standard C++ Includes
#include <iostream> // For initial messages and final exit prompt only
#include <vector>
#include <string>
#include <chrono>
#include <cmath>   // For std::abs
#include <cctype>  // For toupper
#include <conio.h> // For _kbhit() and _getch() on Windows
#include <limits>  // Required for std::numeric_limits
#include <sstream> // For building log messages

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

#include <Windows.h>

// FlatBuffers
#include "flatbuffers/flatbuffers.h"

// Generated FlatBuffer headers (adjust path to your SharedProtocols/Generated/ folder)
#include "../FlatBuffers/V0.0.1/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.1/riftforged_s2c_udp_messages_generated.h" // For S2C_RiftStepExecutedMsg
#include "../FlatBuffers/V0.0.1/riftforged_common_types_generated.h" // For Vec3, Quaternion, etc.

// Utilties Engine
#include "../Utils/MathUtil.h" // For Vec3 and Quaternion math utilities
#include "../Utils/Logger.h"

// Your custom packet header
#include "../NetworkEngine/GamePacketHeader.h" // Ensure this path is correct

// Constants for the client
const int CLIENT_RECEIVE_BUFFER_SIZE = 4096;
const float CLIENT_RIFTSTEP_DISTANCE = 5.0f; // How far RiftStep goes in current facing direction
const float CLIENT_TURN_INCREMENT_DEGREES = 7.5f; // How much Q/E turns by

// Client's current authoritative state (updated by server messages)
RiftForged::Networking::Shared::Vec3 g_client_position(0.0f, 0.0f, 0.0f);
RiftForged::Networking::Shared::Quaternion g_client_orientation_quaternion(0.0f, 0.0f, 0.0f, 1.0f); // Identity
uint64_t g_client_player_id = 0;
uint32_t g_sequenceNumber = 0;
std::string g_last_server_event_for_display = "None";

// --- Packet Building Helper Functions ---
// (BuildPingPacket, BuildMovementInputPacket, BuildTurnIntentPacket, BuildRiftStepActivationPacket)
// These are assumed to be implemented correctly as per our last versions (e.g., response #119, #124)
// For brevity, their full code is omitted here but should be present. Ensure they use the correct MessageTypes.

std::vector<char> BuildPingPacket(uint64_t ts, uint32_t seq) {
    flatbuffers::FlatBufferBuilder builder(128);
    auto payload = RiftForged::Networking::UDP::C2S::CreateC2S_PingMsg(builder, ts);
    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_Ping);
    root_builder.add_payload(payload.Union());
    builder.Finish(root_builder.Finish());
    RiftForged::Networking::GamePacketHeader header(RiftForged::Networking::MessageType::C2S_Ping, seq);
    std::vector<char> packet_buffer(RiftForged::Networking::GetGamePacketHeaderSize() + builder.GetSize());
    memcpy(packet_buffer.data(), &header, RiftForged::Networking::GetGamePacketHeaderSize());
    memcpy(packet_buffer.data() + RiftForged::Networking::GetGamePacketHeaderSize(), builder.GetBufferPointer(), builder.GetSize());
    return packet_buffer;
}

std::vector<char> BuildMovementInputPacket(const RiftForged::Networking::Shared::Vec3& local_direction, bool is_sprinting, uint32_t sequence_number) {
    flatbuffers::FlatBufferBuilder builder(256);
    uint64_t client_ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto payload = RiftForged::Networking::UDP::C2S::CreateC2S_MovementInputMsg(builder, client_ts, &local_direction, is_sprinting);
    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_MovementInput);
    root_builder.add_payload(payload.Union());
    builder.Finish(root_builder.Finish());
    RiftForged::Networking::GamePacketHeader header(RiftForged::Networking::MessageType::C2S_MovementInput, sequence_number);
    std::vector<char> packet_buffer(RiftForged::Networking::GetGamePacketHeaderSize() + builder.GetSize());
    memcpy(packet_buffer.data(), &header, RiftForged::Networking::GetGamePacketHeaderSize());
    memcpy(packet_buffer.data() + RiftForged::Networking::GetGamePacketHeaderSize(), builder.GetBufferPointer(), builder.GetSize());
    return packet_buffer;
}

std::vector<char> BuildTurnIntentPacket(float turn_delta_degrees, uint32_t sequence_number) {
    flatbuffers::FlatBufferBuilder builder(128);
    uint64_t client_ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto payload = RiftForged::Networking::UDP::C2S::CreateC2S_TurnIntentMsg(builder, client_ts, turn_delta_degrees);
    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_TurnIntent);
    root_builder.add_payload(payload.Union());
    builder.Finish(root_builder.Finish());
    RiftForged::Networking::GamePacketHeader header(RiftForged::Networking::MessageType::C2S_TurnIntent, sequence_number);
    std::vector<char> packet_buffer(RiftForged::Networking::GetGamePacketHeaderSize() + builder.GetSize());
    memcpy(packet_buffer.data(), &header, RiftForged::Networking::GetGamePacketHeaderSize());
    memcpy(packet_buffer.data() + RiftForged::Networking::GetGamePacketHeaderSize(), builder.GetBufferPointer(), builder.GetSize());
    return packet_buffer;
}

std::vector<char> BuildRiftStepActivationPacket(uint64_t client_timestamp_ms,
    RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent intent,
    uint32_t sequence_number) {
    flatbuffers::FlatBufferBuilder builder(128);
    auto payload = RiftForged::Networking::UDP::C2S::CreateC2S_RiftStepActivationMsg(builder, client_timestamp_ms, intent);
    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_RiftStepActivation);
    root_builder.add_payload(payload.Union());
    builder.Finish(root_builder.Finish());
    RiftForged::Networking::GamePacketHeader header(RiftForged::Networking::MessageType::C2S_RiftStepActivation, sequence_number);
    std::vector<char> packet_buffer(RiftForged::Networking::GetGamePacketHeaderSize() + builder.GetSize());
    memcpy(packet_buffer.data(), &header, RiftForged::Networking::GetGamePacketHeaderSize());
    memcpy(packet_buffer.data() + RiftForged::Networking::GetGamePacketHeaderSize(), builder.GetBufferPointer(), builder.GetSize());
    return packet_buffer;
}


// --- S2C Packet Parsing Helper Functions (Using Client Logger) ---
void ParsePongPacket(const uint8_t* fb_payload_ptr, int fb_payload_size) {
    flatbuffers::Verifier verifier(fb_payload_ptr, static_cast<size_t>(fb_payload_size));
    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) {
        RF_CORE_ERROR("Client: S2C_PongMsg FlatBuffer verification failed.");
        g_last_server_event_for_display = "Pong Verification Failed"; return;
    }
    auto root = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(fb_payload_ptr);
    if (!root || root->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_Pong) { /* ... error log ... */ return; }
    auto pong = root->payload_as_Pong();
    if (!pong) { /* ... error log ... */ return; }
    uint64_t rtt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - pong->client_timestamp_ms();
    g_last_server_event_for_display = "PONG! RTT: " + std::to_string(rtt) + "ms";
    RF_CORE_INFO("Client: {}", g_last_server_event_for_display);
}

void ParseEntityStateUpdatePacket(const uint8_t* fb_payload_ptr, int fb_payload_size) {
    flatbuffers::Verifier verifier(fb_payload_ptr, static_cast<size_t>(fb_payload_size));
    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) { /* ... error log ... */ return; }
    auto root = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(fb_payload_ptr);
    if (!root || root->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_EntityStateUpdate) { /* ... error log ... */ return; }
    auto update = root->payload_as_EntityStateUpdate();
    if (!update) { /* ... error log ... */ return; }

    std::ostringstream oss;
    oss << "StateUpdate! ID: " << update->entity_id();
    if (g_client_player_id == 0 && update->entity_id() != 0) {
        g_client_player_id = update->entity_id();
        oss << " (My PlayerID now " << g_client_player_id << ")";
    }

    if (update->entity_id() == g_client_player_id) {
        if (update->position()) { g_client_position = *update->position(); }
        if (update->orientation()) { g_client_orientation_quaternion = *update->orientation(); }
    }
    if (update->position()) oss << " Pos: (" << update->position()->x() << "," << update->position()->y() << "," << update->position()->z() << ")";
    // ... (log other fields like health, will to oss) ...
    g_last_server_event_for_display = oss.str();
    RF_CORE_INFO("Client: {}", g_last_server_event_for_display);
}

void ParseRiftStepInitiatedPacket(const uint8_t* fb_payload_ptr, int fb_payload_size) {
    flatbuffers::Verifier verifier(fb_payload_ptr, static_cast<size_t>(fb_payload_size));
    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) { /* ... error log ... */ return; }
    auto root = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(fb_payload_ptr);
    if (!root || root->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_RiftStepInitiated) { /* ... error log ... */ return; }
    auto rs_msg = root->payload_as_RiftStepInitiated();
    if (!rs_msg) { /* ... error log ... */ return; }

    std::ostringstream oss;
    oss << "RIFTSTEP INITIATED! Instigator: " << rs_msg->instigator_entity_id();
    if (rs_msg->instigator_entity_id() == g_client_player_id) {
        if (rs_msg->actual_final_position()) {
            g_client_position = *rs_msg->actual_final_position();
            // Orientation doesn't change from RiftStep itself, server will send update if it does
        }
        oss << " (It was me!)";
    }
    if (rs_msg->actual_final_position()) oss << " FinalPos: (" << rs_msg->actual_final_position()->x() << ", ...)";
    oss << " Duration: " << rs_msg->cosmetic_travel_duration_sec() << "s";
    // TODO: Log effects
    g_last_server_event_for_display = oss.str();
    RF_CORE_INFO("Client: {}", g_last_server_event_for_display);
}

// --- DisplayClientState function ---
void DisplayClientState() {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[H\033[J");
#endif
    RF_CORE_INFO("RiftForged Test Client - Screen Refresh"); // Log to file
    std::cout << "RiftForged Test Client" << std::endl;
    std::cout << "Controls: WASD (move), QE (turn), SPACE (RiftStep), P (Ping), Q (quit)" << std::endl;
    std::cout << "-----------------------------------------------------------------------" << std::endl;

    // Derive display forward vector from authoritative quaternion
    RiftForged::Networking::Shared::Vec3 display_fwd = RiftForged::Utilities::Math::GetWorldForwardVector(g_client_orientation_quaternion);
    std::cout << "Client ID: " << g_client_player_id
        << " Pos: (" << g_client_position.x() << ", " << g_client_position.y() << ", " << g_client_position.z()
        << ") Facing (XY Approx): (" << display_fwd.x() << ", " << display_fwd.y() << ")" << std::endl;
    std::cout << "Last Server Event: " << g_last_server_event_for_display << std::endl;
    std::cout << "-----------------------------------------------------------------------" << std::endl;
    std::cout << "Input: " << std::flush; // Prompt for next input
}

// Helper to get RiftStep intent based on currently HELD WASD keys
RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent GetRiftStepIntentFromHeldKeys() {
    // This checks the current physical state of the keys.
    // Note: For this to work reliably after pressing Space, the Space key event should be fully processed
    // by _getch() before these GetAsyncKeyState calls are made, or Space itself might interfere.
    // A small delay or a more sophisticated input loop might be needed if chording is unreliable.

    // Prioritize specific directions if multiple are held (e.g., W overrides A/S)
    if (GetAsyncKeyState('W') & 0x8000) {
        return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Forward;
    }
    if (GetAsyncKeyState('S') & 0x8000) {
        return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Backward;
    }
    if (GetAsyncKeyState('A') & 0x8000) {
        return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Left;
    }
    if (GetAsyncKeyState('D') & 0x8000) {
        return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Right;
    }
    return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Default_Backward;
}


int main() {
    // Initialize Client-Side Logger
    // Using default levels, log file "logs/test_client.log"
    RiftForged::Utilities::Logger::Init(spdlog::level::info, spdlog::level::trace, "logs/test_client.log");

    RF_CORE_INFO("RiftForged Test Client Starting...");

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { RF_CORE_CRITICAL("Client: WSAStartup failed: {}", WSAGetLastError()); return 1; }
    RF_CORE_INFO("Client: WSAStartup successful.");

    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) { RF_CORE_CRITICAL("Client: Socket creation failed: {}", WSAGetLastError()); WSACleanup(); return 1; }
    RF_CORE_INFO("Client: Socket created (ID: {}).", clientSocket);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    const char* serverIp = "127.0.0.1";
    if (inet_pton(AF_INET, serverIp, &serverAddr.sin_addr) != 1) {
        RF_CORE_CRITICAL("Client: inet_pton failed for IP {}. Error: {}", serverIp, WSAGetLastError());
        closesocket(clientSocket); WSACleanup(); return 1;
    }
    RF_CORE_INFO("Client: Server address configured for {}:{}", serverIp, ntohs(serverAddr.sin_port));

    DWORD timeout_ms = 10;
    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms)) == SOCKET_ERROR) {
        RF_CORE_WARN("Client: setsockopt for SO_RCVTIMEO failed: {}", WSAGetLastError());
    }

    // Initial Ping
    uint64_t initial_ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::vector<char> initial_ping_packet = BuildPingPacket(initial_ts, ++g_sequenceNumber);
    RF_CORE_INFO("Client: Sending initial Ping (Seq: {}) to prime socket...", g_sequenceNumber);
    if (sendto(clientSocket, initial_ping_packet.data(), static_cast<int>(initial_ping_packet.size()), 0,
        (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        RF_CORE_ERROR("Client: sendto for initial Ping failed: {}", WSAGetLastError());
    }
    // Brief pause to allow server to process initial ping and potentially send first state update
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    bool running = true;
    DisplayClientState();

    while (running) {
        std::vector<char> packet_to_send;
        bool wants_to_send_action = false;

        if (_kbhit()) {
            char input_char = static_cast<char>(_getch());
            // DisplayClientState(); // Redraw happens after processing
            RF_CORE_DEBUG("Client Input Raw: '{}'", input_char);

            RiftForged::Networking::Shared::Vec3 local_move_intent(0.f, 0.f, 0.f);

            switch (toupper(input_char)) {
            case 'W':
                local_move_intent = { 0.0f, 1.0f, 0.0f };
                packet_to_send = BuildMovementInputPacket(local_move_intent, false, ++g_sequenceNumber);
                wants_to_send_action = true; break;
            case 'S':
                local_move_intent = { 0.0f, -1.0f, 0.0f };
                packet_to_send = BuildMovementInputPacket(local_move_intent, false, ++g_sequenceNumber);
                wants_to_send_action = true; break;
            case 'A':
                local_move_intent = { -1.0f, 0.0f, 0.0f };
                packet_to_send = BuildMovementInputPacket(local_move_intent, false, ++g_sequenceNumber);
                wants_to_send_action = true; break;
            case 'D':
                local_move_intent = { 1.0f, 0.0f, 0.0f };
                packet_to_send = BuildMovementInputPacket(local_move_intent, false, ++g_sequenceNumber);
                wants_to_send_action = true; break;
            case 'Q':
                packet_to_send = BuildTurnIntentPacket(-CLIENT_TURN_INCREMENT_DEGREES, ++g_sequenceNumber);
                wants_to_send_action = true; break;
            case 'E':
                packet_to_send = BuildTurnIntentPacket(CLIENT_TURN_INCREMENT_DEGREES, ++g_sequenceNumber);
                wants_to_send_action = true; break;
            case ' ': {
                uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent rift_intent = GetRiftStepIntentFromHeldKeys();
                packet_to_send = BuildRiftStepActivationPacket(ts, rift_intent, ++g_sequenceNumber);
                wants_to_send_action = true; break;
            }
            case 'P': {
                uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                packet_to_send = BuildPingPacket(ts, ++g_sequenceNumber);
                wants_to_send_action = true; break;
            }
            case '0': running = false; break;
            }

            if (wants_to_send_action && !packet_to_send.empty()) {
                RF_CORE_DEBUG("Client: Sending packet (Seq {}), Type: {}", g_sequenceNumber,
                    (packet_to_send.size() >= RiftForged::Networking::GetGamePacketHeaderSize() ?
                        std::to_string(static_cast<int>(reinterpret_cast<RiftForged::Networking::GamePacketHeader*>(packet_to_send.data())->messageType)) : "N/A"));
                if (sendto(clientSocket, packet_to_send.data(), static_cast<int>(packet_to_send.size()), 0,
                    (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                    RF_CORE_ERROR("Client: sendto failed: {}", WSAGetLastError());
                }
            }
            if (running) DisplayClientState();
        }

        char recvBuffer[CLIENT_RECEIVE_BUFFER_SIZE];
        sockaddr_in fromAddr;
        int fromAddrLen = sizeof(fromAddr);
        int bytesReceived = recvfrom(clientSocket, recvBuffer, sizeof(recvBuffer), 0, (sockaddr*)&fromAddr, &fromAddrLen);

        bool displayed_after_receive = false;
        if (bytesReceived > 0) {
            RiftForged::Networking::GamePacketHeader s2c_header;
            if (bytesReceived >= static_cast<int>(RiftForged::Networking::GetGamePacketHeaderSize())) {
                memcpy(&s2c_header, recvBuffer, RiftForged::Networking::GetGamePacketHeaderSize());
                const uint8_t* s2c_fb_payload = reinterpret_cast<const uint8_t*>(recvBuffer + RiftForged::Networking::GetGamePacketHeaderSize());
                int s2c_fb_length = bytesReceived - static_cast<int>(RiftForged::Networking::GetGamePacketHeaderSize());

                if (s2c_header.protocolId != RiftForged::Networking::CURRENT_PROTOCOL_ID_VERSION) {
                    g_last_server_event_for_display = "ERROR: S2C Mismatched Protocol ID!";
                }
                else {
                    switch (s2c_header.messageType) {
                    case RiftForged::Networking::MessageType::S2C_Pong:                 ParsePongPacket(s2c_fb_payload, s2c_fb_length); break;
                    case RiftForged::Networking::MessageType::S2C_EntityStateUpdate:    ParseEntityStateUpdatePacket(s2c_fb_payload, s2c_fb_length); break;
                    case RiftForged::Networking::MessageType::S2C_RiftStepInitiated:    ParseRiftStepInitiatedPacket(s2c_fb_payload, s2c_fb_length); break;
                        // case RiftForged::Networking::MessageType::S2C_SystemBroadcast:   ParseSystemBroadcastPacket(s2c_fb_payload, s2c_fb_length); break;
                        // case RiftForged::Networking::MessageType::S2C_CombatEvent:       ParseCombatEventPacket(s2c_fb_payload, s2c_fb_length); break;
                    default:
                        g_last_server_event_for_display = "Received S2C Unhandled Type: " + std::to_string(static_cast<int>(s2c_header.messageType));
                        break;
                    }
                }
            }
            else { g_last_server_event_for_display = "ERROR: S2C Packet too small. Size: " + std::to_string(bytesReceived); }
            if (running) { DisplayClientState(); displayed_after_receive = true; }
        }
        else if (bytesReceived == SOCKET_ERROR) {
            int recvError = WSAGetLastError();
            if (recvError != WSAETIMEDOUT && recvError != WSAEWOULDBLOCK) {
                g_last_server_event_for_display = "ERROR: recvfrom code: " + std::to_string(recvError);
                if (running && !displayed_after_receive) DisplayClientState(); // Show error if no other display happened
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // Aim for ~60Hz loop
    }

    RF_CORE_INFO("Client: Shutting down...");
    closesocket(clientSocket);
    WSACleanup();
    RiftForged::Utilities::Logger::FlushAll();
    RiftForged::Utilities::Logger::Shutdown();

    std::cout << "Client shut down. Press Enter to exit." << std::endl;
    while (_kbhit()) _getch();
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
    return 0;
}