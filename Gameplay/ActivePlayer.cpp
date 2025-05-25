// File: GameplayEngine/ActivePlayer.cpp
// Copyright (c) 2023-2025 RiftForged Game Development Team

#include "ActivePlayer.h"
// RiftStepLogic.h is included via ActivePlayer.h
#include "../Utils/MathUtil.h"
#include "../Utils/Logger.h"

namespace RiftForged {
    namespace GameLogic {

        ActivePlayer::ActivePlayer(uint64_t pId,
            const RiftForged::Networking::NetworkEndpoint& ep,
            const RiftForged::Networking::Shared::Vec3& startPos,
            const RiftForged::Networking::Shared::Quaternion& startOrientation,
			float cap_radius, float cap_half_height)
            : playerId(pId),
            networkEndpoint(ep),
            position(startPos),
            orientation(startOrientation),
			capsule_radius(cap_radius), capsule_half_height(cap_half_height),
            currentWill(100), maxWill(100),
            currentHealth(250), maxHealth(250),
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
            current_rift_step_definition(RiftStepDefinition::CreateBasicRiftStep()),
            current_weapon_category(EquippedWeaponCategory::Unarmed),
            equipped_weapon_definition_id(0),
            animationStateId(static_cast<uint32_t>(RiftForged::Networking::Shared::AnimationState::AnimationState_Idle)),
            movementState(PlayerMovementState::Idle),
            isDirty(true) {
            if (networkEndpoint.ipAddress.empty() || networkEndpoint.port == 0) {
                RF_PLAYERMGR_WARN("ActivePlayer CONSTRUCTOR - ID: {} created with INVALID endpoint details: {}", playerId, networkEndpoint.ToString());
            }
            RF_PLAYERMGR_DEBUG("ActivePlayer {} constructed. Initial RiftStep: {}", playerId, current_rift_step_definition.name_tag);
        }

        void ActivePlayer::SetPosition(const RiftForged::Networking::Shared::Vec3& newPosition) {
            const float POSITION_EPSILON = 0.0001f;
            if (std::abs(position.x() - newPosition.x()) > POSITION_EPSILON ||
                std::abs(position.y() - newPosition.y()) > POSITION_EPSILON ||
                std::abs(position.z() - newPosition.z()) > POSITION_EPSILON) {
                position = newPosition;
                isDirty.store(true);
            }
        }

        void ActivePlayer::SetOrientation(const RiftForged::Networking::Shared::Quaternion& newOrientation) {
            const float ORIENTATION_EPSILON = 0.00001f;
            RiftForged::Networking::Shared::Quaternion normalizedNewOrientation = RiftForged::Utilities::Math::NormalizeQuaternion(newOrientation);
            if (std::abs(orientation.x() - normalizedNewOrientation.x()) > ORIENTATION_EPSILON ||
                std::abs(orientation.y() - normalizedNewOrientation.y()) > ORIENTATION_EPSILON ||
                std::abs(orientation.z() - normalizedNewOrientation.z()) > ORIENTATION_EPSILON ||
                std::abs(orientation.w() - normalizedNewOrientation.w()) > ORIENTATION_EPSILON) {
                orientation = normalizedNewOrientation;
                isDirty.store(true);
            }
        }

        void ActivePlayer::SetWill(int value) {
            int newWill = std::max(0, std::min(value, maxWill));
            if (currentWill != newWill) {
                currentWill = newWill;
                isDirty.store(true);
            }
        }

		void ActivePlayer::DeductWill(int amount) {
			if (amount <= 0) return;
			SetWill(currentWill - amount);
			isDirty.store(true);
		}

        void ActivePlayer::AddWill(int amount) {
            if (amount <= 0) return;
            SetWill(currentWill + amount);
            isDirty.store(true);
        }


        void ActivePlayer::SetHealth(int value) {
            int newHealth = std::max(0, std::min(value, maxHealth));
            if (currentHealth != newHealth) {
                currentHealth = newHealth;
                isDirty.store(true);
            }
        }

        void ActivePlayer::HealDamage(int amount) {
            if (amount <= 0) return;
            SetHealth(currentHealth + amount);
            isDirty.store(true);
        }

        void ActivePlayer::TakeDamage(int raw_damage_amount, RiftForged::Networking::Shared::DamageType damage_type) {
            if (raw_damage_amount <= 0 || movementState == PlayerMovementState::Dead) return;

            float percentage_reduction = 0.0f;
            int32_t flat_reduction = 0;

            // Select appropriate resistance based on damage type
            switch (damage_type) {
            case RiftForged::Networking::Shared::DamageType::DamageType_Physical:   percentage_reduction = percent_physical_damage_reduction; flat_reduction = flat_physical_damage_reduction; break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Radiant:    percentage_reduction = percent_radiant_damage_reduction;  flat_reduction = flat_radiant_damage_reduction;  break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Frost:      percentage_reduction = percent_frost_damage_reduction;    flat_reduction = flat_frost_damage_reduction;    break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Shock:
            case RiftForged::Networking::Shared::DamageType::DamageType_Lightning:  percentage_reduction = percent_shock_damage_reduction;    flat_reduction = flat_shock_damage_reduction;    break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Necrotic:   percentage_reduction = percent_necrotic_damage_reduction; flat_reduction = flat_necrotic_damage_reduction; break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Void:       percentage_reduction = percent_void_damage_reduction;     flat_reduction = flat_void_damage_reduction;     break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Cosmic:     percentage_reduction = percent_cosmic_damage_reduction;   flat_reduction = flat_cosmic_damage_reduction;   break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Poison:     percentage_reduction = percent_poison_damage_reduction;   flat_reduction = flat_poison_damage_reduction;   break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Nature:     percentage_reduction = percent_nature_damage_reduction;   flat_reduction = flat_nature_damage_reduction;   break;
            case RiftForged::Networking::Shared::DamageType::DamageType_Aetherial:  percentage_reduction = percent_aetherial_damage_reduction;flat_reduction = flat_aetherial_damage_reduction;break;
            case RiftForged::Networking::Shared::DamageType::DamageType_None:
            default: break;
            }

            int damage_after_flat_reduction = std::max(0, raw_damage_amount - flat_reduction);
            int final_damage = static_cast<int>(static_cast<float>(damage_after_flat_reduction) * (1.0f - percentage_reduction));
            final_damage = std::max(0, final_damage);

            SetHealth(currentHealth - final_damage);
            RF_GAMEPLAY_INFO("Player {} took {} (final) {} damage. Health: {}/{}", playerId, final_damage, RiftForged::Networking::Shared::EnumNameDamageType(damage_type), currentHealth, maxHealth);

            if (currentHealth == 0) {
                SetMovementState(PlayerMovementState::Dead);
                RF_GAMEPLAY_INFO("Player {} has died.", playerId);
                // Additional death logic (e.g., notify GameplayEngine for respawn, drops)
            }
            isDirty.store(true);
        }

        void ActivePlayer::SetAnimationState(RiftForged::Networking::Shared::AnimationState newState) {
            uint32_t newStateId = static_cast<uint32_t>(newState);
            if (animationStateId != newStateId) {
                animationStateId = newStateId;
                isDirty.store(true);
            }
        }

        void ActivePlayer::SetMovementState(PlayerMovementState newState) {
            if (movementState != newState) {
                movementState = newState;
                isDirty.store(true);
                RF_PLAYERMGR_TRACE("Player {} movement state changed to {}", playerId, static_cast<int>(newState));
                switch (newState) {
                case PlayerMovementState::Idle: SetAnimationState(RiftForged::Networking::Shared::AnimationState::AnimationState_Idle); break;
                case PlayerMovementState::Walking: SetAnimationState(RiftForged::Networking::Shared::AnimationState::AnimationState_Walking); break;
                case PlayerMovementState::Sprinting: SetAnimationState(RiftForged::Networking::Shared::AnimationState::AnimationState_Running); break;
                case PlayerMovementState::Dead: SetAnimationState(RiftForged::Networking::Shared::AnimationState::AnimationState_Dead); break;
                default: break;
                }
            }
        }

        bool ActivePlayer::IsAbilityOnCooldown(uint32_t abilityId) const {
            auto it = abilityCooldowns.find(abilityId);
            if (it != abilityCooldowns.end()) {
                return std::chrono::steady_clock::now() < it->second;
            }
            return false;
        }

        void ActivePlayer::SetAbilityCooldown(uint32_t abilityId, float duration_sec) {
            if (duration_sec <= 0.0f) {
                abilityCooldowns.erase(abilityId);
                RF_PLAYERMGR_TRACE("Player {} cooldown for ability {} cleared.", playerId, abilityId);
            }
            else {
                float modified_duration_sec = duration_sec * base_ability_cooldown_modifier; // Apply player's global CDR
                modified_duration_sec = std::max(0.05f, modified_duration_sec); // Ensure a minimum practical cooldown
                abilityCooldowns[abilityId] = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(static_cast<long long>(modified_duration_sec * 1000.0f));
                RF_PLAYERMGR_TRACE("Player {} cooldown for ability {} set to {:.2f}s (modified).", playerId, abilityId, modified_duration_sec);
            }
        }

        void ActivePlayer::UpdateActiveRiftStepDefinition(const RiftStepDefinition& new_definition) {
            current_rift_step_definition = new_definition;
            isDirty.store(true);
            RF_PLAYERMGR_INFO("Player {} active RiftStep updated to: {}", playerId, current_rift_step_definition.name_tag);
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

        RiftStepOutcome ActivePlayer::PrepareRiftStepOutcome(RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent directional_intent) {
            RiftStepOutcome outcome; // Default constructor initializes success to false, etc.
            outcome.type_executed = current_rift_step_definition.type;
            outcome.actual_start_position = this->position;

            // CanPerformRiftStep already checks states and cooldowns.
            // This method is called by GameplayEngine AFTER CanPerformRiftStep is validated,
            // or GameplayEngine can call CanPerformRiftStep first.
            // For clarity, we can re-check or assume GameplayEngine did.
            // For this example, let's assume GameplayEngine calls CanPerformRiftStep before this.

            RiftForged::Networking::Shared::Vec3 target_direction_vector;
            RiftForged::Networking::Shared::Vec3 world_forward = RiftForged::Utilities::Math::GetWorldForwardVector(this->orientation);
            RiftForged::Networking::Shared::Vec3 world_right = RiftForged::Utilities::Math::GetWorldRightVector(this->orientation);

            switch (directional_intent) {
            case RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Forward:   target_direction_vector = world_forward; break;
            case RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Backward:
            case RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Default_Backward:      target_direction_vector = RiftForged::Utilities::Math::ScaleVector(world_forward, -1.0f); break;
            case RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Left:      target_direction_vector = RiftForged::Utilities::Math::ScaleVector(world_right, -1.0f); break;
            case RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Right:     target_direction_vector = world_right; break;
            default:
                RF_PLAYERMGR_WARN("Player {} used RiftStep with unknown directional_intent: {}. Defaulting to backward.", playerId, static_cast<int>(directional_intent));
                target_direction_vector = RiftForged::Utilities::Math::ScaleVector(world_forward, -1.0f);
                break;
            }
            // Ensure normalization, though GetWorld... vectors should already be normalized.
            target_direction_vector = RiftForged::Utilities::Math::NormalizeVector(target_direction_vector);

            float travel_distance = current_rift_step_definition.max_travel_distance;
            outcome.intended_target_position = RiftForged::Utilities::Math::AddVectors(
                this->position,
                RiftForged::Utilities::Math::ScaleVector(target_direction_vector, travel_distance)
            );

            // Populate Entry/Exit Effects based on current_rift_step_definition
            outcome.start_vfx_id = current_rift_step_definition.default_start_vfx_id;
            outcome.travel_vfx_id = current_rift_step_definition.default_travel_vfx_id;
            outcome.end_vfx_id = current_rift_step_definition.default_end_vfx_id;
            // travel_duration_sec is already defaulted in RiftStepOutcome struct in RiftStepLogic.h

            switch (current_rift_step_definition.type) {
            case ERiftStepType::NatureShieldExit: {
                const auto& params = current_rift_step_definition.nature_pact_props;
                if (params.apply_shield_on_exit) {
                    GameplayEffectInstance shield_effect;
                    shield_effect.effect_payload_type = RiftForged::Networking::UDP::S2C::RiftStepEffectPayload::RiftStepEffectPayload_ApplyBuff;
                    shield_effect.buff_debuff_to_apply = RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_Buff_DamageAbsorption_Shield;
                    shield_effect.duration_ms = params.shield_duration_ms;
                    // Actual shield value (e.g. % of max HP) would be determined when effect is applied by GameplayEngine
                    shield_effect.center_position = outcome.intended_target_position; // At destination
                    shield_effect.radius = params.healing_aura_radius; // Assuming shield can be AoE
                    outcome.exit_effects_data.push_back(shield_effect);
                }
                if (params.apply_minor_healing_aura) {
                    GameplayEffectInstance heal_aura;
                    heal_aura.effect_payload_type = RiftForged::Networking::UDP::S2C::RiftStepEffectPayload::RiftStepEffectPayload_PersistentArea;
                    heal_aura.center_position = outcome.intended_target_position;
                    heal_aura.radius = params.healing_aura_radius;
                    heal_aura.duration_ms = params.healing_aura_duration_ms;
                    heal_aura.buff_debuff_to_apply = RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_Buff_HealOverTime_Generic; // Example
                    // heal_aura.damage.amount = params.healing_aura_amount_per_tick; // If PersistentAreaData also carries periodic damage/heal values
                    heal_aura.visual_effect_tag = "vfx_nature_healing_aura"; // Example
                    outcome.exit_effects_data.push_back(heal_aura);
                }
                break;
            }
            case ERiftStepType::SolarExplosionExit: {
                const auto& params = current_rift_step_definition.solar_explosion_props;
                GameplayEffectInstance explosion_effect(
                    outcome.intended_target_position,
                    params.explosion_radius,
                    params.damage_on_exit
                );
                outcome.exit_effects_data.push_back(explosion_effect);
                break;
            }
            case ERiftStepType::GlacialChilledGroundExit: {
                const auto& params = current_rift_step_definition.glacial_chill_props;
                GameplayEffectInstance chill_ground_effect(
                    outcome.intended_target_position,
                    params.chilled_ground_radius,
                    params.chilled_ground_duration_ms,
                    params.chilled_ground_vfx_tag,
                    {}, // No direct damage from this constructor, effect is the slow
                    params.slow_effect
                );
                outcome.exit_effects_data.push_back(chill_ground_effect);
                break;
            }
            case ERiftStepType::StealthEntrance: {
                const auto& params = current_rift_step_definition.stealth_props;
                GameplayEffectInstance stealth_buff(
                    this->position, // Applied at start
                    0.0f, // Self-buff, radius 0
                    params.stealth_duration_ms,
                    params.stealth_buff_category
                    // Optionally add a vfx_tag if stealth has an immediate visual cue
                );
                outcome.entry_effects_data.push_back(stealth_buff);
                break;
            }
                                               // TODO: Implement effect generation for ALL other ERiftStepTypes
                                               // (SolarFlareBlindEntrance, GlacialFrozenAttackerEntrance, RootingVinesEntrance, Rapid연속이동 etc.)
            case ERiftStepType::Basic: // Basic RiftStep has no special GameplayEffectInstances by default
            default:
                break;
            }

            outcome.success = true; // Mark as successful preparation. Physics engine will confirm final position.
            RF_PLAYERMGR_DEBUG("Player {} prepared RiftStep outcome. Type: {}. Target: ({},{},{})",
                playerId, static_cast<int>(outcome.type_executed),
                outcome.intended_target_position.x(), outcome.intended_target_position.y(), outcome.intended_target_position.z());
            return outcome;
        }

        void ActivePlayer::AddStatusEffects(const std::vector<RiftForged::Networking::Shared::StatusEffectCategory>& effects_to_add) {
            bool changed = false;
            for (const auto& effect : effects_to_add) {
                if (effect == RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_None) continue;
                if (std::find(activeStatusEffects.begin(), activeStatusEffects.end(), effect) == activeStatusEffects.end()) {
                    activeStatusEffects.push_back(effect);
                    changed = true;
                }
            }
            if (changed) isDirty.store(true);
        }

        void ActivePlayer::RemoveStatusEffects(const std::vector<RiftForged::Networking::Shared::StatusEffectCategory>& effects_to_remove) {
            bool changed = false;
            for (const auto& effect : effects_to_remove) {
                if (effect == RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_None) continue;
                auto initial_size = activeStatusEffects.size();
                activeStatusEffects.erase(std::remove(activeStatusEffects.begin(), activeStatusEffects.end(), effect), activeStatusEffects.end());
                if (activeStatusEffects.size() != initial_size) {
                    changed = true;
                }
            }
            if (changed) isDirty.store(true);
        }

        void ActivePlayer::SetEquippedWeapon(uint32_t weapon_def_id, EquippedWeaponCategory category) {
            if (equipped_weapon_definition_id != weapon_def_id || current_weapon_category != category) {
                equipped_weapon_definition_id = weapon_def_id;
                current_weapon_category = category;
                isDirty.store(true);
                RF_PLAYERMGR_INFO("Player {} equipped weapon ID: {}, Category: {}", playerId, weapon_def_id, static_cast<int>(category));
            }
        }

        RiftForged::Networking::Shared::Vec3 ActivePlayer::GetMuzzlePosition() const {
            // Example: Muzzle is 0.5 units forward from player center, and 0.2 units up.
            // This offset should ideally be part of weapon definition or character model data.
            RiftForged::Networking::Shared::Vec3 local_offset(0.0f, 0.5f, 0.2f); // Assuming Y is forward, Z is up locally
            RiftForged::Networking::Shared::Vec3 world_offset = RiftForged::Utilities::Math::RotateVectorByQuaternion(local_offset, this->orientation);
            return RiftForged::Utilities::Math::AddVectors(this->position, world_offset);
        }

    } // namespace GameLogic
} // namespace RiftForged