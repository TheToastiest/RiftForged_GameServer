// File: PhysicsEngine/PhysicsTypes.h
// RiftForged Game Development Team
// Copyright (c) 2025-2028 RiftForged Game Development Team 
#pragma once // If in a new header
#include "physx/foundation/PxFlags.h" // For PxFlags    
#include "physx/PxPhysics.h" // For PxU32, PxFilterData, etc.
#include "physx/PxPhysXConfig.h" // For PxU32 and other PhysX types
#include "physx/PxFiltering.h" // For PxU32
#include "physx/PxQueryFiltering.h"

namespace RiftForged {
    namespace Physics {

        enum class EPhysicsObjectType : physx::PxU32 {
            UNDEFINED = 0,
            PLAYER_CHARACTER = 1,
            SMALL_ENEMY = 2,
            MEDIUM_ENEMY = 3,
            LARGE_ENEMY = 4,
            HUGE_ENEMY = 5,

			RAID_BOSS = 7, // Special large enemy type, could be dynamic or static

            VAELITH = 10,
			// Dynamic World Elements
            COMET = 11,
            MAGIC_PROJECTILE = 12,
			LIGHTNING_BOLT = 13,


            // Static World Elements
            WALL = 20,                 // Impassable static structure
            IMPASSABLE_ROCK = 21,      // Large, static rock that blocks movement and spawning
            // Potentially dynamic or less critical obstacles
            LARGE_ROCK = 30,           // Could be dynamic and pushable, or static but less critical than IMPASSABLE
            SMALL_ROCK = 31,           // Likely dynamic and easily pushable, or cosmetic
            // Add other types as you need them:
            MELEE_WEAPON = 40,
            PROJECTILE = 50,
            INTERACTABLE_OBJECT = 60,
            // ... etc. EDIT AS NEEDED

            // A general category for static things that absolutely block spawning
            STATIC_IMPASSABLE = 100 // Could be assigned to WALLS and IMPASSABLE_ROCKS if you want a single check for them
            // Or you check for WALL or IMPASSABLE_ROCK explicitly.
        };

        enum class ECollisionGroup : physx::PxU32 {
            GROUP_NONE            = 0,          // Belongs to no specific group (or use for "don't care")
            GROUP_PLAYER          = (1u << 0),  // Actor is a player
            GROUP_ENEMY           = (1u << 1),  // Actor is an enemy (can be refined for different enemy types if needed)
            GROUP_PLAYER_PROJECTILE = (1u << 2),  // Projectile fired by players
            GROUP_ENEMY_PROJECTILE  = (1u << 3),  // Projectile fired by enemies
            GROUP_WORLD_STATIC    = (1u << 4),  // Walls, ground, static impassable geometry
            GROUP_WORLD_DYNAMIC   = (1u << 5),  // Pushable rocks, dynamic physics props
            GROUP_MELEE_HITBOX    = (1u << 6),  // A hitbox for a melee attack (often a trigger)
            GROUP_COMET           = (1u << 7),  // Special dynamic element like comets
            GROUP_VAELITH         = (1u << 8),  // Special Vaelith type
            GROUP_RAID_BOSS       = (1u << 9),  // Raid boss specific group

            // Add more groups as needed, up to 32 unique bits.
            // For example:
            // GROUP_INTERACTABLE    = (1u << 10),
            // GROUP_TRIGGER_VOLUME  = (1u << 11), // For generic trigger volumes
        };

        // Helper to enable bitmask operations for ECollisionGroup (e.g. GROUP_PLAYER | GROUP_ENEMY)
        // This comes from physx/foundation/PxFlags.h
        typedef physx::PxFlags<ECollisionGroup, physx::PxU32> CollisionGroupFlags;

        PX_CUDA_CALLABLE PX_INLINE RiftForged::Physics::CollisionGroupFlags operator|(RiftForged::Physics::ECollisionGroup a, RiftForged::Physics::ECollisionGroup b)
        {
            // Use the CollisionGroupFlags typedef which correctly refers to physx::PxFlags
            RiftForged::Physics::CollisionGroupFlags r(a);
            r |= b;
            return r;
        }

        PX_CUDA_CALLABLE PX_INLINE RiftForged::Physics::CollisionGroupFlags operator&(RiftForged::Physics::ECollisionGroup a, RiftForged::Physics::ECollisionGroup b)
        {
            RiftForged::Physics::CollisionGroupFlags r(a);
            r &= b;
            return r;
        }

        PX_CUDA_CALLABLE PX_INLINE RiftForged::Physics::CollisionGroupFlags operator~(RiftForged::Physics::ECollisionGroup a)
        {
            // ~ operator on the PxFlags wrapped enum value
            return ~RiftForged::Physics::CollisionGroupFlags(a);
        }


    } // namespace Physics
} // namespace RiftForged