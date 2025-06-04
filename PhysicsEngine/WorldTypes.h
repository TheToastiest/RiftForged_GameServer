// File: WorldTypes.h
// Description: Contains common type definitions for the game world, zones, etc.

#pragma once

#include <string>
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// Forward declaration for PhysX scene.
// Include the actual "PxScene.h" in your .cpp files where you operate on PxScene.
namespace physx {
    class PxScene;
}

// Forward declaration for your primary player/entity type.
// If you have a base Entity class, you might use that.
// For now, assuming ActivePlayer is the main type in zones.
namespace RiftForged {
    namespace GameLogic {
        struct ActivePlayer; // Defined in "../Gameplay/ActivePlayer.h"
    }
}

namespace RiftForged {
    namespace World {

        // Defines the 2D boundaries of a zone on the X-Y ground plane (since Z is height).
        struct ZoneBounds {
            glm::vec2 minXY; // Minimum (X, Y) corner on the ground plane
            glm::vec2 maxXY; // Maximum (X, Y) corner on the ground plane

            // Constructor for easy initialization
            ZoneBounds(const glm::vec2& min_coords, const glm::vec2& max_coords)
                : minXY(min_coords), maxXY(max_coords) {
            }

            // Default constructor
            ZoneBounds() : minXY(0.0f, 0.0f), maxXY(0.0f, 0.0f) {}

            // Checks if an (X,Y) ground position is within the bounds
            bool Contains(const glm::vec2& ground_plane_position) const {
                return ground_plane_position.x >= minXY.x && ground_plane_position.x <= maxXY.x &&
                    ground_plane_position.y >= minXY.y && ground_plane_position.y <= maxXY.y;
            }

            // Helper to check a 3D world position (uses X and Y for the 2D bounds check)
            bool Contains(const glm::vec3& world_position) const {
                return world_position.x >= minXY.x && world_position.x <= maxXY.x &&
                    world_position.y >= minXY.y && world_position.y <= maxXY.y;
            }

            // Optional: Get center, size, etc.
            glm::vec2 GetCenter() const {
                return (minXY + maxXY) * 0.5f;
            }

            glm::vec2 GetSize() const {
                return maxXY - minXY;
            }
        };

        // Represents a single zone in the game world.
        struct Zone {
            uint32_t id;                    // Unique identifier for the zone
            std::string name;               // Human-readable name (e.g., "StartingArea_NE", "MarketDistrict")
            ZoneBounds bounds;              // The X, Y boundaries of this zone
            physx::PxScene* physicsScene;   // Each zone has its own isolated PhysX scene

            // Entities currently within this zone.
            // You might have separate lists or a more complex entity management system per zone.
            // For now, assuming ActivePlayer is the primary type.
            std::vector<GameLogic::ActivePlayer*> playersInZone;
            // std::vector<GameLogic::NPC*> npcsInZone; // Example for later
            // std::vector<GameLogic::WorldObject*> objectsInZone; // Example for later

            // Constructor
            Zone(uint32_t zone_id, const std::string& zone_name, const ZoneBounds& zone_bounds)
                : id(zone_id), name(zone_name), bounds(zone_bounds), physicsScene(nullptr) {
            }

            // Note: The Update method's implementation would typically go in a Zone.cpp
            // void Update(float deltaTime); 

            // Methods to add/remove entities (implementations in Zone.cpp or ZoneManager.cpp)
            // void AddPlayer(GameLogic::ActivePlayer* player);
            // void RemovePlayer(GameLogic::ActivePlayer* player);
            // ... other entity types ...
        };

        // You might also define other world-related types here in the future,
        // such as types for points of interest, region flags, etc.

    } // namespace World
} // namespace RiftForged