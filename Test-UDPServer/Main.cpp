// File: Test_Client.cpp
// (Incorporates reliability protocol changes)

// Standard C++ Includes
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>   // For std::abs
#include <cctype>  // For toupper
#include <conio.h> // For _kbhit() and _getch() on Windows
#include <limits>  // Required for std::numeric_limits
#include <sstream> // For building log messages
#include <thread>  // For std::this_thread::sleep_for

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

#include <Windows.h> // For GetAsyncKeyState

// FlatBuffers
#include "flatbuffers/flatbuffers.h"

// Generated FlatBuffer headers (ADJUST PATHS AS NEEDED)
#include "../FlatBuffers/V0.0.3/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.3/riftforged_s2c_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.3/riftforged_common_types_generated.h"

// Utilities Engine (ADJUST PATHS AS NEEDED)
#include "../Utils/MathUtil.h"
#include "../Utils/Logger.h"

// Network Engine Headers (ADJUST PATHS AS NEEDED)
#include "../NetworkEngine/GamePacketHeader.h"
#include "../NetworkEngine/UDPReliabilityProtocol.h" // For reliability logic

// Constants for the client
const int CLIENT_RECEIVE_BUFFER_SIZE = 4096;
const float CLIENT_RIFTSTEP_DISTANCE = 5.0f;
const float CLIENT_TURN_INCREMENT_DEGREES = 7.5f;

// Client's current authoritative state (updated by server messages)
RiftForged::Networking::Shared::Vec3 g_client_position(0.0f, 0.0f, 0.0f);
RiftForged::Networking::Shared::Quaternion g_client_orientation_quaternion(0.0f, 0.0f, 0.0f, 1.0f); // Identity
uint64_t g_client_player_id = 0;
std::string g_last_server_event_for_display = "None";

// Global state for managing reliability with the server
RiftForged::Networking::ReliableConnectionState g_clientToServerState;


// --- C2S Packet Building Helper Functions (Using Reliability Protocol) ---

std::vector<char> BuildPingPacket(uint64_t ts) {
    flatbuffers::FlatBufferBuilder builder(128);
    auto fb_payload_offset = RiftForged::Networking::UDP::C2S::CreateC2S_PingMsg(builder, ts);

    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_Ping);
    root_builder.add_payload(fb_payload_offset.Union());
    auto root_message_offset = root_builder.Finish();
    builder.Finish(root_message_offset);

    RiftForged::Networking::MessageType messageType = RiftForged::Networking::MessageType::C2S_Ping;
    // Sending Ping reliably
    uint8_t packetFlags = static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::IS_RELIABLE);

    const uint8_t* fb_data = builder.GetBufferPointer();
    uint16_t fb_size = builder.GetSize();

    std::vector<uint8_t> packet_buffer_uint8 = RiftForged::Networking::PrepareOutgoingPacket(
        g_clientToServerState, messageType, fb_data, fb_size, packetFlags);

    std::vector<char> packet_buffer(packet_buffer_uint8.begin(), packet_buffer_uint8.end());
    return packet_buffer;
}

std::vector<char> BuildMovementInputPacket(const RiftForged::Networking::Shared::Vec3& local_direction, bool is_sprinting) {
    flatbuffers::FlatBufferBuilder builder(256);
    uint64_t client_ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto fb_payload_offset = RiftForged::Networking::UDP::C2S::CreateC2S_MovementInputMsg(builder, client_ts, &local_direction, is_sprinting);

    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_MovementInput);
    root_builder.add_payload(fb_payload_offset.Union());
    auto root_message_offset = root_builder.Finish();
    builder.Finish(root_message_offset);

    RiftForged::Networking::MessageType messageType = RiftForged::Networking::MessageType::C2S_MovementInput;
    // Sending movement unreliably, but it will still carry ACKs.
    uint8_t packetFlags = static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::NONE);

    const uint8_t* fb_data = builder.GetBufferPointer();
    uint16_t fb_size = builder.GetSize();

    std::vector<uint8_t> packet_buffer_uint8 = RiftForged::Networking::PrepareOutgoingPacket(
        g_clientToServerState, messageType, fb_data, fb_size, packetFlags);

    std::vector<char> packet_buffer(packet_buffer_uint8.begin(), packet_buffer_uint8.end());
    return packet_buffer;
}

std::vector<char> BuildTurnIntentPacket(float turn_delta_degrees) {
    flatbuffers::FlatBufferBuilder builder(128);
    uint64_t client_ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto fb_payload_offset = RiftForged::Networking::UDP::C2S::CreateC2S_TurnIntentMsg(builder, client_ts, turn_delta_degrees);

    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_TurnIntent);
    root_builder.add_payload(fb_payload_offset.Union());
    auto root_message_offset = root_builder.Finish();
    builder.Finish(root_message_offset);

    RiftForged::Networking::MessageType messageType = RiftForged::Networking::MessageType::C2S_TurnIntent;
    // Sending turn intents unreliably.
    uint8_t packetFlags = static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::NONE);

    const uint8_t* fb_data = builder.GetBufferPointer();
    uint16_t fb_size = builder.GetSize();

    std::vector<uint8_t> packet_buffer_uint8 = RiftForged::Networking::PrepareOutgoingPacket(
        g_clientToServerState, messageType, fb_data, fb_size, packetFlags);

    std::vector<char> packet_buffer(packet_buffer_uint8.begin(), packet_buffer_uint8.end());
    return packet_buffer;
}

std::vector<char> BuildRiftStepActivationPacket(uint64_t client_timestamp_ms,
    RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent intent) {
    flatbuffers::FlatBufferBuilder builder(128);
    auto fb_payload_offset = RiftForged::Networking::UDP::C2S::CreateC2S_RiftStepActivationMsg(builder, client_timestamp_ms, intent);

    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_RiftStepActivation);
    root_builder.add_payload(fb_payload_offset.Union());
    auto root_message_offset = root_builder.Finish();
    builder.Finish(root_message_offset);

    RiftForged::Networking::MessageType messageType = RiftForged::Networking::MessageType::C2S_RiftStepActivation;
    // Sending RiftStep reliably.
    uint8_t packetFlags = static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::IS_RELIABLE);

    const uint8_t* fb_data = builder.GetBufferPointer();
    uint16_t fb_size = builder.GetSize();

    std::vector<uint8_t> packet_buffer_uint8 = RiftForged::Networking::PrepareOutgoingPacket(
        g_clientToServerState, messageType, fb_data, fb_size, packetFlags);

    std::vector<char> packet_buffer(packet_buffer_uint8.begin(), packet_buffer_uint8.end());
    return packet_buffer;
}

std::vector<char> BuildBasicAttackIntentPacket(
    uint64_t client_timestamp_ms,
    const RiftForged::Networking::Shared::Vec3& world_aim_direction,
    uint64_t target_id) {
    flatbuffers::FlatBufferBuilder builder(256);
    auto fb_payload_offset = RiftForged::Networking::UDP::C2S::CreateC2S_BasicAttackIntentMsg(builder,
        client_timestamp_ms, &world_aim_direction, target_id);

    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_BasicAttackIntent);
    root_builder.add_payload(fb_payload_offset.Union());
    auto root_message_offset = root_builder.Finish();
    builder.Finish(root_message_offset);

    RiftForged::Networking::MessageType messageType = RiftForged::Networking::MessageType::C2S_BasicAttackIntent;
    // Sending basic attack reliably.
    uint8_t packetFlags = static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::IS_RELIABLE);

    const uint8_t* fb_data = builder.GetBufferPointer();
    uint16_t fb_size = builder.GetSize();

    std::vector<uint8_t> packet_buffer_uint8 = RiftForged::Networking::PrepareOutgoingPacket(
        g_clientToServerState, messageType, fb_data, fb_size, packetFlags);

    std::vector<char> packet_buffer(packet_buffer_uint8.begin(), packet_buffer_uint8.end());
    return packet_buffer;
}


// --- S2C Packet Parsing Helper Functions (Using Client Logger) ---
// These functions (ParsePongPacket, ParseEntityStateUpdatePacket, etc.)
// should largely remain the same internally, but they now receive the
// app_payload_to_process_ptr and app_payload_size from the reliability layer.

void ParsePongPacket(const uint8_t* app_payload_ptr, uint16_t app_payload_size) {
    flatbuffers::Verifier verifier(app_payload_ptr, static_cast<size_t>(app_payload_size));
    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) {
        RF_CORE_ERROR("Client: S2C_PongMsg FlatBuffer verification failed.");
        g_last_server_event_for_display = "Pong Verification Failed"; return;
    }
    auto root = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(app_payload_ptr);
    if (!root || root->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_Pong) {
        RF_CORE_WARN("Client: Received non-Pong payload when expected.");
        g_last_server_event_for_display = "ERROR: Not a Pong Payload."; return;
    }
    auto pong = root->payload_as_Pong();
    if (!pong) { RF_CORE_ERROR("Client: Failed to get Pong message from payload."); g_last_server_event_for_display = "ERROR: Failed to get Pong msg."; return; }

    uint64_t rtt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - pong->client_timestamp_ms();
    g_last_server_event_for_display = "PONG! RTT: " + std::to_string(rtt) + "ms";
    RF_CORE_INFO("Client: {}", g_last_server_event_for_display);
}

void ParseEntityStateUpdatePacket(const uint8_t* app_payload_ptr, uint16_t app_payload_size) {
    flatbuffers::Verifier verifier(app_payload_ptr, static_cast<size_t>(app_payload_size));
    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) { RF_CORE_ERROR("Client: S2C_EntityStateUpdateMsg FlatBuffer verification failed."); g_last_server_event_for_display = "StateUpdate Verification Failed"; return; }
    auto root = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(app_payload_ptr);
    if (!root || root->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_EntityStateUpdate) { RF_CORE_WARN("Client: Received non-EntityStateUpdate payload when expected."); g_last_server_event_for_display = "ERROR: Not an EntityStateUpdate Payload."; return; }
    auto update = root->payload_as_EntityStateUpdate();
    if (!update) { RF_CORE_ERROR("Client: Failed to get EntityStateUpdate message from payload."); g_last_server_event_for_display = "ERROR: Failed to get EntityStateUpdate msg."; return; }

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
    oss << " HP: " << update->current_health() << "/" << update->max_health();
    // ... (log other fields like will to oss) ...
    g_last_server_event_for_display = oss.str();
    RF_CORE_INFO("Client: {}", g_last_server_event_for_display);
}

void ParseRiftStepInitiatedPacket(const uint8_t* app_payload_ptr, uint16_t app_payload_size) {
    flatbuffers::Verifier verifier(app_payload_ptr, static_cast<size_t>(app_payload_size));
    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) { RF_CORE_ERROR("Client: S2C_RiftStepInitiatedMsg FlatBuffer verification failed."); g_last_server_event_for_display = "RiftStep Verification Failed"; return; }
    auto root = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(app_payload_ptr);
    if (!root || root->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_RiftStepInitiated) { RF_CORE_WARN("Client: Received non-RiftStepInitiated payload when expected."); g_last_server_event_for_display = "ERROR: Not a RiftStepInitiated Payload."; return; }
    auto rs_msg = root->payload_as_RiftStepInitiated();
    if (!rs_msg) { RF_CORE_ERROR("Client: Failed to get RiftStepInitiated message from payload."); g_last_server_event_for_display = "ERROR: Failed to get RiftStepInitiated msg."; return; }

    std::ostringstream oss;
    oss << "RIFTSTEP INITIATED! Instigator: " << rs_msg->instigator_entity_id();
    if (rs_msg->instigator_entity_id() == g_client_player_id) {
        if (rs_msg->actual_final_position()) {
            g_client_position = *rs_msg->actual_final_position();
        }
        oss << " (It was me!)";
    }
    if (rs_msg->actual_final_position()) oss << " FinalPos: (" << rs_msg->actual_final_position()->x() << ", " << rs_msg->actual_final_position()->y() << ", " << rs_msg->actual_final_position()->z() << ")";
    oss << " Duration: " << rs_msg->cosmetic_travel_duration_sec() << "s";
    g_last_server_event_for_display = oss.str();
    RF_CORE_INFO("Client: {}", g_last_server_event_for_display);
}

void ParseCombatEventPacket(const uint8_t* app_payload_ptr, uint16_t app_payload_size) {
    flatbuffers::Verifier verifier(app_payload_ptr, static_cast<size_t>(app_payload_size));
    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) { RF_CORE_ERROR("Client: S2C_CombatEventMsg FlatBuffer verification failed."); g_last_server_event_for_display = "CombatEvent Verification Failed"; return; }
    auto root = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(app_payload_ptr);
    if (!root || root->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_CombatEvent) { RF_CORE_WARN("Client: Received non-CombatEvent payload when expected."); g_last_server_event_for_display = "ERROR: Not a CombatEvent Payload."; return; }
    auto combat_event_msg = root->payload_as_CombatEvent();
    if (!combat_event_msg) { RF_CORE_ERROR("Client: Failed to get CombatEvent message from payload."); g_last_server_event_for_display = "ERROR: Failed to get CombatEvent msg."; return; }

    std::ostringstream oss;
    oss << "COMBAT EVENT! Type: " << RiftForged::Networking::UDP::S2C::EnumNameCombatEventType(combat_event_msg->event_type());
    if (combat_event_msg->event_payload_type() == RiftForged::Networking::UDP::S2C::CombatEventPayload_DamageDealt) {
        auto details = combat_event_msg->event_payload_as_DamageDealt();
        if (details && details->damage_info()) {
            oss << " | Src: " << details->source_entity_id() << ", Tgt: " << details->target_entity_id()
                << ", Dmg: " << details->damage_info()->amount() << " " << RiftForged::Networking::Shared::EnumNameDamageType(details->damage_info()->type());
            if (details->damage_info()->is_crit()) oss << " (CRIT!)";
            if (details->is_kill()) oss << " (KILL!)";
        }
    }
    g_last_server_event_for_display = oss.str();
    RF_CORE_INFO("Client: {}", g_last_server_event_for_display);
}

// --- DisplayClientState function (remains the same) ---
void DisplayClientState() {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[H\033[J");
#endif
    // RF_CORE_INFO("RiftForged Test Client - Screen Refresh"); // Can be noisy
    std::cout << "RiftForged Test Client (Reliability Enabled)" << std::endl;
    std::cout << "Controls: WASD (move), QE (turn), SPACE (RiftStep), F (Attack), P (Ping), 0 (quit)" << std::endl; // Updated controls display
    std::cout << "-----------------------------------------------------------------------" << std::endl;
    RiftForged::Networking::Shared::Vec3 display_fwd = RiftForged::Utilities::Math::GetWorldForwardVector(g_client_orientation_quaternion);
    std::cout << "Client ID: " << g_client_player_id
        << " Pos: (" << g_client_position.x() << ", " << g_client_position.y() << ", " << g_client_position.z()
        << ") Facing (XY Approx): (" << display_fwd.x() << ", " << display_fwd.y() << ")" << std::endl;
    std::cout << "Last Server Event: " << g_last_server_event_for_display << std::endl;
    std::cout << "-----------------------------------------------------------------------" << std::endl;
    std::cout << "Input: " << std::flush;
}

// --- GetRiftStepIntentFromHeldKeys function (remains the same) ---
RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent GetRiftStepIntentFromHeldKeys() {
    if (GetAsyncKeyState('W') & 0x8000) return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Forward;
    if (GetAsyncKeyState('S') & 0x8000) return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Backward;
    if (GetAsyncKeyState('A') & 0x8000) return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Left;
    if (GetAsyncKeyState('D') & 0x8000) return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Right; 
    return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Default_Backward; // Default if no WASD for step
}

// --- main() ---
int main() {
    RiftForged::Utilities::Logger::Init(spdlog::level::trace, spdlog::level::trace, "logs/test_client_reliable.log"); // Changed log level and file name
    RF_CORE_INFO("RiftForged Test Client (Reliable) Starting (Protocol v0.0.3)...");

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { /* ... WSAStartup error handling ... */ return 1; }
    RF_CORE_INFO("Client: WSAStartup successful.");

    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) { /* ... socket creation error handling ... */ WSACleanup(); return 1; }
    RF_CORE_INFO("Client: Socket created (ID: {}).", clientSocket);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    const char* serverIp = "127.0.0.1";
    if (inet_pton(AF_INET, serverIp, &serverAddr.sin_addr) != 1) { /* ... inet_pton error handling ... */ closesocket(clientSocket); WSACleanup(); return 1; }
    RF_CORE_INFO("Client: Server address configured for {}:{}", serverIp, ntohs(serverAddr.sin_port));

    DWORD timeout_ms = 10;
    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms)) == SOCKET_ERROR) {
        RF_CORE_WARN("Client: setsockopt for SO_RCVTIMEO failed: {}", WSAGetLastError());
    }

    uint64_t initial_ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::vector<char> initial_ping_packet = BuildPingPacket(initial_ts); // No sequence number passed here
    RF_CORE_INFO("Client: Sending initial Ping to prime socket and get reliable connection established...");
    if (initial_ping_packet.size() >= RiftForged::Networking::GetGamePacketHeaderSize()) {
        RiftForged::Networking::GamePacketHeader* hdr_ptr = reinterpret_cast<RiftForged::Networking::GamePacketHeader*>(initial_ping_packet.data());
        RF_CORE_DEBUG("Client: Initial Ping Header - Type: {}, Seq: {}, AckNum: {}, AckBits: 0x{:X}, Flags: 0x{:X}",
            RiftForged::Networking::EnumNameMessageType(hdr_ptr->messageType), hdr_ptr->sequenceNumber,
            hdr_ptr->ackNumber, hdr_ptr->ackBitfield, hdr_ptr->flags);
    }
    if (sendto(clientSocket, initial_ping_packet.data(), static_cast<int>(initial_ping_packet.size()), 0,
        (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        RF_CORE_ERROR("Client: sendto for initial Ping failed: {}", WSAGetLastError());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    bool running = true;
    DisplayClientState();

    while (running) {
        std::vector<char> packet_to_send;
        bool wants_to_send_action = false;

        if (_kbhit()) {
            char input_char_event = static_cast<char>(_getch());
            RF_CORE_DEBUG("Client Raw Input Event: '{}'", input_char_event);
            RiftForged::Networking::Shared::Vec3 local_move_intent(0.f, 0.f, 0.f); // Reset per input event

            switch (toupper(input_char_event)) {
            case 'W': local_move_intent = { 0.0f, 1.0f, 0.0f }; packet_to_send = BuildMovementInputPacket(local_move_intent, false); wants_to_send_action = true; break;
            case 'S': local_move_intent = { 0.0f, -1.0f,0.0f }; packet_to_send = BuildMovementInputPacket(local_move_intent, false); wants_to_send_action = true; break;
            case 'A': local_move_intent = { -1.0f, 0.0f,0.0f }; packet_to_send = BuildMovementInputPacket(local_move_intent, false); wants_to_send_action = true; break;
            case 'D': local_move_intent = { 1.0f,  0.0f,0.0f }; packet_to_send = BuildMovementInputPacket(local_move_intent, false); wants_to_send_action = true; break;
            case 'Q': packet_to_send = BuildTurnIntentPacket(-CLIENT_TURN_INCREMENT_DEGREES); wants_to_send_action = true; break;
            case 'E': packet_to_send = BuildTurnIntentPacket(CLIENT_TURN_INCREMENT_DEGREES); wants_to_send_action = true; break;
            case ' ': {
                uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent rift_intent = GetRiftStepIntentFromHeldKeys();
                packet_to_send = BuildRiftStepActivationPacket(ts, rift_intent);
                wants_to_send_action = true;
                RF_CORE_DEBUG("Client: Action - RiftStep. Intent: {}", static_cast<int>(rift_intent)); // Simplified log
                break;
            }
            case 'F': {
                uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                RiftForged::Networking::Shared::Vec3 world_aim_dir = RiftForged::Utilities::Math::GetWorldForwardVector(g_client_orientation_quaternion);
                packet_to_send = BuildBasicAttackIntentPacket(ts, world_aim_dir, 0);
                wants_to_send_action = true;
                RF_CORE_DEBUG("Client: Action - Basic Attack Intent.");
                break;
            }
            case 'P': {
                uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                packet_to_send = BuildPingPacket(ts);
                wants_to_send_action = true;
                break;
            }
            case '0': running = false; RF_CORE_INFO("Client: Quit command (0) received."); break;
            default: RF_CORE_INFO("Client: Unknown input command '{}'", input_char_event); break;
            }

            if (wants_to_send_action && !packet_to_send.empty()) {
                if (packet_to_send.size() >= RiftForged::Networking::GetGamePacketHeaderSize()) {
                    RiftForged::Networking::GamePacketHeader sent_hdr_preview; // Temp for logging
                    memcpy(&sent_hdr_preview, packet_to_send.data(), RiftForged::Networking::GetGamePacketHeaderSize());
                    RF_CORE_DEBUG("Client: Sending packet. Type: {}, Seq: {}, AckNum: {}, AckBits: 0x{:X}, Flags: 0x{:X}",
                        RiftForged::Networking::EnumNameMessageType(sent_hdr_preview.messageType),
                        sent_hdr_preview.sequenceNumber, sent_hdr_preview.ackNumber,
                        sent_hdr_preview.ackBitfield, sent_hdr_preview.flags);
                }
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
        bool state_updated_by_receive = false;

        if (bytesReceived >= static_cast<int>(RiftForged::Networking::GetGamePacketHeaderSize())) {
            RiftForged::Networking::GamePacketHeader s2c_header;
            memcpy(&s2c_header, recvBuffer, RiftForged::Networking::GetGamePacketHeaderSize());
            const uint8_t* s2c_full_payload_ptr = reinterpret_cast<const uint8_t*>(recvBuffer + RiftForged::Networking::GetGamePacketHeaderSize());
            uint16_t s2c_full_payload_len = static_cast<uint16_t>(bytesReceived - RiftForged::Networking::GetGamePacketHeaderSize());

            RF_CORE_TRACE("Client: Raw S2C Recv: {} bytes. Header - Type: {}, Seq: {}, Ack: {}, AckBits: 0x{:X}, Flags: 0x{:X}",
                bytesReceived, RiftForged::Networking::EnumNameMessageType(s2c_header.messageType),
                s2c_header.sequenceNumber, s2c_header.ackNumber, s2c_header.ackBitfield, s2c_header.flags);

            if (s2c_header.protocolId == RiftForged::Networking::CURRENT_PROTOCOL_ID_VERSION) {
                const uint8_t* app_payload_to_process_ptr = nullptr;
                uint16_t app_payload_size = 0;

                bool should_process_app_payload = RiftForged::Networking::ProcessIncomingPacketHeader(
                    g_clientToServerState, s2c_header, s2c_full_payload_ptr, s2c_full_payload_len,
                    &app_payload_to_process_ptr, &app_payload_size);

                if (should_process_app_payload) {
                    RF_CORE_DEBUG("Client: Reliability approved S2C payload. Type: {}, AppPayloadSize: {}",
                        RiftForged::Networking::EnumNameMessageType(s2c_header.messageType), app_payload_size);
                    state_updated_by_receive = true;
                    switch (s2c_header.messageType) {
                    case RiftForged::Networking::MessageType::S2C_Pong: ParsePongPacket(app_payload_to_process_ptr, app_payload_size); break;
                    case RiftForged::Networking::MessageType::S2C_EntityStateUpdate: ParseEntityStateUpdatePacket(app_payload_to_process_ptr, app_payload_size); break;
                    case RiftForged::Networking::MessageType::S2C_RiftStepInitiated: ParseRiftStepInitiatedPacket(app_payload_to_process_ptr, app_payload_size); break;
                    case RiftForged::Networking::MessageType::S2C_CombatEvent: ParseCombatEventPacket(app_payload_to_process_ptr, app_payload_size); break;
                        // case RiftForged::Networking::MessageType::S2C_SpawnProjectile: ParseSpawnProjectilePacket(app_payload_to_process_ptr, app_payload_size); break;
                        // case RiftForged::Networking::MessageType::S2C_SystemBroadcast: ParseSystemBroadcastPacket(app_payload_to_process_ptr, app_payload_size); break;
                    default:
                        g_last_server_event_for_display = "S2C Unhandled (RelayTrue): " + std::to_string(static_cast<int>(s2c_header.messageType));
                        RF_CORE_WARN("Client: {}", g_last_server_event_for_display);
                        state_updated_by_receive = false;
                        break;
                    }
                }
                else {
                    RF_CORE_TRACE("Client: S2C Payload for MsgType {} (Seq {}) not processed by app (e.g., duplicate, or pure ACK for us).",
                        RiftForged::Networking::EnumNameMessageType(s2c_header.messageType), s2c_header.sequenceNumber);
                    state_updated_by_receive = false;
                }
            }
            else {
                g_last_server_event_for_display = "ERROR: S2C Mismatched Protocol ID! Expected " + std::to_string(RiftForged::Networking::CURRENT_PROTOCOL_ID_VERSION) + " Got " + std::to_string(s2c_header.protocolId);
                RF_CORE_WARN("Client: {}", g_last_server_event_for_display);
                state_updated_by_receive = true; // Update display with error
            }
        }
        else if (bytesReceived > 0) { // Not enough for header
            g_last_server_event_for_display = "ERROR: S2C Packet too small for header. Size: " + std::to_string(bytesReceived);
            RF_CORE_ERROR("Client: {}", g_last_server_event_for_display);
            state_updated_by_receive = true; // Update display with error
        }
        else if (bytesReceived == SOCKET_ERROR) {
            int recvError = WSAGetLastError();
            if (recvError != WSAETIMEDOUT && recvError != WSAEWOULDBLOCK) {
                g_last_server_event_for_display = "ERROR: recvfrom failed with code: " + std::to_string(recvError);
                RF_CORE_ERROR("Client: {}", g_last_server_event_for_display);
                state_updated_by_receive = true; // Update display with error
            }
        }

        // Explicit ACK sending if needed (and no other packet was just sent by input)
        if (g_clientToServerState.hasPendingAckToSend && !wants_to_send_action) {
            auto currentTimeForAck = std::chrono::steady_clock::now();
            auto timeSinceLastClientSend = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTimeForAck - g_clientToServerState.lastPacketSentTimeToRemote);

            if (timeSinceLastClientSend.count() > 50) { // Threshold to send explicit ACK
                RF_CORE_DEBUG("Client: Pending ACK to server. Sending ACK-only packet.");
                std::vector<uint8_t> ack_packet_uint8 = RiftForged::Networking::PrepareOutgoingPacket(
                    g_clientToServerState, RiftForged::Networking::MessageType::Unknown, nullptr, 0,
                    static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::IS_RELIABLE) | static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::IS_ACK_ONLY));

                if (!ack_packet_uint8.empty()) {
                    std::vector<char> ack_packet_char(ack_packet_uint8.begin(), ack_packet_uint8.end());
                    if (ack_packet_char.size() >= RiftForged::Networking::GetGamePacketHeaderSize()) {
                        RiftForged::Networking::GamePacketHeader sent_hdr_preview;
                        memcpy(&sent_hdr_preview, ack_packet_char.data(), RiftForged::Networking::GetGamePacketHeaderSize());
                        RF_CORE_DEBUG("Client: Sending ACK-only. Header - Type: {}, Seq: {}, AckNum: {}, AckBits: 0x{:X}, Flags: 0x{:X}",
                            RiftForged::Networking::EnumNameMessageType(sent_hdr_preview.messageType),
                            sent_hdr_preview.sequenceNumber, sent_hdr_preview.ackNumber,
                            sent_hdr_preview.ackBitfield, sent_hdr_preview.flags);
                    }
                    if (sendto(clientSocket, ack_packet_char.data(), static_cast<int>(ack_packet_char.size()), 0,
                        (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                        RF_CORE_ERROR("Client: sendto for ACK-only packet failed: {}", WSAGetLastError());
                    }
                }
            }
        }

        if (state_updated_by_receive && running) {
            DisplayClientState();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS loop tick
    }

    RF_CORE_INFO("Client: Shutting down...");
    closesocket(clientSocket);
    WSACleanup();
    RiftForged::Utilities::Logger::FlushAll();
    RiftForged::Utilities::Logger::Shutdown();

    std::cout << "\nClient application shut down. Log: logs/test_client_reliable.log" << std::endl;
    std::cout << "Press Enter to exit console." << std::endl;
    while (_kbhit()) { _getch(); } // Clear buffer
    //std::cin.clear(); // Not needed if using _getch for final wait
    //std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Not needed
    _getch(); // Wait for a key press
    return 0;
}