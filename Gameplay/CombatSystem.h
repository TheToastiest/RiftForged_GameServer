#pragma once

#include <vector>
#include <cstdint>
#include <string>

// Assuming these are accessible from your project's include paths:
#include "PlayerManager.h" // Provides PlayerManager, ActivePlayer
#include "../PhysicsEngine/PhysicsEngine.h" // Provides RiftForged::Physics::PhysicsEngine
#include "../Utils/MathUtil.h"      // Provides RiftForged::Utilities::Math::Vec3 and quaternion utils
#include "CombatData.h"    // Provides RiftForged::GameLogic::AttackOutcome, DamageApplicationDetails, DamageInstance
                           // (This was formerly CombatLogic.h)

// Forward declaration for the unpacked FlatBuffers message type
namespace RiftForged { namespace Networking { namespace UDP { namespace C2S { struct C2S_BasicAttackIntentMsgT; } } } }
// Forward declaration for PxRigidActor (if not transitively included)
namespace physx { class PxRigidActor; }


namespace RiftForged {
    namespace GameLogic {
        namespace Combat {

            // Properties defining a specific melee attack's physics query
            // For this first step, these will be hardcoded within the function,
            // but eventually, they'd come from weapon/character stats.
            struct MeleeAttackProperties {
                float sweepDistance;
                float capsuleRadius;
                float capsuleHalfHeight;
                float sweepStartOffset;
                Networking::Shared::DamageInstance damage; // From riftforged_common_types_generated.h

                MeleeAttackProperties(float dist, float radius, float halfHeight, float offset,
                    const Networking::Shared::DamageInstance& dmg)
                    : sweepDistance(dist), capsuleRadius(radius), capsuleHalfHeight(halfHeight),
                    sweepStartOffset(offset), damage(dmg) {
                }
            };

            /**
             * @brief Processes a basic melee attack intent from a client.
             * @param casterPlayerId The ID of the player performing the attack.
             * @param attackIntent The unpacked FlatBuffers message from the client.
             * @param playerManager Reference to the PlayerManager.
             * @param physicsEngine Reference to the PhysicsEngine.
             * @return AttackOutcome detailing the results of the attack.
             *
             * @assumptions (CRITICAL for this function to work):
             * 1. ActivePlayer (from playerManager.FindPlayerById) has methods/members:
             * - GetCurrentPosition() returning Utilities::Math::Vec3 (or `position` member)
             * - GetCurrentOrientation() returning Utilities::Math::Quaternion (or `orientation` member)
             * - GetPhysicsCharacterActor() returning physx::PxRigidActor* (the physics representation of the player,
             * likely their PxController's actor, needed to ignore self in sweep). This is a NEW assumed method.
             * - (Optional for now, but for real damage: methods to get weapon stats or base damage).
             * 2. physicsEngine.GetScene()->sweep(...) and GetPhysicsMutex() are available and work as expected.
             * 3. userData on PxRigidActor stores the uint64_t entity ID.
             */
            AttackOutcome ProcessBasicMeleeAttack(
                uint64_t casterPlayerId,
                const Networking::UDP::C2S::C2S_BasicAttackIntentMsgT& attackIntent,
                PlayerManager& playerManager,
                Physics::PhysicsEngine& physicsEngine
            );

            // We will add functions for projectiles (arrow, bullet) and AoE abilities here in subsequent steps.
            struct AbilityDefinition {
                uint32_t id = 0;
                std::string name = "Unnamed Ability";
                // For this step, ProcessAbilityLaunchPhysicsProjectile will use hardcoded "arrow"
                // properties instead of reading them from this struct.
                // Example members you might add later:
                // float cooldown = 1.0f;
                // int resourceCost = 10;
                // float projectileSpeed = 40.0f;
                // bool projectileUsesGravity = true;
                // Physics::PhysicsEngine::ProjectilePhysicsProperties projectilePhysicsProps;
                // Networking::Shared::DamageInstance projectileDamageOnHit;
                // std::string projectileVfxTag;
                // float projectileMaxRange;
                // EAbilityActionType actionType = EAbilityActionType::PROJECTILE; // If you add such an enum

                AbilityDefinition(uint32_t ability_id = 0, const std::string& ability_name = "Default Ability")
                    : id(ability_id), name(ability_name) {
                }
            };

            /**
             * @brief Processes a 'Use Ability' intent that should result in launching a physics-based projectile.
             * @param casterPlayerId The ID of the player casting the ability.
             * @param useAbilityIntent The unpacked FlatBuffers message from the client.
             * @param abilityDef Static definition of the ability being cast (e.g., looked up by useAbilityIntent.ability_id).
             * For this initial implementation, properties will be hardcoded inside the function.
             * @param playerManager Reference to the PlayerManager.
             * @param physicsEngine Reference to the PhysicsEngine.
             * @return AttackOutcome detailing the projectile spawn event or failure.
             */
            AttackOutcome ProcessAbilityLaunchPhysicsProjectile(
                uint64_t casterPlayerId,
                const Networking::UDP::C2S::C2S_UseAbilityMsgT& useAbilityIntent,
                const AbilityDefinition& abilityDef, // Placeholder for ability's static data
                PlayerManager& playerManager,
                Physics::PhysicsEngine& physicsEngine
            );

        } // namespace Combat
    } // namespace GameLogic
} // namespace RiftForged