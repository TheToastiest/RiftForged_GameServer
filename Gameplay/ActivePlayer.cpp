// File: Gameplay/ActivePlayer.cpp (Refactored with current RiftStepLogic.h)
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#include "ActivePlayer.h"
#include "../Utils/MathUtil.h" // For NormalizeQuaternion, AreVectorsClose, DistanceSquared, etc.
#include "../Utils/Logger.h"   // For RF_GAMELOGIC_DEBUG, RF_GAMEPLAY_INFO etc.

// RiftStepLogic.h is included via ActivePlayer.h
// FlatBuffer headers are included via ActivePlayer.h for types like Vec3, Quaternion, DamageType, AnimationState

namespace RiftForged {
    namespace GameLogic {

        // --- Constructor ---
        ActivePlayer::ActivePlayer(uint64_t pId,
            const RiftForged::Networking::Shared::Vec3& startPos,
            const RiftForged::Networking::Shared::Quaternion& startOrientation,
            float cap_radius, float cap_half_height)
            : playerId(pId),
            position(startPos),
            orientation(RiftForged::Utilities::Math::NormalizeQuaternion(startOrientation)),
            capsule_radius(cap_radius),
            capsule_half_height(cap_half_height),
            currentHealth(250), maxHealth(250),
            currentWill(100), maxWill(100),
            base_ability_cooldown_modifier(1.0f),
            base_critical_hit_chance_percent(5.0f),
            base_critical_hit_damage_multiplier(2.0f),
            base_accuracy_rating_percent(75.0f),
            base_basic_attack_cooldown_sec(1.0f),
            flat_physical_damage_reduction(10), percent_physical_damage_reduction(0.0f),
            flat_radiant_damage_reduction(0), percent_radiant_damage_reduction(0.0f),
            flat_frost_damage_reduction(0), percent_frost_damage_reduction(0.0f),
            flat_shock_damage_reduction(0), percent_shock_damage_reduction(0.0f),
            flat_necrotic_damage_reduction(0), percent_necrotic_damage_reduction(0.0f),
            flat_void_damage_reduction(0), percent_void_damage_reduction(-0.15f),
            flat_cosmic_damage_reduction(0), percent_cosmic_damage_reduction(0.0f),
            flat_poison_damage_reduction(0), percent_poison_damage_reduction(0.0f),
            flat_nature_damage_reduction(0), percent_nature_damage_reduction(0.0f),
            flat_aetherial_damage_reduction(0), percent_aetherial_damage_reduction(-0.50f),
            current_rift_step_definition(RiftStepDefinition::CreateBasicRiftStep()), // Uses static factory from RiftStepLogic.h
            current_weapon_category(EquippedWeaponCategory::Unarmed),
            equipped_weapon_definition_id(0),
            movementState(PlayerMovementState::Idle),
            animationStateId(static_cast<uint32_t>(RiftForged::Networking::Shared::AnimationState::AnimationState_Idle)),
            isDirty(true),
            last_processed_movement_intent({ 0.f, 0.f, 0.f }),
            was_sprint_intended(false) {
            RF_GAMELOGIC_DEBUG("ActivePlayer {} constructed. Initial RiftStep: '{}'. Pos:({:.1f},{:.1f},{:.1f})",
                playerId, current_rift_step_definition.name_tag, position.x(), position.y(), position.z());
        }

        void ActivePlayer::MarkDirty() {
            isDirty.store(true, std::memory_order_release);
        }

        // --- State Modification Methods ---
        // (SetPosition, SetOrientation, SetWill, DeductWill, AddWill, SetHealth, HealDamage, TakeDamage,
        //  SetAnimationState, SetAnimationStateId, SetMovementState methods remain largely the same as the previous merged version,
        //  as they were already using types compatible with RiftStepLogic.h's FlatBuffer dependencies.
        //  Ensure EnumNameDamageType and EnumNameRiftStepDirectionalIntent are available or adapt logging.)

        void ActivePlayer::SetPosition(const RiftForged::Networking::Shared::Vec3& newPosition) {
            const float POSITION_EPSILON_SQUARED = 0.0001f * 0.0001f;
            if (RiftForged::Utilities::Math::DistanceSquared(position, newPosition) > POSITION_EPSILON_SQUARED) {
                position = newPosition;
                MarkDirty();
            }
        }

        void ActivePlayer::SetOrientation(const RiftForged::Networking::Shared::Quaternion& newOrientation) {
            RiftForged::Networking::Shared::Quaternion normalizedNewOrientation = RiftForged::Utilities::Math::NormalizeQuaternion(newOrientation);
            if (!RiftForged::Utilities::Math::AreQuaternionsClose(orientation, normalizedNewOrientation, 0.99999f)) {
                orientation = normalizedNewOrientation;
                MarkDirty();
            }
        }

        void ActivePlayer::SetWill(int32_t value) {
            int32_t newWill = std::max(0, std::min(value, static_cast<int32_t>(maxWill)));
            if (currentWill != newWill) {
                currentWill = newWill;
                MarkDirty();
            }
        }

        void ActivePlayer::DeductWill(int32_t amount) {
            if (amount <= 0) return;
            SetWill(currentWill - amount);
        }

        void ActivePlayer::AddWill(int32_t amount) {
            if (amount <= 0) return;
            SetWill(currentWill + amount);
        }

        void ActivePlayer::SetHealth(int32_t value) {
            int32_t newHealth = std::max(0, std::min(value, static_cast<int32_t>(maxHealth)));
            if (currentHealth != newHealth) {
                currentHealth = newHealth;
                MarkDirty();
                if (currentHealth == 0 && movementState != PlayerMovementState::Dead) {
                    SetMovementState(PlayerMovementState::Dead);
                    RF_GAMEPLAY_INFO("Player {} health reached 0. Marked as Dead.", playerId);
                }
            }
        }

        void ActivePlayer::HealDamage(int32_t amount) {
            if (amount <= 0 || movementState == PlayerMovementState::Dead) return;
            SetHealth(currentHealth + amount);
        }

        int32_t ActivePlayer::TakeDamage(int32_t raw_damage_amount, RiftForged::Networking::Shared::DamageType damage_type) {
            if (raw_damage_amount <= 0 || movementState == PlayerMovementState::Dead) return 0;

            float percentage_reduction = 0.0f;
            int32_t flat_reduction = 0;

            switch (damage_type) {
            case RiftForged::Networking::Shared::DamageType::DamageType_Physical:  percentage_reduction = percent_physical_damage_reduction; flat_reduction = flat_physical_damage_reduction; break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Radiant:   percentage_reduction = percent_radiant_damage_reduction;  flat_reduction = flat_radiant_damage_reduction;  break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Frost:     percentage_reduction = percent_frost_damage_reduction;    flat_reduction = flat_frost_damage_reduction;    break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Shock:
            case RiftForged::Networking::Shared::DamageType::DamageType_Lightning: percentage_reduction = percent_shock_damage_reduction;    flat_reduction = flat_shock_damage_reduction;    break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Necrotic:  percentage_reduction = percent_necrotic_damage_reduction; flat_reduction = flat_necrotic_damage_reduction; break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Void:      percentage_reduction = percent_void_damage_reduction;     flat_reduction = flat_void_damage_reduction;     break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Cosmic:    percentage_reduction = percent_cosmic_damage_reduction;   flat_reduction = flat_cosmic_damage_reduction;   break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Poison:    percentage_reduction = percent_poison_damage_reduction;   flat_reduction = flat_poison_damage_reduction;   break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Nature:    percentage_reduction = percent_nature_damage_reduction;   flat_reduction = flat_nature_damage_reduction;   break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Aetherial: percentage_reduction = percent_aetherial_damage_reduction;flat_reduction = flat_aetherial_damage_reduction;break;
            case RiftForged::Networking::Shared::DamageType::DamageType_None:
            default: RF_GAMEPLAY_WARN("Player {} TakeDamage: Unhandled or 'None' damage type ({}) received. No reductions applied.", playerId, static_cast<int>(damage_type)); break;
            }

            int32_t damage_after_flat_reduction = std::max(0, raw_damage_amount - flat_reduction);
            float effective_percent_reduction = std::max(0.0f, std::min(1.0f, percentage_reduction / 100.0f));
            int32_t final_damage = static_cast<int32_t>(static_cast<float>(damage_after_flat_reduction) * (1.0f - effective_percent_reduction));
            final_damage = std::max(0, final_damage);

            // Assuming EnumNameDamageType is available globally or in a utility header.
            // If not, you might need to cast damage_type to int for logging or use a helper.
            RF_GAMEPLAY_INFO("Player {} taking {} raw damage of type {}. FlatRed: {}, PctRedVal: {:.2f}. Final: {}.",
                playerId, raw_damage_amount, static_cast<int>(damage_type), // logging type as int for now
                flat_reduction, percentage_reduction, final_damage);

            int32_t health_before_damage = currentHealth;
            SetHealth(currentHealth - final_damage);
            return health_before_damage - currentHealth;
        }

        void ActivePlayer::SetAnimationState(RiftForged::Networking::Shared::AnimationState newState) {
            SetAnimationStateId(static_cast<uint32_t>(newState));
        }

        void ActivePlayer::SetAnimationStateId(uint32_t newStateId) {
            if (animationStateId != newStateId) {
                animationStateId = newStateId;
                MarkDirty();
            }
        }

        void ActivePlayer::SetMovementState(PlayerMovementState newState) {
            if (movementState != newState) {
                PlayerMovementState oldState = movementState;
                movementState = newState;
                MarkDirty();
                RF_GAMELOGIC_TRACE("Player {} movement state changed from {} to {}", playerId, static_cast<int>(oldState), static_cast<int>(newState));

                switch (newState) {
                case PlayerMovementState::Idle:      SetAnimationStateId(static_cast<uint32_t>(RiftForged::Networking::Shared::AnimationState::AnimationState_Idle)); break;
                case PlayerMovementState::Walking:   SetAnimationStateId(static_cast<uint32_t>(RiftForged::Networking::Shared::AnimationState::AnimationState_Walking)); break;
                case PlayerMovementState::Sprinting: SetAnimationStateId(static_cast<uint32_t>(RiftForged::Networking::Shared::AnimationState::AnimationState_Running)); break;
                case PlayerMovementState::Dead:      SetAnimationStateId(static_cast<uint32_t>(RiftForged::Networking::Shared::AnimationState::AnimationState_Dead)); break;
                case PlayerMovementState::Stunned:   SetAnimationStateId(static_cast<uint32_t>(RiftForged::Networking::Shared::AnimationState::AnimationState_Stunned)); break;
                case PlayerMovementState::Rifting: break;
                case PlayerMovementState::Ability_In_Use: break;
                case PlayerMovementState::Rooted: break;
                default: RF_GAMELOGIC_WARN("Player {}: SetMovementState called with unhandled new state {}", playerId, static_cast<int>(newState)); break;
                }
            }
        }

        // --- Ability Cooldown Management ---
        bool ActivePlayer::IsAbilityOnCooldown(uint32_t abilityId) const {
            std::lock_guard<std::mutex> lock(m_internalDataMutex);
            auto it = abilityCooldowns.find(abilityId);
            if (it != abilityCooldowns.end()) {
                return std::chrono::steady_clock::now() < it->second;
            }
            return false;
        }

        void ActivePlayer::StartAbilityCooldown(uint32_t abilityId, float base_duration_sec) {
            std::lock_guard<std::mutex> lock(m_internalDataMutex);
            if (base_duration_sec <= 0.0f) {
                abilityCooldowns.erase(abilityId);
                RF_GAMELOGIC_TRACE("Player {} cooldown for ability {} cleared.", playerId, abilityId);
            }
            else {
                float modified_duration_sec = base_duration_sec * base_ability_cooldown_modifier;
                modified_duration_sec = std::max(0.05f, modified_duration_sec);
                abilityCooldowns[abilityId] = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(static_cast<long long>(modified_duration_sec * 1000.0f));
                RF_GAMELOGIC_TRACE("Player {} cooldown for ability {} set to {:.2f}s (modified from {:.2f}s base).", playerId, abilityId, modified_duration_sec, base_duration_sec);
            }
        }

        // --- Action Specific Logic Helpers ---
        void ActivePlayer::UpdateActiveRiftStepDefinition(const RiftStepDefinition& new_definition) {
            current_rift_step_definition = new_definition;
            // MarkDirty(); // Only if definition change needs immediate client update (e.g. UI hint change)
            RF_GAMELOGIC_INFO("Player {} active RiftStep updated to: {}", playerId, current_rift_step_definition.name_tag);
        }

        bool ActivePlayer::CanPerformRiftStep() const {
            if (movementState == PlayerMovementState::Stunned ||
                movementState == PlayerMovementState::Rooted ||
                movementState == PlayerMovementState::Dead ||
                movementState == PlayerMovementState::Ability_In_Use) {
                RF_PLAYERMGR_TRACE("Player {} cannot RiftStep due to movement state: {}", playerId, static_cast<int>(movementState));
                return false;
            }
            if (IsAbilityOnCooldown(RIFTSTEP_ABILITY_ID)) {
                RF_PLAYERMGR_TRACE("Player {} cannot RiftStep: ability {} on cooldown.", playerId, RIFTSTEP_ABILITY_ID);
                return false;
            }
            return true;
        }

        RiftStepOutcome ActivePlayer::PrepareRiftStepOutcome(RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent directional_intent, ERiftStepType type) {
            RiftStepOutcome outcome; // Default constructor initializes success to false, etc.
            outcome.type_executed = current_rift_step_definition.type;
            outcome.actual_start_position = this->position;

            outcome.travel_duration_sec = 0.05f; // Default cosmetic duration

            Networking::Shared::Vec3 target_direction_vector;
            RiftForged::Networking::Shared::Quaternion currentOrientationQuat = this->orientation;
            RiftForged::Networking::Shared::Vec3 world_forward = Utilities::Math::GetWorldForwardVector(currentOrientationQuat);
            RiftForged::Networking::Shared::Vec3 world_right = Utilities::Math::GetWorldRightVector(currentOrientationQuat);

            switch (directional_intent) {
            case Networking::UDP::C2S::RiftStepDirectionalIntent::RiftStepDirectionalIntent_Intentional_Forward:  target_direction_vector = world_forward; break;
            case Networking::UDP::C2S::RiftStepDirectionalIntent::RiftStepDirectionalIntent_Intentional_Backward:
            case Networking::UDP::C2S::RiftStepDirectionalIntent::RiftStepDirectionalIntent_Default_Backward:     target_direction_vector = Utilities::Math::ScaleVector(world_forward, -1.0f); break;
            case Networking::UDP::C2S::RiftStepDirectionalIntent::RiftStepDirectionalIntent_Intentional_Left:      target_direction_vector = Utilities::Math::ScaleVector(world_right, -1.0f); break;
            case Networking::UDP::C2S::RiftStepDirectionalIntent::RiftStepDirectionalIntent_Intentional_Right:     target_direction_vector = world_right; break;
            default:
                // Assuming EnumNameRiftStepDirectionalIntent is available. If not, log as int.
                RF_GAMELOGIC_WARN("Player {} used RiftStep with unknown directional_intent: {}. Defaulting to backward.", playerId, static_cast<int>(directional_intent));
                target_direction_vector = Utilities::Math::ScaleVector(world_forward, -1.0f);
                break;
            }
            target_direction_vector = Utilities::Math::NormalizeVector(target_direction_vector);

            float travel_distance = current_rift_step_definition.max_travel_distance;
            RiftForged::Networking::Shared::Vec3 scaled_direction = Utilities::Math::ScaleVector(target_direction_vector, travel_distance);
            outcome.intended_target_position = Utilities::Math::AddVectors(this->position, scaled_direction);
            outcome.calculated_target_position = outcome.intended_target_position; // Physics will adjust this

            outcome.start_vfx_id = current_rift_step_definition.default_start_vfx_id;
            outcome.travel_vfx_id = current_rift_step_definition.default_travel_vfx_id;
            outcome.end_vfx_id = current_rift_step_definition.default_end_vfx_id;

            // Populate entry/exit effects based on the *type_executed*
            // This uses the convenience constructors from your RiftStepLogic.h for GameplayEffectInstance
            switch (outcome.type_executed) {
            case ERiftStepType::Basic:
                RF_GAMEPLAY_DEBUG("Player {}: Basic RiftStep prepared.", playerId);
                break;
            case ERiftStepType::SolarExplosionExit: {
                const auto& params = current_rift_step_definition.solar_explosion_props;
                outcome.exit_effects_data.emplace_back(
                    outcome.intended_target_position, // center
                    params.explosion_radius,          // rad
                    params.damage_on_exit             // dmg_instance
					//"vfx_solar_explosion_exit" // vfx_tag for the exit effect
                );
                // visual_effect_tag could be set on the instance if needed, or assumed by client based on type
                outcome.exit_effects_data.back().visual_effect_tag = "vfx_solar_explosion_exit"; // Example
                RF_GAMEPLAY_DEBUG("Player {}: SolarExplosionExit RiftStep prepared.", playerId);
                break;
            }
            case ERiftStepType::SolarFlareBlindEntrance: {
                const auto& params = current_rift_step_definition.solar_blind_props;
                outcome.entry_effects_data.emplace_back(
                    this->position,                   // center (entrance effect at start position)
                    params.blind_radius,              // rad
                    params.blind_duration_ms,         // effect_duration_ms
                    params.blind_effect,              // effect_to_apply
                    "vfx_solar_flare_blind_entrance"  // vfx_tag
                );
                RF_GAMEPLAY_DEBUG("Player {}: SolarFlareBlindEntrance RiftStep prepared.", playerId);
                break;
            }
            case ERiftStepType::GlacialFrozenAttackerEntrance: { // Assuming "FrozenAttacker" implies a stun
                const auto& params = current_rift_step_definition.glacial_freeze_props;
                outcome.entry_effects_data.emplace_back(
                    this->position,                     // center
                    params.freeze_radius,               // rad
                    params.freeze_stun_on_entrance      // stun_instance
                );
                outcome.entry_effects_data.back().visual_effect_tag = "vfx_glacial_freeze_entrance";
                RF_GAMEPLAY_DEBUG("Player {}: GlacialFrozenAttackerEntrance RiftStep prepared.", playerId);
                break;
            }
            case ERiftStepType::GlacialChilledGroundExit: {
                const auto& params = current_rift_step_definition.glacial_chill_props;
                outcome.exit_effects_data.emplace_back(
                    outcome.intended_target_position,       // center
                    params.chilled_ground_radius,           // rad
                    params.chilled_ground_duration_ms,      // area_duration_ms
                    params.chilled_ground_vfx_tag,          // persistent_vfx_tag
                    // Default DamageInstance(),             // periodic_damage_instance (no direct damage from this effect type)
                    RiftForged::Networking::Shared::DamageInstance(0, RiftForged::Networking::Shared::DamageType::DamageType_None, false),
                    params.slow_effect                      // periodic_effect_to_apply
					//"vfx_glacial_chill_ground" // vfx_tag for the area effect
                );
                RF_GAMEPLAY_DEBUG("Player {}: GlacialChilledGroundExit RiftStep prepared.", playerId);
                break;
            }
            case ERiftStepType::RootingVinesEntrance: {
                const auto& params = current_rift_step_definition.rooting_vines_props;
                outcome.entry_effects_data.emplace_back(
                    this->position,                         // center
                    params.root_radius,                     // rad
                    params.root_duration_ms,                // effect_duration_ms
                    params.root_effect,                     // effect_to_apply
                    "vfx_rooting_vines_entrance"            // vfx_tag
                );
                RF_GAMEPLAY_DEBUG("Player {}: RootingVinesEntrance RiftStep prepared.", playerId);
                break;
            }
            case ERiftStepType::NatureShieldExit: {
                const auto& params = current_rift_step_definition.nature_pact_props;
                if (params.apply_shield_on_exit) {
                    // Shield effect applied to self (radius 0 for self-target, or small radius for 'aura' shield)
                    // The GameplayEffectInstance for ApplyBuff does not directly take shield magnitude.
                    // This needs to be handled by how StatusEffectCategory_Buff_DamageAbsorption_Shield is processed
                    // by your buff system (e.g., it looks up shield_percent_of_max_health from definition).
                    outcome.exit_effects_data.emplace_back(
                        outcome.intended_target_position, // center (effect on player at destination)
                        0.5f, // Small radius to ensure player is affected
                        params.shield_duration_ms,
                        Networking::Shared::StatusEffectCategory::StatusEffectCategory_Buff_DamageAbsorption_Shield,
                        "vfx_nature_shield_exit"
                    );
                }
                if (params.apply_minor_healing_aura) {
                    // Create a persistent area effect for the healing aura
                    RiftForged::Networking::Shared::DamageInstance no_damage(0, RiftForged::Networking::Shared::DamageType::DamageType_None, false);
                    outcome.exit_effects_data.emplace_back(
                        outcome.intended_target_position,
                        params.healing_aura_radius,
                        params.healing_aura_duration_ms,
                        "vfx_nature_healing_aura", // persistent_vfx_tag
                        no_damage, // no direct damage from this constructor, healing is a status effect
                        Networking::Shared::StatusEffectCategory::StatusEffectCategory_Buff_HealOverTime_Generic // periodic_effect_to_apply
                        // The actual healing amount per tick (params.healing_aura_amount_per_tick)
                        // needs to be associated with the StatusEffectCategory_Buff_HealOverTime_Generic
                        // when it's processed by the effect system.
                    );
                }
                RF_GAMEPLAY_DEBUG("Player {}: NatureShieldExit RiftStep prepared.", playerId);
                break;
            }
            case ERiftStepType::StealthEntrance: {
                const auto& params = current_rift_step_definition.stealth_props;
                outcome.entry_effects_data.emplace_back(
                    this->position,                         // center (effect on self at start)
                    0.1f,                                   // radius (small, for self)
                    params.stealth_duration_ms,             // effect_duration_ms
                    params.stealth_buff_category,           // effect_to_apply
                    "vfx_stealth_entrance"                  // vfx_tag
                );
                RF_GAMEPLAY_DEBUG("Player {}: StealthEntrance RiftStep prepared.", playerId);
                break;
            }
                                               // TODO: Implement cases for Rapid연속이동, GravityWarpEntrance, TimeDilationExit
                                               // using their respective 'props' from current_rift_step_definition and
                                               // the appropriate GameplayEffectInstance constructors.
            default:
                RF_GAMEPLAY_WARN("Player {}: PrepareRiftStepOutcome - Unhandled ERiftStepType ({}) for specific effect generation.",
                    playerId, static_cast<int>(outcome.type_executed));
                break;
            }

            StartAbilityCooldown(RIFTSTEP_ABILITY_ID, current_rift_step_definition.base_cooldown_sec);
            // MarkDirty(); // Cooldown started, but core state for sync (pos, health) might not have changed YET.
                         // GameplayEngine will set pos/orientation after physics, then that sets dirty.

            outcome.success = true; // Successfully prepared the data for physics sweep
            RF_GAMELOGIC_DEBUG("Player {} prepared RiftStep. Type: {}. Target: ({:.1f},{:.1f},{:.1f}). Effects: Entry({}), Exit({})",
                playerId, static_cast<int>(outcome.type_executed),
                outcome.intended_target_position.x(), outcome.intended_target_position.y(), outcome.intended_target_position.z(),
                outcome.entry_effects_data.size(), outcome.exit_effects_data.size());
            return outcome;
        }

        // --- Status Effect Management ---
        void ActivePlayer::AddStatusEffects(const std::vector<RiftForged::Networking::Shared::StatusEffectCategory>& effects_to_add) {
            bool changed = false;
            std::lock_guard<std::mutex> lock(m_internalDataMutex);
            for (const auto& effect : effects_to_add) {
                if (effect == RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_None) continue;
                if (std::find(activeStatusEffects.begin(), activeStatusEffects.end(), effect) == activeStatusEffects.end()) {
                    activeStatusEffects.push_back(effect);
                    changed = true;
                    RF_GAMEPLAY_DEBUG("Player {}: Added status effect {}", playerId, static_cast<uint32_t>(effect));
                }
            }
            if (changed) MarkDirty();
        }

        void ActivePlayer::RemoveStatusEffects(const std::vector<RiftForged::Networking::Shared::StatusEffectCategory>& effects_to_remove) {
            bool changed = false;
            std::lock_guard<std::mutex> lock(m_internalDataMutex);
            for (const auto& effect_to_remove_item : effects_to_remove) { // Renamed inner loop var
                if (effect_to_remove_item == RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_None) continue;
                auto it = std::remove(activeStatusEffects.begin(), activeStatusEffects.end(), effect_to_remove_item);
                if (it != activeStatusEffects.end()) {
                    activeStatusEffects.erase(it, activeStatusEffects.end());
                    changed = true;
                    RF_GAMEPLAY_DEBUG("Player {}: Removed status effect {}", playerId, static_cast<uint32_t>(effect_to_remove_item));
                }
            }
            if (changed) MarkDirty();
        }

        bool ActivePlayer::HasStatusEffect(Networking::Shared::StatusEffectCategory effect) const {
            std::lock_guard<std::mutex> lock(m_internalDataMutex);
            return std::find(activeStatusEffects.begin(), activeStatusEffects.end(), effect) != activeStatusEffects.end();
        }

        // --- Equipment ---
        void ActivePlayer::SetEquippedWeapon(uint32_t weapon_def_id, EquippedWeaponCategory category) {
            bool changed = false;
            if (equipped_weapon_definition_id != weapon_def_id) {
                equipped_weapon_definition_id = weapon_def_id;
                changed = true;
            }
            if (current_weapon_category != category) {
                current_weapon_category = category;
                changed = true;
            }
            if (changed) {
                MarkDirty();
                RF_GAMELOGIC_INFO("Player {} equipped weapon ID: {}, Category: {}", playerId, weapon_def_id, static_cast<int>(category));
                // TODO: Update player stats based on new weapon
            }
        }

        // --- Helpers ---
        RiftForged::Networking::Shared::Vec3 ActivePlayer::GetMuzzlePosition() const {
            Networking::Shared::Vec3 local_muzzle_offset(0.0f, 1.0f, 0.5f);
            RiftForged::Networking::Shared::Quaternion currentOrientationQuat = this->orientation;
            Networking::Shared::Vec3 world_offset = Utilities::Math::RotateVectorByQuaternion(local_muzzle_offset, currentOrientationQuat);
            return Utilities::Math::AddVectors(this->position, world_offset);
        }

    } // namespace GameLogic
    // Fix the issue by ensuring the arguments passed to `emplace_back` match the constructors of `GameplayEffectInstance`.
    // The error occurs because the arguments provided to `emplace_back` do not match any of the constructors defined in `GameplayEffectInstance`.
    // Update the code to use the correct constructor or provide the required arguments.


} // namespace RiftForged