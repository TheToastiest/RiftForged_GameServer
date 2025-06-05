// File: Test_Client.cpp
// (Incorporates reliability protocol and FlatBuffers payload_type dispatch)

// Standard C++ Includes
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <cctype> // For toupper
#include <conio.h> // For _kbhit, _getch
#include <limits>
#include <sstream>
#include <thread>

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

// FlatBuffers & Project Headers (Ensure paths are correct)
#include "flatbuffers/flatbuffers.h"
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h"
#include "../Utils/MathUtil.h"
#include "../Utils/Logger.h"
// GamePacketHeader is still needed for GetGamePacketHeaderSize, struct definition for reliability, flags, and protocol ID
#include "../NetworkEngine/GamePacketHeader.h"
#include "../NetworkEngine/UDPReliabilityProtocol.h"

// Constants
const int CLIENT_RECEIVE_BUFFER_SIZE = 4096;
const float CLIENT_TURN_INCREMENT_DEGREES = 7.5f;

// Client State
RiftForged::Networking::Shared::Vec3 g_client_position(0.0f, 0.0f, 1.0f);
RiftForged::Networking::Shared::Quaternion g_client_orientation_quaternion(0.0f, 0.0f, 0.0f, 1.0f);
uint64_t g_client_player_id = 0;
std::string g_last_server_event_for_display = "Initializing...";
RiftForged::Networking::ReliableConnectionState g_clientToServerState;

enum class ClientJoinState {
    Disconnected,
    AttemptingJoin,
    Joined,
    FailedToJoin
};
ClientJoinState g_join_state = ClientJoinState::Disconnected;
std::string g_character_id_to_load = "TestChar123";

// --- C2S Packet Building ---
// Note: RiftForged::Networking::MessageType is removed from these build functions.
// PrepareOutgoingPacket will handle the header without a specific MessageType enum from here.

std::vector<char> BuildPingPacket(uint64_t ts) {
    flatbuffers::FlatBufferBuilder builder(128);
    auto fb_payload_offset = RiftForged::Networking::UDP::C2S::CreateC2S_PingMsg(builder, ts);

    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_Ping);
    root_builder.add_payload(fb_payload_offset.Union());
    auto root_message_offset = root_builder.Finish();
    builder.Finish(root_message_offset);

    uint8_t packetFlags = static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::IS_RELIABLE);
    const uint8_t* fb_data = builder.GetBufferPointer();
    uint16_t fb_size = builder.GetSize();

    std::vector<uint8_t> packet_buffer_uint8 = RiftForged::Networking::PrepareOutgoingPacket(
        g_clientToServerState, fb_data, fb_size, packetFlags); // MessageType removed
    return std::vector<char>(packet_buffer_uint8.begin(), packet_buffer_uint8.end());
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

    uint8_t packetFlags = static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::NONE); // Unreliable
    const uint8_t* fb_data = builder.GetBufferPointer();
    uint16_t fb_size = builder.GetSize();

    std::vector<uint8_t> packet_buffer_uint8 = RiftForged::Networking::PrepareOutgoingPacket(
        g_clientToServerState, fb_data, fb_size, packetFlags); // MessageType removed
    return std::vector<char>(packet_buffer_uint8.begin(), packet_buffer_uint8.end());
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

    uint8_t packetFlags = static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::NONE); // Unreliable
    const uint8_t* fb_data = builder.GetBufferPointer();
    uint16_t fb_size = builder.GetSize();
    std::vector<uint8_t> packet_buffer_uint8 = RiftForged::Networking::PrepareOutgoingPacket(
        g_clientToServerState, fb_data, fb_size, packetFlags); // MessageType removed
    return std::vector<char>(packet_buffer_uint8.begin(), packet_buffer_uint8.end());
}

std::vector<char> BuildRiftStepActivationPacket(uint64_t client_timestamp_ms, RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent intent) {
    flatbuffers::FlatBufferBuilder builder(128);
    auto fb_payload_offset = RiftForged::Networking::UDP::C2S::CreateC2S_RiftStepActivationMsg(builder, client_timestamp_ms, intent);
    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_RiftStepActivation);
    root_builder.add_payload(fb_payload_offset.Union());
    auto root_message_offset = root_builder.Finish();
    builder.Finish(root_message_offset);

    uint8_t packetFlags = static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::IS_RELIABLE);
    const uint8_t* fb_data = builder.GetBufferPointer();
    uint16_t fb_size = builder.GetSize();
    std::vector<uint8_t> packet_buffer_uint8 = RiftForged::Networking::PrepareOutgoingPacket(
        g_clientToServerState, fb_data, fb_size, packetFlags); // MessageType removed
    return std::vector<char>(packet_buffer_uint8.begin(), packet_buffer_uint8.end());
}

std::vector<char> BuildBasicAttackIntentPacket(uint64_t client_timestamp_ms, const RiftForged::Networking::Shared::Vec3& world_aim_direction, uint64_t target_id) {
    flatbuffers::FlatBufferBuilder builder(256);
    auto fb_payload_offset = RiftForged::Networking::UDP::C2S::CreateC2S_BasicAttackIntentMsg(builder, client_timestamp_ms, &world_aim_direction, target_id);
    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_BasicAttackIntent);
    root_builder.add_payload(fb_payload_offset.Union());
    auto root_message_offset = root_builder.Finish();
    builder.Finish(root_message_offset);

    uint8_t packetFlags = static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::IS_RELIABLE);
    const uint8_t* fb_data = builder.GetBufferPointer();
    uint16_t fb_size = builder.GetSize();
    std::vector<uint8_t> packet_buffer_uint8 = RiftForged::Networking::PrepareOutgoingPacket(
        g_clientToServerState, fb_data, fb_size, packetFlags); // MessageType removed
    return std::vector<char>(packet_buffer_uint8.begin(), packet_buffer_uint8.end());
}

std::vector<char> BuildJoinShardRequestPacket(uint64_t client_timestamp_ms, const std::string& character_id) {
    flatbuffers::FlatBufferBuilder builder(256);
    auto charIdOffset = character_id.empty() ? 0 : builder.CreateString(character_id);
    auto join_request_payload = RiftForged::Networking::UDP::C2S::CreateC2S_JoinRequestMsg(
        builder, client_timestamp_ms, charIdOffset);

    RiftForged::Networking::UDP::C2S::Root_C2S_UDP_MessageBuilder root_builder(builder);
    root_builder.add_payload_type(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_JoinRequest);
    root_builder.add_payload(join_request_payload.Union());
    auto root_message_offset = root_builder.Finish();
    builder.Finish(root_message_offset);

    uint8_t packetFlags = static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::IS_RELIABLE);
    const uint8_t* fb_data = builder.GetBufferPointer();
    uint16_t fb_size = builder.GetSize();

    std::vector<uint8_t> packet_buffer_uint8 = RiftForged::Networking::PrepareOutgoingPacket(
        g_clientToServerState, fb_data, fb_size, packetFlags); // MessageType removed
    return std::vector<char>(packet_buffer_uint8.begin(), packet_buffer_uint8.end());
}

// --- S2C Packet Parsing ---
// These functions still receive the full S2C FlatBuffer root message as their payload.
// Their internal logic (verifying root, checking specific payload_type, casting) remains valid.

void ParsePongPacket(const uint8_t* app_payload_ptr, uint16_t app_payload_size) {
    flatbuffers::Verifier verifier(app_payload_ptr, static_cast<size_t>(app_payload_size));
    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) { RF_CORE_ERROR("Client: S2C_PongMsg FlatBuffer verification failed."); g_last_server_event_for_display = "Pong Verification Failed"; return; }
    auto root = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(app_payload_ptr);
    if (!root || root->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_Pong) { RF_CORE_WARN("Client: Received non-Pong payload when expected."); g_last_server_event_for_display = "ERROR: Not a Pong Payload."; return; }
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
    if (g_client_player_id == 0 && update->entity_id() != 0 && g_join_state == ClientJoinState::AttemptingJoin) {
        g_client_player_id = update->entity_id();
        oss << " (My PlayerID assigned to " << g_client_player_id << ")";
        RF_CORE_INFO("Client: Player ID assigned to {} via EntityStateUpdate during join attempt.", g_client_player_id);
    }
    if (update->entity_id() == g_client_player_id) {
        if (update->position()) { g_client_position = *update->position(); }
        if (update->orientation()) { g_client_orientation_quaternion = *update->orientation(); }
    }
    if (update->position()) oss << " Pos: (" << update->position()->x() << "," << update->position()->y() << "," << update->position()->z() << ")";
    oss << " HP: " << update->current_health() << "/" << update->max_health();
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

void ParseJoinSuccessPacket(const uint8_t* app_payload_ptr, uint16_t app_payload_size) {
    RF_CORE_INFO("Client: Entered ParseJoinSuccessPacket. Payload size: {}", app_payload_size);
    flatbuffers::Verifier verifier(app_payload_ptr, static_cast<size_t>(app_payload_size));
    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) {
        RF_CORE_ERROR("Client: S2C_JoinSuccessMsg FlatBuffer verification failed.");
        g_last_server_event_for_display = "JoinSuccess Verification Failed";
        g_join_state = ClientJoinState::FailedToJoin;
        return;
    }
    RF_CORE_INFO("Client: S2C_JoinSuccessMsg verification successful.");

    auto root = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(app_payload_ptr);
    if (!root) {
        RF_CORE_ERROR("Client: GetRoot_S2C_UDP_Message returned null for JoinSuccess.");
        g_join_state = ClientJoinState::FailedToJoin; return;
    }
    RF_CORE_INFO("Client: Got Root_S2C_UDP_Message. Expected Payload Type: S2C_JoinSuccessMsg, Actual: {}",
        RiftForged::Networking::UDP::S2C::EnumNameS2C_UDP_Payload(root->payload_type()));

    if (root->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_S2C_JoinSuccessMsg) {
        RF_CORE_WARN("Client: Received non-JoinSuccess payload when expected.");
        // ... (rest of your existing error handling for this case) ...
        g_join_state = ClientJoinState::FailedToJoin; return;
    }
    RF_CORE_INFO("Client: Payload type is S2C_JoinSuccessMsg.");

    auto join_success_msg = root->payload_as_S2C_JoinSuccessMsg();
    if (!join_success_msg) {
        RF_CORE_ERROR("Client: Failed to get JoinSuccess message from payload (payload_as_S2C_JoinSuccessMsg returned null).");
        g_last_server_event_for_display = "ERROR: Failed to cast to JoinSuccess msg.";
        g_join_state = ClientJoinState::FailedToJoin;
        return;
    }
    RF_CORE_INFO("Client: Successfully cast to S2C_JoinSuccessMsg.");

    // Log before accessing each field
    RF_CORE_INFO("Client: Accessing assigned_player_id...");
    g_client_player_id = join_success_msg->assigned_player_id();
    RF_CORE_INFO("Client: Assigned Player ID: {}", g_client_player_id);

    g_join_state = ClientJoinState::Joined;

    std::string welcome_msg_str = "Welcome!";
    RF_CORE_INFO("Client: Accessing welcome_message (optional)...");
    if (join_success_msg->welcome_message()) {
        RF_CORE_INFO("Client: welcome_message exists. Accessing str()...");
        welcome_msg_str = join_success_msg->welcome_message()->str(); // Potential crash if welcome_message() is non-null but invalid
        RF_CORE_INFO("Client: Welcome message: {}", welcome_msg_str);
    }
    else {
        RF_CORE_INFO("Client: No welcome_message present.");
    }

    RF_CORE_INFO("Client: Accessing server_tick_rate_hz...");
    uint16_t tick_rate = join_success_msg->server_tick_rate_hz();
    RF_CORE_INFO("Client: Server tick rate: {}", tick_rate);

    g_last_server_event_for_display = "JOIN SUCCESSFUL! Player ID: " + std::to_string(g_client_player_id) +
        " Msg: " + welcome_msg_str +
        " TickRate: " + std::to_string(tick_rate) + "Hz";
    RF_CORE_INFO("Client: ParseJoinSuccessPacket completed: {}", g_last_server_event_for_display);
    RF_CORE_CRITICAL("Client: ParseJoinSuccessPacket FULLY COMPLETED. Player ID: {}, Join State: {}", g_client_player_id, static_cast<int>(g_join_state));

}

void ParseJoinFailedPacket(const uint8_t* app_payload_ptr, uint16_t app_payload_size) {
    g_join_state = ClientJoinState::FailedToJoin;
    flatbuffers::Verifier verifier(app_payload_ptr, static_cast<size_t>(app_payload_size));
    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) {
        RF_CORE_ERROR("Client: S2C_JoinFailedMsg FlatBuffer verification failed.");
        g_last_server_event_for_display = "JoinFailed Verification Failed"; return;
    }
    auto root = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(app_payload_ptr);
    if (!root || root->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_S2C_JoinFailedMsg) {
        RF_CORE_WARN("Client: Received non-JoinFailed payload when expected (Type: {}), or root is null.", root ? RiftForged::Networking::UDP::S2C::EnumNameS2C_UDP_Payload(root->payload_type()) : "null");
        g_last_server_event_for_display = "ERROR: Not a JoinFailed Payload."; return;
    }
    auto join_failed_msg = root->payload_as_S2C_JoinFailedMsg();
    std::string reason = "Unknown reason";
    int16_t reason_code = 0;
    if (join_failed_msg) {
        if (join_failed_msg->reason_message()) {
            reason = join_failed_msg->reason_message()->str();
        }
        reason_code = join_failed_msg->reason_code();
        g_last_server_event_for_display = "JOIN FAILED! Reason: " + reason + " (Code: " + std::to_string(reason_code) + ")";
    }
    else {
        g_last_server_event_for_display = "JOIN FAILED! (Could not parse details).";
    }
    RF_CORE_ERROR("Client: {}", g_last_server_event_for_display);
}

void DisplayClientState() {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[H\033[J");
#endif
    std::cout << "RiftForged Test Client (Reliability Enabled, FB Payload Dispatch)" << std::endl;
    std::cout << "Controls: J (Join), WASD (move), QE (turn), SPACE (RiftStep), F (Attack), P (Ping), 0 (quit)" << std::endl;
    std::cout << "-----------------------------------------------------------------------" << std::endl;
    RiftForged::Networking::Shared::Vec3 display_fwd = RiftForged::Utilities::Math::GetWorldForwardVector(g_client_orientation_quaternion);
    std::cout << "Client ID: " << g_client_player_id << " | Join State: ";
    switch (g_join_state) {
    case ClientJoinState::Disconnected: std::cout << "Disconnected"; break;
    case ClientJoinState::AttemptingJoin: std::cout << "Attempting Join..."; break;
    case ClientJoinState::Joined: std::cout << "Joined"; break;
    case ClientJoinState::FailedToJoin: std::cout << "Join Failed"; break;
    }
    std::cout << std::endl;
    std::cout << "Pos: (" << g_client_position.x() << ", " << g_client_position.y() << ", " << g_client_position.z()
        << ") Facing (Approx): (" << display_fwd.x() << ", " << display_fwd.y() << ")" << std::endl;
    std::cout << "Last Server Event: " << g_last_server_event_for_display << std::endl;
    std::cout << "-----------------------------------------------------------------------" << std::endl;
    std::cout << "Input: " << std::flush;
}

RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent GetRiftStepIntentFromHeldKeys() {
    if (GetAsyncKeyState('W') & 0x8000) return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Forward;
    if (GetAsyncKeyState('S') & 0x8000) return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Backward;
    if (GetAsyncKeyState('A') & 0x8000) return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Left;
    if (GetAsyncKeyState('D') & 0x8000) return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Right;
    return RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Default_Backward;
}

int main() {
    RiftForged::Utilities::Logger::Init(spdlog::level::trace, spdlog::level::trace, "logs/test_client_final.log");
    RF_CORE_INFO("RiftForged Test Client Starting (Protocol v{:#0X})...", RiftForged::Networking::CURRENT_PROTOCOL_ID_VERSION);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        RF_CORE_CRITICAL("Client: WSAStartup failed: {}", WSAGetLastError());
        return 1;
    }
    RF_CORE_INFO("Client: WSAStartup successful.");

    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) {
        RF_CORE_CRITICAL("Client: Socket creation failed: {}", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    RF_CORE_INFO("Client: Socket created (ID: {}).", clientSocket);

    sockaddr_in clientAddr;
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = 0;

    if (bind(clientSocket, (sockaddr*)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR) {
        RF_CORE_CRITICAL("Client: bind failed with error: {}. Cannot receive replies reliably.", WSAGetLastError());
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    int clientAddrLen = sizeof(clientAddr);
    if (getsockname(clientSocket, (sockaddr*)&clientAddr, &clientAddrLen) == 0) {
        RF_CORE_INFO("Client: Socket bound successfully to local port {}.", ntohs(clientAddr.sin_port));
    }
    else {
        RF_CORE_INFO("Client: Socket bound successfully (could not retrieve assigned port).");
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345); // Default server port
    const char* serverIp = "192.168.50.186"; // Changed to localhost for easier local testing
    if (inet_pton(AF_INET, serverIp, &serverAddr.sin_addr) != 1) {
        RF_CORE_CRITICAL("Client: inet_pton failed for IP {}: {}", serverIp, WSAGetLastError());
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    RF_CORE_INFO("Client: Server address configured for {}:{}", serverIp, ntohs(serverAddr.sin_port));

    DWORD client_socket_timeout_ms = 10; // Non-blocking receive
    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&client_socket_timeout_ms, sizeof(client_socket_timeout_ms)) == SOCKET_ERROR) {
        RF_CORE_WARN("Client: setsockopt for SO_RCVTIMEO failed: {}. recvfrom will block if no data.", WSAGetLastError());
    }

    bool running = true;
    g_join_state = ClientJoinState::Disconnected;
    DisplayClientState();
    RF_CORE_INFO("Client: Ready. Press 'J' to send Join Request.");

    auto last_periodic_action_time = std::chrono::steady_clock::now();
    const auto periodic_action_interval = std::chrono::seconds(3);

    while (running) {
        std::vector<char> packet_to_send;
        bool wants_to_send_action_this_loop = false;
        RiftForged::Networking::Shared::Vec3 local_move_intent(0.f, 0.f, 0.f);

        if (_kbhit()) {
            char input_char_event = static_cast<char>(_getch());
            RF_CORE_DEBUG("Client Raw Input Event: '{}'", input_char_event);

            switch (toupper(input_char_event)) {
            case 'J':
                if (g_join_state == ClientJoinState::Disconnected || g_join_state == ClientJoinState::FailedToJoin) {
                    g_join_state = ClientJoinState::AttemptingJoin;
                    uint64_t join_ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    packet_to_send = BuildJoinShardRequestPacket(join_ts, g_character_id_to_load);
                    wants_to_send_action_this_loop = true;
                    g_last_server_event_for_display = "Sent C2S_JoinRequest...";
                    RF_CORE_INFO("Client: Action - Sending Join Request.");
                }
                else {
                    g_last_server_event_for_display = "Already joined or attempting to join.";
                    RF_CORE_INFO("Client: Action - Join (State: {}). No new request sent.", static_cast<int>(g_join_state));
                }
                break;
            case 'W': if (g_join_state == ClientJoinState::Joined) { local_move_intent = { 0.0f, 1.0f, 0.0f }; packet_to_send = BuildMovementInputPacket(local_move_intent, GetAsyncKeyState(VK_SHIFT) & 0x8000); wants_to_send_action_this_loop = true; } break;
            case 'S': if (g_join_state == ClientJoinState::Joined) { local_move_intent = { 0.0f, -1.0f,0.0f }; packet_to_send = BuildMovementInputPacket(local_move_intent, GetAsyncKeyState(VK_SHIFT) & 0x8000); wants_to_send_action_this_loop = true; } break;
            case 'A': if (g_join_state == ClientJoinState::Joined) { local_move_intent = { -1.0f, 0.0f,0.0f }; packet_to_send = BuildMovementInputPacket(local_move_intent, GetAsyncKeyState(VK_SHIFT) & 0x8000); wants_to_send_action_this_loop = true; } break;
            case 'D': if (g_join_state == ClientJoinState::Joined) { local_move_intent = { 1.0f,  0.0f,0.0f }; packet_to_send = BuildMovementInputPacket(local_move_intent, GetAsyncKeyState(VK_SHIFT) & 0x8000); wants_to_send_action_this_loop = true; } break;
            case 'Q': if (g_join_state == ClientJoinState::Joined) { packet_to_send = BuildTurnIntentPacket(-CLIENT_TURN_INCREMENT_DEGREES); wants_to_send_action_this_loop = true; } break;
            case 'E': if (g_join_state == ClientJoinState::Joined) { packet_to_send = BuildTurnIntentPacket(CLIENT_TURN_INCREMENT_DEGREES); wants_to_send_action_this_loop = true; } break;
            case ' ': if (g_join_state == ClientJoinState::Joined) {
                uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent rift_intent = GetRiftStepIntentFromHeldKeys();
                packet_to_send = BuildRiftStepActivationPacket(ts, rift_intent);
                wants_to_send_action_this_loop = true;
                RF_CORE_DEBUG("Client: Action - RiftStep. Intent: {}", RiftForged::Networking::UDP::C2S::EnumNameRiftStepDirectionalIntent(rift_intent));
                break;
            }
            case 'F': if (g_join_state == ClientJoinState::Joined) {
                uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                RiftForged::Networking::Shared::Vec3 world_aim_dir = RiftForged::Utilities::Math::GetWorldForwardVector(g_client_orientation_quaternion);
                packet_to_send = BuildBasicAttackIntentPacket(ts, world_aim_dir, 0); // Target ID 0 for now
                wants_to_send_action_this_loop = true;
                RF_CORE_DEBUG("Client: Action - Basic Attack Intent.");
                break;
            }
            case 'P':
                if (g_join_state == ClientJoinState::Joined || g_join_state == ClientJoinState::AttemptingJoin) {
                    uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    packet_to_send = BuildPingPacket(ts);
                    wants_to_send_action_this_loop = true;
                    RF_CORE_DEBUG("Client: Action - Ping.");
                }
                else {
                    g_last_server_event_for_display = "Cannot Ping: Not joined or attempting to join.";
                }
                break;
            case '0': running = false; RF_CORE_INFO("Client: Quit command (0) received."); break;
            default: RF_CORE_INFO("Client: Unknown input command '{}'", input_char_event); g_last_server_event_for_display = "Unknown command: " + std::string(1, input_char_event); break;
            }
            if (running) DisplayClientState();
        }

        auto now = std::chrono::steady_clock::now();
        if (!wants_to_send_action_this_loop && (now - last_periodic_action_time > periodic_action_interval)) {
            if (g_join_state == ClientJoinState::AttemptingJoin) {
                RF_CORE_INFO("Client: Resending C2S_JoinRequest (periodic)...");
                packet_to_send = BuildJoinShardRequestPacket(
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(),
                    g_character_id_to_load
                );
                wants_to_send_action_this_loop = true;
            }
            else if (g_join_state == ClientJoinState::Joined) {
                RF_CORE_DEBUG("Client: Sending periodic Ping (keep-alive)...");
                packet_to_send = BuildPingPacket(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
                wants_to_send_action_this_loop = true;
            }
            if (wants_to_send_action_this_loop) last_periodic_action_time = now;
        }

        if (wants_to_send_action_this_loop && !packet_to_send.empty()) {
            if (packet_to_send.size() >= RiftForged::Networking::GetGamePacketHeaderSize()) {
                RiftForged::Networking::GamePacketHeader sent_hdr_preview;
                memcpy(&sent_hdr_preview, packet_to_send.data(), RiftForged::Networking::GetGamePacketHeaderSize());
                // The sent_hdr_preview.messageType would show a generic type or be 0 if removed from header.
                // To log specific type, you'd need to inspect the FlatBuffer before wrapping it with reliability header.
                // For now, we'll keep the old log which might show a generic MessageType.
                RF_CORE_DEBUG("Client: Sending packet. Seq: {}, AckNum: {}, AckBits: 0x{:X}, Flags: 0x{:X}",
                    sent_hdr_preview.sequenceNumber, sent_hdr_preview.ackNumber,
                    sent_hdr_preview.ackBitfield, sent_hdr_preview.flags);
            }
            if (sendto(clientSocket, packet_to_send.data(), static_cast<int>(packet_to_send.size()), 0,
                (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                RF_CORE_ERROR("Client: sendto failed: {}", WSAGetLastError());
            }
        }

        char recvBuffer[CLIENT_RECEIVE_BUFFER_SIZE];
        sockaddr_in fromAddr;
        int fromAddrLen = sizeof(fromAddr);
        int bytesReceived = recvfrom(clientSocket, recvBuffer, sizeof(recvBuffer), 0, (sockaddr*)&fromAddr, &fromAddrLen);
        bool state_changed_by_receive_this_loop = false;

        if (bytesReceived >= static_cast<int>(RiftForged::Networking::GetGamePacketHeaderSize())) {
            RiftForged::Networking::GamePacketHeader s2c_header;
            memcpy(&s2c_header, recvBuffer, RiftForged::Networking::GetGamePacketHeaderSize());
            const uint8_t* s2c_full_payload_ptr = reinterpret_cast<const uint8_t*>(recvBuffer + RiftForged::Networking::GetGamePacketHeaderSize());
            uint16_t s2c_full_payload_len = static_cast<uint16_t>(bytesReceived - RiftForged::Networking::GetGamePacketHeaderSize());

            if (s2c_header.protocolId == RiftForged::Networking::CURRENT_PROTOCOL_ID_VERSION) {
                const uint8_t* app_payload_to_process_ptr = nullptr;
                uint16_t app_payload_size = 0;
                bool should_process_app_payload = RiftForged::Networking::ProcessIncomingPacketHeader(
                    g_clientToServerState, s2c_header, s2c_full_payload_ptr, s2c_full_payload_len,
                    &app_payload_to_process_ptr, &app_payload_size);

                if (should_process_app_payload && app_payload_to_process_ptr && app_payload_size > 0) {
                    flatbuffers::Verifier verifier(app_payload_to_process_ptr, static_cast<size_t>(app_payload_size));
                    if (!RiftForged::Networking::UDP::S2C::VerifyRoot_S2C_UDP_MessageBuffer(verifier)) {
                        RF_CORE_ERROR("Client: S2C Root FlatBuffer verification failed. Size: {}", app_payload_size);
                        g_last_server_event_for_display = "S2C Root Verification Failed";
                        state_changed_by_receive_this_loop = true;
                    }
                    else {
                        auto root_s2c_message = RiftForged::Networking::UDP::S2C::GetRoot_S2C_UDP_Message(app_payload_to_process_ptr);
                        if (root_s2c_message && root_s2c_message->payload_type() != RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_NONE) {
                            RF_CORE_DEBUG("Client: Reliability approved S2C FB payload. FB_Type: {}, AppPayloadSize: {}",
                                RiftForged::Networking::UDP::S2C::EnumNameS2C_UDP_Payload(root_s2c_message->payload_type()), app_payload_size);
                            state_changed_by_receive_this_loop = true; // Assume any valid message might change display state
                            switch (root_s2c_message->payload_type()) {
                            case RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_S2C_JoinSuccessMsg:
                                ParseJoinSuccessPacket(app_payload_to_process_ptr, app_payload_size);
                                break;
                            case RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_S2C_JoinFailedMsg:
                                ParseJoinFailedPacket(app_payload_to_process_ptr, app_payload_size);
                                break;
                            case RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_Pong:
                                ParsePongPacket(app_payload_to_process_ptr, app_payload_size);
                                break;
                            case RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_EntityStateUpdate:
                                ParseEntityStateUpdatePacket(app_payload_to_process_ptr, app_payload_size);
                                break;
                            case RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_RiftStepInitiated:
                                ParseRiftStepInitiatedPacket(app_payload_to_process_ptr, app_payload_size);
                                break;
                            case RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_CombatEvent:
                                ParseCombatEventPacket(app_payload_to_process_ptr, app_payload_size);
                                break;
                                // Add cases for S2C_SpawnProjectileMsg etc. if they are defined
                            default:
                                g_last_server_event_for_display = "S2C Unhandled FB Payload (Type: " +
                                    std::string(RiftForged::Networking::UDP::S2C::EnumNameS2C_UDP_Payload(root_s2c_message->payload_type())) + ")";
                                RF_CORE_WARN("Client: {}", g_last_server_event_for_display);
                                state_changed_by_receive_this_loop = false; // Or true to show "unhandled"
                                break;
                            }
                        }
                        else if (root_s2c_message && root_s2c_message->payload_type() == RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_NONE) {
                            RF_CORE_TRACE("Client: Received S2C Root message with explicit NONE payload. Likely an ACK with no app data. Seq: {}", s2c_header.sequenceNumber);
                            // No specific app payload to process, but reliability layer handled it.
                        }
                        else {
                            RF_CORE_ERROR("Client: Failed to get Root_S2C_UDP_Message from payload, or payload_type is NONE incorrectly. AppPayloadSize: {}", app_payload_size);
                            g_last_server_event_for_display = "ERROR: S2C Root Message Null or Invalid Payload_NONE";
                            state_changed_by_receive_this_loop = true;
                        }
                    }
                }
                else if (RiftForged::Networking::HasFlag(s2c_header.flags, RiftForged::Networking::GamePacketFlag::IS_ACK_ONLY) ||
                    (RiftForged::Networking::HasFlag(s2c_header.flags, RiftForged::Networking::GamePacketFlag::IS_RELIABLE) && !should_process_app_payload)) {
                    // This case handles ACK-only packets or reliable packets that were duplicates/out of order and thus have no app payload to process further by this layer.
                    RF_CORE_TRACE("Client: S2C Reliability packet processed (Seq {}). No app payload for client logic or it was a pure ACK.", s2c_header.sequenceNumber);
                }
            }
            else {
                g_last_server_event_for_display = "ERROR: S2C Mismatched Protocol ID! Expected " +
                    std::to_string(RiftForged::Networking::CURRENT_PROTOCOL_ID_VERSION) +
                    " Got " + std::to_string(s2c_header.protocolId);
                RF_CORE_WARN("Client: {}", g_last_server_event_for_display);
                state_changed_by_receive_this_loop = true;
            }
        }
        else if (bytesReceived > 0) {
            g_last_server_event_for_display = "ERROR: S2C Packet too small for header. Size: " + std::to_string(bytesReceived);
            RF_CORE_ERROR("Client: {}", g_last_server_event_for_display);
            state_changed_by_receive_this_loop = true;
        }
        else if (bytesReceived == SOCKET_ERROR) {
            int recvError = WSAGetLastError();
            if (recvError != WSAETIMEDOUT && recvError != WSAEWOULDBLOCK) {
                g_last_server_event_for_display = "ERROR: recvfrom failed with code: " + std::to_string(recvError);
                RF_CORE_ERROR("Client: {}", g_last_server_event_for_display);
                state_changed_by_receive_this_loop = true;
            }
        }

        if (g_clientToServerState.hasPendingAckToSend && !wants_to_send_action_this_loop && running) {
            auto currentTimeForAck = std::chrono::steady_clock::now();
            auto timeSinceLastClientSend = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTimeForAck - g_clientToServerState.lastPacketSentTimeToRemote);

            // Send ACK if no action packet sent for a bit, or if it's the very first ACK needed
            if (g_clientToServerState.lastPacketSentTimeToRemote == std::chrono::steady_clock::time_point::min() ||
                timeSinceLastClientSend.count() > 50) { // Send ACK if no packet sent for 50ms
                RF_CORE_DEBUG("Client: Pending ACK to server. Sending ACK-only packet.");
                std::vector<uint8_t> ack_packet_uint8 = RiftForged::Networking::PrepareOutgoingPacket(
                    g_clientToServerState, nullptr, 0, // No FB payload for ACK-only
                    static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::IS_RELIABLE) | static_cast<uint8_t>(RiftForged::Networking::GamePacketFlag::IS_ACK_ONLY));

                if (!ack_packet_uint8.empty()) {
                    std::vector<char> ack_packet_char(ack_packet_uint8.begin(), ack_packet_uint8.end());
                    if (sendto(clientSocket, ack_packet_char.data(), static_cast<int>(ack_packet_char.size()), 0,
                        (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                        RF_CORE_ERROR("Client: sendto for ACK-only packet failed: {}", WSAGetLastError());
                    }
                }
            }
        }

        if (state_changed_by_receive_this_loop && running) {
            DisplayClientState();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // Approx 60 FPS loop tick
    }

    RF_CORE_INFO("Client: Shutting down...");
    closesocket(clientSocket);
    WSACleanup();
    RiftForged::Utilities::Logger::FlushAll();
    RiftForged::Utilities::Logger::Shutdown();

    std::cout << "\nClient application shut down. Log file: logs/test_client_final.log" << std::endl;
    std::cout << "Press any key to exit console." << std::endl;
    while (_kbhit()) { _getch(); } // Clear buffer
    _getch(); // Wait for final key press
    return 0;
}