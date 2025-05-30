// File: RiftPointManager.cpp
// RiftForged Game Engine - RiftPointManager Implementation
// Copyright (C) 2025-2028 RiftForged Team

//#include "RiftPointManager.h"        // For RiftPointManager class and RiftPoint struct
//#include "../PhysicsEngine/PhysicsEngine.h" // For PhysicsEngine class definition
//#include "../PhysicsEngine/PhysicsTypes.h"  // For EPhysicsObjectType enum
//#include "../Utils/MathUtil.h"      // For Vec3 and any math constants/functions
//#include "physx/PxPhysicsAPI.h"      // For PxCapsuleGeometry, PxTransform, PxQueryFilterData, PxOverlapHit, etc.
//#include <cmath>                     // For std::cos, std::sin for random point generation if you reimplement it here
//#include <vector>                    // For std::vector
//#include <algorithm>                 // For std::any_of (optional, for checking hardBlockerTypes)


// If you have a common logging header, include it here.
// For now, using placeholder comments for logs.
// #include "YourLoggingSystem.h" 

//namespace RiftForged {
//    namespace Gameplay {
//
//        RiftPointManager::RiftPointManager() {
//            // Initialize the random number generator with a time-based seed.
//            // For deterministic testing, you might want to use a fixed seed.
//            m_rng.seed(static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count()));
//        }
//
//        RiftPointManager::~RiftPointManager() {
//            // Cleanup if necessary, though m_riftPoints will clear itself.
//        }
//
//        // --- Initialization & Data Management ---
//
//        bool RiftPointManager::LoadRiftPointsFromFile(const std::string& filePath) {
//            // STUBBED: Implement actual file loading logic here (e.g., JSON, XML, FlatBuffers).
//            // For now, this does nothing and indicates failure.
//            // RF_LOG_WARN("RiftPointManager::LoadRiftPointsFromFile - STUBBED for path: %s", filePath.c_str());
//            (void)filePath; // Mark as unused for stub
//            ClearAllRiftPoints(); // Ensure a clean state if called
//            // Example placeholder point for testing if needed:
//            // RegisterRiftPoint(RiftPoint(1, "Placeholder Loaded Point", Utilities::Math::Vec3(10.f, 10.f, 0.f), ERiftPointType::SPAWN_POINT, 50.f, 10.f, 15.f, true, true));
//            return false;
//        }
//
//        bool RiftPointManager::RegisterRiftPoint(const RiftPoint& pointData) {
//            if (m_riftPoints.count(pointData.id)) {
//                // RF_LOG_WARN("RiftPointManager::RegisterRiftPoint - Point with ID %u already exists.", pointData.id);
//                return false; // ID already exists
//            }
//            m_riftPoints[pointData.id] = pointData;
//            // RF_LOG_INFO("RiftPointManager::RegisterRiftPoint - Registered point ID %u: %s", pointData.id, pointData.name.c_str());
//            return true;
//        }
//
//        bool RiftPointManager::UpdateRiftPoint(const RiftPoint& pointData) {
//            auto it = m_riftPoints.find(pointData.id);
//            if (it != m_riftPoints.end()) {
//                it->second = pointData;
//                // RF_LOG_INFO("RiftPointManager::UpdateRiftPoint - Updated point ID %u: %s", pointData.id, pointData.name.c_str());
//                return true;
//            }
//            // RF_LOG_WARN("RiftPointManager::UpdateRiftPoint - Point with ID %u not found for update.", pointData.id);
//            return false;
//        }
//
//        void RiftPointManager::UnregisterRiftPoint(uint32_t pointId) {
//            if (m_riftPoints.erase(pointId) > 0) {
//                // RF_LOG_INFO("RiftPointManager::UnregisterRiftPoint - Unregistered point ID %u.", pointId);
//            }
//            else {
//                // RF_LOG_WARN("RiftPointManager::UnregisterRiftPoint - Point with ID %u not found for unregistration.", pointId);
//            }
//        }
//
//        void RiftPointManager::ClearAllRiftPoints() {
//            m_riftPoints.clear();
//            // RF_LOG_INFO("RiftPointManager::ClearAllRiftPoints - All points cleared.");
//        }
//
//
//        // --- Accessors & Queries ---
//
//        const RiftPoint* RiftPointManager::GetRiftPoint(uint32_t pointId) const {
//            auto it = m_riftPoints.find(pointId);
//            if (it != m_riftPoints.end()) {
//                return &(it->second);
//            }
//            return nullptr;
//        }
//
//        std::vector<const RiftPoint*> RiftPointManager::GetAllRiftPoints() const {
//            std::vector<const RiftPoint*> allPoints;
//            allPoints.reserve(m_riftPoints.size());
//            for (const auto& pair : m_riftPoints) {
//                allPoints.push_back(&(pair.second));
//            }
//            return allPoints;
//        }
//
//        std::vector<const RiftPoint*> RiftPointManager::GetRiftPointsByRole(ERiftPointType roleMask, bool matchAnyFlagInMask) const {
//            std::vector<const RiftPoint*> pointsWithRole;
//            for (const auto& pair : m_riftPoints) {
//                const RiftPoint& point = pair.second;
//                bool match = false;
//                if (matchAnyFlagInMask) { // True if any flag in roleMask is present in pointTypeFlags
//                    match = (static_cast<uint8_t>(point.pointTypeFlags) & static_cast<uint8_t>(roleMask)) != 0;
//                }
//                else { // True if all flags in roleMask are present in pointTypeFlags
//                    match = (static_cast<uint8_t>(point.pointTypeFlags) & static_cast<uint8_t>(roleMask)) == static_cast<uint8_t>(roleMask);
//                }
//                if (match) {
//                    pointsWithRole.push_back(&point);
//                }
//            }
//            return pointsWithRole;
//        }
//
//        std::vector<const RiftPoint*> RiftPointManager::GetRiftPointsInRadius(
//            const Utilities::Math::Vec3& origin,
//            float radius,
//            ERiftPointType roleFilter,
//            bool filterByEnabledStatus
//        ) const {
//            std::vector<const RiftPoint*> pointsInRadius;
//            float radiusSq = radius * radius;
//
//            for (const auto& pair : m_riftPoints) {
//                const RiftPoint& point = pair.second;
//
//                if (filterByEnabledStatus && !point.isEnabled) {
//                    continue;
//                }
//                if (roleFilter != ERiftPointType::NONE && !point.HasRole(roleFilter)) {
//                    // This check assumes roleFilter is a single role. If it's a mask, adjust logic.
//                    // For mask: if (!HasFlag(point.pointTypeFlags, roleFilter)) continue;
//                    // Or if any role in the filter mask: if ((point.pointTypeFlags & roleFilter) == ERiftPointType::NONE) continue;
//                    // Current HasRole is good for single role check. For multiple roles in filter, use HasFlag with care.
//                    // Let's assume roleFilter is a specific role or a mask that point.HasRole can interpret (if HasRole handles masks).
//                    // Given HasRole checks a single flag, let's adjust slightly or rely on caller to iterate roles if 'roleFilter' is a mask of ORed types.
//                    // For simplicity, let's say if roleFilter is not NONE, at least one common bit must exist.
//                    if (roleFilter != ERiftPointType::NONE && (static_cast<uint8_t>(point.pointTypeFlags) & static_cast<uint8_t>(roleFilter)) == 0) {
//                        continue;
//                    }
//                }
//
//                if (Utilities::Math::DistanceSquared(origin, point.centralPosition) <= radiusSq) {
//                    pointsInRadius.push_back(&point);
//                }
//            }
//            return pointsInRadius;
//        }
//
//        // --- Server-Side Logic ---
//
//        void RiftPointManager::UpdatePointActivations(const std::vector<Utilities::Math::Vec3>& playerPositions) {
//            // RF_LOG_TRACE("RiftPointManager::UpdatePointActivations - STUBBED. Number of players: %zu", playerPositions.size());
//            for (auto& pair : m_riftPoints) {
//                UpdateSinglePointActivation(pair.second, playerPositions);
//            }
//        }
//
//        bool RiftPointManager::SetPointEnabledState(uint32_t pointId, bool enabled) {
//            auto it = m_riftPoints.find(pointId);
//            if (it != m_riftPoints.end()) {
//                it->second.isEnabled = enabled;
//                // RF_LOG_INFO("RiftPointManager::SetPointEnabledState - Point ID %u set to %s.", pointId, enabled ? "enabled" : "disabled");
//                return true;
//            }
//            // RF_LOG_WARN("RiftPointManager::SetPointEnabledState - Point ID %u not found.", pointId);
//            return false;
//        }
//
//        bool RiftPointManager::GetRandomPositionInSpawnArea(uint32_t pointId, Utilities::Math::Vec3& outPosition) const {
//            const RiftPoint* point = GetRiftPoint(pointId);
//            if (!point || point->spawnAreaRadius <= 0.0f) {
//                return false;
//            }
//
//            // Generate a random angle and distance within the radius for XY plane
//            std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * Utilities::Math::PI_F);
//            // To ensure uniform distribution within circle area, sqrt the uniform random for radius
//            std::uniform_real_distribution<float> radius_dist_normalized(0.0f, 1.0f);
//
//            float angle = angle_dist(m_rng);
//            float random_normalized_radius = std::sqrt(radius_dist_normalized(m_rng)); // For uniform distribution in area
//            float distance_from_center = random_normalized_radius * point->spawnAreaRadius;
//
//            float offsetX = std::cos(angle) * distance_from_center;
//            float offsetY = std::sin(angle) * distance_from_center;
//
//            // Assuming your FlatBuffer Vec3 has a constructor that takes 3 floats
//            outPosition = Utilities::Math::Vec3(
//                point->centralPosition.x() + offsetX,
//                point->centralPosition.y() + offsetY,
//                point->centralPosition.z() // Z remains the same as the point's center
//                // TODO: Consider terrain height lookup for Z if necessary
//            );
//            return true;
//        }
//
//
//        // --- Fast Travel Specific Logic ---
//
//        bool RiftPointManager::ArePointsFastTravelLinked(uint32_t fromPointId, uint32_t toPointId) const {
//            const RiftPoint* fromPoint = GetRiftPoint(fromPointId);
//            if (fromPoint && fromPoint->HasRole(ERiftPointType::FAST_TRAVEL_NODE)) {
//                return std::find(fromPoint->fastTravelLinks.begin(), fromPoint->fastTravelLinks.end(), toPointId) != fromPoint->fastTravelLinks.end();
//            }
//            return false;
//        }
//
//        float RiftPointManager::CalculateFastTravelCost(uint32_t destinationPointId, const Utilities::Math::Vec3& travelInitiationPosition) const {
//            // RF_LOG_TRACE("RiftPointManager::CalculateFastTravelCost - STUBBED.");
//            const RiftPoint* destPoint = GetRiftPoint(destinationPointId);
//            if (!destPoint || !destPoint->HasRole(ERiftPointType::FAST_TRAVEL_NODE) || !destPoint->isEnabled) {
//                return -1.0f; // Indicate invalid or unable to travel
//            }
//
//            float baseCost = static_cast<float>(destPoint->shimmerUseCost);
//            float distance = Utilities::Math::Distance(travelInitiationPosition, destPoint->centralPosition);
//
//            // TODO: Define your distance fee calculation, e.g., feePerUnit * distance
//            float distanceFee = 0.0f; // Placeholder for distance fee calculation
//            // Example: float feePerUnit = 0.1f; distanceFee = distance * feePerUnit;
//
//            return baseCost + distanceFee;
//        }
//
//        float RiftPointManager::CalculateFastTravelCostBetweenPoints(uint32_t sourcePointId, uint32_t destinationPointId) const {
//            // RF_LOG_TRACE("RiftPointManager::CalculateFastTravelCostBetweenPoints - STUBBED.");
//            const RiftPoint* sourcePoint = GetRiftPoint(sourcePointId);
//            const RiftPoint* destPoint = GetRiftPoint(destinationPointId);
//
//            if (!sourcePoint || !sourcePoint->HasRole(ERiftPointType::FAST_TRAVEL_NODE) || !sourcePoint->isEnabled ||
//                !destPoint || !destPoint->HasRole(ERiftPointType::FAST_TRAVEL_NODE) /* optionally check if dest is enabled too */) {
//                return -1.0f; // Indicate invalid
//            }
//            if (!ArePointsFastTravelLinked(sourcePointId, destinationPointId)) {
//                return -1.0f; // Not linked
//            }
//
//            float baseCost = static_cast<float>(sourcePoint->shimmerUseCost); // Or destinationPoint's cost, or average, based on game design
//            float distance = Utilities::Math::Distance(sourcePoint->centralPosition, destPoint->centralPosition);
//
//            // TODO: Define your distance fee calculation
//            float distanceFee = 0.0f; // Placeholder
//
//            return baseCost + distanceFee;
//        }
//
//
//        // --- Role-Specific Selection Logic (Examples) ---
//
//        std::vector<const RiftPoint*> RiftPointManager::GetInitialSpawnPoints(bool requireEnabled) const {
//            std::vector<const RiftPoint*> initialSpawns;
//            for (const auto& pair : m_riftPoints) {
//                const RiftPoint& point = pair.second;
//                if (point.HasRole(ERiftPointType::SPAWN_POINT) && point.isInitialSpawnPoint) {
//                    if (requireEnabled && !point.isEnabled) {
//                        continue;
//                    }
//                    initialSpawns.push_back(&point);
//                }
//            }
//            return initialSpawns;
//        }
//
//        std::vector<const RiftPoint*> RiftPointManager::GetActiveFastTravelNodes(bool requireEnabled) const {
//            std::vector<const RiftPoint*> activeNodes;
//            for (const auto& pair : m_riftPoints) {
//                const RiftPoint& point = pair.second;
//                if (point.HasRole(ERiftPointType::FAST_TRAVEL_NODE)) {
//                    if (requireEnabled && !point.isEnabled) {
//                        continue;
//                    }
//                    activeNodes.push_back(&point);
//                }
//            }
//            return activeNodes;
//        }
//
//        const RiftPoint* RiftPointManager::SelectRespawnPoint(const Utilities::Math::Vec3& deathLocation/*, other criteria */) const {
//            // RF_LOG_TRACE("RiftPointManager::SelectRespawnPoint - STUBBED. Death location: (%.1f, %.1f, %.1f)", deathLocation.x(), deathLocation.y(), deathLocation.z());
//            (void)deathLocation; // Mark as unused for stub
//
//            // STUBBED: Implement actual respawn selection logic.
//            // Example: find the closest enabled respawn point.
//            const RiftPoint* chosenPoint = nullptr;
//            float minDistanceSq = -1.0f;
//
//            for (const auto& pair : m_riftPoints) {
//                const RiftPoint& point = pair.second;
//                if (point.HasRole(ERiftPointType::RESPAWN_POINT) && point.isEnabled) {
//                    // TODO: Add more sophisticated selection criteria (e.g., safety, no enemies nearby, faction)
//                    float distSq = Utilities::Math::DistanceSquared(deathLocation, point.centralPosition);
//                    if (!chosenPoint || distSq < minDistanceSq) {
//                        minDistanceSq = distSq;
//                        chosenPoint = &point;
//                    }
//                }
//            }
//            if (!chosenPoint) { // Fallback: find *any* enabled respawn point if closest logic yields none or is too complex for stub
//                for (const auto& pair : m_riftPoints) {
//                    const RiftPoint& point = pair.second;
//                    if (point.HasRole(ERiftPointType::RESPAWN_POINT) && point.isEnabled) {
//                        return &point; // Return first one found for basic stub
//                    }
//                }
//            }
//            return chosenPoint;
//        }
//
//
//        // --- Private Helper Methods ---
//
//        void RiftPointManager::UpdateSinglePointActivation(RiftPoint& point, const std::vector<Utilities::Math::Vec3>& playerPositions) {
//            // STUBBED: Implement actual activation logic.
//            // If a point is not isDefaultActive, it might become isEnabled if a player is within activationRadius.
//            // It might become disabled if no players are nearby (or this is handled elsewhere / it's sticky once enabled).
//
//            if (point.isDefaultActive) { // Default active points don't change state based on proximity here
//                if (!point.isEnabled) point.isEnabled = true; // Ensure default active points are enabled initially
//                return;
//            }
//
//            bool playerNearby = false;
//            float activationRadiusSq = point.activationRadius * point.activationRadius;
//            for (const auto& playerPos : playerPositions) {
//                if (Utilities::Math::DistanceSquared(playerPos, point.centralPosition) <= activationRadiusSq) {
//                    playerNearby = true;
//                    break;
//                }
//            }
//
//            if (playerNearby) {
//                if (!point.isEnabled) {
//                    point.isEnabled = true;
//                    // RF_LOG_INFO("RiftPointManager: Point ID %u '%s' activated by player proximity.", point.id, point.name.c_str());
//                    // TODO: Potentially send a message to clients that this point is now active/discovered.
//                }
//            }
//            else {
//                // Optional: Disable if no longer near and not defaultActive (depends on desired game mechanics)
//                // if (point.isEnabled) {
//                // point.isEnabled = false;
//                // RF_LOG_INFO("RiftPointManager: Point ID %u '%s' deactivated (no players nearby).", point.id, point.name.c_str());
//                // }
//            }
//        }
//
//        bool RiftPointManager::FindValidSpawnPosition(
//            uint32_t riftPointId,
//            float playerCapsuleRadius,
//            float playerCapsuleCylindricalHeight, // This is the height of the cylindrical part
//            RiftForged::Physics::PhysicsEngine& physicsEngine,
//            int maxAttempts,
//            const std::vector<RiftForged::Physics::EPhysicsObjectType>& hardBlockerTypes,
//            RiftForged::Physics::EPhysicsObjectType playerType, // This will be EPhysicsObjectType::PLAYER_CHARACTER
//            float dropInOffset, // The extra height from which the player "drops in"
//            RiftForged::Utilities::Math::Vec3& outPosition // Output parameter for the spawn position
//        ) const {
//            // 1. Get the RiftPoint data
//            const RiftPoint* riftPoint = GetRiftPoint(riftPointId);
//            if (!riftPoint) {
//                // RF_LOG_ERROR("FindValidSpawnPosition: RiftPoint with ID %u not found.", riftPointId);
//                return false; // RiftPoint doesn't exist, cannot determine a spawn point
//            }
//
//            // 2. Validate input player dimensions
//            if (playerCapsuleRadius <= 0.0f || playerCapsuleCylindricalHeight < 0.0f) {
//                // Note: Cylindrical height can be 0 for a perfect sphere capsule (total height = 2*radius)
//                // RF_LOG_ERROR("FindValidSpawnPosition: Invalid player capsule dimensions (radius: %.2f, cylHeight: %.2f).", playerCapsuleRadius, playerCapsuleCylindricalHeight);
//                return false;
//            }
//
//            // 3. Prepare player's capsule geometry for overlap check
//            // PxCapsuleGeometry constructor takes (radius, halfCylindricalHeight)
//            float halfPlayerCylindricalHeight = playerCapsuleCylindricalHeight / 2.0f;
//            physx::PxCapsuleGeometry playerGeom(playerCapsuleRadius, halfPlayerCylindricalHeight);
//
//            // 4. Variables to store the best candidate positions found
//            RiftForged::Utilities::Math::Vec3 suitableSpotIfNoIdealFound;
//            bool hasFoundSuitableSpotIfNoIdeal = false;
//
//            // 5. Define the ultimate fallback position (center of RiftPoint, Z-adjusted for drop-in)
//            // Player's logical Vec3 position is the center of their capsule.
//            // riftPoint->centralPosition.z() is the target Z for the player's capsule center (on the "ground").
//            Utilities::Math::Vec3 finalFallbackPos(
//                riftPoint->centralPosition.x(),
//                riftPoint->centralPosition.y(),
//                riftPoint->centralPosition.z() + dropInOffset
//            );
//
//            // 6. Loop to find a suitable spot
//            for (int attempt = 0; attempt < maxAttempts; ++attempt) {
//                Utilities::Math::Vec3 candidateXyBase;
//
//                // 6a. Generate a random X,Y candidate position within the spawn area
//                // Your GetRandomPositionInSpawnArea returns Z as riftPoint->centralPosition.z()
//                if (riftPoint->spawnAreaRadius > 0.0f) {
//                    if (!GetRandomPositionInSpawnArea(riftPointId, candidateXyBase)) {
//                        // Should not happen if riftPoint is valid here and has radius, but as a safeguard
//                        // RF_LOG_WARN("FindValidSpawnPosition: GetRandomPositionInSpawnArea failed for RiftPoint ID %u on attempt %d. Using center.", riftPointId, attempt + 1);
//                        candidateXyBase = riftPoint->centralPosition; // Use center if random generation fails
//                    }
//                }
//                else {
//                    // If spawnAreaRadius is zero, all attempts will use the central position
//                    candidateXyBase = riftPoint->centralPosition;
//                }
//
//                // 6b. Calculate final Z for the candidate position (player origin is center, add drop-in)
//                Utilities::Math::Vec3 candidatePos(
//                    candidateXyBase.x(),
//                    candidateXyBase.y(),
//                    riftPoint->centralPosition.z() + dropInOffset
//                );
//
//                // 6c. Create the pose (position and orientation) for the overlap check
//                physx::PxTransform candidatePose(
//                    physx::PxVec3(candidatePos.x(), candidatePos.y(), candidatePos.z()),
//                    physx::PxQuat(physx::PxIdentity) // Default orientation for spawn (can be adjusted later)
//                );
//
//                // 6d. Setup query filter data for the overlap call
//                // We want to detect both static and dynamic objects to check their types.
//                physx::PxQueryFilterData queryFilterData;
//                queryFilterData.flags = physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC;
//                // No specific PxFilterData.data is needed here for the query itself,
//                // as we will inspect the .word0 of the *hit shapes*.
//                // No custom PxQueryFilterCallback is needed for this type of check.
//
//                // 6e. Perform the overlap query
//                std::vector<RiftForged::Physics::HitResult> hits = physicsEngine.OverlapMultiple(
//                    playerGeom,
//                    candidatePose,
//                    MAX_SPAWN_OVERLAP_HITS, // Use the constant defined at the top of the file
//                    queryFilterData,
//                    nullptr // No custom filter callback
//                );
//
//                // 6f. Process the hits for this candidate position
//                bool currentSpotIsHardBlocked = false;
//                bool currentSpotHasPlayers = false;
//
//                if (!hits.empty()) {
//                    for (const RiftForged::Physics::HitResult& hit : hits) {
//                        if (hit.hit_shape) { // Ensure the shape pointer is valid
//                            physx::PxFilterData hitShapeFilterData = hit.hit_shape->getQueryFilterData();
//                            auto hitType = static_cast<RiftForged::Physics::EPhysicsObjectType>(hitShapeFilterData.word0);
//
//                            // Check if the hit object is one of the defined hard blockers
//                            for (RiftForged::Physics::EPhysicsObjectType blockerType : hardBlockerTypes) {
//                                if (hitType == blockerType) {
//                                    currentSpotIsHardBlocked = true;
//                                    break; // Found a hard blocker, no need to check other hits for *this candidate*
//                                }
//                            }
//                            if (currentSpotIsHardBlocked) {
//                                break; // Exit the loop over hits for this candidate, as it's unusable
//                            }
//
//                            // If not a hard blocker, check if it's another player
//                            if (hitType == playerType) {
//                                currentSpotHasPlayers = true;
//                                // We don't break here; a spot with players is still better than a hard-blocked one.
//                                // We also want to ensure no *other* hit at this spot is a hard blocker.
//                            }
//                        }
//                    }
//                }
//
//                // 6g. Decide on this candidate position
//
//                if (!currentSpotIsHardBlocked) {
//                    if (!currentSpotHasPlayers) {
//                        // Ideal spot: No hard blockers AND no players. Use this immediately.
//                        outPosition = candidatePos;
//                        // RF_LOG_DEBUG("FindValidSpawnPosition: Attempt %d: Found IDEAL empty spot.", attempt + 1);
//                        return true;
//                    }
//                    else {
//                        // Spot is not hard-blocked, but has players. This is a "suitable" spot.
//                        // Store this as our preferred fallback if we haven't stored one yet,
//                        // in case we don't find a completely empty one in subsequent attempts.
//                        if (!hasFoundSuitableSpotIfNoIdeal) {
//                            suitableSpotIfNoIdealFound = candidatePos;
//                            hasFoundSuitableSpotIfNoIdeal = true;
//                            // RF_LOG_DEBUG("FindValidSpawnPosition: Attempt %d: Found suitable (player-occupied, not hard-blocked) spot. Will continue for empty.", attempt + 1);
//                        }
//                        // We continue looping for the remaining maxAttempts to see if we can find a truly empty one.
//                    }
//                }
//            } // 
//
//            // 7. After all attempts, if we haven't returned an ideal spot:
//            if (hasFoundSuitableSpotIfNoIdeal) {
//                // We didn't find a completely empty spot, but we found one clear of hard blockers
//                // that was only (or also) occupied by players. Use this one.
//                outPosition = suitableSpotIfNoIdealFound;
//                // RF_LOG_WARN("FindValidSpawnPosition: No completely empty spot after %d attempts for RiftPoint %u. Using best found spot (possibly player-occupied).", maxAttempts, riftPointId);
//                return true;
//            }
//
//            // 8. Fallback: All attempts either hit hard blockers or no suitable spot was found.
//            // Use the pre-calculated finalFallbackPos (center of RiftPoint, Z-adjusted).
//            // This embodies the "force spawn" behavior.
//            // RF_LOG_ERROR("FindValidSpawnPosition: All %d attempts resulted in hard-blocked spots or no suitable candidates for RiftPoint %u. Forcing spawn at central point.", maxAttempts, riftPointId);
//            outPosition = finalFallbackPos;
//
//            // Optional: You could perform one last overlap check here for the finalFallbackPos
//            // just to log if even the center is hard-blocked, for level design feedback.
//            // physx::PxTransform fallbackPose( RiftForged::Physics::ToPxVec3(finalFallbackPos), physx::PxQuat(physx::PxIdentity));
//            // std::vector<RiftForged::Physics::HitResult> fallbackHits = physicsEngine.OverlapMultiple(playerGeom, fallbackPose, MAX_SPAWN_OVERLAP_HITS, queryFilterData, nullptr);
//            // bool fallbackIsActuallyHardBlocked = false;
//            // for (const auto& hit : fallbackHits) { /* check for hardBlockerTypes */ if (isHardBlocker) fallbackIsActuallyHardBlocked = true; break; }
//            // if (fallbackIsActuallyHardBlocked) { /* RF_LOG_CRITICAL("RiftPoint %u center fallback position is ALSO hard-blocked!", riftPointId); */ }
//
//            return true; // We always provide a spawn position due to "force spawn" policy, even if it's just the center.
//        }
//
//    } // namespace Gameplay
//} // namespace RiftForged