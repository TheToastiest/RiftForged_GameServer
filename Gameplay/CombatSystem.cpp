#include "CombatSystem.h"
#include "CombatData.h"
#include "PlayerManager.h" // For ActivePlayer definition
#include "../PhysicsEngine/PhysicsEngine.h"
#include "../Utils/MathUtil.h"      // For vector math
#include "../FlatBuffers/V0.0.3/riftforged_common_types_generated.h"
#include "../FlatBuffers/V0.0.3/riftforged_c2s_udp_messages_generated.h"
#include "../Utils/Logger.h" // For logging macros


// For PhysX types
#include "physx/PxPhysicsAPI.h" 

namespace RiftForged {
    namespace GameLogic {
        namespace Combat {

            // Helper Query Filter Callback for Melee Sweeps
            struct MeleeSweepQueryFilterCallback : public physx::PxQueryFilterCallback {
                physx::PxRigidActor* m_casterPhysicsActor; // The specific PxRigidActor of the caster
                uint64_t m_casterEntityId;           // The entity ID of the caster

                MeleeSweepQueryFilterCallback(physx::PxRigidActor* casterActor, uint64_t casterId)
                    : m_casterPhysicsActor(casterActor), m_casterEntityId(casterId) {
                }

                virtual physx::PxQueryHitType::Enum preFilter(
                    const physx::PxFilterData& /*filterData*/,
                    const physx::PxShape* /*shape*/,
                    const physx::PxRigidActor* hitActor,
                    physx::PxHitFlags& /*queryFlags*/) override {

                    if (!hitActor) {
                        return physx::PxQueryHitType::eNONE;
                    }

                    // Ignore hitting the caster's own physics actor directly
                    if (hitActor == m_casterPhysicsActor && m_casterPhysicsActor != nullptr) {
                        return physx::PxQueryHitType::eNONE;
                    }

                    // Also, ensure we don't process a hit against the caster via entity ID if userData is available
                    if (hitActor->userData) {
                        uint64_t hitEntityId = reinterpret_cast<uint64_t>(hitActor->userData);
                        if (hitEntityId == m_casterEntityId) {
                            return physx::PxQueryHitType::eNONE;
                        }
                    }

                    // TODO: More sophisticated filtering based on game rules:
                    // - Is the hitActor a damageable entity (e.g., another player, NPC)?
                    // - Faction checks (can caster attack this target's faction)?
                    // - Is the target alive/in a state where it can be hit?

                    // For now, any other actor is a potential target.
                    return physx::PxQueryHitType::eBLOCK;
                }

                virtual physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&, const physx::PxQueryHit&, const physx::PxShape*, const physx::PxRigidActor*) override {
                    return physx::PxQueryHitType::eBLOCK; // Process all blocking hits that passed preFilter
                }
            };


            AttackOutcome ProcessBasicMeleeAttack(
                uint64_t casterPlayerId,
                const Networking::UDP::C2S::C2S_BasicAttackIntentMsgT& attackIntent,
                PlayerManager& playerManager,
                Physics::PhysicsEngine& physicsEngine
            ) {
                AttackOutcome outcome;
                outcome.is_basic_attack = true; // Mark this as a basic attack outcome
                // Default to success, set to false on specific failures
                outcome.success = false;
                // outcome.simulated_combat_event_type will be set based on hits/misses

                ActivePlayer* caster = playerManager.FindPlayerById(casterPlayerId);
                if (!caster) {
                    RF_COMBAT_WARN("ProcessBasicMeleeAttack: Caster with ID %llu not found.", casterPlayerId);
                    outcome.failure_reason_code = "CASTER_NOT_FOUND";
                    return outcome;
                }

                // --- ASSUMPTION: ActivePlayer needs these methods/members ---
                // Utilities::Math::Vec3 casterPos = caster->GetCurrentPosition(); 
                // Utilities::Math::Quaternion casterOrientation = caster->GetCurrentOrientation();
                // physx::PxRigidActor* casterPhysicsActor = caster->GetPhysicsCharacterActor(); 
                // --- Implement or adjust the lines below to use your actual ActivePlayer accessors ---

                // Using direct member access as shown in your ActivePlayer.cpp for position/orientation
                Utilities::Math::Vec3 casterPos = caster->position;
                Utilities::Math::Quaternion casterOrientation = caster->orientation;

                // CRITICAL ASSUMPTION: ActivePlayer needs a way to provide its PxRigidActor.
                // For now, let's try to get it via PhysicsEngine if the player is registered there.
                // This might be the PxController actor.
                physx::PxRigidActor* casterPhysicsActor = physicsEngine.GetRigidActor(casterPlayerId); // Tries m_entityActors
                if (!casterPhysicsActor) {
                    physx::PxController* controller = physicsEngine.GetPlayerController(casterPlayerId); // Tries m_playerControllers
                    if (controller) {
                        casterPhysicsActor = controller->getActor();
                    }
                }
                if (!casterPhysicsActor) {
                    RF_COMBAT_WARN("ProcessBasicMeleeAttack: Could not retrieve PxRigidActor for caster ID %llu. Melee sweep might hit self or have incorrect filtering.", casterPlayerId);
                    // Proceeding without self-filtering via PxActor pointer might be risky.
                    // Consider if this should be a hard failure. For now, filter callback will rely on entity ID if actor ptr is null.
                }
                // -----

                Utilities::Math::Vec3 casterForward = Utilities::Math::GetWorldForwardVector(casterOrientation);
                // Utilities::Math::Vec3 casterUp = Utilities::Math::GetWorldUpVector(casterOrientation); // Needed if capsule is oriented along Up vector

                // 1. Define Melee Attack Properties (HARDCODED for this step)
                //    Later, these would come from caster's weapon, stats, or ability definition.
                Networking::Shared::DamageInstance baseDamage(15, Networking::Shared::DamageType::DamageType_Physical, false); // 15 Physical damage, not a crit
                MeleeAttackProperties props(
                    /*sweepDistance:*/ 2.0f,    // How far the sweep extends
                    /*capsuleRadius:*/ 0.6f,    // "Width" of the swing
                    /*capsuleHalfHeight:*/ caster->capsule_half_height, // Vertical extent based on player's capsule
                    /*sweepStartOffset:*/ 0.5f  // How far in front of caster's origin the sweep starts
                    , baseDamage
                );
                outcome.attack_animation_tag_for_caster = "BasicMelee_Sword_01"; // Placeholder

                // 2. Define the capsule geometry
                //    Capsule oriented along character's local Up-axis (Z for PhysX default character controller up)
                //    and swept forward.
                physx::PxCapsuleGeometry capsuleGeometry(props.capsuleRadius, props.capsuleHalfHeight);

                // 3. Calculate the starting pose of the capsule for the sweep
                Utilities::Math::Vec3 sweepStartPos = Utilities::Math::AddVectors(casterPos, Utilities::Math::ScaleVector(casterForward, props.sweepStartOffset));
                physx::PxTransform capsuleInitialPose(
                    RiftForged::Physics::ToPxVec3(sweepStartPos), // Using Physics::ToPxVec3 from PhysicsEngine header
                    RiftForged::Physics::ToPxQuat(casterOrientation)  // Using Physics::ToPxQuat from PhysicsEngine header
                );

                // 4. Setup filter data and callback
                physx::PxQueryFilterData filterData;
                filterData.flags = physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER;
                // Example: Set collision group for caster's attacks if you have a filtering system
                // filterData.data.word0 = YOUR_PLAYER_ATTACK_COLLISION_GROUP; 

                MeleeSweepQueryFilterCallback filterCallback(casterPhysicsActor, casterPlayerId);

                // 5. Perform the sweep
                const physx::PxU32 maxMeleeHits = 10; // Max targets a single melee swing can hit
                physx::PxSweepHit hitBufferArray[maxMeleeHits];
                physx::PxSweepBuffer multipleHitBuffer(hitBufferArray, maxMeleeHits);

                bool bSweepHitOccurred = false;
                if (physicsEngine.GetScene()) {
                    bSweepHitOccurred = physicsEngine.GetScene()->sweep(
                        capsuleGeometry,
                        capsuleInitialPose,
                        RiftForged::Physics::ToPxVec3(casterForward), // Sweep direction
                        props.sweepDistance,
                        multipleHitBuffer,
                        physx::PxHitFlag::eDEFAULT | physx::PxHitFlag::eMESH_BOTH_SIDES, // Default + allow hitting backfaces
                        filterData,
                        &filterCallback,
                        nullptr // PxQueryCache
                    );
                }
                else {
                    RF_COMBAT_ERROR("ProcessBasicMeleeAttack: Physics scene is null for caster ID %llu.", casterPlayerId);
                    outcome.failure_reason_code = "SCENE_NULL";
                    return outcome;
                }

                outcome.success = true; // Sweep was performed

                if (bSweepHitOccurred && multipleHitBuffer.getNbTouches() > 0) {
                    RF_COMBAT_TRACE("Melee sweep for caster %llu hit %u actor(s).", casterPlayerId, multipleHitBuffer.getNbTouches());
                    for (physx::PxU32 i = 0; i < multipleHitBuffer.getNbTouches(); ++i) {
                        const physx::PxSweepHit& touch = multipleHitBuffer.getTouch(i);
                        if (touch.actor && touch.actor->userData) {
                            uint64_t hitEntityId = reinterpret_cast<uint64_t>(touch.actor->userData);

                            // Redundant check if callback is perfect, but good for safety:
                            if (hitEntityId == casterPlayerId) continue;

                            RF_COMBAT_TRACE("Caster %llu melee hit Entity ID: %llu", casterPlayerId, hitEntityId);

                            DamageApplicationDetails DADetails;
                            DADetails.target_id = hitEntityId;
                            // For this first step, use damage directly from properties. Mitigation comes later.
                            DADetails.final_damage_dealt = props.damage.amount();
                            DADetails.damage_type = props.damage.type();
                            DADetails.was_crit = props.damage.is_crit();
                            // DADetails.was_kill = ...; // This requires checking target health *after* damage application
                            // DADetails.impact_point = RiftForged::Physics::FromPxVec3(touch.position); // If needed

                            outcome.damage_events.push_back(DADetails);
                        }
                    }
                    if (!outcome.damage_events.empty()) {
                        outcome.simulated_combat_event_type = Networking::UDP::S2C::CombatEventType::CombatEventType_DamageDealt; // Or more specific
                    }
                    else {
                        // Sweep happened, but no valid targets were processed (e.g., all filtered out or no userData)
                        outcome.simulated_combat_event_type = Networking::UDP::S2C::CombatEventType::CombatEventType_Miss; // Or "SwingAndMiss"
                    }
                }
                else {
                    RF_COMBAT_TRACE("Melee sweep for caster %llu reported no hits.", casterPlayerId);
                    outcome.simulated_combat_event_type = Networking::UDP::S2C::CombatEventType::CombatEventType_Miss; // Or "SwingAndMiss"
                }

                // TODO: Server-side cooldown application for basic attack
                // caster->SetAbilityCooldown(BASIC_ATTACK_ID, caster->base_basic_attack_cooldown_sec);

                return outcome;
            }

            AttackOutcome ProcessAbilityLaunchPhysicsProjectile(
                uint64_t casterPlayerId,
                const Networking::UDP::C2S::C2S_UseAbilityMsgT& useAbilityIntent,
                const AbilityDefinition& abilityDef,
                PlayerManager& playerManager,
                Physics::PhysicsEngine& physicsEngine
            ) {
                AttackOutcome outcome;
                outcome.success = false;

                ActivePlayer* caster = playerManager.FindPlayerById(casterPlayerId);
                if (!caster) {
                    RF_COMBAT_WARN("ProcessAbilityLaunchPhysicsProjectile: Caster ID %llu not found.", casterPlayerId);
                    outcome.failure_reason_code = "CASTER_NOT_FOUND";
                    return outcome;
                }

                Utilities::Math::Vec3 projectileStartPosition = caster->GetMuzzlePosition();
                Utilities::Math::Vec3 projectileInitialDirection; // This is Utilities::Math::Vec3 (alias for Networking::Shared::Vec3)
                bool hasExplicitTarget = false;

                if (useAbilityIntent.target_position) { // Check if the unique_ptr holds a value
                    // Call with fully qualified namespace:
                    projectileInitialDirection = ::RiftForged::Utilities::Math::SubtractVectors(
                        *(useAbilityIntent.target_position), // Dereference unique_ptr to get the Networking::Shared::Vec3
                        projectileStartPosition              // This is Utilities::Math::Vec3 (Networking::Shared::Vec3)
                    );
                    hasExplicitTarget = true;
                }
                else if (useAbilityIntent.target_entity_id != 0) {
                    ActivePlayer* targetEntity = playerManager.FindPlayerById(useAbilityIntent.target_entity_id);
                    if (targetEntity) {
                        // Call with fully qualified namespace:
                        projectileInitialDirection = ::RiftForged::Utilities::Math::SubtractVectors(
                            targetEntity->position,          // This is Networking::Shared::Vec3
                            projectileStartPosition          // This is Utilities::Math::Vec3 (Networking::Shared::Vec3)
                        );
                        hasExplicitTarget = true;
                    }
                    else {
                        RF_COMBAT_WARN("ProcessAbilityLaunchPhysicsProjectile: Target entity ID %llu for ability %u not found. Defaulting to caster forward.",
                            useAbilityIntent.target_entity_id, useAbilityIntent.ability_id);
                        // Call with fully qualified namespace:
                        projectileInitialDirection = ::RiftForged::Utilities::Math::GetWorldForwardVector(caster->orientation);
                    }
                }
                else {
                    // Call with fully qualified namespace:
                    projectileInitialDirection = ::RiftForged::Utilities::Math::GetWorldForwardVector(caster->orientation);
                }

                // Normalize the direction vector
                // Call with fully qualified namespace:
                if (::RiftForged::Utilities::Math::Magnitude(projectileInitialDirection) > ::RiftForged::Utilities::Math::VECTOR_NORMALIZATION_EPSILON) {
                    // Call with fully qualified namespace:
                    projectileInitialDirection = ::RiftForged::Utilities::Math::NormalizeVector(projectileInitialDirection);
                }
                else {
                    RF_COMBAT_WARN("ProcessAbilityLaunchPhysicsProjectile: Target direction for ability %u is zero. Defaulting to caster forward.", useAbilityIntent.ability_id);
                    // Call with fully qualified namespace:
                    projectileInitialDirection = ::RiftForged::Utilities::Math::GetWorldForwardVector(caster->orientation);
                    // Call with fully qualified namespace again:
                    if (::RiftForged::Utilities::Math::Magnitude(projectileInitialDirection) > ::RiftForged::Utilities::Math::VECTOR_NORMALIZATION_EPSILON) {
                        // Call with fully qualified namespace:
                        projectileInitialDirection = ::RiftForged::Utilities::Math::NormalizeVector(projectileInitialDirection);
                    }
                    else {
                        RF_COMBAT_ERROR("ProcessAbilityLaunchPhysicsProjectile: Caster forward vector is zero for ability %u. Defaulting to Y-axis.", useAbilityIntent.ability_id);
                        // Ensure Vec3 constructor is available and correctly namespaced if this is a Utilities::Math::Vec3 alias
                        projectileInitialDirection = ::RiftForged::Utilities::Math::Vec3(0.f, 1.f, 0.f);
                    }
                }

                // --- Hardcoded "Arrow" properties and rest of the function from previous response ---
                Physics::PhysicsEngine::ProjectilePhysicsProperties arrowPhysProps;
                arrowPhysProps.radius = 0.05f;    arrowPhysProps.halfHeight = 0.25f;
                arrowPhysProps.mass = 0.1f;       arrowPhysProps.enableGravity = true;
                arrowPhysProps.enableCCD = true;

                float arrowSpeed = 40.0f;
                Networking::Shared::DamageInstance arrowDamageOnHit(20, Networking::Shared::DamageType::DamageType_Physical, false);
                std::string arrowVfxTag = "VFX_Arrow_Flying_Standard";
                float arrowMaxRangeOrLifetime = 100.0f;

                // Calculate initial velocity vector (Corrected call with full namespace)
                Utilities::Math::Vec3 initialVelocity = ::RiftForged::Utilities::Math::ScaleVector(projectileInitialDirection, arrowSpeed);

                uint64_t newProjectileId = playerManager.GetNextAvailableProjectileID();
                Physics::PhysicsEngine::ProjectileGameData gameData(
                    newProjectileId, casterPlayerId, arrowDamageOnHit, arrowVfxTag, arrowMaxRangeOrLifetime
                );

                physx::PxRigidDynamic* projectileActor = physicsEngine.CreatePhysicsProjectileActor(
                    arrowPhysProps, gameData, projectileStartPosition, initialVelocity, nullptr
                );

                if (projectileActor) {
                    outcome.success = true;
                    outcome.spawned_projectile = true;
                    outcome.projectile_id = newProjectileId;
                    outcome.projectile_start_position = Networking::Shared::Vec3(projectileStartPosition.x(), projectileStartPosition.y(), projectileStartPosition.z());
                    outcome.projectile_direction = Networking::Shared::Vec3(projectileInitialDirection.x(), projectileInitialDirection.y(), projectileInitialDirection.z());
                    outcome.projectile_speed = arrowSpeed;
                    outcome.projectile_max_range = arrowMaxRangeOrLifetime;
                    outcome.projectile_vfx_tag = arrowVfxTag;
                    outcome.projectile_damage_on_hit = arrowDamageOnHit;
                    outcome.simulated_combat_event_type = Networking::UDP::S2C::CombatEventType::CombatEventType_None;
                }
                else {
                    outcome.success = false;
                    outcome.failure_reason_code = "PROJECTILE_PHYSICS_CREATION_FAILED";
                }
                // --- End of simplified rest of function ---

                return outcome;
            }

        } // namespace Combat
    } // namespace GameLogic
} // namespace RiftForged
