// File: PhysicsEngine/PhysicsEngine.cpp
// Copyright (c) 2023-2025 RiftForged Game Development Team

#include "PhysicsEngine.h"
#include <vector> // Used by some method signatures in .h (e.g., MoveCharacterController)

// PhysX specific headers needed for implementation
#include "physx/cooking/PxCooking.h"
#include "physx/cooking/PxCookingInternal.h"
#include "physx/cooking/PxTriangleMeshDesc.h"
#include "physx/cooking/PxConvexMeshDesc.h"
#include "physx/extensions/PxDefaultErrorCallback.h"
#include "physx/extensions/PxDefaultAllocator.h"
#include "physx/pvd/PxPvd.h"
#include "physx/pvd/PxPvdTransport.h"
#include "physx/extensions/PxDefaultCpuDispatcher.h"
#include "physx/PxRigidStatic.h"
#include "physx/PxRigidDynamic.h"
#include "physx/PxMaterial.h"
#include "physx/PxScene.h"
#include "physx/PxSceneDesc.h"
#include "physx/PxPhysics.h"
#include "physx/PxPhysicsAPI.h"
#include "physx/common/PxInsertionCallback.h"
#include "physx/characterkinematic/PxControllerManager.h"
#include "physx/common/PxTolerancesScale.h"
#include "physx/extensions/PxDefaultStreams.h"
#include "physx/foundation/PxPhysicsVersion.h" // For PX_PHYSICS_VERSION
#include "physx/foundation/PxFoundation.h"      // For PxCreateFoundation
#include "physx/extensions/PxExtensionsAPI.h" // For PxInitExtensions, PxCloseExtensions
#include "physx/PxQueryFiltering.h"
#include "physx/PxShape.h"
#include "physx/geometry/PxCapsuleGeometry.h"
#include "physx/extensions/PxRigidActorExt.h"

// TEMPORARY DIAGNOSTIC FUNCTION
//static void TestPhysXHeaderInclusion() {
//    physx::PxTolerancesScale tolerancesScale;
//    physx::PxCookingParams cookingParams(tolerancesScale); // This compiled fine
//    (void)cookingParams;
//
 //   physx::PxCooking* localTestCooking = nullptr; // This compiled fine
 //   (void)localTestCooking;

    // NEW DIAGNOSTIC LINE:
    // Let's see if the compiler considers physx::PxCooking a complete type here.
    // sizeof can only be applied to complete types.
    // [[maybe_unused]] and volatile are to ensure the compiler doesn't optimize this away
    // if it doesn't directly result in an error.
   // [[maybe_unused]] volatile size_t sizeOfPxCooking = sizeof(physx::PxCooking);
    // ----> TRY TO BUILD WITH THIS NEW LINE IN TestPhysXHeaderInclusion()
    // ----> Make sure the problematic m_cooking->createTriangleMesh line in CreateStaticTriangleMesh is still commented out for this test.
//}


// Assuming V0.0.3 for common types, adjust path as needed
#include "../FlatBuffers/V0.0.3/riftforged_common_types_generated.h" 
// Logger is included via PhysicsEngine.h
//#pragma comment(lib, "PhysXCooking_64.lib")



namespace RiftForged {
    namespace Physics {

        // These can be members if you prefer, but global static is common for PhysX samples.
        static physx::PxDefaultErrorCallback gDefaultErrorCallback;
        static physx::PxDefaultAllocator gDefaultAllocatorCallback;
        
        physx::PxTolerancesScale tolerancesScale; // Needs PxTolerancesScale definition
        physx::PxCookingParams cookingParams(tolerancesScale); // PxCookingParams is defined in PxCooking.h

        // --- Helper to setup filter data on a shape ---
        // Implementation of the private helper method
        void PhysicsEngine::SetupShapeFiltering(physx::PxShape* shape, const CollisionFilterData& filter_data) {
            if (shape) {
                physx::PxFilterData px_filter_data;
                px_filter_data.word0 = filter_data.word0;
                px_filter_data.word1 = filter_data.word1;
                px_filter_data.word2 = filter_data.word2;
                px_filter_data.word3 = filter_data.word3;
                shape->setQueryFilterData(px_filter_data);
                shape->setSimulationFilterData(px_filter_data);
            }
        }

        // --- Constructor & Destructor ---
        PhysicsEngine::PhysicsEngine()
            : m_foundation(nullptr),
            m_physics(nullptr),
            m_dispatcher(nullptr),
            m_scene(nullptr),
            m_default_material(nullptr),
            m_controller_manager(nullptr),
            m_pvd(nullptr),
            m_pvd_transport(nullptr)
            // m_playerControllers and m_entityActors are default-initialized (empty maps)
            // m_playerControllersMutex and m_entityActorsMutex are default-initialized
        {
            RF_PHYSICS_INFO("PhysicsEngine: Constructed.");
        }

        PhysicsEngine::~PhysicsEngine() {
            RF_PHYSICS_INFO("PhysicsEngine: Destructor called. Ensuring Shutdown.");
            Shutdown();
        }

        // --- Initialization and Shutdown ---
        bool PhysicsEngine::Initialize(const SharedVec3& gravity, bool connect_to_pvd) {
            RF_PHYSICS_INFO("PhysicsEngine: Initializing...");

            // 1. Foundation
            m_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocatorCallback, gDefaultErrorCallback);
			//m_cooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_foundation, physx::PxCookingParams(physx::PxTolerancesScale()));
            if (!m_foundation) {
                RF_PHYSICS_CRITICAL("PhysicsEngine: PxCreateFoundation failed!");
                return false;
            }
            RF_PHYSICS_INFO("PhysicsEngine: PxFoundation created.");


            // 2. PhysX Visual Debugger (PVD)
            if (connect_to_pvd) {
                m_pvd = PxCreatePvd(*m_foundation);
                if (m_pvd) {
                    m_pvd_transport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
                    if (m_pvd_transport) {
                        if (m_pvd->connect(*m_pvd_transport, physx::PxPvdInstrumentationFlag::eALL)) {
                            RF_PHYSICS_INFO("PhysicsEngine: PVD connection successful to 127.0.0.1:5425.");
                        }
                        else {
                            RF_PHYSICS_WARN("PhysicsEngine: PVD connection failed. Transport created but connection error.");
                            // Clean up PVD if connection failed but transport was created
                            m_pvd_transport->release();
                            m_pvd_transport = nullptr;
                            m_pvd->release();
                            m_pvd = nullptr;
                        }
                    }
                    else {
                        RF_PHYSICS_WARN("PhysicsEngine: PVD transport creation failed. PVD will not be available.");
                        m_pvd->release(); // Release PVD if transport failed
                        m_pvd = nullptr;
                    }
                }
                else {
                    RF_PHYSICS_WARN("PhysicsEngine: PxCreatePvd failed. PVD will not be available.");
                }
            }
            else {
                RF_PHYSICS_INFO("PhysicsEngine: PVD connection explicitly disabled.");
            }

            // 3. Physics Object
            physx::PxTolerancesScale tolerances_scale; // Use default scales
            m_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, tolerances_scale, true, m_pvd);
            if (!m_physics) {
                RF_PHYSICS_CRITICAL("PhysicsEngine: PxCreatePhysics failed!");
                // Shutdown() will release PVD and foundation if they were created
                if (m_pvd_transport) { m_pvd_transport->release(); m_pvd_transport = nullptr; }
                if (m_pvd) { m_pvd->release(); m_pvd = nullptr; }
                if (m_foundation) { m_foundation->release(); m_foundation = nullptr; }
                return false;
            }
            RF_PHYSICS_INFO("PhysicsEngine: PxPhysics created.");

            // 4. Initialize Extensions (provides essential utilities including mesh creation capabilities via PxPhysics)
            if (!PxInitExtensions(*m_physics, m_pvd)) { // Pass m_pvd here
                RF_PHYSICS_CRITICAL("PhysicsEngine: PxInitExtensions failed!");
                // Shutdown() will release physics, PVD, foundation
				//if (m_cooking) { m_cooking->release(); m_cooking = nullptr; }
                if (m_physics) { m_physics->release(); m_physics = nullptr; }
                if (m_pvd_transport) { m_pvd_transport->release(); m_pvd_transport = nullptr; }
                if (m_pvd) { m_pvd->release(); m_pvd = nullptr; }
                if (m_foundation) { m_foundation->release(); m_foundation = nullptr; }
                return false;
            }
            RF_PHYSICS_INFO("PhysicsEngine: PxExtensions initialized.");

			// 4.5 Cooking (optional, but useful for mesh creation)
			// Removed, PxPhysics used directly after PxInitExtensions for basic mesh creation in PhysX 5+

            // 5. Dispatcher
            uint32_t num_hardware_threads = std::thread::hardware_concurrency();
            uint32_t num_threads_for_dispatcher;

            if (num_hardware_threads > 1) {
                num_threads_for_dispatcher = num_hardware_threads - 1; // Leave one core for main thread/other tasks
            }
            else if (num_hardware_threads == 1) {
                num_threads_for_dispatcher = 1; // Use the single core
            }
            else {
                // hardware_concurrency() might return 0 if indeterminable
                RF_PHYSICS_WARN("PhysicsEngine: Could not determine hardware concurrency, defaulting dispatcher to 1 thread.");
                num_threads_for_dispatcher = 1;
            }

            // It's also common to cap this or use a fixed number for a server, e.g., std::min(num_threads_for_dispatcher, 4u);
            // Or even just num_threads_for_dispatcher = 2; (for example)

            m_dispatcher = physx::PxDefaultCpuDispatcherCreate(num_threads_for_dispatcher);
            if (!m_dispatcher) {
                RF_PHYSICS_CRITICAL("PhysicsEngine: PxDefaultCpuDispatcherCreate failed!");
                // Shutdown() will handle prior releases
                PxCloseExtensions();
                if (m_physics) { m_physics->release(); m_physics = nullptr; }
                if (m_pvd_transport) { m_pvd_transport->release(); m_pvd_transport = nullptr; }
                if (m_pvd) { m_pvd->release(); m_pvd = nullptr; }
                if (m_foundation) { m_foundation->release(); m_foundation = nullptr; }
                return false;
            }
            RF_PHYSICS_INFO("PhysicsEngine: PxDefaultCpuDispatcher created with {} threads (Hardware reported: {}).",
                num_threads_for_dispatcher, num_hardware_threads);

            // 6. Scene Description and Scene
            physx::PxSceneDesc scene_desc(m_physics->getTolerancesScale());
            scene_desc.gravity = ToPxVec3(gravity);
            scene_desc.cpuDispatcher = m_dispatcher; // Assign the PxDefaultCpuDispatcher* to PxCpuDispatcher*
            scene_desc.filterShader = physx::PxDefaultSimulationFilterShader;
            // TODO: Setup custom simulationEventCallback if needed: scene_desc.simulationEventCallback = &m_mySimulationEventCallbackInstance;
            // TODO: Setup custom contactModifyCallback if needed: scene_desc.contactModifyCallback = &m_myContactModifyCallbackInstance;

            m_scene = m_physics->createScene(scene_desc);
            if (!m_scene) {
                RF_PHYSICS_CRITICAL("PhysicsEngine: m_physics->createScene failed!");
                // Shutdown() will handle prior releases
                if (m_dispatcher) { m_dispatcher->release(); m_dispatcher = nullptr; }
                PxCloseExtensions();
                if (m_physics) { m_physics->release(); m_physics = nullptr; }
                if (m_pvd_transport) { m_pvd_transport->release(); m_pvd_transport = nullptr; }
                if (m_pvd) { m_pvd->release(); m_pvd = nullptr; }
                if (m_foundation) { m_foundation->release(); m_foundation = nullptr; }
                return false;
            }
            RF_PHYSICS_INFO("PhysicsEngine: PxScene created. Gravity: ({}, {}, {})", gravity.x(), gravity.y(), gravity.z());

            // Configure scene PVD flags for better debugging
            if (m_pvd && m_scene->getScenePvdClient()) {
                m_scene->getScenePvdClient()->setScenePvdFlags(
                    physx::PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS |
                    physx::PxPvdSceneFlag::eTRANSMIT_CONTACTS | // Note: Can be verbose
                    physx::PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES);
            }

            // 7. Default Material
            m_default_material = m_physics->createMaterial(0.5f, 0.5f, 0.1f); // staticFriction, dynamicFriction, restitution
            if (!m_default_material) {
                RF_PHYSICS_CRITICAL("PhysicsEngine: m_physics->createMaterial for default material failed!");
                // Shutdown() will handle prior releases
                if (m_scene) { m_scene->release(); m_scene = nullptr; }
                if (m_dispatcher) { m_dispatcher->release(); m_dispatcher = nullptr; }
                PxCloseExtensions();
                if (m_physics) { m_physics->release(); m_physics = nullptr; }
                if (m_pvd_transport) { m_pvd_transport->release(); m_pvd_transport = nullptr; }
                if (m_pvd) { m_pvd->release(); m_pvd = nullptr; }
                if (m_foundation) { m_foundation->release(); m_foundation = nullptr; }
                return false;
            }
            RF_PHYSICS_INFO("PhysicsEngine: Default PxMaterial created.");

            // 8. Character Controller Manager
            m_controller_manager = PxCreateControllerManager(*m_scene);
            if (!m_controller_manager) {
                RF_PHYSICS_CRITICAL("PhysicsEngine: PxCreateControllerManager failed!");
                // Shutdown() will handle prior releases
                if (m_default_material) { m_default_material->release(); m_default_material = nullptr; }
                if (m_scene) { m_scene->release(); m_scene = nullptr; }
                if (m_dispatcher) { m_dispatcher->release(); m_dispatcher = nullptr; }
                PxCloseExtensions();
                if (m_physics) { m_physics->release(); m_physics = nullptr; }
                if (m_pvd_transport) { m_pvd_transport->release(); m_pvd_transport = nullptr; }
                if (m_pvd) { m_pvd->release(); m_pvd = nullptr; }
                if (m_foundation) { m_foundation->release(); m_foundation = nullptr; }
                return false;
            }
            RF_PHYSICS_INFO("PhysicsEngine: PxControllerManager created.");

            RF_PHYSICS_INFO("PhysicsEngine: Initialization successful.");
            return true;
        }

        void PhysicsEngine::Shutdown() {
            RF_PHYSICS_INFO("PhysicsEngine: Shutting down...");

            // 1. Clear managed controllers and actors first
            {
                std::lock_guard<std::mutex> lock(m_playerControllersMutex);
                for (auto& pair : m_playerControllers) {
                    if (pair.second) {
                        // PxController::release() also removes it from the PxControllerManager
                        pair.second->release();
                    }
                }
                m_playerControllers.clear();
                RF_PHYSICS_INFO("PhysicsEngine: Player controllers map cleared and PxControllers released.");
            }
            {
                std::lock_guard<std::mutex> lock(m_entityActorsMutex);
                for (auto& pair : m_entityActors) {
                    if (pair.second) {
                        // Actors must be removed from scene before releasing, if they were added.
                        // PxScene::release() will release actors it contains.
                        // If an actor was created but not added to scene and is in this map, it needs release.
                        // Best practice: UnregisterRigidActor handles scene removal and release.
                        // This is a final cleanup; if actors are still in map, they likely need release.
                        if (m_scene && pair.second->getScene() == m_scene) { // Check if it's in our scene
                            m_scene->removeActor(*(pair.second), false); // false = do not wake actors
                        }
                        pair.second->release();
                    }
                }
                m_entityActors.clear();
                RF_PHYSICS_INFO("PhysicsEngine: Entity actors map cleared and PxRigidActors released.");
            }

            // 2. Release PhysX objects in reverse order of creation
            if (m_controller_manager) { m_controller_manager->release(); m_controller_manager = nullptr; RF_PHYSICS_DEBUG("PxControllerManager released."); }
            if (m_default_material) { m_default_material->release(); m_default_material = nullptr; RF_PHYSICS_DEBUG("Default PxMaterial released."); }

            if (m_scene) { m_scene->release(); m_scene = nullptr; RF_PHYSICS_DEBUG("PxScene released."); } // Releases actors within it
            if (m_dispatcher) { m_dispatcher->release(); m_dispatcher = nullptr; RF_PHYSICS_DEBUG("PxDispatcher released."); }

            PxCloseExtensions(); // Clean up extensions if PxInitExtensions was called.
            RF_PHYSICS_DEBUG("PxExtensions closed.");

            if (m_physics) { m_physics->release(); m_physics = nullptr; RF_PHYSICS_DEBUG("PxPhysics released."); }

            if (m_pvd) {
                if (m_pvd_transport && m_pvd_transport->isConnected()) {
                    m_pvd->disconnect();
                }
                if (m_pvd_transport) {
                    m_pvd_transport->release();
                    m_pvd_transport = nullptr;
                }
                m_pvd->release();
                m_pvd = nullptr;
                RF_PHYSICS_DEBUG("PVD released.");
            }
            if (m_foundation) { m_foundation->release(); m_foundation = nullptr; RF_PHYSICS_DEBUG("PxFoundation released."); }

            RF_PHYSICS_INFO("PhysicsEngine: Shutdown complete.");
        }

        void PhysicsEngine::StepSimulation(float delta_time_sec) {
            if (!m_scene) {
                RF_PHYSICS_ERROR("PhysicsEngine::StepSimulation: Scene is null!");
                return;
            }
            if (delta_time_sec <= 0.0f) {
                // RF_PHYSICS_TRACE("PhysicsEngine::StepSimulation: delta_time_sec is zero or negative ({:.4f}s). Skipping.", delta_time_sec);
                return;
            }
            // RF_PHYSICS_TRACE("PhysicsEngine::StepSimulation: Simulating for {:.4f}s.", delta_time_sec);
            m_scene->simulate(delta_time_sec);
            m_scene->fetchResults(true); // Block until simulation is complete
        }

        physx::PxMaterial* PhysicsEngine::CreateMaterial(float static_friction, float dynamic_friction, float restitution) {
            if (!m_physics) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateMaterial: PxPhysics not initialized.");
                return nullptr;
            }
            physx::PxMaterial* new_material = m_physics->createMaterial(static_friction, dynamic_friction, restitution);
            if (!new_material) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateMaterial: m_physics->createMaterial failed.");
            }
            else {
                RF_PHYSICS_INFO("PhysicsEngine: Created new PxMaterial.");
            }
            return new_material;
        }

        // --- Player Character Controller Management ---
        void PhysicsEngine::RegisterPlayerController(uint64_t player_id, physx::PxController* controller) {
            if (!controller) {
                RF_PHYSICS_ERROR("PhysicsEngine::RegisterPlayerController: Attempted to register null controller for player ID {}.", player_id);
                return;
            }
            std::lock_guard<std::mutex> lock(m_playerControllersMutex);
            if (m_playerControllers.count(player_id)) {
                RF_PHYSICS_WARN("PhysicsEngine::RegisterPlayerController: Player ID {} already has a controller. Old one will be orphaned if not released. Overwriting.", player_id);
            }
            m_playerControllers[player_id] = controller;
            RF_PHYSICS_INFO("PhysicsEngine::Registered PxController for player ID {}.", player_id);
        }

        bool PhysicsEngine::SetCharacterControllerOrientation(uint64_t player_id, const SharedQuaternion& orientation) {
            // Get the controller using your existing GetPlayerController method, which is thread-safe
            physx::PxController* controller = GetPlayerController(player_id); //

            if (controller) {
                physx::PxRigidActor* actor = controller->getActor();
                if (actor) {
                    physx::PxTransform currentPose = actor->getGlobalPose();
                    currentPose.q = ToPxQuat(orientation); // ToPxQuat is your helper
                    actor->setGlobalPose(currentPose);
                    // RF_PHYSICS_DEBUG("PhysicsEngine: Set orientation for player ID {}.", player_id);
                    return true;
                }
                else {
                    RF_PHYSICS_ERROR("PhysicsEngine::SetCharacterControllerOrientation: Could not get actor for controller of player ID {}.", player_id);
                }
            }
            else {
                // Warning for controller not found is already in GetPlayerController
                RF_PHYSICS_WARN("PhysicsEngine::SetCharacterControllerOrientation: Controller not found via GetPlayerController for player ID {}. Orientation not set.", player_id);
            }
            return false;
        }

        void PhysicsEngine::UnregisterPlayerController(uint64_t player_id) {
            physx::PxController* controller_to_release = nullptr;
            {
                std::lock_guard<std::mutex> lock(m_playerControllersMutex);
                auto it = m_playerControllers.find(player_id);
                if (it != m_playerControllers.end()) {
                    controller_to_release = it->second;
                    m_playerControllers.erase(it);
                }
            }

            if (controller_to_release) {
                // PxController::release() also removes it from the PxControllerManager
                controller_to_release->release();
                RF_PHYSICS_INFO("PhysicsEngine::Unregistered and released PxController for player ID {}.", player_id);
            }
            else {
                RF_PHYSICS_WARN("PhysicsEngine::UnregisterPlayerController: No PxController found for player ID {}.", player_id);
            }
        }

        // In PhysicsEngine.cpp
        physx::PxController* PhysicsEngine::GetPlayerController(uint64_t player_id) const {
            std::lock_guard<std::mutex> lock(m_playerControllersMutex); // for mutex member
            auto it = m_playerControllers.find(player_id); // for m_playerControllers
            if (it != m_playerControllers.end()) {
                return it->second;
            }
            return nullptr;
        }

        void PhysicsEngine::SetActorUserData(physx::PxActor* actor, void* userData) {
            if (actor) {
                actor->userData = userData;
            }
        }

        // --- Generic Rigid Actor Management ---
        void PhysicsEngine::RegisterRigidActor(uint64_t entity_id, physx::PxRigidActor* actor) {
            if (!actor) {
                RF_PHYSICS_ERROR("PhysicsEngine::RegisterRigidActor: Attempted to register null actor for entity ID {}.", entity_id);
                return;
            }
            if (!m_scene) {
                RF_PHYSICS_ERROR("PhysicsEngine::RegisterRigidActor: Scene not initialized. Cannot add actor for entity ID {}.", entity_id);
                actor->release(); // Release the actor if we can't add it to the scene
                return;
            }
            SetActorUserData(actor, reinterpret_cast<void*>(entity_id)); // Store entity_id
            {
                std::lock_guard<std::mutex> lock(m_entityActorsMutex);
                if (m_entityActors.count(entity_id)) {
                    RF_PHYSICS_WARN("PhysicsEngine::RegisterRigidActor: Entity ID {} already has a registered actor. Overwriting. Old actor might be leaked if not properly released.", entity_id);
                }
                m_entityActors[entity_id] = actor;
            }
            m_scene->addActor(*actor);
            RF_PHYSICS_INFO("PhysicsEngine::RegisterRigidActor: Registered PxRigidActor and added to scene for entity ID {}.", entity_id);
        }

        void PhysicsEngine::UnregisterRigidActor(uint64_t entity_id) {
            physx::PxRigidActor* actor_to_release_and_remove = nullptr;
            {
                std::lock_guard<std::mutex> lock(m_entityActorsMutex);
                auto it = m_entityActors.find(entity_id);
                if (it != m_entityActors.end()) {
                    actor_to_release_and_remove = it->second;
                    m_entityActors.erase(it);
                }
            }

            if (actor_to_release_and_remove) {
                if (m_scene && actor_to_release_and_remove->getScene() == m_scene) {
                    m_scene->removeActor(*actor_to_release_and_remove, false);
                }
                actor_to_release_and_remove->release();
                RF_PHYSICS_INFO("PhysicsEngine::UnregisterRigidActor: Removed from scene (if applicable) and released PxRigidActor for entity ID {}.", entity_id);
            }
            else {
                RF_PHYSICS_WARN("PhysicsEngine::UnregisterRigidActor: No PxRigidActor found for entity ID {}.", entity_id);
            }
        }

        physx::PxRigidActor* PhysicsEngine::GetRigidActor(uint64_t entity_id) const {
            std::lock_guard<std::mutex> lock(m_entityActorsMutex);
            auto it = m_entityActors.find(entity_id);
            return (it != m_entityActors.end()) ? it->second : nullptr;
        }

        // This callback will be used by CapsuleSweepSingle.
// It ignores a specified actor and allows further filtering logic.
        struct RiftStepSweepQueryFilterCallback : public physx::PxQueryFilterCallback {
            physx::PxRigidActor* m_actorToIgnore;
            // You might add other members here if your callback needs more context,
            // e.g., pointers to game data to determine if a hit actor is "dense" or "minor".

            RiftStepSweepQueryFilterCallback(physx::PxRigidActor* actorToIgnore)
                : m_actorToIgnore(actorToIgnore) {
            }

            // preFilter is called by PhysX for each shape encountered by the sweep's broadphase.
            // It decides how to treat the hit: block, touch, or ignore.
            virtual physx::PxQueryHitType::Enum preFilter(
                const physx::PxFilterData& PxShapeFilterData, // Filter data of the shape being swept against
                const physx::PxShape* shape,
                const physx::PxRigidActor* hitActor,
                physx::PxHitFlags& queryFlags) override
            {
                PX_UNUSED(queryFlags); // Mark as unused if you don't change them

                // 1. Always ignore the actor performing the RiftStep
                if (hitActor == m_actorToIgnore) {
                    return physx::PxQueryHitType::eNONE; // Ignore self
                }

                // 2. Implement your "dense" vs "minor" obstacle logic here
                // This is where you check PxShapeFilterData (or hitActor->userData, or query game data based on hitActor)
                // to decide if this obstacle should block the RiftStep.

                // EXAMPLE LOGIC (You'll need to define how you identify dense vs. minor obstacles):
                // Assume PxShapeFilterData.word0 has bits for these categories.
                // Let's say bit 0 means "BLOCKS_RIFTSTEP" (dense)
                // And bit 1 means "PASS_THROUGH_RIFTSTEP" (minor obstacle like a tree, player)

                // Example: if (PxShapeFilterData.word0 & YOUR_RIFTSTEP_BLOCKING_BITMASK) {
                //     RF_PHYSICS_TRACE("RiftStep Sweep: Hit DENSE obstacle.");
                //     return physx::PxQueryHitType::eBLOCK; // Stop the sweep here
                // }
                // else if (PxShapeFilterData.word0 & YOUR_RIFTSTEP_PASS_THROUGH_BITMASK) {
                //     RF_PHYSICS_TRACE("RiftStep Sweep: Passing through MINOR obstacle.");
                //     return physx::PxQueryHitType::eNONE; // Ignore this hit, continue sweep
                // }
                // else if (PxShapeFilterData.word0 & YOUR_PLAYER_OR_NPC_BITMASK) {
                //     RF_PHYSICS_TRACE("RiftStep Sweep: Passing through another character.");
                //     return physx::PxQueryHitType::eNONE; // Ignore other characters for pass-through
                // }


                // Default behavior if no specific rule matches: treat as blocking.
                // You might want to adjust this default (e.g., default to eNONE if most things are pass-through)
                RF_PHYSICS_TRACE("RiftStep Sweep: Hit generic obstacle, treating as BLOCK.");
                return physx::PxQueryHitType::eBLOCK;
            }

            // postFilter is called after a hit has passed preFilter and an exact impact point is found.
            // Usually not needed for simple blocking/non-blocking logic but can be used for further refinement.
            virtual physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData& filterData, const physx::PxQueryHit& hit, const physx::PxShape* shape, const physx::PxRigidActor* actor) override {
                PX_UNUSED(filterData);
                PX_UNUSED(hit);
                PX_UNUSED(shape);
                PX_UNUSED(actor);
                // For RiftStep, if preFilter decided it's a block, it's a block. If eNONE, it's ignored.
                // If preFilter returned eTOUCH, postFilter could potentially upgrade it to eBLOCK or downgrade to eNONE.
                // For your current goal, preFilter is doing most of the work.
                return physx::PxQueryHitType::eBLOCK; // Or whatever the preFilter decided effectively
            }
        };

        bool PhysicsEngine::CapsuleSweepSingle(
            const SharedVec3& start_pos,
            const SharedQuaternion& orientation,
            float radius,
            float half_height,
            const SharedVec3& unit_direction,
            float max_distance,
            HitResult& out_hit_result,
            physx::PxRigidActor* actor_to_ignore, // Changed from player_id to PxRigidActor*
            const physx::PxQueryFilterData& filter_data,
            physx::PxQueryFilterCallback* filter_callback_override // Optional override
        ) {
            if (!m_scene || !m_physics) {
                RF_PHYSICS_ERROR("PhysicsEngine::CapsuleSweepSingle: Physics scene not initialized.");
                out_hit_result.distance = 0; // Indicate no travel
                return true; // Treat as a hit to prevent movement if scene is bad
            }
            if (max_distance <= 0.0f) {
                out_hit_result.distance = 0;
                return false; // No distance to sweep, no hit.
            }

            physx::PxCapsuleGeometry capsule_geometry(radius, half_height);
            physx::PxTransform initial_pose(ToPxVec3(start_pos), ToPxQuat(orientation)); //
            physx::PxVec3 sweep_direction_px = ToPxVec3(unit_direction); //

            physx::PxSweepBuffer sweep_buffer; // For first blocking hit

            // Use the provided filter_callback_override if available, otherwise use our RiftStep specific one.
            RiftStepSweepQueryFilterCallback default_riftstep_filter(actor_to_ignore);
            physx::PxQueryFilterCallback* actual_filter_callback = filter_callback_override ? filter_callback_override : &default_riftstep_filter;

            // Hit flags: ePOSITION and eNORMAL are useful. eMESH_BOTH_SIDES if terrain might be single-sided.
            // eASSUME_NO_INITIAL_OVERLAP is an optimization if you're sure the capsule isn't already stuck.
            physx::PxHitFlags hit_flags = physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL | physx::PxHitFlag::eMESH_BOTH_SIDES | physx::PxHitFlag::eASSUME_NO_INITIAL_OVERLAP;

            bool hit_found = m_scene->sweep(
                capsule_geometry,
                initial_pose,
                sweep_direction_px,
                max_distance,
                sweep_buffer,       // Output hits
                hit_flags,          // Hit flags
                filter_data,        // Query filter data (for broadphase/midphase)
                actual_filter_callback // Narrowphase/exact hit filtering callback
            );

            if (hit_found && sweep_buffer.hasBlock) { // Check for blocking hit specifically
                const physx::PxSweepHit& blocking_hit = sweep_buffer.block;

                out_hit_result.hit_actor = blocking_hit.actor; //
                out_hit_result.hit_shape = blocking_hit.shape; //
                out_hit_result.hit_point = FromPxVec3(blocking_hit.position); //
                out_hit_result.hit_normal = FromPxVec3(blocking_hit.normal);   //
                out_hit_result.distance = blocking_hit.distance; //
                out_hit_result.hit_face_index = blocking_hit.faceIndex; //

                out_hit_result.hit_entity_id = 0; //
                if (blocking_hit.actor && blocking_hit.actor->userData) {
                    // Assuming userData on controller actors is set to player_id (uint64_t)
                    // Be cautious with reinterpret_cast if userData can be other things.
                    out_hit_result.hit_entity_id = reinterpret_cast<uint64_t>(blocking_hit.actor->userData);
                }
                RF_PHYSICS_DEBUG("CapsuleSweepSingle: Blocking hit found at distance {}.", blocking_hit.distance);
                return true; // Blocking hit found
            }

            RF_PHYSICS_DEBUG("CapsuleSweepSingle: No blocking hit found within {}m.", max_distance);
            out_hit_result.distance = max_distance; // No block, so effectively traveled full distance
            return false; // No blocking hit
        }


        // --- Player Character Controller Creation & Actions ---
        physx::PxController* PhysicsEngine::CreateCharacterController(
            uint64_t player_id,
            const SharedVec3& initial_position,
            float radius,
            float height,
            physx::PxMaterial* material,
            void* user_data_for_controller_actor // User data for the controller's PxRigidDynamic actor
        ) {
            if (!m_controller_manager || !m_physics) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: Not initialized properly for player ID {}.", player_id);
                return nullptr;
            }
            if (radius <= 0.0f || height <= 0.0f) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: Invalid radius ({}) or height ({}) for player ID {}.", radius, height, player_id);
                return nullptr;
            }

            physx::PxCapsuleControllerDesc desc;
            desc.height = height;
            desc.radius = radius;
            desc.position = physx::PxExtendedVec3(initial_position.x(), initial_position.y(), initial_position.z());
            desc.material = material ? material : m_default_material;
            if (!desc.material) { // Should have m_default_material if material is null
                RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: No valid PxMaterial available for player ID {}.", player_id);
                return nullptr;
            }
            desc.stepOffset = 0.5f; // How high steps it can climb (configurable)
            float slope_limit_degrees = 45.0f;
            desc.slopeLimit = std::cos(slope_limit_degrees * (physx::PxPi / 180.0f)); // Max slope angle (e.g., 45 degrees)
            desc.contactOffset = 0.05f; // Contact offset for CCT (should be > 0)
            desc.upDirection = physx::PxVec3(0.0f, 0.0f, 1.0f); // Assuming Z-up
            // desc.reportCallback = your_hit_report_callback_instance; // For PxUserControllerHitReport
            // desc.behaviorCallback = your_behavior_callback_instance; // For PxControllerBehaviorCallback

            if (!desc.isValid()) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: PxCapsuleControllerDesc is invalid for player ID {}. Radius: {}, Height: {}", player_id, radius, height);
                return nullptr;
            }

            physx::PxController* controller = m_controller_manager->createController(desc);
            if (controller) {
                // The PxController's actor (a PxRigidDynamic) can have user data.
                // It's good practice to set this to the player_id for easy lookup in collision/query callbacks.
                physx::PxRigidActor* controller_actor = controller->getActor();
                if (controller_actor) {
                    SetActorUserData(controller_actor, user_data_for_controller_actor ? user_data_for_controller_actor : reinterpret_cast<void*>(player_id));
                }
                RegisterPlayerController(player_id, controller); // Register with our map
                RF_PHYSICS_INFO("PhysicsEngine: Created and registered PxController for player ID {}.", player_id);
            }
            else {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: m_controller_manager->createController failed for player ID {}.", player_id);
            }
            return controller;
        }

        uint32_t PhysicsEngine::MoveCharacterController(
            physx::PxController* controller,
            const SharedVec3& world_space_displacement,
            float delta_time_sec,
            const std::vector<physx::PxController*>& other_controllers_to_ignore // TODO: Implement filtering
        ) {
            if (!controller) {
                RF_PHYSICS_ERROR("PhysicsEngine::MoveCharacterController: Null controller passed.");
                return 0; // No flags
            }
            if (delta_time_sec <= 0.0f) {
                return 0; // No movement if no time has passed
            }

            physx::PxControllerFilters filters; // Default filters for now
            // TODO: If other_controllers_to_ignore is not empty, you would set up a custom PxQueryFilterCallback
            // or ensure the filterData on the controllers is set up to ignore each other based on some group.
            // For simple cases, PhysX CCTs don't automatically push each other without some extra setup or kinematic targets.

            physx::PxVec3 disp = ToPxVec3(world_space_displacement);
            // The minDist parameter (0.001f) is a small skin width to prevent numerical issues.
            // The delta_time_sec is used by the CCT for auto-stepping and other internal calculations.
            physx::PxControllerCollisionFlags collision_flags = controller->move(disp, 0.001f, delta_time_sec, filters, nullptr /*obstacles - for explicit obstacle shapes*/);

            return collision_flags;
        }

        void PhysicsEngine::SetCharacterControllerPose(physx::PxController* controller, const SharedVec3& world_position) {
            if (!controller) {
                RF_PHYSICS_ERROR("PhysicsEngine::SetCharacterControllerPose: Null controller passed.");
                return;
            }
            if (!controller->setPosition(physx::PxExtendedVec3(world_position.x(), world_position.y(), world_position.z()))) {
                RF_PHYSICS_WARN("PhysicsEngine::SetCharacterControllerPose: controller->setPosition failed. Position may be invalid or obstructed.");
            }
        }

        SharedVec3 PhysicsEngine::GetCharacterControllerPosition(physx::PxController* controller) const {
            if (!controller) {
                RF_PHYSICS_ERROR("PhysicsEngine::GetCharacterControllerPosition: Null controller passed. Returning zero vector.");
                return SharedVec3(0.0f, 0.0f, 0.0f);
            }
            physx::PxExtendedVec3 pos = controller->getPosition();
            return SharedVec3(static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z));
        }

        // --- Static Rigid Actor Creation ---
        physx::PxRigidStatic* PhysicsEngine::CreateStaticPlane(
            const SharedVec3& normal, float distance,
            physx::PxMaterial* material, const CollisionFilterData& filter_data
        ) {
            if (!m_physics || !m_scene) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticPlane: Not initialized.");
                return nullptr;
            }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticPlane: No valid material."); return nullptr; }

            physx::PxRigidStatic* plane_actor = physx::PxCreatePlane(*m_physics, physx::PxPlane(ToPxVec3(normal), distance), *mat);
            if (!plane_actor) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticPlane: PxCreatePlane failed.");
                return nullptr;
            }

            // Setup filtering if a shape exists (PxCreatePlane creates one implicitly)
            physx::PxShape* shape_ptr[1];
            if (plane_actor->getShapes(shape_ptr, 1) == 1) {
                SetupShapeFiltering(shape_ptr[0], filter_data);
            }

            m_scene->addActor(*plane_actor);
            RF_PHYSICS_INFO("PhysicsEngine: Static plane created and added to scene.");
            return plane_actor; // Note: Not registered with m_entityActors unless an entity_id is provided/needed for planes
        }

        physx::PxRigidStatic* PhysicsEngine::CreateStaticBox(
            uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation,
            const SharedVec3& half_extents, physx::PxMaterial* material,
            const CollisionFilterData& filter_data, void* user_data
        ) {
            if (!m_physics) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticBox: Not initialized."); return nullptr; }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticBox: No valid material."); return nullptr; }

            physx::PxBoxGeometry geometry(ToPxVec3(half_extents));
            physx::PxTransform pose(ToPxVec3(position), ToPxQuat(orientation));
            physx::PxRigidStatic* actor = m_physics->createRigidStatic(pose);

            if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticBox: createRigidStatic failed."); return nullptr; }

            physx::PxShape* shape = m_physics->createShape(geometry, *mat, true); // true for per-shape filter data
            if (!shape) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticBox: createShape failed."); actor->release(); return nullptr; }

            SetupShapeFiltering(shape, filter_data);
            actor->attachShape(*shape);
            shape->release(); // Shape is now owned by actor

            SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
            RegisterRigidActor(entity_id, actor); // Adds to scene and map
            RF_PHYSICS_INFO("PhysicsEngine: Static box for entity ID {} created.", entity_id);
            return actor;
        }

        physx::PxRigidStatic* PhysicsEngine::CreateStaticSphere(
            uint64_t entity_id, const SharedVec3& position, float radius,
            physx::PxMaterial* material, const CollisionFilterData& filter_data, void* user_data
        ) {
            if (!m_physics) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticSphere: Not initialized."); return nullptr; }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticSphere: No valid material."); return nullptr; }

            physx::PxSphereGeometry geometry(radius);
            physx::PxTransform pose(ToPxVec3(position)); // Orientation defaults to identity
            physx::PxRigidStatic* actor = m_physics->createRigidStatic(pose);

            if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticSphere: createRigidStatic failed."); return nullptr; }

            physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
            if (!shape) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticSphere: createShape failed."); actor->release(); return nullptr; }

            SetupShapeFiltering(shape, filter_data);
            actor->attachShape(*shape);
            shape->release();

            SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
            RegisterRigidActor(entity_id, actor);
            RF_PHYSICS_INFO("PhysicsEngine: Static sphere for entity ID {} created.", entity_id);
            return actor;
        }

        physx::PxRigidStatic* PhysicsEngine::CreateStaticCapsule(
            uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation,
            float radius, float half_height, physx::PxMaterial* material,
            const CollisionFilterData& filter_data, void* user_data
        ) {
            if (!m_physics) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticCapsule: Not initialized."); return nullptr; }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticCapsule: No valid material."); return nullptr; }

            physx::PxCapsuleGeometry geometry(radius, half_height);
            physx::PxTransform pose(ToPxVec3(position), ToPxQuat(orientation));
            // By default, PxCapsuleGeometry is aligned along the X-axis.
            // If you want it Y-up or Z-up for game world, you need to rotate it.
            // Often, capsules are created with an additional local rotation if the game's "up" is not PhysX's capsule "up" (X).
            // For example, to align with Y-axis: PxQuat local_rot(PxHalfPi, PxVec3(0,0,1)); pose = pose * PxTransform(local_rot);

            physx::PxRigidStatic* actor = m_physics->createRigidStatic(pose);

            if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticCapsule: createRigidStatic failed."); return nullptr; }

            physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
            if (!shape) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticCapsule: createShape failed."); actor->release(); return nullptr; }

            SetupShapeFiltering(shape, filter_data);
            actor->attachShape(*shape);
            shape->release();

            SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
            RegisterRigidActor(entity_id, actor);
            RF_PHYSICS_INFO("PhysicsEngine: Static capsule for entity ID {} created.", entity_id);
            return actor;
        }


        // --- Dynamic Rigid Actor Creation ---
        physx::PxRigidDynamic* PhysicsEngine::CreateDynamicSphere(
            uint64_t entity_id, const SharedVec3& position, float radius, float density,
            physx::PxMaterial* material, const CollisionFilterData& filter_data, void* user_data
        ) {
            if (!m_physics) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicSphere: Not initialized."); return nullptr; }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicSphere: No valid material."); return nullptr; }

            physx::PxSphereGeometry geometry(radius);
            physx::PxTransform pose(ToPxVec3(position));
            physx::PxRigidDynamic* actor = m_physics->createRigidDynamic(pose);

            if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicSphere: createRigidDynamic failed."); return nullptr; }

            physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
            if (!shape) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicSphere: createShape failed."); actor->release(); return nullptr; }

            SetupShapeFiltering(shape, filter_data);
            actor->attachShape(*shape);
            shape->release();

            if (density > 0.0f) {
                physx::PxRigidBodyExt::updateMassAndInertia(*actor, density);
            }
            else { // Make it kinematic if density is zero or less
                actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
                RF_PHYSICS_WARN("PhysicsEngine::CreateDynamicSphere: Entity ID {} created with density <= 0. Set to kinematic.", entity_id);
            }

            SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
            RegisterRigidActor(entity_id, actor); // Adds to scene and map
            RF_PHYSICS_INFO("PhysicsEngine: Dynamic sphere for entity ID {} created.", entity_id);
            return actor;
        }

        physx::PxRigidDynamic* PhysicsEngine::CreateDynamicBox(
            uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation,
            const SharedVec3& half_extents, float density, physx::PxMaterial* material,
            const CollisionFilterData& filter_data, void* user_data
        ) {
            if (!m_physics) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicBox: Not initialized."); return nullptr; }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicBox: No valid material."); return nullptr; }

            physx::PxBoxGeometry geometry(ToPxVec3(half_extents));
            physx::PxTransform pose(ToPxVec3(position), ToPxQuat(orientation));
            physx::PxRigidDynamic* actor = m_physics->createRigidDynamic(pose);

            if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicBox: createRigidDynamic failed."); return nullptr; }

            physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
            if (!shape) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicBox: createShape failed."); actor->release(); return nullptr; }

            SetupShapeFiltering(shape, filter_data);
            actor->attachShape(*shape);
            shape->release();

            if (density > 0.0f) {
                physx::PxRigidBodyExt::updateMassAndInertia(*actor, density);
            }
            else {
                actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
                RF_PHYSICS_WARN("PhysicsEngine::CreateDynamicBox: Entity ID {} created with density <= 0. Set to kinematic.", entity_id);
            }

            SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
            RegisterRigidActor(entity_id, actor);
            RF_PHYSICS_INFO("PhysicsEngine: Dynamic box for entity ID {} created.", entity_id);
            return actor;
        }

        physx::PxRigidDynamic* PhysicsEngine::CreateDynamicCapsule(
            uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation,
            float radius, float half_height, float density, physx::PxMaterial* material,
            const CollisionFilterData& filter_data, void* user_data
        ) {
            if (!m_physics) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicCapsule: Not initialized."); return nullptr; }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicCapsule: No valid material."); return nullptr; }

            physx::PxCapsuleGeometry geometry(radius, half_height);
            physx::PxTransform pose(ToPxVec3(position), ToPxQuat(orientation));
            // Remember PxCapsuleGeometry default alignment (X-axis) if specific game world orientation is needed.

            physx::PxRigidDynamic* actor = m_physics->createRigidDynamic(pose);

            if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicCapsule: createRigidDynamic failed."); return nullptr; }

            physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
            if (!shape) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicCapsule: createShape failed."); actor->release(); return nullptr; }

            SetupShapeFiltering(shape, filter_data);
            actor->attachShape(*shape);
            shape->release();

            if (density > 0.0f) {
                physx::PxRigidBodyExt::updateMassAndInertia(*actor, density);
            }
            else {
                actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
                RF_PHYSICS_WARN("PhysicsEngine::CreateDynamicCapsule: Entity ID {} created with density <= 0. Set to kinematic.", entity_id);
            }

            SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
            RegisterRigidActor(entity_id, actor);
            RF_PHYSICS_INFO("PhysicsEngine: Dynamic capsule for entity ID {} created.", entity_id);
            return actor;
        }

        // --- Triangle Mesh Creation ---
        physx::PxRigidStatic* PhysicsEngine::CreateStaticTriangleMesh(
            uint64_t entity_id,
            const std::vector<SharedVec3>& vertices,
            const std::vector<uint32_t>& indices,
            const SharedVec3& scale_vec,
            physx::PxMaterial* material,
            const CollisionFilterData& filter_data,
            void* user_data) {


            // 1. Initial Checks & Prerequisites (Same as your original code)
            if (!m_physics || !m_scene) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: PxPhysics or PxScene not initialized for entity ID {}.", entity_id);
                return nullptr;
            }
            if (vertices.empty() || indices.empty() || (indices.size() % 3 != 0)) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: Invalid vertex or index data for entity ID {}.", entity_id);
                return nullptr;
            }


            // 2. Vertex Data Conversion (Same as your original code)
            std::vector<physx::PxVec3> px_vertices(vertices.size());
            for (size_t i = 0; i < vertices.size(); ++i) {
                px_vertices[i] = ToPxVec3(vertices[i]);
            }



            // 3. Triangle Mesh Description (Same as your original code)
            physx::PxTriangleMeshDesc mesh_desc;
            mesh_desc.points.count = static_cast<physx::PxU32>(px_vertices.size());
            mesh_desc.points.stride = sizeof(physx::PxVec3);
            mesh_desc.points.data = px_vertices.data();

            mesh_desc.triangles.count = static_cast<physx::PxU32>(indices.size() / 3);
            mesh_desc.triangles.stride = 3 * sizeof(uint32_t);
            mesh_desc.triangles.data = indices.data();

            physx::PxCooking* m_cooking = nullptr;
            
            // Ensure m_cooking is initialized
            if (!m_cooking) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: PxCooking not initialized.");
                return nullptr;
            }

            // 4. Direct Cooking and PxTriangleMesh Creation
            // This is the modified section:
            if (!m_physics) return nullptr; // Should be caught by earlier check
            physx::PxCookingParams params(m_physics->getTolerancesScale());
            // Configure params if needed, e.g., params.meshWeldTolerance etc. based on your requirements.

            physx::PxTriangleMeshCookingResult::Enum cooking_result;
            physx::PxTriangleMesh* triangle_mesh = nullptr;

            // Call the global C-API function:
            triangle_mesh = PxCreateTriangleMesh(params, mesh_desc, m_physics->getPhysicsInsertionCallback(), &cooking_result); //

            if (!triangle_mesh) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: PxCreateTriangleMesh (C-API) failed for entity ID {}. Result: {}",
                    entity_id, static_cast<int>(cooking_result));
                return nullptr;
            }   
            //    // Note: PxTriangleMeshCookingResult::eSUCCESS is 0. You might want to check if cooking_result != PxTriangleMeshCookingResult::eSUCCESS
            //    // PxCreateTriangleMesh may return a mesh even with some warnings, check specific requirements.
            //    // A more robust check:
            if (cooking_result != physx::PxTriangleMeshCookingResult::eSUCCESS) {
                // Even if triangle_mesh is not null, there might have been non-fatal issues.
                // Log warning or handle as error depending on the severity of 'cooking_result'
                RF_PHYSICS_WARN("PhysicsEngine::CreateStaticTriangleMesh: m_cooking->createTriangleMesh for entity ID {} completed with result: {}.",
                    entity_id,
                    static_cast<int>(cooking_result)); // <-- Cast here
                // If certain results are unacceptable, release and return:
                if (cooking_result == physx::PxTriangleMeshCookingResult::eLARGE_TRIANGLE || cooking_result == physx::PxTriangleMeshCookingResult::eEMPTY_MESH) { // Example conditions
                    if (triangle_mesh) triangle_mesh->release();
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: Cooking failed critically for entity ID {}.", entity_id);
                    return nullptr;
                }
            }


            // 5. Setting up Geometry and Scale (Same as your original code)
            physx::PxMeshScale mesh_scale(ToPxVec3(scale_vec), physx::PxQuat(physx::PxIdentity));
            physx::PxTriangleMeshGeometry geometry(triangle_mesh, mesh_scale);

            // 6. Material and Actor Creation (Same as your original code)
            physx::PxMaterial* mat_to_use = material ? material : m_default_material;
            if (!mat_to_use) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: No material available for entity ID {}.", entity_id);
                triangle_mesh->release();
                return nullptr;
            }
               physx::PxRigidStatic* actor = m_physics->createRigidStatic(physx::PxTransform(physx::PxIdentity));
           if (!actor) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: m_physics->createRigidStatic failed for entity ID {}.", entity_id);
                triangle_mesh->release();
                return nullptr;
            }
                    //    // 7. Shape Creation and Setup (Same as your original code)
            physx::PxShape* shape = physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *mat_to_use);
            if (!shape) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: PxRigidActorExt::createExclusiveShape failed for entity ID {}.", entity_id);
                actor->release();
                triangle_mesh->release();
                return nullptr;
            }

            SetupShapeFiltering(shape, filter_data);

        //    // 8. Resource Management & Final Steps (Same as your original code)
            triangle_mesh->release(); // Mesh data is now owned by the PxTriangleMeshGeometry/shape

            SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
            RegisterRigidActor(entity_id, actor);
            RF_PHYSICS_INFO("PhysicsEngine: Static triangle mesh for entity ID {} created.", entity_id);
            return actor;
        }

        // --- Scene Queries ---
        bool PhysicsEngine::RaycastSingle(
            const SharedVec3& start,
            const SharedVec3& unit_direction,
            float max_distance,
            HitResult& out_hit,
            const physx::PxQueryFilterData& filter_data_from_caller, // Using PxQueryFilterData directly
            physx::PxQueryFilterCallback* filter_callback
        ) {
            if (!m_scene) {
                RF_PHYSICS_ERROR("PhysicsEngine::RaycastSingle: Scene not initialized.");
                return false;
            }
            if (max_distance <= 0.0f) return false;

            physx::PxVec3 px_origin = ToPxVec3(start);
            physx::PxVec3 px_unit_dir = ToPxVec3(unit_direction);

            physx::PxRaycastBuffer hit_buffer; // For single closest hit (blocking)

            // Setup PxQueryFilterData. It's passed in now.
            // Default flags in header: PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER
            // If filter_callback is non-null, PxQueryFlag::ePREFILTER and/or PxQueryFlag::ePOSTFILTER should typically be set in filter_data_from_caller.flags.

            physx::PxHitFlags hit_flags = physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL | physx::PxHitFlag::eFACE_INDEX;
            // Add eMESH_MULTIPLE if you want multiple hits on the same mesh (for non-blocking raycasts typically)
            // Add eUV if you need UV coordinates

            bool did_hit = m_scene->raycast(px_origin, px_unit_dir, max_distance, hit_buffer, hit_flags, filter_data_from_caller, filter_callback);

            if (did_hit && hit_buffer.hasBlock) { // For raycasts, check hasBlock
                out_hit.hit_actor = hit_buffer.block.actor;
                out_hit.hit_shape = hit_buffer.block.shape;
                if (hit_buffer.block.actor && hit_buffer.block.actor->userData) {
                    out_hit.hit_entity_id = reinterpret_cast<uint64_t>(hit_buffer.block.actor->userData);
                }
                else {
                    out_hit.hit_entity_id = 0;
                }
                out_hit.hit_point = FromPxVec3(hit_buffer.block.position);
                out_hit.hit_normal = FromPxVec3(hit_buffer.block.normal);
                out_hit.distance = hit_buffer.block.distance;
                out_hit.hit_face_index = hit_buffer.block.faceIndex;
                return true;
            }
            return false;
        }

        std::vector<HitResult> PhysicsEngine::RaycastMultiple(
            const SharedVec3& start,
            const SharedVec3& unit_direction,
            float max_distance,
            uint32_t max_hits,
            const physx::PxQueryFilterData& filter_data_from_caller,
            physx::PxQueryFilterCallback* filter_callback
        ) {
            std::vector<HitResult> results;
            if (!m_scene || max_hits == 0 || max_distance <= 0.0f) {
                RF_PHYSICS_ERROR("PhysicsEngine::RaycastMultiple: Scene not initialized, max_hits is 0, or invalid max_distance.");
                return results;
            }

            physx::PxVec3 px_origin = ToPxVec3(start);
            physx::PxVec3 px_unit_dir = ToPxVec3(unit_direction);

            std::vector<physx::PxRaycastHit> hit_touches(max_hits);
            physx::PxRaycastBufferN<16> hit_buffer; // 16 is the max number of hits


            // For RaycastMultiple, PxQueryFlag::eNO_BLOCK is often used in filter_data_from_caller.flags
            // if you want all hits up to max_distance / max_hits.
            physx::PxHitFlags hit_flags = physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL | physx::PxHitFlag::eFACE_INDEX;

            bool did_hit_anything = m_scene->raycast(px_origin, px_unit_dir, max_distance, hit_buffer, hit_flags, filter_data_from_caller, filter_callback);

            if (did_hit_anything) {
                results.reserve(hit_buffer.getNbAnyHits());
                for (physx::PxU32 i = 0; i < hit_buffer.getNbAnyHits(); ++i) {
                    const physx::PxRaycastHit& touch = hit_buffer.getAnyHit(i);
                    HitResult res;
                    res.hit_actor = touch.actor;
                    res.hit_shape = touch.shape;
                    if (touch.actor && touch.actor->userData) {
                        res.hit_entity_id = reinterpret_cast<uint64_t>(touch.actor->userData);
                    }
                    else {
                        res.hit_entity_id = 0;
                    }
                    res.hit_point = FromPxVec3(touch.position);
                    res.hit_normal = FromPxVec3(touch.normal);
                    res.distance = touch.distance;
                    res.hit_face_index = touch.faceIndex;
                    results.push_back(res);
                }
            }
            return results;
        }

        std::vector<HitResult> PhysicsEngine::OverlapMultiple(
            const physx::PxGeometry& geometry, const physx::PxTransform& pose,
            uint32_t max_hits_alloc,
            const physx::PxQueryFilterData& filter_data_from_caller,
            physx::PxQueryFilterCallback* filter_callback) {
            std::vector<HitResult> results;
            if (!m_scene || max_hits_alloc == 0) return results;

            std::vector<physx::PxOverlapHit> touches(max_hits_alloc);
            // Use PxOverlapBuffer directly
            physx::PxOverlapBuffer hit_buffer(touches.data(), max_hits_alloc);

            // PxQueryFilterData flags like eNO_BLOCK and eANY_HIT are common for overlaps
            m_scene->overlap(geometry, pose, hit_buffer, filter_data_from_caller, filter_callback);

            uint32_t nb_hits = hit_buffer.getNbTouches(); // Or getNbAnyHits
            results.reserve(nb_hits);
            for (physx::PxU32 i = 0; i < nb_hits; ++i) {
                const physx::PxOverlapHit& touch = hit_buffer.getTouch(i); // Or getAnyHit(i)
                HitResult res;
                res.hit_actor = touch.actor;
                res.hit_shape = touch.shape;
                if (touch.actor && touch.actor->userData) { res.hit_entity_id = reinterpret_cast<uint64_t>(touch.actor->userData); }
                res.distance = 0.0f;
                res.hit_face_index = touch.faceIndex;
                results.push_back(res);
            }
            return results;
        }

        // --- Force & Effect Application ---
        void PhysicsEngine::ApplyForceToActor(physx::PxRigidBody* actor, const SharedVec3& force, physx::PxForceMode::Enum mode, bool wakeup) {
            if (actor && actor->getScene()) { // Ensure actor is valid and in a scene
                actor->addForce(ToPxVec3(force), mode, wakeup);
            }
            else if (actor) {
                RF_PHYSICS_WARN("PhysicsEngine::ApplyForceToActor: Actor is not in a scene. Force not applied.");
            }
            else {
                RF_PHYSICS_WARN("PhysicsEngine::ApplyForceToActor: Null actor passed.");
            }
        }

        void PhysicsEngine::ApplyForceToActorById(uint64_t entity_id, const SharedVec3& force, physx::PxForceMode::Enum mode, bool wakeup) {
            physx::PxRigidActor* actor_base = GetRigidActor(entity_id); // Uses mutex internally

            if (!actor_base) { // If not found in general actors, check player controllers
                physx::PxController* controller = GetPlayerController(entity_id); // Uses mutex internally
                if (controller) {
                    actor_base = controller->getActor();
                }
            }

            if (actor_base && actor_base->is<physx::PxRigidBody>()) {
                ApplyForceToActor(static_cast<physx::PxRigidBody*>(actor_base), force, mode, wakeup);
            }
            else {
                RF_PHYSICS_WARN("PhysicsEngine::ApplyForceToActorById: No PxRigidBody found for entity ID {}.", entity_id);
            }
        }

        // --- Conceptual Stubs for Advanced Effects ---
        void PhysicsEngine::CreateRadialForceField(uint64_t instigator_id, const SharedVec3& center, float strength, float radius, float duration_sec, bool is_push, float falloff) {
            RF_PHYSICS_INFO("PhysicsEngine::CreateRadialForceField (Conceptual): Instigator {}, Center({:.1f},{:.1f},{:.1f}), Str:{:.1f}, Rad:{:.1f}, Dur:{:.1f}s, Push:{}, Falloff:{:.1f}",
                instigator_id, center.x(), center.y(), center.z(), strength, radius, duration_sec, is_push, falloff);
            // TODO:
            // 1. This would typically be managed by a separate "EffectSystem" or similar.
            // 2. On creation, or each tick for its duration:
            //    a. Perform an overlap query (e.g., PxSphereGeometry) around 'center' with 'radius'.
            //    b. For each PxRigidBody hit (excluding instigator_id if necessary):
            //       i. Calculate direction vector from 'center' to hit actor's position (or reverse for push).
            //       ii. Calculate force magnitude based on 'strength', 'falloff' (e.g., linear, squared), and distance.
            //       iii. Call ApplyForceToActor.
            //    c. Decrement duration_sec or manage via a timer.
            RF_PHYSICS_WARN("CreateRadialForceField is a conceptual stub and requires significant implementation.");
        }

        void PhysicsEngine::ApplyLocalizedGravity(const SharedVec3& center, float strength, float radius, float duration_sec, const SharedVec3& gravity_direction) {
            RF_PHYSICS_INFO("PhysicsEngine::ApplyLocalizedGravity (Conceptual): Center({:.1f},{:.1f},{:.1f}), Str:{:.1f}, Rad:{:.1f}, Dur:{:.1f}s, Dir:({:.1f},{:.1f},{:.1f})",
                center.x(), center.y(), center.z(), strength, radius, duration_sec, gravity_direction.x(), gravity_direction.y(), gravity_direction.z());
            // TODO:
            // 1. Similar to RadialForceField, manage over duration.
            // 2. For actors within radius:
            //    a. Optionally disable scene gravity for them: actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true)
            //    b. Apply a force: ToPxVec3(gravity_direction) * strength * actor->getMass() (if strength is acceleration)
            //    c. After duration, re-enable scene gravity if it was disabled.
            RF_PHYSICS_WARN("ApplyLocalizedGravity is a conceptual stub and requires significant implementation.");
        }

        bool PhysicsEngine::DeformTerrainRegion(const SharedVec3& impact_point, float radius, float depth_or_intensity, int deformation_type) {
            RF_PHYSICS_INFO("PhysicsEngine::DeformTerrainRegion (Conceptual STUB): Impact({:.1f},{:.1f},{:.1f}), Radius:{:.1f}, Depth/Intensity:{:.1f}, Type:{}",
                impact_point.x(), impact_point.y(), impact_point.z(), radius, depth_or_intensity, deformation_type);
            RF_PHYSICS_WARN("Terrain deformation is a highly complex feature requiring a dedicated terrain system (e.g., voxel, dynamic heightfield modification) and physics mesh rebuilding. This PhysicsEngine method is a placeholder for an interface to such a system.");
            return false;
        }

    } // namespace Physics
} // namespace RiftForged