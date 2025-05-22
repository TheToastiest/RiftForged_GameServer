#pragma once

#include <cstdint> // For uint64_t, uint32_t
#include <string>  // For potential name/ID if needed later
#include <vector>  // For active_status_effects
#include "../FlatBuffers/V0.0.1/riftforged_common_types_generated.h" // For Shared::Vec3 and Shared::StatusEffectCategory
#include <iostream> // replace with Logger.h later.
#include "../NetworkEngine/NetworkEndpoint.h"
#include <chrono>
#include <map>
#include <cmath>

namespace RiftForged {
    namespace GameLogic {

        // Represents the current movement/action state of the player
        enum class PlayerMovementState : uint8_t {
            Idle = 0,
            Walking,
            Sprinting,
            // Rifting, // No longer a distinct long-lived state if server-side RiftStep position update is instant
            Stunned,
            Rooted,
            Dead,
            Ability_In_Use // Generic state for an instant ability animation/effect
        };

        struct ActivePlayer {
            // --- Identifiers ---
            uint64_t playerId;
            RiftForged::Networking::NetworkEndpoint networkEndpoint; // Stores the actual endpoint

            // --- Core Transform State ---
            RiftForged::Networking::Shared::Vec3 position;
            RiftForged::Networking::Shared::Quaternion orientation; // For 3D facing

            // --- Core Resources & Stats ---
            int currentWill;
            int maxWill;
            int currentHealth;
            int maxHealth;

            // --- Gameplay Modifiers & States ---
            uint32_t activeRiftStepModifierId; // ID of the currently selected RiftStep modifier
            std::vector<RiftForged::Networking::Shared::StatusEffectCategory> activeStatusEffects;
            uint32_t animationStateId;
            PlayerMovementState movementState;

            // --- Ability Cooldowns ---
            // Key: AbilityDefID (e.g., a const uint32_t RIFTSTEP_ABILITY_ID), Value: Time point when cooldown ends
            std::map<uint32_t, std::chrono::steady_clock::time_point> abilityCooldowns;

            // --- RiftStep Base Parameters (Loaded from config/class data, modified by passives) ---
            float base_riftstep_distance;
            float base_riftstep_cooldown_sec;
            // float base_riftstep_cosmetic_travel_time_sec; // If client needs a hint for visual duration

            // --- Synchronization ---
            bool isDirty; // Flag to indicate state has changed and needs sync for S2C_EntityStateUpdateMsg

            // Constructor
            ActivePlayer(uint64_t pId = 0,
                const RiftForged::Networking::NetworkEndpoint& ep = {},
                const RiftForged::Networking::Shared::Vec3& startPos = { 0.0f, 0.0f, 0.0f },
                const RiftForged::Networking::Shared::Quaternion& startOrientation = { 0.0f, 0.0f, 0.0f, 1.0f }) // Identity quaternion
                : playerId(pId),
                networkEndpoint(ep),
                position(startPos),
                orientation(startOrientation),
                currentWill(100), maxWill(100),     // Example default for "Will" resource
                currentHealth(250), maxHealth(250), // Example default
                activeRiftStepModifierId(0),        // Default: standard RiftStep or no specific modifier
                animationStateId(static_cast<uint32_t>(RiftForged::Networking::Shared::AnimationState_Idle)), // Use generated enum name
                movementState(PlayerMovementState::Idle),
                isDirty(true), // Start as dirty to send initial state to client
                base_riftstep_distance(5.0f),          // Default RiftStep distance
                base_riftstep_cooldown_sec(1.25f)      // Default RiftStep cooldown
                // base_riftstep_cosmetic_travel_time_sec(0.25f) // Example for client visuals
            {
                // std::cout << "ActivePlayer CONSTRUCTOR for ID: " << playerId 
                //           << " from endpoint: " << networkEndpoint.ToString() 
                //           << " Initial Pos: (" << position.x() << "," << position.y() << "," << position.z() << ")"
                //           << std::endl;
                if (networkEndpoint.ipAddress.empty() || networkEndpoint.port == 0) {
                    // This log helps catch the "ghost player 0" if it's from a default-constructed endpoint
                    std::cerr << "WARNING: ActivePlayer CONSTRUCTOR ID: " << playerId << " created with INVALID endpoint details: " << networkEndpoint.ToString() << std::endl;
                }
            }

            // --- State Modifying Methods ---
            void SetPosition(const RiftForged::Networking::Shared::Vec3& newPosition) {
                const float POSITION_EPSILON = 0.0001f;
                if (std::abs(position.x() - newPosition.x()) > POSITION_EPSILON ||
                    std::abs(position.y() - newPosition.y()) > POSITION_EPSILON ||
                    std::abs(position.z() - newPosition.z()) > POSITION_EPSILON) {
                    position = newPosition;
                    isDirty = true;
                    // std::cout << "ActivePlayer::SetPosition - PlayerID " << playerId << " new pos: (" << position.x() << "," << position.y() << "," << position.z() << "). Marked DIRTY." << std::endl;
                }
            }

            void SetOrientation(const RiftForged::Networking::Shared::Quaternion& newOrientation) {
                const float ORIENTATION_EPSILON = 0.00001f;
                if (std::abs(orientation.x() - newOrientation.x()) > ORIENTATION_EPSILON ||
                    std::abs(orientation.y() - newOrientation.y()) > ORIENTATION_EPSILON ||
                    std::abs(orientation.z() - newOrientation.z()) > ORIENTATION_EPSILON ||
                    std::abs(orientation.w() - newOrientation.w()) > ORIENTATION_EPSILON) {
                    orientation = newOrientation; // Assuming newOrientation is already normalized if needed
                    isDirty = true;
                    // std::cout << "ActivePlayer::SetOrientation - PlayerID " << playerId << ". Marked DIRTY." << std::endl;
                }
            }

            void SetWill(int value) {
                int newWill = value;
                if (newWill > maxWill) newWill = maxWill;
                if (newWill < 0) newWill = 0;

                if (currentWill != newWill) {
                    currentWill = newWill;
                    isDirty = true;
                }
            }

            void DeductWill(int amount) {
                if (amount == 0) return;
                SetWill(currentWill - amount); // Use SetWill to apply logic and dirty flag
            }

            void SetHealth(int value) {
                int newHealth = value;
                if (newHealth > maxHealth) newHealth = maxHealth;
                if (newHealth < 0) newHealth = 0;

                if (currentHealth != newHealth) {
                    currentHealth = newHealth;
                    isDirty = true;
                }
            }

            void TakeDamage(int amount) {
                if (amount <= 0) return;
                int newHealth = currentHealth - amount;
                SetHealth(newHealth); // Use SetHealth to apply logic and dirty flag

                if (currentHealth == 0 && movementState != PlayerMovementState::Dead) {
                    SetMovementState(PlayerMovementState::Dead);
                    SetAnimationState(static_cast<uint32_t>(RiftForged::Networking::Shared::AnimationState_Dead));
                    // Additional death logic here
                }
            }

            void SetAnimationState(uint32_t newStateId) {
                if (animationStateId != newStateId) {
                    animationStateId = newStateId;
                    isDirty = true;
                }
            }

            void SetMovementState(PlayerMovementState newState) {
                if (movementState != newState) {
                    movementState = newState;
                    isDirty = true;
                    // Potentially update animationStateId based on movementState here too
                    // e.g., if newState is Idle, set animationStateId to AnimationState_Idle
                    // For simplicity, keeping them somewhat separate but linked by game logic.
                }
            }

            // --- Cooldown Management ---
            bool IsAbilityOnCooldown(uint32_t abilityId) const {
                auto it = abilityCooldowns.find(abilityId);
                if (it != abilityCooldowns.end()) {
                    return std::chrono::steady_clock::now() < it->second;
                }
                return false; // Not on cooldown (or never used and tracked)
            }

            void SetAbilityCooldown(uint32_t abilityId, int duration_ms) {
                if (duration_ms <= 0) { // Clear cooldown or invalid duration
                    abilityCooldowns.erase(abilityId);
                    return;
                }
                abilityCooldowns[abilityId] = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
                // Setting a cooldown doesn't usually make the player "dirty" for a full state update,
                // as the client often tracks its own cooldowns predictively after a successful ability cast.
                // However, if buffs/debuffs affect cooldowns, those would make the player dirty.
            }

            void SetActiveRiftStepModifier(uint32_t modifierId) {
                if (activeRiftStepModifierId != modifierId) {
                    activeRiftStepModifierId = modifierId;
                    isDirty = true;
                }
            }

            // TODO: Methods for AddStatusEffect, RemoveStatusEffect which would modify activeStatusEffects and set isDirty
        };

    } // namespace GameLogic
} // namespace RiftForged