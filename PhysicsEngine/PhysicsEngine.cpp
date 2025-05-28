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
        bool PhysicsEngine::Initialize(const SharedVec3& gravityVec, bool connect_to_pvd) { // Renamed gravity to gravityVec to avoid conflict with sceneDesc.gravity
            RF_PHYSICS_INFO("PhysicsEngine: Initializing...");

            // 1. Foundation
            m_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocatorCallback, gDefaultErrorCallback);
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
                            m_pvd_transport->release(); m_pvd_transport = nullptr;
                            m_pvd->release(); m_pvd = nullptr;
                        }
                    }
                    else {
                        RF_PHYSICS_WARN("PhysicsEngine: PVD transport creation failed. PVD will not be available.");
                        m_pvd->release(); m_pvd = nullptr;
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
            // Enable PVD for m_physics object if PVD connection was successful
            m_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, tolerances_scale, true, m_pvd);
            if (!m_physics) {
                RF_PHYSICS_CRITICAL("PhysicsEngine: PxCreatePhysics failed!");
                if (m_pvd_transport) { m_pvd_transport->release(); m_pvd_transport = nullptr; }
                if (m_pvd) { m_pvd->release(); m_pvd = nullptr; }
                if (m_foundation) { m_foundation->release(); m_foundation = nullptr; }
                return false;
            }
            RF_PHYSICS_INFO("PhysicsEngine: PxPhysics created.");

            // 4. Initialize Extensions
            if (!PxInitExtensions(*m_physics, m_pvd)) { // Pass m_pvd here
                RF_PHYSICS_CRITICAL("PhysicsEngine: PxInitExtensions failed!");
                if (m_physics) { m_physics->release(); m_physics = nullptr; }
                if (m_pvd_transport) { m_pvd_transport->release(); m_pvd_transport = nullptr; }
                if (m_pvd) { m_pvd->release(); m_pvd = nullptr; }
                if (m_foundation) { m_foundation->release(); m_foundation = nullptr; }
                return false;
            }
            RF_PHYSICS_INFO("PhysicsEngine: PxExtensions initialized.");

            // 4.A Create CUDA Context Manager (for GPU Acceleration)
            physx::PxCudaContextManagerDesc cudaContextManagerDesc;
            // You can get the profiler callback from PVD if PVD is available and connected.
            // physx::PxProfilerCallback* profilerCallback = (m_pvd && m_pvd->getProfileZoneManager()) ? m_pvd->getProfileZoneManager() : nullptr;
            // m_cudaContextManager = PxCreateCudaContextManager(*m_foundation, cudaContextManagerDesc, profilerCallback);
            // Simpler call if PVD profiler integration isn't immediately needed or handled differently:
            m_cudaContextManager = PxCreateCudaContextManager(*m_foundation, cudaContextManagerDesc);


            if (m_cudaContextManager) {
                if (!m_cudaContextManager->contextIsValid()) {
                    RF_PHYSICS_WARN("PhysicsEngine: PxCudaContextManager context is NOT valid. GPU acceleration will be disabled.");
                    m_cudaContextManager->release();
                    m_cudaContextManager = nullptr;
                }
                else {
                    RF_PHYSICS_INFO("PhysicsEngine: PxCudaContextManager created and context is valid. GPU acceleration may be available.");
                }
            }
            else {
                RF_PHYSICS_WARN("PhysicsEngine: PxCreateCudaContextManager failed. GPU acceleration will be disabled.");
            }

            // 5. Dispatcher (CPU)
            uint32_t num_hardware_threads = std::thread::hardware_concurrency();
            uint32_t num_threads_for_dispatcher;

            if (num_hardware_threads > 1) {
                num_threads_for_dispatcher = num_hardware_threads - 1;
            }
            else if (num_hardware_threads == 1) {
                num_threads_for_dispatcher = 1;
            }
            else {
                RF_PHYSICS_WARN("PhysicsEngine: Could not determine hardware concurrency, defaulting CPU dispatcher to 1 thread.");
                num_threads_for_dispatcher = 1;
            }
            // Consider capping or fixing thread count for server stability, e.g., num_threads_for_dispatcher = std::min(num_threads_for_dispatcher, 4u);

            m_dispatcher = physx::PxDefaultCpuDispatcherCreate(num_threads_for_dispatcher);
            if (!m_dispatcher) {
                RF_PHYSICS_CRITICAL("PhysicsEngine: PxDefaultCpuDispatcherCreate failed!");
                if (m_cudaContextManager) { m_cudaContextManager->release(); m_cudaContextManager = nullptr; }
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
            scene_desc.gravity = ToPxVec3(gravityVec); // Using your conversion utility
            scene_desc.cpuDispatcher = m_dispatcher;
            scene_desc.filterShader = physx::PxDefaultSimulationFilterShader; // Or your custom one
            // scene_desc.simulationEventCallback = &m_yourSimulationEventCallbackInstance; // TODO
            // scene_desc.contactModifyCallback = &m_yourContactModifyCallbackInstance; // TODO

            // Configure for GPU acceleration if CudaContextManager is valid
            if (m_cudaContextManager && m_cudaContextManager->contextIsValid()) {
                scene_desc.cudaContextManager = m_cudaContextManager;
                scene_desc.flags |= physx::PxSceneFlag::eENABLE_GPU_DYNAMICS;
                scene_desc.broadPhaseType = physx::PxBroadPhaseType::eGPU;
                // Optional: For PhysX 5, TGS solver is often recommended for GPU scenes
                // scene_desc.solverType = physx::PxSolverType::eTGS; 
                RF_PHYSICS_INFO("PhysicsEngine: PxSceneDesc configured for GPU acceleration.");
            }
            else {
                scene_desc.broadPhaseType = physx::PxBroadPhaseType::eSAP; // Default CPU broadphase
                RF_PHYSICS_INFO("PhysicsEngine: PxSceneDesc configured for CPU-only (GPU not available/enabled).");
            }

            m_scene = m_physics->createScene(scene_desc);
            if (!m_scene) {
                RF_PHYSICS_CRITICAL("PhysicsEngine: m_physics->createScene failed!");
                if (m_dispatcher) { m_dispatcher->release(); m_dispatcher = nullptr; }
                if (m_cudaContextManager) { m_cudaContextManager->release(); m_cudaContextManager = nullptr; }
                PxCloseExtensions();
                if (m_physics) { m_physics->release(); m_physics = nullptr; }
                if (m_pvd_transport) { m_pvd_transport->release(); m_pvd_transport = nullptr; }
                if (m_pvd) { m_pvd->release(); m_pvd = nullptr; }
                if (m_foundation) { m_foundation->release(); m_foundation = nullptr; }
                return false;
            }
            RF_PHYSICS_INFO("PhysicsEngine: PxScene created. Gravity: ({}, {}, {})", gravityVec.x(), gravityVec.y(), gravityVec.z());

            // Configure scene PVD flags
            if (m_pvd && m_scene->getScenePvdClient()) {
                m_scene->getScenePvdClient()->setScenePvdFlags(
                    physx::PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS |
                    physx::PxPvdSceneFlag::eTRANSMIT_CONTACTS |
                    physx::PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES);
            }

            // 7. Default Material
            m_default_material = m_physics->createMaterial(0.5f, 0.5f, 0.1f);
            if (!m_default_material) {
                RF_PHYSICS_CRITICAL("PhysicsEngine: m_physics->createMaterial for default material failed!");
                // Your existing detailed cleanup for this case...
                if (m_scene) { m_scene->release(); m_scene = nullptr; }
                if (m_dispatcher) { m_dispatcher->release(); m_dispatcher = nullptr; }
                if (m_cudaContextManager) { m_cudaContextManager->release(); m_cudaContextManager = nullptr; }
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
                // Your existing detailed cleanup for this case...
                if (m_default_material) { m_default_material->release(); m_default_material = nullptr; }
                if (m_scene) { m_scene->release(); m_scene = nullptr; }
                if (m_dispatcher) { m_dispatcher->release(); m_dispatcher = nullptr; }
                if (m_cudaContextManager) { m_cudaContextManager->release(); m_cudaContextManager = nullptr; }
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

            // Acquire the main physics mutex to ensure no operations are ongoing during shutdown
            // std::lock_guard<std::mutex> lock(m_physicsMutex); // If you have operations in other threads using these objects directly.
                                                             // For a typical single-threaded StepSimulation and controlled API, this might
                                                             // not be strictly needed if Shutdown is called from the main physics thread
                                                             // after ensuring no more steps are called.

            if (m_controller_manager) {
                m_controller_manager->release();
                m_controller_manager = nullptr;
                RF_PHYSICS_INFO("PhysicsEngine: PxControllerManager released.");
            }

            if (m_default_material) {
                m_default_material->release();
                m_default_material = nullptr;
                RF_PHYSICS_INFO("PhysicsEngine: Default PxMaterial released.");
            }

            if (m_scene) {
                m_scene->release(); // Releases actors, etc., within the scene
                m_scene = nullptr;
                RF_PHYSICS_INFO("PhysicsEngine: PxScene released.");
            }

            if (m_dispatcher) { // CPU Dispatcher
                m_dispatcher->release();
                m_dispatcher = nullptr;
                RF_PHYSICS_INFO("PhysicsEngine: PxDefaultCpuDispatcher released.");
            }

            // Release CUDA Context Manager (GPU related)
            if (m_cudaContextManager) {
                m_cudaContextManager->release();
                m_cudaContextManager = nullptr;
                RF_PHYSICS_INFO("PhysicsEngine: PxCudaContextManager released.");
            }

            // Note: PxCloseExtensions should be called *before* m_physics is released if PxInitExtensions was called.
            // However, some samples show it called after physics release or not at all if PxPhysics manages everything.
            // According to PhysX 4.1 docs, "PxCloseExtensions should be called before releasing the PxPhysics object."
            // For PhysX 5, extensions are often more integrated. Let's assume PxCloseExtensions is still good practice.
            PxCloseExtensions();
            RF_PHYSICS_INFO("PhysicsEngine: PxExtensions closed.");


            if (m_physics) {
                m_physics->release();
                m_physics = nullptr;
                RF_PHYSICS_INFO("PhysicsEngine: PxPhysics released.");
            }

            if (m_pvd) {
                if (m_pvd_transport) { // Ensure transport is released before PVD if PVD owns it or connected
                    m_pvd_transport->release();
                    m_pvd_transport = nullptr;
                }
                m_pvd->release();
                m_pvd = nullptr;
                RF_PHYSICS_INFO("PhysicsEngine: PVD released.");
            }
            else if (m_pvd_transport) {
                // If PVD wasn't created but transport was (shouldn't happen with current init logic but good for safety)
                m_pvd_transport->release();
                m_pvd_transport = nullptr;
            }


            if (m_foundation) {
                m_foundation->release();
                m_foundation = nullptr;
                RF_PHYSICS_INFO("PhysicsEngine: PxFoundation released.");
            }

            RF_PHYSICS_INFO("PhysicsEngine: Shutdown complete.");
        }

        void PhysicsEngine::StepSimulation(float delta_time_sec) {
            // Early exit for invalid delta_time_sec, m_scene is checked after lock
            if (delta_time_sec <= 0.0f) {
                // RF_PHYSICS_TRACE("PhysicsEngine::StepSimulation: delta_time_sec is zero or negative ({:.4f}s). Skipping.", delta_time_sec);
                return;
            }

            std::lock_guard<std::mutex> lock(m_physicsMutex); // <<<< LOCK ADDED HERE

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
			std::lock_guard<std::mutex> lock(m_physicsMutex); // Ensure thread-safe access to m_physics
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
            physx::PxController* controller = GetPlayerController(player_id); // This handles m_playerControllersMutex internally for the map read

            if (!controller) {
                RF_PHYSICS_WARN("PhysicsEngine::SetCharacterControllerOrientation: Controller not found for player ID {}. Orientation not set.", player_id);
                return false;
            }

            // Now, lock the main physics mutex for operations on the actor
            std::lock_guard<std::mutex> lock(m_physicsMutex); // <<<< LOCK ADDED HERE

            physx::PxRigidActor* actor = controller->getActor(); // This call itself is safe once controller is valid
            if (!actor) { // Actor might be null if controller is not properly associated or is being released
                RF_PHYSICS_ERROR("PhysicsEngine::SetCharacterControllerOrientation: Could not get PxActor for controller of player ID {}.", player_id);
                return false; // Exits scope, m_physicsMutex is released
            }

            // Check if actor is still in a scene. This is a defensive check.
            // The main protection comes from m_physicsMutex ensuring exclusive access during this operation.
            if (!actor->getScene()) {
                RF_PHYSICS_WARN("PhysicsEngine::SetCharacterControllerOrientation: Actor for player ID {} is not in a scene. Orientation not set.", player_id);
                return false; // Exits scope, m_physicsMutex is released
            }

            physx::PxTransform currentPose = actor->getGlobalPose(); // PhysX read, protected by m_physicsMutex
            currentPose.q = ToPxQuat(orientation);
            actor->setGlobalPose(currentPose); // PhysX write, protected by m_physicsMutex

            // RF_PHYSICS_DEBUG("PhysicsEngine: Set orientation for player ID {}.", player_id);
            return true;
            // m_physicsMutex is released automatically when 'lock' goes out of scope
        }

        void PhysicsEngine::UnregisterPlayerController(uint64_t player_id) {
            physx::PxController* controller_to_release = nullptr;

            // Scope for m_playerControllersMutex to find and remove from map
            {
                std::lock_guard<std::mutex> mapLock(m_playerControllersMutex);
                auto it = m_playerControllers.find(player_id);
                if (it != m_playerControllers.end()) {
                    controller_to_release = it->second;
                    m_playerControllers.erase(it);
                    // Important: controller_to_release pointer is now saved, 
                    // map operation is done. Map lock will be released.
                }
            } // mapLock released here

            if (controller_to_release) {
                std::lock_guard<std::mutex> physicsLock(m_physicsMutex); // <<<< LOCK ADDED for PhysX operation
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
            if (!actor) {
                return;
            }

            std::lock_guard<std::mutex> lock(m_physicsMutex); // <<<< LOCK ADDED HERE
            // Assuming the actor might be in the scene

        // It's good practice to also check if the actor is still in a scene,
        // though the mutex protects against concurrent scene modification.
        if (actor->getScene()) { // Optional: more defensive check
            actor->userData = userData;
        } else {
            RF_PHYSICS_WARN("PhysicsEngine::SetActorUserData: Actor (0x%p) is not in a scene. UserData not set.", static_cast<void*>(actor));
        }
            actor->userData = userData; // Directly set after acquiring lock
        }

        // --- Generic Rigid Actor Management ---
        void PhysicsEngine::RegisterRigidActor(uint64_t entity_id, physx::PxRigidActor* actor) {
            if (!actor) {
                RF_PHYSICS_ERROR("PhysicsEngine::RegisterRigidActor: Attempted to register null actor for entity ID {}.", entity_id);
                return;
            }

            std::lock_guard<std::mutex> physicsLock(m_physicsMutex); // <<<< LOCK ADDED HERE for overall physics operation

            if (!m_scene) { // Check m_scene after acquiring physicsLock
                RF_PHYSICS_ERROR("PhysicsEngine::RegisterRigidActor: Scene not initialized. Cannot add actor for entity ID {}.", entity_id);
                // Since m_physicsMutex is held, it's safe to call actor->release() if it's a standalone PhysX call
                // However, actor->release() itself might have thread safety considerations if m_physics is involved.
                // For simplicity, this path assumes release is okay or actor isn't part of broader system yet.
                actor->release();
                return; // physicsLock is released
            }

            // SetActorUserData is now called while m_physicsMutex is held.
            // If SetActorUserData also takes m_physicsMutex, this would be a recursive lock,
            // which is okay if using std::recursive_mutex, but deadlock with std::mutex.
            // Better: SetActorUserData should be a private helper that assumes caller holds the lock,
            // or it doesn't take m_physicsMutex if actor is not yet in scene.
            // For now, assuming our modified SetActorUserData takes the lock, we need to be careful.
            // Let's make a "raw" version or ensure SetActorUserData is safe if physicsLock is already held.
            // Simpler: just set it directly here.
            actor->userData = reinterpret_cast<void*>(entity_id); // Store entity_id, protected by physicsLock

            {
                std::lock_guard<std::mutex> mapLock(m_entityActorsMutex); // Nested lock for map operation
                if (m_entityActors.count(entity_id)) {
                    RF_PHYSICS_WARN("PhysicsEngine::RegisterRigidActor: Entity ID {} already has a registered actor. Overwriting. Old actor might be leaked if not properly released.", entity_id);
                    // Consider releasing the old actor if it exists:
                    // physx::PxRigidActor* oldActor = m_entityActors[entity_id];
                    // if (oldActor && oldActor->getScene()) { m_scene->removeActor(*oldActor); }
                    // if (oldActor) { oldActor->release(); }
                }
                m_entityActors[entity_id] = actor;
            } // mapLock released

            m_scene->addActor(*actor); // PhysX scene modification, protected by physicsLock
            RF_PHYSICS_INFO("PhysicsEngine::RegisterRigidActor: Registered PxRigidActor and added to scene for entity ID {}.", entity_id);
            // physicsLock released
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
				std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <<<< LOCK ADDED HERE for PhysX operation
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
               
            physx::PxTransform initial_pose(ToPxVec3(start_pos), ToPxQuat(orientation)); //
            physx::PxCapsuleGeometry capsule_geometry(radius, half_height);
            physx::PxVec3 sweep_direction_px = ToPxVec3(unit_direction); //

            physx::PxSweepBuffer sweep_buffer; // For first blocking hit

            // Use the provided filter_callback_override if available, otherwise use our RiftStep specific one.
            RiftStepSweepQueryFilterCallback default_riftstep_filter(actor_to_ignore);
            physx::PxQueryFilterCallback* actual_filter_callback = filter_callback_override ? filter_callback_override : &default_riftstep_filter;

            // Hit flags: ePOSITION and eNORMAL are useful. eMESH_BOTH_SIDES if terrain might be single-sided.
            // eASSUME_NO_INITIAL_OVERLAP is an optimization if you're sure the capsule isn't already stuck.
            physx::PxHitFlags hit_flags = physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL | physx::PxHitFlag::eMESH_BOTH_SIDES | physx::PxHitFlag::eASSUME_NO_INITIAL_OVERLAP;

            std::lock_guard<std::mutex> physics_lock(m_physicsMutex);

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
            if (!m_physics || !m_scene) { // Added m_scene check, as controller manager operates on a scene
                RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: Physics system or scene not initialized for player ID {}.", player_id);
                return nullptr;
            }
            // m_controller_manager is typically created with m_scene. If m_scene is null, m_controller_manager might be too or invalid.
            if (!m_controller_manager) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: Controller manager not initialized for player ID {}.", player_id);
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
            if (!desc.material) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: No valid PxMaterial available for player ID {}.", player_id);
                return nullptr;
            }
            desc.stepOffset = 0.5f;
            float slope_limit_degrees = 45.0f;
            desc.slopeLimit = std::cos(slope_limit_degrees * (physx::PxPi / 180.0f));
            desc.contactOffset = 0.05f;
            desc.upDirection = physx::PxVec3(0.0f, 0.0f, 1.0f); // Assuming Z-up
            // desc.reportCallback = your_hit_report_callback_instance;
            // desc.behaviorCallback = your_behavior_callback_instance;

            if (!desc.isValid()) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: PxCapsuleControllerDesc is invalid for player ID {}. Radius: {}, Height: {}", player_id, radius, height);
                return nullptr;
            }

            physx::PxController* controller = nullptr;
            { // Scope for the physics lock
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for PhysX operations

                controller = m_controller_manager->createController(desc);
                if (controller) {
                    physx::PxRigidActor* controller_actor = controller->getActor();
                    if (controller_actor) {
                        // Assuming SetActorUserData is a lightweight method, e.g., actor->userData = ...
                        // If SetActorUserData itself tries to lock m_physicsMutex non-recursively,
                        // this would need adjustment. Given its typical role, it's likely safe.
                        SetActorUserData(controller_actor, user_data_for_controller_actor ? user_data_for_controller_actor : reinterpret_cast<void*>(player_id));
                    }
                }
            } // m_physicsMutex is released here

            if (controller) {
                // RegisterPlayerController should handle its own locking (e.g., m_playerControllersMutex)
                RegisterPlayerController(player_id, controller);
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
            const std::vector<physx::PxController*>& other_controllers_to_ignore
        ) {
            if (!controller) {
                RF_PHYSICS_ERROR("PhysicsEngine::MoveCharacterController: Null controller passed.");
                return 0;
            }
            if (delta_time_sec <= 0.0f) {
                return 0;
            }

            // Prepare PxVec3 outside the lock
            physx::PxVec3 disp = ToPxVec3(world_space_displacement);

            // TODO: Setup for PxControllerFilters if other_controllers_to_ignore is used.
            // This setup logic should ideally also be outside the main physics lock if it doesn't query the scene,
            // or inside if it needs to inspect properties of 'other_controllers_to_ignore' that require scene access.
            // For now, assuming default filters are fine or filter setup is quick.
            physx::PxControllerFilters filters;
            // If setting up filters involves accessing properties of other_controllers_to_ignore
            // that are themselves physics objects (e.g. their actors, shapes), that part might also need protection
            // or careful handling. For this example, keeping filter setup simple.

            physx::PxControllerCollisionFlags collision_flags;
            {
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for PhysX operations
                collision_flags = controller->move(disp, 0.001f, delta_time_sec, filters, nullptr);
            }

            return collision_flags;
        }

        void PhysicsEngine::SetCharacterControllerPose(physx::PxController* controller, const SharedVec3& world_position) {
            if (!controller) {
                RF_PHYSICS_ERROR("PhysicsEngine::SetCharacterControllerPose: Null controller passed.");
                return;
            }

            // Prepare PxExtendedVec3 outside the lock
            physx::PxExtendedVec3 pos = physx::PxExtendedVec3(world_position.x(), world_position.y(), world_position.z());
            bool success = false;
            {
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for PhysX operations
                success = controller->setPosition(pos);
            }

            if (!success) {
                RF_PHYSICS_WARN("PhysicsEngine::SetCharacterControllerPose: controller->setPosition failed. Position may be invalid or obstructed.");
            }
        }

        SharedVec3 PhysicsEngine::GetCharacterControllerPosition(physx::PxController* controller) const {
            if (!controller) {
                RF_PHYSICS_ERROR("PhysicsEngine::GetCharacterControllerPosition: Null controller passed. Returning zero vector.");
                // Consider if throwing an exception or returning an optional would be better for error handling.
                return SharedVec3(0.0f, 0.0f, 0.0f);
            }

            physx::PxExtendedVec3 pos;
            {
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for PhysX operations
                pos = controller->getPosition();
            }

            return SharedVec3(static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z));
        }

        // --- Static Rigid Actor Creation ---
        physx::PxRigidStatic* PhysicsEngine::CreateStaticPlane(
            const SharedVec3& normal, float distance,
            physx::PxMaterial* material, const CollisionFilterData& filter_data
        ) {
            if (!m_physics || !m_scene) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticPlane: Physics system or scene not initialized.");
                return nullptr;
            }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticPlane: No valid material.");
                return nullptr;
            }

            physx::PxRigidStatic* plane_actor = nullptr;
            { // Scope for the physics lock
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for PhysX operations

                plane_actor = physx::PxCreatePlane(*m_physics, physx::PxPlane(ToPxVec3(normal), distance), *mat);
                if (!plane_actor) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticPlane: PxCreatePlane failed.");
                    return nullptr; // Exits locked scope
                }

                // Setup filtering if a shape exists (PxCreatePlane creates one implicitly)
                // Assuming GetNumShapes and GetShapes are safe to call on a newly created actor before adding to scene.
                physx::PxShape* shape_ptr[1]; // Max 1 shape for a plane
                if (plane_actor->getNbShapes() == 1 && plane_actor->getShapes(shape_ptr, 1) == 1) {
                    // SetupShapeFiltering is a member function, assumed to make PhysX calls (e.g., shape->setFilterData)
                    SetupShapeFiltering(shape_ptr[0], filter_data);
                }
                else {
                    RF_PHYSICS_WARN("PhysicsEngine::CreateStaticPlane: Could not retrieve shape from plane actor for filtering.");
                    // Continue, but filtering might not be applied as expected.
                }

                m_scene->addActor(*plane_actor);
            } // m_physicsMutex is released here

            RF_PHYSICS_INFO("PhysicsEngine: Static plane created and added to scene.");
            return plane_actor;
        }

        physx::PxRigidStatic* PhysicsEngine::CreateStaticBox(
            uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation,
            const SharedVec3& half_extents, physx::PxMaterial* material,
            const CollisionFilterData& filter_data, void* user_data
        ) {
            if (!m_physics || !m_scene) { // Added m_scene check
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticBox: Physics system or scene not initialized.");
                return nullptr;
            }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticBox: No valid material.");
                return nullptr;
            }

            // Prepare geometry and pose outside the lock if they don't involve PhysX API calls yet
            physx::PxBoxGeometry geometry(ToPxVec3(half_extents));
            physx::PxTransform pose(ToPxVec3(position), ToPxQuat(orientation));
            physx::PxRigidStatic* actor = nullptr;

            { // Scope for the physics lock
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for PhysX operations

                actor = m_physics->createRigidStatic(pose);
                if (!actor) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticBox: createRigidStatic failed.");
                    return nullptr; // Exits locked scope
                }

                // Create shape with isExclusive = true so it can have per-shape filter data
                physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
                if (!shape) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticBox: createShape failed.");
                    actor->release(); // Release the created actor before returning
                    return nullptr;   // Exits locked scope
                }

                // SetupShapeFiltering and SetActorUserData are member functions,
                // assumed to make PhysX calls and thus need to be within the lock.
                SetupShapeFiltering(shape, filter_data);
                actor->attachShape(*shape);
                // The shape is now owned by the actor. Release our reference to it.
                shape->release();

                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));

                m_scene->addActor(*actor); // Explicitly add to scene under this lock
            } // m_physicsMutex is released here

            // RegisterRigidActor now only updates the map (m_entityActors with m_entityActorsMutex)
            // This happens after m_physicsMutex is released.
            if (actor) { // Check if actor creation was successful overall
                RegisterRigidActor(entity_id, actor);
                RF_PHYSICS_INFO("PhysicsEngine: Static box for entity ID {} created and registered.", entity_id);
            }
            return actor;
        }

        physx::PxRigidStatic* PhysicsEngine::CreateStaticSphere(
            uint64_t entity_id, const SharedVec3& position, float radius,
            physx::PxMaterial* material, const CollisionFilterData& filter_data, void* user_data
        ) {
            if (!m_physics || !m_scene) { // Added m_scene check
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticSphere: Physics system or scene not initialized.");
                return nullptr;
            }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticSphere: No valid material.");
                return nullptr;
            }

            physx::PxSphereGeometry geometry(radius);
            physx::PxTransform pose(ToPxVec3(position));
            physx::PxRigidStatic* actor = nullptr;

            { // Scope for the physics lock
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for PhysX operations

                actor = m_physics->createRigidStatic(pose);
                if (!actor) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticSphere: createRigidStatic failed.");
                    return nullptr;
                }

                physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
                if (!shape) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticSphere: createShape failed.");
                    actor->release();
                    return nullptr;
                }

                SetupShapeFiltering(shape, filter_data);
                actor->attachShape(*shape);
                shape->release();

                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));

                m_scene->addActor(*actor); // Explicitly add to scene
            } // m_physicsMutex is released here

            if (actor) {
                RegisterRigidActor(entity_id, actor);
                RF_PHYSICS_INFO("PhysicsEngine: Static sphere for entity ID {} created and registered.", entity_id);
            }
            return actor;
        }

        physx::PxRigidStatic* PhysicsEngine::CreateStaticCapsule(
            uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation,
            float radius, float half_height, physx::PxMaterial* material,
            const CollisionFilterData& filter_data, void* user_data
        ) {
            if (!m_physics || !m_scene) { // Added m_scene check
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticCapsule: Physics system or scene not initialized.");
                return nullptr;
            }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticCapsule: No valid material.");
                return nullptr;
            }

            physx::PxCapsuleGeometry geometry(radius, half_height);
            physx::PxTransform pose(ToPxVec3(position), ToPxQuat(orientation));
            // Optional: Adjust pose for capsule alignment if needed, as per your original comment
            // physx::PxQuat local_rot(physx::PxHalfPi, physx::PxVec3(0,0,1)); // Example for Z-up capsule from Y-up default
            // pose = pose * physx::PxTransform(local_rot);

            physx::PxRigidStatic* actor = nullptr;

            { // Scope for the physics lock
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for PhysX operations

                actor = m_physics->createRigidStatic(pose);
                if (!actor) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticCapsule: createRigidStatic failed.");
                    return nullptr;
                }

                physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
                if (!shape) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticCapsule: createShape failed.");
                    actor->release();
                    return nullptr;
                }

                SetupShapeFiltering(shape, filter_data);
                actor->attachShape(*shape);
                shape->release();

                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));

                m_scene->addActor(*actor); // Explicitly add to scene
            } // m_physicsMutex is released here

            if (actor) {
                RegisterRigidActor(entity_id, actor);
                RF_PHYSICS_INFO("PhysicsEngine: Static capsule for entity ID {} created and registered.", entity_id);
            }
            return actor;
        }

        // --- Dynamic Rigid Actor Creation ---
        physx::PxRigidDynamic* PhysicsEngine::CreateDynamicSphere(
            uint64_t entity_id, const SharedVec3& position, float radius, float density,
            physx::PxMaterial* material, const CollisionFilterData& filter_data, void* user_data
        ) {
            if (!m_physics || !m_scene) { // Added m_scene check
                RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicSphere: Physics system or scene not initialized.");
                return nullptr;
            }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicSphere: No valid material.");
                return nullptr;
            }

            physx::PxSphereGeometry geometry(radius);
            physx::PxTransform pose(ToPxVec3(position));
            physx::PxRigidDynamic* actor = nullptr;

            { // Scope for the physics lock
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for PhysX operations

                actor = m_physics->createRigidDynamic(pose);
                if (!actor) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicSphere: createRigidDynamic failed.");
                    return nullptr; // Exits locked scope
                }

                physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
                if (!shape) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicSphere: createShape failed.");
                    actor->release(); // Release the created actor
                    return nullptr;   // Exits locked scope
                }

                SetupShapeFiltering(shape, filter_data); // Assuming this makes PhysX calls
                actor->attachShape(*shape);
                shape->release(); // Shape is now owned by actor

                if (density > 0.0f) {
                    physx::PxRigidBodyExt::updateMassAndInertia(*actor, density);
                }
                else {
                    actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
                    // RF_PHYSICS_WARN is logged outside the lock if possible, but decision here depends on actor state
                    // For simplicity here, this log can stay, or be moved if actor ptr is checked outside.
                }

                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id)); // Assuming this makes PhysX calls or modifies PhysX object state

                m_scene->addActor(*actor); // Explicitly add to scene under this lock
            } // m_physicsMutex is released here

            if (actor) {
                if (density <= 0.0f) { // Moved log here to be outside lock, if preferred
                    RF_PHYSICS_WARN("PhysicsEngine::CreateDynamicSphere: Entity ID {} created with density <= 0. Set to kinematic.", entity_id);
                }
                // RegisterRigidActor now only updates the map (m_entityActors with m_entityActorsMutex)
                RegisterRigidActor(entity_id, actor);
                RF_PHYSICS_INFO("PhysicsEngine: Dynamic sphere for entity ID {} created and registered.", entity_id);
            }
            return actor;
        }

        physx::PxRigidDynamic* PhysicsEngine::CreateDynamicBox(
            uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation,
            const SharedVec3& half_extents, float density, physx::PxMaterial* material,
            const CollisionFilterData& filter_data, void* user_data
        ) {
            if (!m_physics || !m_scene) { // Added m_scene check
                RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicBox: Physics system or scene not initialized.");
                return nullptr;
            }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicBox: No valid material.");
                return nullptr;
            }

            physx::PxBoxGeometry geometry(ToPxVec3(half_extents));
            physx::PxTransform pose(ToPxVec3(position), ToPxQuat(orientation));
            physx::PxRigidDynamic* actor = nullptr;

            { // Scope for the physics lock
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for PhysX operations

                actor = m_physics->createRigidDynamic(pose);
                if (!actor) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicBox: createRigidDynamic failed.");
                    return nullptr;
                }

                physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
                if (!shape) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicBox: createShape failed.");
                    actor->release();
                    return nullptr;
                }

                SetupShapeFiltering(shape, filter_data);
                actor->attachShape(*shape);
                shape->release();

                if (density > 0.0f) {
                    physx::PxRigidBodyExt::updateMassAndInertia(*actor, density);
                }
                else {
                    actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
                }

                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));

                m_scene->addActor(*actor); // Explicitly add to scene
            } // m_physicsMutex is released here

            if (actor) {
                if (density <= 0.0f) {
                    RF_PHYSICS_WARN("PhysicsEngine::CreateDynamicBox: Entity ID {} created with density <= 0. Set to kinematic.", entity_id);
                }
                RegisterRigidActor(entity_id, actor);
                RF_PHYSICS_INFO("PhysicsEngine: Dynamic box for entity ID {} created and registered.", entity_id);
            }
            return actor;
        }

        physx::PxRigidDynamic* PhysicsEngine::CreateDynamicCapsule(
            uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation,
            float radius, float half_height, float density, physx::PxMaterial* material,
            const CollisionFilterData& filter_data, void* user_data
        ) {
            if (!m_physics || !m_scene) { // Added m_scene check
                RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicCapsule: Physics system or scene not initialized.");
                return nullptr;
            }
            physx::PxMaterial* mat = material ? material : m_default_material;
            if (!mat) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicCapsule: No valid material.");
                return nullptr;
            }

            physx::PxCapsuleGeometry geometry(radius, half_height);
            physx::PxTransform pose(ToPxVec3(position), ToPxQuat(orientation));
            physx::PxRigidDynamic* actor = nullptr;

            { // Scope for the physics lock
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for PhysX operations

                actor = m_physics->createRigidDynamic(pose);
                if (!actor) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicCapsule: createRigidDynamic failed.");
                    return nullptr;
                }

                physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
                if (!shape) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicCapsule: createShape failed.");
                    actor->release();
                    return nullptr;
                }

                SetupShapeFiltering(shape, filter_data);
                actor->attachShape(*shape);
                shape->release();

                if (density > 0.0f) {
                    physx::PxRigidBodyExt::updateMassAndInertia(*actor, density);
                }
                else {
                    actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
                }

                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));

                m_scene->addActor(*actor); // Explicitly add to scene
            } // m_physicsMutex is released here

            if (actor) {
                if (density <= 0.0f) {
                    RF_PHYSICS_WARN("PhysicsEngine::CreateDynamicCapsule: Entity ID {} created with density <= 0. Set to kinematic.", entity_id);
                }
                RegisterRigidActor(entity_id, actor);
                RF_PHYSICS_INFO("PhysicsEngine: Dynamic capsule for entity ID {} created and registered.", entity_id);
            }
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
            void* user_data
        ) {
            // 1. Initial Checks & Prerequisites
            if (!m_physics || !m_scene) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: PxPhysics or PxScene not initialized for entity ID {}.", entity_id);
                return nullptr;
            }
            if (vertices.empty() || indices.empty() || (indices.size() % 3 != 0)) {
                RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: Invalid vertex or index data for entity ID {}.", entity_id);
                return nullptr;
            }

            // 2. Vertex Data Conversion
            std::vector<physx::PxVec3> px_vertices(vertices.size());
            for (size_t i = 0; i < vertices.size(); ++i) {
                px_vertices[i] = ToPxVec3(vertices[i]);
            }

            // 3. Triangle Mesh Description
            physx::PxTriangleMeshDesc mesh_desc;
            mesh_desc.points.count = static_cast<physx::PxU32>(px_vertices.size());
            mesh_desc.points.stride = sizeof(physx::PxVec3);
            mesh_desc.points.data = px_vertices.data();

            mesh_desc.triangles.count = static_cast<physx::PxU32>(indices.size() / 3);
            mesh_desc.triangles.stride = 3 * sizeof(uint32_t);
            mesh_desc.triangles.data = indices.data();

            // Variables to hold PhysX objects; declared outside lock to be returnable/manageable
            physx::PxTriangleMesh* triangle_mesh = nullptr;
            physx::PxRigidStatic* actor = nullptr;

            { // Scope for the physics lock
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for all PhysX operations

                // 4. Direct Cooking and PxTriangleMesh Creation
                physx::PxCookingParams params(m_physics->getTolerancesScale());
                // Configure params if needed

                physx::PxTriangleMeshCookingResult::Enum cooking_result_enum; // Renamed to avoid conflict
                triangle_mesh = PxCreateTriangleMesh(params, mesh_desc, m_physics->getPhysicsInsertionCallback(), &cooking_result_enum);

                if (!triangle_mesh) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: PxCreateTriangleMesh (C-API) failed for entity ID {}. Result: {}",
                        entity_id, static_cast<int>(cooking_result_enum));
                    return nullptr; // Exits locked scope
                }
                if (cooking_result_enum != physx::PxTriangleMeshCookingResult::eSUCCESS) {
                    RF_PHYSICS_WARN("PhysicsEngine::CreateStaticTriangleMesh: PxCreateTriangleMesh for entity ID {} completed with result: {}.",
                        entity_id, static_cast<int>(cooking_result_enum));
                    if (cooking_result_enum == physx::PxTriangleMeshCookingResult::eLARGE_TRIANGLE || cooking_result_enum == physx::PxTriangleMeshCookingResult::eEMPTY_MESH) {
                        triangle_mesh->release(); // Release the potentially problematic mesh
                        RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: Cooking failed critically for entity ID {}.", entity_id);
                        return nullptr; // Exits locked scope
                    }
                }

                // 5. Setting up Geometry and Scale
                physx::PxMeshScale mesh_scale(ToPxVec3(scale_vec), physx::PxQuat(physx::PxIdentity));
                physx::PxTriangleMeshGeometry geometry(triangle_mesh, mesh_scale);

                // 6. Material
                physx::PxMaterial* mat_to_use = material ? material : m_default_material;
                if (!mat_to_use) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: No material available for entity ID {}.", entity_id);
                    triangle_mesh->release(); // Release cooked mesh
                    return nullptr;           // Exits locked scope
                }

                // Actor Creation
                actor = m_physics->createRigidStatic(physx::PxTransform(physx::PxIdentity)); // Triangle meshes are typically static and positioned by vertices
                if (!actor) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: m_physics->createRigidStatic failed for entity ID {}.", entity_id);
                    triangle_mesh->release(); // Release cooked mesh
                    return nullptr;           // Exits locked scope
                }

                // 7. Shape Creation and Setup
                physx::PxShape* shape = physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *mat_to_use);
                if (!shape) {
                    RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: PxRigidActorExt::createExclusiveShape failed for entity ID {}.", entity_id);
                    actor->release();         // Release actor
                    triangle_mesh->release(); // Release cooked mesh
                    return nullptr;           // Exits locked scope
                }

                SetupShapeFiltering(shape, filter_data); // Assuming this makes PhysX calls

                // 8. Resource Management for triangle_mesh
                triangle_mesh->release(); // Mesh data is now owned by the PxTriangleMeshGeometry/shape.
                // Set to null to avoid double release if error occurs later before returning actor.
                triangle_mesh = nullptr;

                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id)); // Assuming this modifies PhysX object

                m_scene->addActor(*actor); // Explicitly add to scene under this lock

            } // m_physicsMutex is released here

            // Final registration and logging if actor was successfully created and added
            if (actor) {
                // RegisterRigidActor now only updates the map (m_entityActors with m_entityActorsMutex)
                RegisterRigidActor(entity_id, actor);
                RF_PHYSICS_INFO("PhysicsEngine: Static triangle mesh for entity ID {} created and registered.", entity_id);
            }
            // If triangle_mesh was not set to nullptr after release and an error happened *after* the lock
            // but before returning (e.g. in RegisterRigidActor or logging), it might be double-released.
            // However, actor would be null if major errors occurred in lock, so this path might not be hit.
            // Best to ensure triangle_mesh is null after its release if it's not returned.

            return actor;
        }

        // --- Scene Queries ---
        bool PhysicsEngine::RaycastSingle(
            const SharedVec3& start,
            const SharedVec3& unit_direction,
            float max_distance,
            HitResult& out_hit,
            const physx::PxQueryFilterData& filter_data_from_caller,
            physx::PxQueryFilterCallback* filter_callback
        ) {
            if (!m_scene) {
                RF_PHYSICS_ERROR("PhysicsEngine::RaycastSingle: Scene not initialized.");
                return false;
            }
            if (max_distance <= 0.0f) { // Or handle as error/warning
                return false;
            }

            physx::PxVec3 px_origin = ToPxVec3(start);
            physx::PxVec3 px_unit_dir = ToPxVec3(unit_direction);
            physx::PxQueryFilterData query_filter_data = filter_data_from_caller; // Use the provided filter data

            // Ensure pre/post filter flags are set if a callback is provided.
            // (This logic could be here or responsibility of the caller)
            if (filter_callback != nullptr) {
                if (!(query_filter_data.flags & physx::PxQueryFlag::ePREFILTER)) {
                    // RF_PHYSICS_WARN("RaycastSingle: filter_callback provided but ePREFILTER not set. Adding it.");
                    // query_filter_data.flags |= physx::PxQueryFlag::ePREFILTER;
                }
                // Similarly for ePOSTFILTER if your callback uses it.
            }

            physx::PxHitFlags hit_flags = physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL | physx::PxHitFlag::eFACE_INDEX;
            physx::PxRaycastBuffer hit_buffer; // For single closest hit
            bool did_hit = false;

            { // Scope for the physics lock
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for m_scene->raycast

                did_hit = m_scene->raycast(
                    px_origin,
                    px_unit_dir,
                    max_distance,
                    hit_buffer, // Output
                    hit_flags,
                    query_filter_data, // Filter data
                    filter_callback    // Filter callback
                );
            } // m_physicsMutex is released here

            if (did_hit && hit_buffer.hasBlock) {
                out_hit.hit_actor = hit_buffer.block.actor;
                out_hit.hit_shape = hit_buffer.block.shape;
                if (hit_buffer.block.actor && hit_buffer.block.actor->userData) {
                    out_hit.hit_entity_id = reinterpret_cast<uint64_t>(hit_buffer.block.actor->userData);
                }
                else {
                    out_hit.hit_entity_id = 0; // Default or indicate no ID
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
            uint32_t max_hits_param, // Renamed to avoid confusion with buffer capacity
            const physx::PxQueryFilterData& filter_data_from_caller,
            physx::PxQueryFilterCallback* filter_callback
        ) {
            std::vector<HitResult> results;
            if (!m_scene || max_hits_param == 0 || max_distance <= 0.0f) {
                // RF_PHYSICS_ERROR is fine, but this could also be a warning or simply return empty if it's a common case.
                // RF_PHYSICS_ERROR("PhysicsEngine::RaycastMultiple: Scene not initialized, max_hits_param is 0, or invalid max_distance.");
                return results;
            }

            physx::PxVec3 px_origin = ToPxVec3(start);
            physx::PxVec3 px_unit_dir = ToPxVec3(unit_direction);

            // This vector is currently unused with PxRaycastBufferN.
            // std::vector<physx::PxRaycastHit> hit_touches(max_hits_param); 

            // PxRaycastBufferN can store up to 16 hits on the stack.
            // If you need more, use PxRaycastBuffer with a heap-allocated buffer (e.g., using hit_touches).
            physx::PxRaycastBufferN<16> hit_buffer;

            physx::PxHitFlags hit_flags = physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL | physx::PxHitFlag::eFACE_INDEX;
            bool did_hit_anything = false;

            { // Scope for the physics lock
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for m_scene->raycast

                did_hit_anything = m_scene->raycast(
                    px_origin,
                    px_unit_dir,
                    max_distance,
                    hit_buffer, // Output buffer (PxRaycastBufferN<16> in this case)
                    hit_flags,
                    filter_data_from_caller,
                    filter_callback
                );
            } // m_physicsMutex is released here

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
            physx::PxQueryFilterCallback* filter_callback
        ) {
            std::vector<HitResult> results;
            if (!m_scene || max_hits_alloc == 0) {
                // RF_PHYSICS_ERROR("PhysicsEngine::OverlapMultiple: Scene not initialized or max_hits_alloc is 0.");
                return results;
            }

            // This vector provides the memory for PxOverlapBuffer.
            std::vector<physx::PxOverlapHit> touches_buffer_mem(max_hits_alloc);
            physx::PxOverlapBuffer hit_buffer(touches_buffer_mem.data(), max_hits_alloc);
            bool success = false; // overlap doesn't return bool, success is determined by nbHits > 0

            { // Scope for the physics lock
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for m_scene->overlap

                // m_scene->overlap returns void, results are in hit_buffer
                m_scene->overlap(geometry, pose, hit_buffer, filter_data_from_caller, filter_callback);
            } // m_physicsMutex is released here

            uint32_t nb_hits = hit_buffer.getNbAnyHits(); // Use getNbAnyHits for consistency if eANY_HIT is possible
            if (nb_hits > 0) {
                results.reserve(nb_hits);
                for (physx::PxU32 i = 0; i < nb_hits; ++i) {
                    const physx::PxOverlapHit& touch = hit_buffer.getAnyHit(i);
                    HitResult res;
                    res.hit_actor = touch.actor;
                    res.hit_shape = touch.shape;
                    if (touch.actor && touch.actor->userData) {
                        res.hit_entity_id = reinterpret_cast<uint64_t>(touch.actor->userData);
                    }
                    else {
                        res.hit_entity_id = 0;
                    }
                    res.distance = 0.0f; // Overlaps don't typically have a 'distance' like raycasts
                    res.hit_face_index = touch.faceIndex;
                    // For overlaps, hit_point and hit_normal are not provided in PxOverlapHit.
                    // You would need to calculate contact points if needed, e.g. via PxComputePenetration.
                    results.push_back(res);
                }
            }
            return results;
        }

        // --- Force & Effect Application ---
        void PhysicsEngine::ApplyForceToActor(physx::PxRigidBody* actor, const SharedVec3& force, physx::PxForceMode::Enum mode, bool wakeup) {
            if (!actor) {
                RF_PHYSICS_WARN("PhysicsEngine::ApplyForceToActor: Null actor passed.");
                return;
            }

            physx::PxVec3 px_force = ToPxVec3(force); // Prepare data outside lock

            { // Scope for the physics lock
                std::lock_guard<std::mutex> physics_lock(m_physicsMutex); // <-- Lock for actor modification

                // It's good practice to ensure the actor is still in a scene when modifying it.
                // addForce itself is often a no-op or safe if the actor isn't in a scene, but explicit check is clearer.
                if (actor->getScene()) { // Check scene membership while locked
                    actor->addForce(px_force, mode, wakeup);
                }
                else {
                    // Warning moved to outside lock, as actor pointer is checked initially
                    // RF_PHYSICS_WARN("PhysicsEngine::ApplyForceToActor: Actor is not in a scene. Force not applied.");
                    // This specific warning log can be outside if we decide not to proceed if not in scene.
                    // However, if addForce handles it gracefully, the lock is still for the potential modification.
                }
            } // m_physicsMutex is released here

            // Conditional warning if not in scene (can be logged if needed, actor ptr is stable)
            // if (!actor->getScene()) { // Re-checking outside lock is not thread-safe for getScene state
            //    RF_PHYSICS_WARN("PhysicsEngine::ApplyForceToActor: Actor was found to not be in a scene. Force might not have been applied as expected.");
            // }
            // For simplicity, the warning for "not in scene" can be omitted if PhysX handles addForce on non-scene actors benignly.
            // Or keep the check inside the lock if strictness is required.
        }

        void PhysicsEngine::ApplyForceToActorById(uint64_t entity_id, const SharedVec3& force, physx::PxForceMode::Enum mode, bool wakeup) {
            physx::PxRigidActor* actor_base = GetRigidActor(entity_id); // Internally locks m_entityActorsMutex

            if (!actor_base) {
                physx::PxController* controller = GetPlayerController(entity_id); // Internally locks m_playerControllersMutex
                if (controller) {
                    actor_base = controller->getActor(); // Reading actor from controller
                }
            }

            if (actor_base && actor_base->is<physx::PxRigidBody>()) {
                // ApplyForceToActor will internally lock m_physicsMutex
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
            // TODO for implementation:
            // If this function performs physics operations (e.g., overlap queries, applying forces):
            // - The overlap query (e.g., m_scene->overlap) must be protected by std::lock_guard<std::mutex>(m_physicsMutex).
            // - Applying forces to multiple actors:
            //   - Option A: Loop and call ApplyForceToActor (which locks m_physicsMutex per call).
            //   - Option B (more efficient): Lock m_physicsMutex once, perform overlap, then loop through results
            //     and directly call PxRigidBody::addForce on each actor. Release lock after all forces are applied.
            RF_PHYSICS_WARN("CreateRadialForceField is a conceptual stub and requires significant implementation including thread safety for physics operations.");
        }

        void PhysicsEngine::ApplyLocalizedGravity(const SharedVec3& center, float strength, float radius, float duration_sec, const SharedVec3& gravity_direction) {
            RF_PHYSICS_INFO("PhysicsEngine::ApplyLocalizedGravity (Conceptual): Center({:.1f},{:.1f},{:.1f}), Str:{:.1f}, Rad:{:.1f}, Dur:{:.1f}s, Dir:({:.1f},{:.1f},{:.1f})",
                center.x(), center.y(), center.z(), strength, radius, duration_sec, gravity_direction.x(), gravity_direction.y(), gravity_direction.z());
            // TODO for implementation:
            // Similar to CreateRadialForceField:
            // - Any scene queries to find actors in the radius must be protected by m_physicsMutex.
            // - Modifying actor flags (e.g., PxActorFlag::eDISABLE_GRAVITY) and applying forces
            //   (e.g., PxRigidBody::addForce or PxRigidBody::setActorFlag) must be protected by m_physicsMutex.
            //   Consider locking once for the entire operation (query + modification loop) for efficiency.
            RF_PHYSICS_WARN("ApplyLocalizedGravity is a conceptual stub and requires significant implementation including thread safety for physics operations.");
        }

        bool PhysicsEngine::DeformTerrainRegion(const SharedVec3& impact_point, float radius, float depth_or_intensity, int deformation_type) {
            RF_PHYSICS_INFO("PhysicsEngine::DeformTerrainRegion (Conceptual STUB): Impact({:.1f},{:.1f},{:.1f}), Radius:{:.1f}, Depth/Intensity:{:.1f}, Type:{}",
                impact_point.x(), impact_point.y(), impact_point.z(), radius, depth_or_intensity, deformation_type);
            // TODO for implementation:
            // If this interacts with PhysX heightfields or triangle meshes directly:
            // - Reading heightfield/mesh data might need m_physicsMutex if the PhysX object can be modified concurrently.
            // - Modifying heightfield/mesh data (e.g., PxHeightField::modifySamples, PxTriangleMesh cooking & replacement)
            //   are significant operations and must be protected by m_physicsMutex.
            // - Notifying PhysX of updates (e.g., PxShape::setGeometry if mesh is replaced) also needs m_physicsMutex.
            RF_PHYSICS_WARN("Terrain deformation is a highly complex feature. Physics interactions require careful thread safety management (m_physicsMutex).");
            return false;
        }

        physx::PxRigidDynamic* PhysicsEngine::CreatePhysicsProjectileActor(
            const ProjectilePhysicsProperties& properties,
            const ProjectileGameData& gameData,
            const RiftForged::Utilities::Math::Vec3& startPosition,
            const RiftForged::Utilities::Math::Vec3& initialVelocity,
            physx::PxMaterial* projectileMaterial // Renamed from 'material' to avoid conflict if class has m_material
        ) {
            std::lock_guard<std::mutex> lock(m_physicsMutex); // Ensure thread safety for scene modifications

            if (!m_physics || !m_scene) {
                RF_PHYSICS_ERROR("CreatePhysicsProjectileActor: Physics system or scene not initialized.");
                return nullptr;
            }

            physx::PxMaterial* matToUse = projectileMaterial ? projectileMaterial : m_default_material;
            if (!matToUse) {
                RF_PHYSICS_ERROR("CreatePhysicsProjectileActor: No valid PxMaterial available (neither provided nor default).");
                return nullptr;
            }

            // 1. Create the rigid dynamic actor
            //    The initial orientation of the projectile actor can often be identity, 
            //    as its movement is governed by velocity. Visual orientation is handled by the client.
            //    However, if the collision shape itself has a specific local orientation (e.g. a capsule not aligned with X),
            //    that local orientation is part of the PxShape's localPose.
            physx::PxTransform initialPosePx(ToPxVec3(startPosition), physx::PxQuat(physx::PxIdentity));
            physx::PxRigidDynamic* projectileActor = m_physics->createRigidDynamic(initialPosePx);

            if (!projectileActor) {
                RF_PHYSICS_ERROR("CreatePhysicsProjectileActor: m_physics->createRigidDynamic failed for projectile ID %llu.", gameData.projectileId);
                return nullptr;
            }

            // 2. Create and attach the projectile's collision shape
            physx::PxShape* projectileShape = nullptr;
            physx::PxTransform localShapePose(physx::PxIdentity); // Default: shape centered on actor, no local rotation

            if (properties.halfHeight > 0.0f && properties.radius > 0.0f) { // Capsule
                // PhysX PxCapsuleGeometry is oriented along the X-axis by default.
                // If your projectile model (e.g., an arrow) is modeled to fly along its Y or Z axis,
                // you'd apply a local rotation here to align the physics capsule with the visual model's forward.
                // Example: To align an X-axis capsule to fly along the Y-axis:
                // localShapePose = physx::PxTransform(physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0, 0, 1)));
                projectileShape = m_physics->createShape(
                    physx::PxCapsuleGeometry(properties.radius, properties.halfHeight),
                    *matToUse,
                    true, // isExclusive
                    physx::PxShapeFlag::eSIMULATION_SHAPE | physx::PxShapeFlag::eTRIGGER_SHAPE // Or just eSIMULATION_SHAPE
                );
                // If using localShapePose: projectileActor->attachShape(*projectileShape); projectileShape->setLocalPose(localShapePose);
            }
            else if (properties.radius > 0.0f) { // Sphere
                projectileShape = m_physics->createShape(
                    physx::PxSphereGeometry(properties.radius),
                    *matToUse,
                    true, // isExclusive
                    physx::PxShapeFlag::eSIMULATION_SHAPE | physx::PxShapeFlag::eTRIGGER_SHAPE // Or just eSIMULATION_SHAPE
                );
            }
            else {
                RF_PHYSICS_ERROR("CreatePhysicsProjectileActor: Invalid projectile shape properties for projectile ID %llu (radius or halfHeight is zero or negative).", gameData.projectileId);
                projectileActor->release(); // Clean up the created actor
                return nullptr;
            }

            if (!projectileShape) {
                RF_PHYSICS_ERROR("CreatePhysicsProjectileActor: m_physics->createShape failed for projectile ID %llu.", gameData.projectileId);
                projectileActor->release();
                return nullptr;
            }
            projectileActor->attachShape(*projectileShape);
            projectileShape->release(); // Actor now owns the shape, release our reference

            // 3. Set physics body properties
            if (properties.mass > 0.0f) {
                physx::PxRigidBodyExt::updateMassAndInertia(*projectileActor, properties.mass);
            }
            else {
                RF_PHYSICS_WARN("CreatePhysicsProjectileActor: Projectile ID %llu mass is <= 0. Using default small mass 0.01kg for dynamic simulation.", gameData.projectileId);
                physx::PxRigidBodyExt::updateMassAndInertia(*projectileActor, 0.01f);
            }

            projectileActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !properties.enableGravity);
            if (properties.enableCCD) {
                projectileActor->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_CCD, true);
            }

            // 4. Store game data in userData
            // CRITICAL: This allocates memory that MUST be deleted when the projectile is destroyed.
            // The PxSimulationEventCallback::onContact or a ProjectileManager should handle this.
            ProjectileGameData* userDataPtr = new ProjectileGameData(gameData);
            projectileActor->userData = static_cast<void*>(userDataPtr);

            // 5. Set initial velocity
            projectileActor->setLinearVelocity(ToPxVec3(initialVelocity));
            // Optional: Set a maximum linear velocity if desired, though this is usually for constraints.
            // projectileActor->setMaxLinearVelocity(someMaxSpeed); 

            // 6. Add to scene
            m_scene->addActor(*projectileActor);

            RF_PHYSICS_INFO("PhysicsEngine: Launched projectile ID %llu, Owner ID %llu.", gameData.projectileId, gameData.ownerId);
            return projectileActor;
        }

    } // namespace Physics
} // namespace RiftForged
