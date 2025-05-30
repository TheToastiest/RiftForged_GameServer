//#pragma once
//#include "../PhysicsEngine/PhysicsEngine.h"
//#include "../Utils/MathUtil.h" // For RiftForged::Utilities::Math::Vec3 (alias to FlatBuffers Vec3)
//#include <string>
//#include <vector>
//#include <cstdint>
//#include <map>
//#include <functional> // For std::function if used later for complex predicate queries
//#include <random>     // For random point generation in spawn area
//
//// Forward declaration if player positions are from another module/namespace
//// namespace RiftForged { namespace Server { struct PlayerState; } }
//
//const physx::PxU32 MAX_SPAWN_OVERLAP_HITS = 16;
//
//namespace RiftForged {
//    namespace Gameplay {
//
//        // Enum to define the primary roles a RiftPoint can serve (as a bitmask)
//        enum class ERiftPointType : uint8_t {
//            NONE = 0,
//            SPAWN_POINT = 1 << 0,      // For general entity spawning
//            RESPAWN_POINT = 1 << 1,    // For player/entity respawning after defeat
//            FAST_TRAVEL_NODE = 1 << 2, // For fast travel network
//            // Add other types as needed, e.g., QUEST_OBJECTIVE = 1 << 3;
//        };
//
//        // Helper to allow bitwise operations for ERiftPointType flags
//        inline ERiftPointType operator|(ERiftPointType a, ERiftPointType b) {
//            return static_cast<ERiftPointType>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
//        }
//        inline ERiftPointType operator&(ERiftPointType a, ERiftPointType b) {
//            return static_cast<ERiftPointType>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
//        }
//        inline ERiftPointType& operator|=(ERiftPointType& a, ERiftPointType b) {
//            a = a | b;
//            return a;
//        }
//        // Helper to check if a specific flag is set
//        inline bool HasFlag(ERiftPointType currentFlags, ERiftPointType flagToCheck) {
//            return (static_cast<uint8_t>(currentFlags) & static_cast<uint8_t>(flagToCheck)) == static_cast<uint8_t>(flagToCheck) && static_cast<uint8_t>(flagToCheck) != 0;
//        }
//        inline bool IsExactly(ERiftPointType currentFlags, ERiftPointType flagToCheck) {
//            return static_cast<uint8_t>(currentFlags) == static_cast<uint8_t>(flagToCheck);
//        }
//
//
//        struct RiftPoint {
//            uint32_t id;                            // Unique identifier for this Rift Point
//            std::string name;                       // e.g., "Whispering Woods - Central Clearing"
//            RiftForged::Utilities::Math::Vec3 centralPosition; // The main coordinate (uses FlatBuffers Vec3 via MathUtil.h alias)
//
//            ERiftPointType pointTypeFlags;          // Bitmask of ERiftPointType to define roles
//
//            float activationRadius;                 // Distance within which players activate/discover this point (server-side check)
//            float spawnAreaRadius;                  // Radius around centralPosition where players can actually spawn/arrive
//            float visualEffectRadius;               // How large the visual effect appears (primarily a hint for the client)
//
//            bool isEnabled;                         // Server-controlled: Is this point currently functional/discovered/activated?
//            bool isDefaultActive;                   // Should this point be active from the start, or does it require discovery/activation?
//            
//            // --- Role-Specific Data ---
//            // For SPAWN_POINT
//            bool isInitialSpawnPoint;               // Can new characters spawn here for the first time in the world?
//
//            // For FAST_TRAVEL_NODE
//            int shimmerUseCost;                     // Base cost to use this point for fast travel
//            std::vector<uint32_t> fastTravelLinks;  // IDs of other RiftPoints this one can travel to
//
//            // For RESPAWN_POINT
//            // (No extra data members for respawn points for now, as per discussion)
//
//            // Constructor
//            RiftPoint(uint32_t _id = 0,
//                const std::string& _name = "Unnamed RiftPoint",
//                const Utilities::Math::Vec3& _pos = Utilities::Math::Vec3(0.f, 0.f, 0.f), // Assumes Vec3() or Vec3(0,0,0) constructor
//                ERiftPointType _typeFlags = ERiftPointType::NONE,
//                float _activationRadius = 50.0f,
//                float _spawnRadius = 5.0f,
//                float _visualRadius = 10.0f,
//                bool _isDefaultActive = true,
//                bool _isInitial = false,
//                int _cost = 0)
//                : id(_id), name(_name), centralPosition(_pos), pointTypeFlags(_typeFlags),
//                activationRadius(_activationRadius), spawnAreaRadius(_spawnRadius), visualEffectRadius(_visualRadius),
//                isEnabled(_isDefaultActive), isDefaultActive(_isDefaultActive),
//                isInitialSpawnPoint(_isInitial), shimmerUseCost(_cost) {
//            }
//
//            bool HasRole(ERiftPointType role) const {
//                return HasFlag(pointTypeFlags, role);
//            }
//        };
//
//        class RiftPointManager {
//        public:
//            RiftPointManager();
//            ~RiftPointManager();
//
//            // --- Initialization & Data Management ---
//            // For internal testing with placeholders, you'll primarily use RegisterRiftPoint.
//            // LoadRiftPointsFromFile would be implemented when you move to "datasets".
//            bool LoadRiftPointsFromFile(const std::string& filePath); // STUBBED for now
//
//            bool RegisterRiftPoint(const RiftPoint& pointData);
//            bool UpdateRiftPoint(const RiftPoint& pointData); // To update an existing point
//            void UnregisterRiftPoint(uint32_t pointId);
//            void ClearAllRiftPoints();
//
//            // --- Accessors & Queries ---
//            const RiftPoint* GetRiftPoint(uint32_t pointId) const;
//            std::vector<const RiftPoint*> GetAllRiftPoints() const;
//            std::vector<const RiftPoint*> GetRiftPointsByRole(ERiftPointType roleMask, bool matchAnyFlagInMask = false) const;
//            std::vector<const RiftPoint*> GetRiftPointsInRadius(
//                const Utilities::Math::Vec3& origin,
//                float radius,
//                ERiftPointType roleFilter = ERiftPointType::NONE, // Optional role filter
//                bool filterByEnabledStatus = true                 // Optional filter for only isEnabled points
//            ) const;
//
//            // --- Server-Side Logic ---
//            // Periodically called by the server with current player positions to update point enabled states.
//            // (PlayerPositions would likely be a vector of Vec3 or a more complex player state struct)
//            void UpdatePointActivations(const std::vector<Utilities::Math::Vec3>& playerPositions);
//
//            bool SetPointEnabledState(uint32_t pointId, bool enabled);
//
//            // Get a valid spawn position within a specific RiftPoint's area.
//            bool GetRandomPositionInSpawnArea(uint32_t pointId, Utilities::Math::Vec3& outPosition) const;
//
//            // --- Fast Travel Specific Logic ---
//            bool ArePointsFastTravelLinked(uint32_t fromPointId, uint32_t toPointId) const;
//            // Calculates total cost for fast travel, including base shimmer cost and distance fee.
//            // Distance can be from player's current location or between two RiftPoints.
//            float CalculateFastTravelCost(uint32_t destinationPointId, const Utilities::Math::Vec3& travelInitiationPosition) const;
//            float CalculateFastTravelCostBetweenPoints(uint32_t sourcePointId, uint32_t destinationPointId) const;
//
//
//            // --- Role-Specific Selection Logic (Examples) ---
//            std::vector<const RiftPoint*> GetInitialSpawnPoints(bool requireEnabled = true) const;
//            std::vector<const RiftPoint*> GetActiveFastTravelNodes(bool requireEnabled = true) const;
//
//            // Selects an appropriate respawn point. Criteria might include proximity to death, faction, etc.
//            // For now, a simple selection might be "any enabled respawn point".
//            const RiftPoint* SelectRespawnPoint(const Utilities::Math::Vec3& deathLocation /*, other criteria */) const;
//
//            bool FindValidSpawnPosition(
//                uint32_t riftPointId,
//                float playerCapsuleRadius,
//                float playerCapsuleCylindricalHeight, // We confirmed this is what CreateCharacterController uses
//                RiftForged::Physics::PhysicsEngine& physicsEngine,
//                int maxAttempts,
//                const std::vector<RiftForged::Physics::EPhysicsObjectType>& hardBlockerTypes,
//                RiftForged::Physics::EPhysicsObjectType playerType, // Should be EPhysicsObjectType::PLAYER_CHARACTER
//                float dropInOffset,
//                RiftForged::Utilities::Math::Vec3& outPosition
//            ) const;
//
//        private:
//            std::map<uint32_t, RiftPoint> m_riftPoints;
//
//            // For random number generation within spawn areas
//            mutable std::mt19937 m_rng;
//
//            // For spatial partitioning to optimize UpdatePointActivations if needed (advanced, for later)
//            // e.g., // SomeGridType m_spatialGrid; 
//
//            // Helper method (implementation would be in .cpp)
//            void UpdateSinglePointActivation(
//                RiftPoint& point,
//                const std::vector<Utilities::Math::Vec3>& playerPositions
//            );
//        };
//
//    } // namespace Gameplay
//} // namespace RiftForged