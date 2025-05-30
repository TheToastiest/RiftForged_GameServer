// File: PhysicsEngine/PhysicsEngine.cpp
// Copyright (c) 2023-2025 RiftForged Game Development Team

//#include "FixPhysicsEngine.h"
//#include <vector> 
//#include <thread> 

// PhysX specific headers needed for implementation
//#include "physx/cooking/PxCooking.h"          // For PxCookingParams, PxCreateTriangleMesh
//#include "physx/cooking/PxTriangleMeshDesc.h" // For PxTriangleMeshDesc
// #include "physx/cooking/PxCookingInternal.h" // May not be needed if not using PxCooking object directly
// #include "physx/cooking/PxConvexMeshDesc.h"  // If you add convex mesh cooking
//#include "physx/extensions/PxDefaultErrorCallback.h"
//#include "physx/extensions/PxDefaultAllocator.h"
//#include "physx/pvd/PxPvd.h"
//#include "physx/pvd/PxPvdTransport.h"
//#include "physx/extensions/PxDefaultCpuDispatcher.h"
//#include "physx/PxRigidStatic.h"
//#include "physx/PxRigidDynamic.h"
//#include "physx/PxMaterial.h"
//#include "physx/PxScene.h"
//#include "physx/PxSceneDesc.h"
//#include "physx/PxPhysics.h"
//#include "physx/PxPhysicsAPI.h" // Includes PxCreateTriangleMesh
//#include "physx/common/PxInsertionCallback.h" // For PxPhysicsInsertionCallback
//#include "physx/characterkinematic/PxControllerManager.h"
//#include "physx/common/PxTolerancesScale.h"
//#include "physx/extensions/PxDefaultStreams.h"
//#include "physx/foundation/PxPhysicsVersion.h" 
//#include "physx/foundation/PxFoundation.h"      
//#include "physx/extensions/PxExtensionsAPI.h" 
//#include "physx/PxQueryFiltering.h"
//#include "physx/PxShape.h"
//#include "physx/geometry/PxCapsuleGeometry.h"
//#include "physx/extensions/PxRigidActorExt.h"
//#include "physx/cudamanager/PxCudaContextManager.h"
//

//#include "../FlatBuffers/V0.0.3/riftforged_common_types_generated.h" 
//#include "../PhysicsEngine/PhysicsTypes.h" // ADDED - Ensure this path is correct
// Logger is included via PhysicsEngine.h
//#pragma comment(lib, "PhysXCooking_64.lib")


//namespace RiftForged {
//    namespace Physics {
//
//        static physx::PxDefaultErrorCallback gDefaultErrorCallback;
//        static physx::PxDefaultAllocator gDefaultAllocatorCallback;
//
//        // Custom Filter Shader (same as before)
//        physx::PxFilterFlags CustomFilterShader(
//            physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0,
//            physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1,
//            physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize)
//        {
//            PX_UNUSED(constantBlock);
//            PX_UNUSED(constantBlockSize);
//
//            if (physx::PxFilterObjectIsTrigger(attributes0) || physx::PxFilterObjectIsTrigger(attributes1)) {
//                pairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
//                return physx::PxFilterFlag::eDEFAULT;
//            }
//
//            pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;
//
//            EPhysicsObjectType type0 = static_cast<EPhysicsObjectType>(filterData0.word0);
//            EPhysicsObjectType type1 = static_cast<EPhysicsObjectType>(filterData1.word0);
//
//            if (type0 == EPhysicsObjectType::MAGIC_PROJECTILE && type1 == EPhysicsObjectType::MAGIC_PROJECTILE) {
//                return physx::PxFilterFlag::eSUPPRESS;
//            }
//            if (type0 == EPhysicsObjectType::PROJECTILE && type1 == EPhysicsObjectType::PROJECTILE) {
//                return physx::PxFilterFlag::eSUPPRESS;
//            }
//            if ((type0 == EPhysicsObjectType::MAGIC_PROJECTILE && type1 == EPhysicsObjectType::PROJECTILE) ||
//                (type0 == EPhysicsObjectType::PROJECTILE && type1 == EPhysicsObjectType::MAGIC_PROJECTILE)) {
//                return physx::PxFilterFlag::eSUPPRESS;
//            }
//
//            bool type0IsProjectile = (type0 == EPhysicsObjectType::MAGIC_PROJECTILE || type0 == EPhysicsObjectType::PROJECTILE);
//            bool type1IsProjectile = (type1 == EPhysicsObjectType::MAGIC_PROJECTILE || type1 == EPhysicsObjectType::PROJECTILE);
//            bool type0IsTargetable = (type0 == EPhysicsObjectType::PLAYER_CHARACTER || type0 == EPhysicsObjectType::SMALL_ENEMY || type0 == EPhysicsObjectType::MEDIUM_ENEMY || type0 == EPhysicsObjectType::LARGE_ENEMY || type0 == EPhysicsObjectType::HUGE_ENEMY || type0 == EPhysicsObjectType::RAID_BOSS || type0 == EPhysicsObjectType::VAELITH);
//            bool type1IsTargetable = (type1 == EPhysicsObjectType::PLAYER_CHARACTER || type1 == EPhysicsObjectType::SMALL_ENEMY || type1 == EPhysicsObjectType::MEDIUM_ENEMY || type1 == EPhysicsObjectType::LARGE_ENEMY || type1 == EPhysicsObjectType::HUGE_ENEMY || type1 == EPhysicsObjectType::RAID_BOSS || type1 == EPhysicsObjectType::VAELITH);
//
//            if ((type0IsProjectile && type1IsTargetable) || (type1IsProjectile && type0IsTargetable)) {
//                pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
//                pairFlags |= physx::PxPairFlag::eNOTIFY_CONTACT_POINTS;
//            }
//
//            bool type0IsMelee = (type0 == EPhysicsObjectType::MELEE_WEAPON);
//            bool type1IsMelee = (type1 == EPhysicsObjectType::MELEE_WEAPON);
//
//            if ((type0IsMelee && type1IsTargetable) || (type1IsMelee && type0IsTargetable)) {
//                pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
//            }
//
//            return physx::PxFilterFlag::eDEFAULT;
//        }
//
//
//        void PhysicsEngine::SetupShapeFiltering(physx::PxShape* shape, const CollisionFilterData& filter_data) {
//            if (shape) {
//                physx::PxFilterData px_filter_data;
//                px_filter_data.word0 = filter_data.word0;
//                px_filter_data.word1 = filter_data.word1;
//                px_filter_data.word2 = filter_data.word2;
//                px_filter_data.word3 = filter_data.word3;
//                shape->setQueryFilterData(px_filter_data);
//                shape->setSimulationFilterData(px_filter_data);
//            }
//        }
//
//        PhysicsEngine::PhysicsEngine()
//            : m_foundation(nullptr),
//            m_physics(nullptr),
//            m_dispatcher(nullptr),
//            m_scene(nullptr),
//            m_default_material(nullptr),
//            m_controller_manager(nullptr),
//            m_pvd(nullptr),
//            m_pvd_transport(nullptr),
//            // m_cooking(nullptr), // REMOVED
//            m_cudaContextManager(nullptr)
//        {
//            RF_PHYSICS_INFO("PhysicsEngine: Constructed.");
//        }
//
//        PhysicsEngine::~PhysicsEngine() {
//            RF_PHYSICS_INFO("PhysicsEngine: Destructor called. Ensuring Shutdown.");
//            Shutdown();
//        }
//
//        bool PhysicsEngine::Initialize(const SharedVec3& gravityVec, bool connect_to_pvd) {
//            RF_PHYSICS_INFO("PhysicsEngine: Initializing...");
//
//            m_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocatorCallback, gDefaultErrorCallback);
//            if (!m_foundation) { /* ... */ return false; }
//            RF_PHYSICS_INFO("PhysicsEngine: PxFoundation created.");
//
//            if (connect_to_pvd) { /* ... PVD setup ... */ }
//            else { RF_PHYSICS_INFO("PhysicsEngine: PVD connection explicitly disabled."); }
//
//            physx::PxTolerancesScale tolerances_scale;
//            m_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, tolerances_scale, true, m_pvd);
//            if (!m_physics) { /* ... */ return false; }
//            RF_PHYSICS_INFO("PhysicsEngine: PxPhysics created.");
//
//            if (!PxInitExtensions(*m_physics, m_pvd)) { /* ... */ return false; }
//            RF_PHYSICS_INFO("PhysicsEngine: PxExtensions initialized.");
//
//            // physx::PxCookingParams cookingParams(m_physics->getTolerancesScale()); // REMOVED
//            // m_cooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_foundation, cookingParams); // REMOVED
//            // if (!m_cooking) { /* ... */ return false; } // REMOVED
//            // RF_PHYSICS_INFO("PhysicsEngine: PxCooking created."); // REMOVED
//
//
//            physx::PxCudaContextManagerDesc cudaContextManagerDesc;
//            m_cudaContextManager = PxCreateCudaContextManager(*m_foundation, cudaContextManagerDesc);
//            if (m_cudaContextManager) { /* ... CUDA checks ... */ }
//            else { RF_PHYSICS_WARN("PhysicsEngine: PxCreateCudaContextManager failed. GPU acceleration will be disabled."); }
//
//            uint32_t num_hardware_threads = std::thread::hardware_concurrency();
//            uint32_t num_threads_for_dispatcher = (num_hardware_threads > 1) ? num_hardware_threads - 1 : 1;
//            if (num_hardware_threads == 0) {
//                RF_PHYSICS_WARN("PhysicsEngine: Could not determine hardware concurrency or 0 reported, defaulting CPU dispatcher to 1 thread.");
//                num_threads_for_dispatcher = 1;
//            }
//            m_dispatcher = physx::PxDefaultCpuDispatcherCreate(num_threads_for_dispatcher);
//            if (!m_dispatcher) { /* ... */ return false; }
//            RF_PHYSICS_INFO("PhysicsEngine: PxDefaultCpuDispatcher created with {} threads (Hardware reported: {}).",
//                num_threads_for_dispatcher, num_hardware_threads);
//
//            physx::PxSceneDesc scene_desc(m_physics->getTolerancesScale());
//            scene_desc.gravity = ToPxVec3(gravityVec);
//            scene_desc.cpuDispatcher = m_dispatcher;
//            scene_desc.filterShader = CustomFilterShader; // Use our custom shader
//            // scene_desc.simulationEventCallback = &m_yourSimulationEventCallbackInstance; 
//            // scene_desc.contactModifyCallback = &m_yourContactModifyCallbackInstance; 
//
//            if (m_cudaContextManager && m_cudaContextManager->contextIsValid()) { /* ... GPU scene setup ... */ }
//            else { /* ... CPU scene setup ... */ }
//
//            m_scene = m_physics->createScene(scene_desc);
//            if (!m_scene) { /* ... */ return false; }
//            RF_PHYSICS_INFO("PhysicsEngine: PxScene created. Gravity: ({}, {}, {})", gravityVec.x(), gravityVec.y(), gravityVec.z());
//
//            if (m_pvd && m_scene->getScenePvdClient()) { /* ... PVD scene flags ... */ }
//
//            m_default_material = m_physics->createMaterial(0.5f, 0.5f, 0.1f);
//            if (!m_default_material) { /* ... */ return false; }
//            RF_PHYSICS_INFO("PhysicsEngine: Default PxMaterial created.");
//
//            m_controller_manager = PxCreateControllerManager(*m_scene);
//            if (!m_controller_manager) { /* ... */ return false; }
//            RF_PHYSICS_INFO("PhysicsEngine: PxControllerManager created.");
//
//            RF_PHYSICS_INFO("PhysicsEngine: Initialization successful.");
//            return true;
//        }
//
//        void PhysicsEngine::Shutdown() {
//            RF_PHYSICS_INFO("PhysicsEngine: Shutting down...");
//
//            if (m_controller_manager) { m_controller_manager->release(); m_controller_manager = nullptr; RF_PHYSICS_INFO("PhysicsEngine: PxControllerManager released."); }
//            if (m_default_material) { m_default_material->release(); m_default_material = nullptr; RF_PHYSICS_INFO("PhysicsEngine: Default PxMaterial released."); }
//            if (m_scene) { m_scene->release(); m_scene = nullptr; RF_PHYSICS_INFO("PhysicsEngine: PxScene released."); }
//            if (m_dispatcher) { m_dispatcher->release(); m_dispatcher = nullptr; RF_PHYSICS_INFO("PhysicsEngine: PxDefaultCpuDispatcher released."); }
//            if (m_cudaContextManager) { m_cudaContextManager->release(); m_cudaContextManager = nullptr; RF_PHYSICS_INFO("PhysicsEngine: PxCudaContextManager released."); }
//
//            // if (m_cooking) { m_cooking->release(); m_cooking = nullptr; RF_PHYSICS_INFO("PhysicsEngine: PxCooking released."); } // REMOVED
//
//            PxCloseExtensions(); RF_PHYSICS_INFO("PhysicsEngine: PxExtensions closed.");
//
//            if (m_physics) { m_physics->release(); m_physics = nullptr; RF_PHYSICS_INFO("PhysicsEngine: PxPhysics released."); }
//            if (m_pvd) { /* ... PVD release ... */ }
//            else if (m_pvd_transport) { m_pvd_transport->release(); m_pvd_transport = nullptr; }
//            if (m_foundation) { m_foundation->release(); m_foundation = nullptr; RF_PHYSICS_INFO("PhysicsEngine: PxFoundation released."); }
//
//            RF_PHYSICS_INFO("PhysicsEngine: Shutdown complete.");
//        }
//
//        // ... StepSimulation, CreateMaterial, PlayerController methods, SetActorUserData, Generic Rigid Actor Management methods ...
//        // (These remain the same as in the previous full C++ response, with EPhysicsObjectType integrated)
//
//        void PhysicsEngine::StepSimulation(float delta_time_sec) {
//            if (delta_time_sec <= 0.0f) {
//                RF_PHYSICS_TRACE("PhysicsEngine::StepSimulation: delta_time_sec is zero or negative ({:.4f}s). Skipping.", delta_time_sec);
//                return;
//            }
//            std::lock_guard<std::mutex> lock(m_physicsMutex);
//            if (!m_scene) {
//                RF_PHYSICS_ERROR("PhysicsEngine::StepSimulation: Scene is null!");
//                return;
//            }
//            m_scene->simulate(delta_time_sec);
//            m_scene->fetchResults(true);
//        }
//
//        physx::PxMaterial* PhysicsEngine::CreateMaterial(float static_friction, float dynamic_friction, float restitution) {
//            std::lock_guard<std::mutex> lock(m_physicsMutex);
//            if (!m_physics) {
//                RF_PHYSICS_ERROR("PhysicsEngine::CreateMaterial: PxPhysics not initialized.");
//                return nullptr;
//            }
//            physx::PxMaterial* new_material = m_physics->createMaterial(static_friction, dynamic_friction, restitution);
//            if (!new_material) { RF_PHYSICS_ERROR("PhysicsEngine::CreateMaterial: m_physics->createMaterial failed."); }
//            else { RF_PHYSICS_INFO("PhysicsEngine: Created new PxMaterial."); }
//            return new_material;
//        }
//
//        void PhysicsEngine::RegisterPlayerController(uint64_t player_id, physx::PxController* controller) {
//            if (!controller) { RF_PHYSICS_ERROR("PhysicsEngine::RegisterPlayerController: Attempted to register null controller for player ID {}.", player_id); return; }
//            std::lock_guard<std::mutex> lock(m_playerControllersMutex);
//            if (m_playerControllers.count(player_id)) { RF_PHYSICS_WARN("PhysicsEngine::RegisterPlayerController: Player ID {} already has a controller. Overwriting.", player_id); }
//            m_playerControllers[player_id] = controller;
//            RF_PHYSICS_INFO("PhysicsEngine::Registered PxController for player ID {}.", player_id);
//        }
//
//        bool PhysicsEngine::SetCharacterControllerOrientation(uint64_t player_id, const SharedQuaternion& orientation) {
//            physx::PxController* controller = GetPlayerController(player_id);
//            if (!controller) { RF_PHYSICS_WARN("PhysicsEngine::SetCharacterControllerOrientation: Controller not found for player ID {}.", player_id); return false; }
//            std::lock_guard<std::mutex> lock(m_physicsMutex);
//            physx::PxRigidActor* actor = controller->getActor();
//            if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::SetCharacterControllerOrientation: Could not get PxActor for player ID {}.", player_id); return false; }
//            if (!actor->getScene()) { RF_PHYSICS_WARN("PhysicsEngine::SetCharacterControllerOrientation: Actor for player ID {} is not in a scene.", player_id); return false; }
//            physx::PxTransform currentPose = actor->getGlobalPose();
//            currentPose.q = ToPxQuat(orientation);
//            actor->setGlobalPose(currentPose);
//            return true;
//        }
//
//        void PhysicsEngine::UnregisterPlayerController(uint64_t player_id) {
//            physx::PxController* controller_to_release = nullptr;
//            {
//                std::lock_guard<std::mutex> mapLock(m_playerControllersMutex);
//                auto it = m_playerControllers.find(player_id);
//                if (it != m_playerControllers.end()) {
//                    controller_to_release = it->second;
//                    m_playerControllers.erase(it);
//                }
//            }
//            if (controller_to_release) {
//                std::lock_guard<std::mutex> physicsLock(m_physicsMutex);
//                controller_to_release->release();
//                RF_PHYSICS_INFO("PhysicsEngine::Unregistered and released PxController for player ID {}.", player_id);
//            }
//            else { RF_PHYSICS_WARN("PhysicsEngine::UnregisterPlayerController: No PxController found for player ID {}.", player_id); }
//        }
//
//        physx::PxController* PhysicsEngine::GetPlayerController(uint64_t player_id) const {
//            std::lock_guard<std::mutex> lock(m_playerControllersMutex);
//            auto it = m_playerControllers.find(player_id);
//            if (it != m_playerControllers.end()) { return it->second; }
//            return nullptr;
//        }
//
//        void PhysicsEngine::SetActorUserData(physx::PxActor* actor, void* userData) {
//            if (actor) {
//                // Original code did not lock physics mutex here.
//                // For safety, if actor is in scene and physics is running, this should be locked.
//                // However, it is typically called during actor creation when the main physics lock is already held.
//                if (actor->getScene()) { // Optional check
//                    actor->userData = userData;
//                }
//                else {
//                    // Actor not in scene, can still set user data if needed before adding
//                    actor->userData = userData;
//                    // RF_PHYSICS_TRACE("PhysicsEngine::SetActorUserData: Actor (0x%p) is not in a scene but UserData set.", static_cast<void*>(actor));
//                }
//            }
//        }
//
//        void PhysicsEngine::RegisterRigidActor(uint64_t entity_id, physx::PxRigidActor* actor) {
//            if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::RegisterRigidActor: Attempted to register null actor for entity ID {}.", entity_id); return; }
//            std::lock_guard<std::mutex> mapLock(m_entityActorsMutex);
//            if (m_entityActors.count(entity_id)) { RF_PHYSICS_WARN("PhysicsEngine::RegisterRigidActor: Entity ID {} already has a registered actor. Overwriting.", entity_id); }
//            m_entityActors[entity_id] = actor;
//        }
//
//        void PhysicsEngine::UnregisterRigidActor(uint64_t entity_id) {
//            physx::PxRigidActor* actor_to_release_and_remove = nullptr;
//            {
//                std::lock_guard<std::mutex> lock(m_entityActorsMutex);
//                auto it = m_entityActors.find(entity_id);
//                if (it != m_entityActors.end()) {
//                    actor_to_release_and_remove = it->second;
//                    m_entityActors.erase(it);
//                }
//            }
//            if (actor_to_release_and_remove) {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                if (m_scene && actor_to_release_and_remove->getScene() == m_scene) {
//                    m_scene->removeActor(*actor_to_release_and_remove, false);
//                }
//                actor_to_release_and_remove->release();
//                RF_PHYSICS_INFO("PhysicsEngine::UnregisterRigidActor: Removed and released PxRigidActor for entity ID {}.", entity_id);
//            }
//            else { RF_PHYSICS_WARN("PhysicsEngine::UnregisterRigidActor: No PxRigidActor found for entity ID {}.", entity_id); }
//        }
//
//        physx::PxRigidActor* PhysicsEngine::GetRigidActor(uint64_t entity_id) const {
//            std::lock_guard<std::mutex> lock(m_entityActorsMutex);
//            auto it = m_entityActors.find(entity_id);
//            return (it != m_entityActors.end()) ? it->second : nullptr;
//        }
//
//        physx::PxController* PhysicsEngine::CreateCharacterController(
//            uint64_t player_id,
//            const SharedVec3& initial_position,
//            float radius,
//            float height,
//            physx::PxMaterial* material,
//            void* user_data_for_controller_actor
//        ) {
//            if (!m_physics || !m_scene || !m_controller_manager) { RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: Physics system, scene or controller manager not initialized for player ID {}.", player_id); return nullptr; }
//            if (radius <= 0.0f || height <= 0.0f) { RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: Invalid radius ({}) or height ({}) for player ID {}.", radius, height, player_id); return nullptr; }
//
//            physx::PxCapsuleControllerDesc desc;
//            desc.height = height;
//            desc.radius = radius;
//            desc.position = physx::PxExtendedVec3(initial_position.x(), initial_position.y(), initial_position.z());
//            desc.material = material ? material : m_default_material;
//            if (!desc.material) { RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: No valid PxMaterial for player ID {}.", player_id); return nullptr; }
//            desc.stepOffset = 0.5f;
//            desc.slopeLimit = std::cos(45.0f * physx::PxPi / 180.0f);
//            desc.contactOffset = 0.05f;
//            desc.upDirection = physx::PxVec3(0.0f, 0.0f, 1.0f);
//            // desc.reportCallback = your_hit_report_callback_instance;
//            // desc.behaviorCallback = your_behavior_callback_instance;
//
//            if (!desc.isValid()) { RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: PxCapsuleControllerDesc is invalid for player ID {}. Radius: {}, Height: {}", player_id, radius, height); return nullptr; }
//
//            physx::PxController* controller = nullptr;
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                controller = m_controller_manager->createController(desc);
//                if (controller) {
//                    physx::PxRigidActor* controller_actor = controller->getActor();
//                    if (controller_actor) {
//                        SetActorUserData(controller_actor, user_data_for_controller_actor ? user_data_for_controller_actor : reinterpret_cast<void*>(player_id));
//
//                        physx::PxShape* shapes[1];
//                        if (controller_actor->getNbShapes() >= 1 && controller_actor->getShapes(shapes, 1)) {
//                            CollisionFilterData player_filter_data;
//                            player_filter_data.word0 = static_cast<physx::PxU32>(EPhysicsObjectType::PLAYER_CHARACTER);
//                            SetupShapeFiltering(shapes[0], player_filter_data);
//                        }
//                    }
//                }
//            }
//
//            if (controller) {
//                RegisterPlayerController(player_id, controller);
//                RF_PHYSICS_INFO("PhysicsEngine: Created and registered PxController for player ID {}.", player_id);
//            }
//            else {
//                RF_PHYSICS_ERROR("PhysicsEngine::CreateCharacterController: m_controller_manager->createController failed for player ID {}.", player_id);
//            }
//            return controller;
//        }
//
//        uint32_t PhysicsEngine::MoveCharacterController(
//            physx::PxController* controller,
//            const SharedVec3& world_space_displacement,
//            float delta_time_sec,
//            const std::vector<physx::PxController*>& other_controllers_to_ignore
//        ) {
//            if (!controller) { RF_PHYSICS_ERROR("PhysicsEngine::MoveCharacterController: Null controller passed."); return 0; }
//            if (delta_time_sec <= 0.0f) { return 0; }
//            physx::PxVec3 disp = ToPxVec3(world_space_displacement);
//            physx::PxControllerFilters filters;
//            // TODO: Populate filters if other_controllers_to_ignore is used.
//            physx::PxU32 collision_flags; // Changed from PxControllerCollisionFlags for direct assignment
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                collision_flags = controller->move(disp, 0.001f, delta_time_sec, filters, nullptr);
//            }
//            return collision_flags;
//        }
//
//        void PhysicsEngine::SetCharacterControllerPose(physx::PxController* controller, const SharedVec3& world_position) {
//            if (!controller) { RF_PHYSICS_ERROR("PhysicsEngine::SetCharacterControllerPose: Null controller passed."); return; }
//            physx::PxExtendedVec3 pos = physx::PxExtendedVec3(world_position.x(), world_position.y(), world_position.z());
//            bool success = false;
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                success = controller->setPosition(pos);
//            }
//            if (!success) { RF_PHYSICS_WARN("PhysicsEngine::SetCharacterControllerPose: controller->setPosition failed."); }
//        }
//
//        SharedVec3 PhysicsEngine::GetCharacterControllerPosition(physx::PxController* controller) const {
//            if (!controller) { RF_PHYSICS_ERROR("PhysicsEngine::GetCharacterControllerPosition: Null controller. Returning zero."); return SharedVec3(0.0f, 0.0f, 0.0f); }
//            physx::PxExtendedVec3 pos;
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                pos = controller->getPosition();
//            }
//            return SharedVec3(static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z));
//        }
//
//        physx::PxRigidStatic* PhysicsEngine::CreateStaticPlane(
//            const SharedVec3& normal,
//            float distance,
//            EPhysicsObjectType object_type,
//            physx::PxMaterial* material = nullptr
//        ) {
//            if (!m_physics || !m_scene) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticPlane: Physics system or scene not initialized."); return nullptr; }
//            physx::PxMaterial* mat = material ? material : m_default_material;
//            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticPlane: No valid material."); return nullptr; }
//
//            CollisionFilterData sim_filter_data;
//            sim_filter_data.word0 = static_cast<physx::PxU32>(object_type);
//
//            physx::PxRigidStatic* plane_actor = nullptr;
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                plane_actor = physx::PxCreatePlane(*m_physics, physx::PxPlane(ToPxVec3(normal), distance), *mat);
//                if (!plane_actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticPlane: PxCreatePlane failed."); return nullptr; }
//
//                physx::PxShape* shape_ptr[1];
//                if (plane_actor->getNbShapes() == 1 && plane_actor->getShapes(shape_ptr, 1) == 1) {
//                    SetupShapeFiltering(shape_ptr[0], sim_filter_data);
//                }
//                else { RF_PHYSICS_WARN("PhysicsEngine::CreateStaticPlane: Could not retrieve shape from plane actor for filtering."); }
//                m_scene->addActor(*plane_actor);
//            }
//            RF_PHYSICS_INFO("PhysicsEngine: Static plane (type {}) created and added to scene.", static_cast<int>(object_type));
//            return plane_actor;
//        }
//
//        physx::PxRigidStatic* PhysicsEngine::CreateStaticBox(
//            uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation,
//            const SharedVec3& half_extents, EPhysicsObjectType object_type,
//            physx::PxMaterial* material, void* user_data
//        ) {
//            if (!m_physics || !m_scene) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticBox: Physics system or scene not initialized."); return nullptr; }
//            physx::PxMaterial* mat = material ? material : m_default_material;
//            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticBox: No valid material."); return nullptr; }
//
//            physx::PxBoxGeometry geometry(ToPxVec3(half_extents));
//            physx::PxTransform pose(ToPxVec3(position), ToPxQuat(orientation));
//            physx::PxRigidStatic* actor = nullptr;
//
//            CollisionFilterData sim_filter_data;
//            sim_filter_data.word0 = static_cast<physx::PxU32>(object_type);
//
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                actor = m_physics->createRigidStatic(pose);
//                if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticBox: createRigidStatic failed."); return nullptr; }
//
//                physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
//                if (!shape) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticBox: createShape failed."); actor->release(); return nullptr; }
//
//                SetupShapeFiltering(shape, sim_filter_data);
//                actor->attachShape(*shape);
//                shape->release();
//
//                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
//                m_scene->addActor(*actor);
//            }
//
//            if (actor) {
//                RegisterRigidActor(entity_id, actor);
//                RF_PHYSICS_INFO("PhysicsEngine: Static box (type {}) for entity ID {} created and registered.", static_cast<int>(object_type), entity_id);
//            }
//            return actor;
//        }
//
//        physx::PxRigidStatic* PhysicsEngine::CreateStaticSphere(
//            uint64_t entity_id, const SharedVec3& position, float radius, EPhysicsObjectType object_type,
//            physx::PxMaterial* material, void* user_data
//        ) {
//            if (!m_physics || !m_scene) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticSphere: Physics system or scene not initialized."); return nullptr; }
//            physx::PxMaterial* mat = material ? material : m_default_material;
//            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticSphere: No valid material."); return nullptr; }
//
//            physx::PxSphereGeometry geometry(radius);
//            physx::PxTransform pose(ToPxVec3(position));
//            physx::PxRigidStatic* actor = nullptr;
//
//            CollisionFilterData sim_filter_data;
//            sim_filter_data.word0 = static_cast<physx::PxU32>(object_type);
//
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                actor = m_physics->createRigidStatic(pose);
//                if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticSphere: createRigidStatic failed."); return nullptr; }
//                physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
//                if (!shape) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticSphere: createShape failed."); actor->release(); return nullptr; }
//                SetupShapeFiltering(shape, sim_filter_data);
//                actor->attachShape(*shape);
//                shape->release();
//                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
//                m_scene->addActor(*actor);
//            }
//            if (actor) { RegisterRigidActor(entity_id, actor); RF_PHYSICS_INFO("PhysicsEngine: Static sphere (type {}) for entity ID {} created and registered.", static_cast<int>(object_type), entity_id); }
//            return actor;
//        }
//
//        physx::PxRigidStatic* PhysicsEngine::CreateStaticCapsule(
//            uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation,
//            float radius, float half_height, EPhysicsObjectType object_type,
//            physx::PxMaterial* material, void* user_data
//        ) {
//            if (!m_physics || !m_scene) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticCapsule: Physics system or scene not initialized."); return nullptr; }
//            physx::PxMaterial* mat = material ? material : m_default_material;
//            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticCapsule: No valid material."); return nullptr; }
//
//            physx::PxCapsuleGeometry geometry(radius, half_height);
//            physx::PxTransform pose(ToPxVec3(position), ToPxQuat(orientation));
//            physx::PxRigidStatic* actor = nullptr;
//
//            CollisionFilterData sim_filter_data;
//            sim_filter_data.word0 = static_cast<physx::PxU32>(object_type);
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                actor = m_physics->createRigidStatic(pose);
//                if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticCapsule: createRigidStatic failed."); return nullptr; }
//                physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
//                if (!shape) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticCapsule: createShape failed."); actor->release(); return nullptr; }
//                SetupShapeFiltering(shape, sim_filter_data);
//                actor->attachShape(*shape);
//                shape->release();
//                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
//                m_scene->addActor(*actor);
//            }
//            if (actor) { RegisterRigidActor(entity_id, actor); RF_PHYSICS_INFO("PhysicsEngine: Static capsule (type {}) for entity ID {} created and registered.", static_cast<int>(object_type), entity_id); }
//            return actor;
//        }
//
//        physx::PxRigidStatic* PhysicsEngine::CreateStaticTriangleMesh(
//            uint64_t entity_id,
//            const std::vector<SharedVec3>& vertices,
//            const std::vector<uint32_t>& indices,
//            EPhysicsObjectType object_type,
//            const SharedVec3& scale_vec,
//            physx::PxMaterial* material,
//            void* user_data
//        ) {
//            // Check m_physics for tolerances scale and insertion callback
//            if (!m_physics || !m_scene) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: Physics system or scene not initialized for entity ID {}.", entity_id); return nullptr; }
//            if (vertices.empty() || indices.empty() || (indices.size() % 3 != 0)) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: Invalid vertex or index data for entity ID {}.", entity_id); return nullptr; }
//
//            std::vector<physx::PxVec3> px_vertices(vertices.size());
//            for (size_t i = 0; i < vertices.size(); ++i) px_vertices[i] = ToPxVec3(vertices[i]);
//
//            physx::PxTriangleMeshDesc mesh_desc;
//            mesh_desc.points.count = static_cast<physx::PxU32>(px_vertices.size());
//            mesh_desc.points.stride = sizeof(physx::PxVec3);
//            mesh_desc.points.data = px_vertices.data();
//            mesh_desc.triangles.count = static_cast<physx::PxU32>(indices.size() / 3);
//            mesh_desc.triangles.stride = 3 * sizeof(uint32_t);
//            mesh_desc.triangles.data = indices.data();
//
//            CollisionFilterData sim_filter_data;
//            sim_filter_data.word0 = static_cast<physx::PxU32>(object_type);
//
//            physx::PxTriangleMesh* triangle_mesh = nullptr;
//            physx::PxRigidStatic* actor = nullptr;
//
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//
//                // Create PxCookingParams locally using m_physics's tolerances scale
//                physx::PxCookingParams params(m_physics->getTolerancesScale());
//                // Configure params if needed (e.g., for mesh precomputation options)
//                // params.meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;
//                // params.midphaseDesc = physx::PxMeshMidPhase::eBVH34; // Example customization
//
//                physx::PxTriangleMeshCookingResult::Enum cooking_result_enum;
//                // Use C-API PxCreateTriangleMesh
//                triangle_mesh = PxCreateTriangleMesh(params, mesh_desc, m_physics->getPhysicsInsertionCallback(), &cooking_result_enum);
//
//                if (!triangle_mesh) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: PxCreateTriangleMesh failed for entity ID {}. Result: {}", entity_id, static_cast<int>(cooking_result_enum)); return nullptr; }
//                if (cooking_result_enum != physx::PxTriangleMeshCookingResult::eSUCCESS) {
//                    RF_PHYSICS_WARN("PhysicsEngine::CreateStaticTriangleMesh: PxCreateTriangleMesh for entity ID {} completed with result: {}.", entity_id, static_cast<int>(cooking_result_enum));
//                    if (cooking_result_enum == physx::PxTriangleMeshCookingResult::eLARGE_TRIANGLE || cooking_result_enum == physx::PxTriangleMeshCookingResult::eEMPTY_MESH) {
//                        triangle_mesh->release();
//                        RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: Cooking failed critically for entity ID {}.", entity_id);
//                        return nullptr;
//                    }
//                }
//
//
//                physx::PxMeshScale mesh_scale(ToPxVec3(scale_vec), physx::PxQuat(physx::PxIdentity));
//                physx::PxTriangleMeshGeometry geometry(triangle_mesh, mesh_scale);
//                physx::PxMaterial* mat_to_use = material ? material : m_default_material;
//                if (!mat_to_use) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: No material for entity ID {}.", entity_id); triangle_mesh->release(); return nullptr; }
//
//                actor = m_physics->createRigidStatic(physx::PxTransform(physx::PxIdentity));
//                if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: createRigidStatic failed for entity ID {}.", entity_id); triangle_mesh->release(); return nullptr; }
//
//                physx::PxShape* shape = physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *mat_to_use);
//                if (!shape) { RF_PHYSICS_ERROR("PhysicsEngine::CreateStaticTriangleMesh: createExclusiveShape failed for entity ID {}.", entity_id); actor->release(); triangle_mesh->release(); return nullptr; }
//
//                SetupShapeFiltering(shape, sim_filter_data);
//                // Shape is already attached by createExclusiveShape. PxRigidActorExt handles this.
//                // The shape now has a reference to triangle_mesh.
//
//                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
//                m_scene->addActor(*actor);
//            }
//            // Release our reference to the cooked triangle mesh.
//            // The PxTriangleMeshGeometry (and thus the shape/actor) holds its own reference.
//            if (triangle_mesh) {
//                triangle_mesh->release();
//            }
//
//            if (actor) { RegisterRigidActor(entity_id, actor); RF_PHYSICS_INFO("PhysicsEngine: Static triangle mesh (type {}) for entity ID {} created and registered.", static_cast<int>(object_type), entity_id); }
//            return actor;
//        }
//
//        physx::PxRigidDynamic* PhysicsEngine::CreateDynamicSphere(
//            uint64_t entity_id, const SharedVec3& position, float radius, float density, EPhysicsObjectType object_type,
//            physx::PxMaterial* material, void* user_data
//        ) {
//            if (!m_physics || !m_scene) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicSphere: Physics system or scene not initialized."); return nullptr; }
//            physx::PxMaterial* mat = material ? material : m_default_material;
//            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicSphere: No valid material."); return nullptr; }
//
//            physx::PxSphereGeometry geometry(radius);
//            physx::PxTransform pose(ToPxVec3(position));
//            physx::PxRigidDynamic* actor = nullptr;
//            CollisionFilterData sim_filter_data;
//            sim_filter_data.word0 = static_cast<physx::PxU32>(object_type);
//
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                actor = m_physics->createRigidDynamic(pose);
//                if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicSphere: createRigidDynamic failed."); return nullptr; }
//                physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
//                if (!shape) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicSphere: createShape failed."); actor->release(); return nullptr; }
//                SetupShapeFiltering(shape, sim_filter_data);
//                actor->attachShape(*shape);
//                shape->release();
//                if (density > 0.0f) { physx::PxRigidBodyExt::updateMassAndInertia(*actor, density); }
//                else { actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true); RF_PHYSICS_WARN("PhysicsEngine::CreateDynamicSphere: Entity ID {} density <= 0. Set to kinematic.", entity_id); }
//                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
//                m_scene->addActor(*actor);
//            }
//            if (actor) { RegisterRigidActor(entity_id, actor); RF_PHYSICS_INFO("PhysicsEngine: Dynamic sphere (type {}) for entity ID {} created and registered.", static_cast<int>(object_type), entity_id); }
//            return actor;
//        }
//
//        physx::PxRigidDynamic* PhysicsEngine::CreateDynamicBox(
//            uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation,
//            const SharedVec3& half_extents, float density, EPhysicsObjectType object_type,
//            physx::PxMaterial* material, void* user_data
//        ) {
//            if (!m_physics || !m_scene) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicBox: Physics system or scene not initialized."); return nullptr; }
//            physx::PxMaterial* mat = material ? material : m_default_material;
//            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicBox: No valid material."); return nullptr; }
//            physx::PxBoxGeometry geometry(ToPxVec3(half_extents));
//            physx::PxTransform pose(ToPxVec3(position), ToPxQuat(orientation));
//            physx::PxRigidDynamic* actor = nullptr;
//            CollisionFilterData sim_filter_data;
//            sim_filter_data.word0 = static_cast<physx::PxU32>(object_type);
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                actor = m_physics->createRigidDynamic(pose);
//                if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicBox: createRigidDynamic failed."); return nullptr; }
//                physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
//                if (!shape) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicBox: createShape failed."); actor->release(); return nullptr; }
//                SetupShapeFiltering(shape, sim_filter_data);
//                actor->attachShape(*shape);
//                shape->release();
//                if (density > 0.0f) { physx::PxRigidBodyExt::updateMassAndInertia(*actor, density); }
//                else { actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true); RF_PHYSICS_WARN("PhysicsEngine::CreateDynamicBox: Entity ID {} density <= 0. Set to kinematic.", entity_id); }
//                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
//                m_scene->addActor(*actor);
//            }
//            if (actor) { RegisterRigidActor(entity_id, actor); RF_PHYSICS_INFO("PhysicsEngine: Dynamic box (type {}) for entity ID {} created and registered.", static_cast<int>(object_type), entity_id); }
//            return actor;
//        }
//
//        physx::PxRigidDynamic* PhysicsEngine::CreateDynamicCapsule(
//            uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation,
//            float radius, float half_height, float density, EPhysicsObjectType object_type,
//            physx::PxMaterial* material, void* user_data
//        ) {
//            if (!m_physics || !m_scene) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicCapsule: Physics system or scene not initialized."); return nullptr; }
//            physx::PxMaterial* mat = material ? material : m_default_material;
//            if (!mat) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicCapsule: No valid material."); return nullptr; }
//            physx::PxCapsuleGeometry geometry(radius, half_height);
//            physx::PxTransform pose(ToPxVec3(position), ToPxQuat(orientation));
//            physx::PxRigidDynamic* actor = nullptr;
//            CollisionFilterData sim_filter_data;
//            sim_filter_data.word0 = static_cast<physx::PxU32>(object_type);
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                actor = m_physics->createRigidDynamic(pose);
//                if (!actor) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicCapsule: createRigidDynamic failed."); return nullptr; }
//                physx::PxShape* shape = m_physics->createShape(geometry, *mat, true);
//                if (!shape) { RF_PHYSICS_ERROR("PhysicsEngine::CreateDynamicCapsule: createShape failed."); actor->release(); return nullptr; }
//                SetupShapeFiltering(shape, sim_filter_data);
//                actor->attachShape(*shape);
//                shape->release();
//                if (density > 0.0f) { physx::PxRigidBodyExt::updateMassAndInertia(*actor, density); }
//                else { actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true); RF_PHYSICS_WARN("PhysicsEngine::CreateDynamicCapsule: Entity ID {} density <= 0. Set to kinematic.", entity_id); }
//                SetActorUserData(actor, user_data ? user_data : reinterpret_cast<void*>(entity_id));
//                m_scene->addActor(*actor);
//            }
//            if (actor) { RegisterRigidActor(entity_id, actor); RF_PHYSICS_INFO("PhysicsEngine: Dynamic capsule (type {}) for entity ID {} created and registered.", static_cast<int>(object_type), entity_id); }
//            return actor;
//        }
//
//        physx::PxRigidDynamic* PhysicsEngine::CreatePhysicsProjectileActor(
//            const ProjectilePhysicsProperties& properties,
//            const ProjectileGameData& gameData,
//            EPhysicsObjectType projectile_type,
//            const RiftForged::Utilities::Math::Vec3& startPosition,
//            const RiftForged::Utilities::Math::Vec3& initialVelocity,
//            physx::PxMaterial* projectileMaterial
//        ) {
//            std::lock_guard<std::mutex> lock(m_physicsMutex);
//            if (!m_physics || !m_scene) { RF_PHYSICS_ERROR("CreatePhysicsProjectileActor: Physics system or scene not initialized."); return nullptr; }
//            physx::PxMaterial* matToUse = projectileMaterial ? projectileMaterial : m_default_material;
//            if (!matToUse) { RF_PHYSICS_ERROR("CreatePhysicsProjectileActor: No valid PxMaterial available."); return nullptr; }
//
//            physx::PxTransform initialPosePx(ToPxVec3(startPosition), physx::PxQuat(physx::PxIdentity));
//            physx::PxRigidDynamic* projectileActor = m_physics->createRigidDynamic(initialPosePx);
//            if (!projectileActor) { RF_PHYSICS_ERROR("CreatePhysicsProjectileActor: m_physics->createRigidDynamic failed for projectile ID %llu.", gameData.projectileId); return nullptr; }
//
//            physx::PxShape* projectileShape = nullptr;
//            CollisionFilterData sim_filter_data;
//            sim_filter_data.word0 = static_cast<physx::PxU32>(projectile_type);
//            physx::PxShapeFlags shape_flags = physx::PxShapeFlag::eSIMULATION_SHAPE | physx::PxShapeFlag::eSCENE_QUERY_SHAPE;
//
//
//            if (properties.halfHeight > 0.0f && properties.radius > 0.0f) { // Capsule
//                projectileShape = m_physics->createShape(physx::PxCapsuleGeometry(properties.radius, properties.halfHeight), *matToUse, true, shape_flags);
//            }
//            else if (properties.radius > 0.0f) { // Sphere
//                projectileShape = m_physics->createShape(physx::PxSphereGeometry(properties.radius), *matToUse, true, shape_flags);
//            }
//            else { RF_PHYSICS_ERROR("CreatePhysicsProjectileActor: Invalid projectile shape for projectile ID %llu.", gameData.projectileId); projectileActor->release(); return nullptr; }
//
//            if (!projectileShape) { RF_PHYSICS_ERROR("CreatePhysicsProjectileActor: m_physics->createShape failed for projectile ID %llu.", gameData.projectileId); projectileActor->release(); return nullptr; }
//
//            SetupShapeFiltering(projectileShape, sim_filter_data);
//            projectileActor->attachShape(*projectileShape);
//            projectileShape->release();
//
//            if (properties.mass > 0.0f) { physx::PxRigidBodyExt::updateMassAndInertia(*projectileActor, properties.mass); }
//            else { physx::PxRigidBodyExt::updateMassAndInertia(*projectileActor, 0.01f); RF_PHYSICS_WARN("CreatePhysicsProjectileActor: Projectile ID %llu mass <=0. Using 0.01kg.", gameData.projectileId); }
//
//            projectileActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !properties.enableGravity);
//            if (properties.enableCCD) { projectileActor->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_CCD, true); }
//
//            ProjectileGameData* userDataPtr = new ProjectileGameData(gameData);
//            projectileActor->userData = static_cast<void*>(userDataPtr);
//            projectileActor->setLinearVelocity(ToPxVec3(initialVelocity));
//            m_scene->addActor(*projectileActor);
//
//            RF_PHYSICS_INFO("PhysicsEngine: Launched projectile ID %llu (Type: %d), Owner ID %llu.", gameData.projectileId, static_cast<int>(projectile_type), gameData.ownerId);
//            return projectileActor;
//        }
//
//
//        // RiftStepSweepQueryFilterCallback (same as before)
//        struct RiftStepSweepQueryFilterCallback : public physx::PxQueryFilterCallback {
//            physx::PxRigidActor* m_actorToIgnore;
//            RiftStepSweepQueryFilterCallback(physx::PxRigidActor* actorToIgnore) : m_actorToIgnore(actorToIgnore) {}
//
//            virtual physx::PxQueryHitType::Enum preFilter(
//                const physx::PxFilterData& PxShapeFilterData,
//                const physx::PxShape* shape,
//                const physx::PxRigidActor* hitActor,
//                physx::PxHitFlags& queryFlags) override
//            {
//                PX_UNUSED(queryFlags); PX_UNUSED(shape);
//                if (hitActor == m_actorToIgnore) return physx::PxQueryHitType::eNONE;
//
//                EPhysicsObjectType hitObjectType = static_cast<EPhysicsObjectType>(PxShapeFilterData.word0);
//
//                if (hitObjectType == EPhysicsObjectType::WALL || hitObjectType == EPhysicsObjectType::IMPASSABLE_ROCK || hitObjectType == EPhysicsObjectType::STATIC_IMPASSABLE) {
//                    RF_PHYSICS_TRACE("RiftStep Sweep: Hit DENSE obstacle type %u.", static_cast<unsigned int>(hitObjectType));
//                    return physx::PxQueryHitType::eBLOCK;
//                }
//                if (hitObjectType == EPhysicsObjectType::PLAYER_CHARACTER ||
//                    hitObjectType == EPhysicsObjectType::SMALL_ENEMY ||
//                    hitObjectType == EPhysicsObjectType::SMALL_ROCK) {
//                    RF_PHYSICS_TRACE("RiftStep Sweep: Passing through MINOR type %u.", static_cast<unsigned int>(hitObjectType));
//                    return physx::PxQueryHitType::eNONE;
//                }
//                RF_PHYSICS_TRACE("RiftStep Sweep: Hit generic obstacle type %u, treating as BLOCK.", static_cast<unsigned int>(hitObjectType));
//                return physx::PxQueryHitType::eBLOCK;
//            }
//            virtual physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData& filterData, const physx::PxQueryHit& hit, const physx::PxShape* shape, const physx::PxRigidActor* actor) override { PX_UNUSED(filterData); PX_UNUSED(hit); PX_UNUSED(shape); PX_UNUSED(actor); return physx::PxQueryHitType::eBLOCK; }
//        };
//
//
//        bool PhysicsEngine::CapsuleSweepSingle(
//            const SharedVec3& start_pos, const SharedQuaternion& orientation,
//            float radius, float half_height, const SharedVec3& unit_direction,
//            float max_distance, HitResult& out_hit_result,
//            physx::PxRigidActor* actor_to_ignore,
//            const physx::PxQueryFilterData& filter_data,
//            physx::PxQueryFilterCallback* filter_callback_override
//        ) {
//            if (!m_scene || !m_physics) { RF_PHYSICS_ERROR("PhysicsEngine::CapsuleSweepSingle: Scene/Physics not initialized."); out_hit_result.distance = 0; return true; }
//            if (max_distance <= 0.0f) { out_hit_result.distance = 0; return false; }
//
//            physx::PxTransform initial_pose(ToPxVec3(start_pos), ToPxQuat(orientation));
//            physx::PxCapsuleGeometry capsule_geometry(radius, half_height);
//            physx::PxVec3 sweep_direction_px = ToPxVec3(unit_direction);
//            physx::PxSweepBuffer sweep_buffer;
//
//            RiftStepSweepQueryFilterCallback default_riftstep_filter(actor_to_ignore);
//            physx::PxQueryFilterCallback* actual_filter_callback = filter_callback_override ? filter_callback_override : &default_riftstep_filter;
//            physx::PxHitFlags hit_flags = physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL | physx::PxHitFlag::eMESH_BOTH_SIDES | physx::PxHitFlag::eASSUME_NO_INITIAL_OVERLAP;
//            std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//            bool hit_found = m_scene->sweep(capsule_geometry, initial_pose, sweep_direction_px, max_distance, sweep_buffer, hit_flags, filter_data, actual_filter_callback);
//
//            if (hit_found && sweep_buffer.hasBlock) {
//                const physx::PxSweepHit& blocking_hit = sweep_buffer.block;
//                out_hit_result.hit_actor = blocking_hit.actor;
//                out_hit_result.hit_shape = blocking_hit.shape;
//                out_hit_result.hit_point = FromPxVec3(blocking_hit.position);
//                out_hit_result.hit_normal = FromPxVec3(blocking_hit.normal);
//                out_hit_result.distance = blocking_hit.distance;
//                out_hit_result.hit_face_index = blocking_hit.faceIndex;
//                out_hit_result.hit_entity_id = (blocking_hit.actor && blocking_hit.actor->userData) ? reinterpret_cast<uint64_t>(blocking_hit.actor->userData) : 0;
//                RF_PHYSICS_DEBUG("CapsuleSweepSingle: Blocking hit found at distance {}.", blocking_hit.distance);
//                return true;
//            }
//            RF_PHYSICS_DEBUG("CapsuleSweepSingle: No blocking hit found within {}m.", max_distance);
//            out_hit_result.distance = max_distance;
//            return false;
//        }
//
//        bool PhysicsEngine::RaycastSingle(
//            const SharedVec3& start, const SharedVec3& unit_direction, float max_distance,
//            HitResult& out_hit, const physx::PxQueryFilterData& filter_data_from_caller,
//            physx::PxQueryFilterCallback* filter_callback
//        ) {
//            if (!m_scene) { RF_PHYSICS_ERROR("PhysicsEngine::RaycastSingle: Scene not initialized."); return false; }
//            if (max_distance <= 0.0f) { return false; }
//
//            physx::PxVec3 px_origin = ToPxVec3(start);
//            physx::PxVec3 px_unit_dir = ToPxVec3(unit_direction);
//            physx::PxHitFlags hit_flags = physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL | physx::PxHitFlag::eFACE_INDEX;
//            physx::PxRaycastBuffer hit_buffer;
//            bool did_hit = false;
//
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                did_hit = m_scene->raycast(px_origin, px_unit_dir, max_distance, hit_buffer, hit_flags, filter_data_from_caller, filter_callback);
//            }
//
//            if (did_hit && hit_buffer.hasBlock) {
//                out_hit.hit_actor = hit_buffer.block.actor;
//                out_hit.hit_shape = hit_buffer.block.shape;
//                out_hit.hit_entity_id = (hit_buffer.block.actor && hit_buffer.block.actor->userData) ? reinterpret_cast<uint64_t>(hit_buffer.block.actor->userData) : 0;
//                out_hit.hit_point = FromPxVec3(hit_buffer.block.position);
//                out_hit.hit_normal = FromPxVec3(hit_buffer.block.normal);
//                out_hit.distance = hit_buffer.block.distance;
//                out_hit.hit_face_index = hit_buffer.block.faceIndex;
//                return true;
//            }
//            return false;
//        }
//
//        std::vector<HitResult> PhysicsEngine::RaycastMultiple(
//            const SharedVec3& start, const SharedVec3& unit_direction, float max_distance,
//            uint32_t max_hits_param, const physx::PxQueryFilterData& filter_data_from_caller,
//            physx::PxQueryFilterCallback* filter_callback
//        ) {
//            std::vector<HitResult> results;
//            if (!m_scene || max_hits_param == 0 || max_distance <= 0.0f) { return results; }
//
//            physx::PxVec3 px_origin = ToPxVec3(start);
//            physx::PxVec3 px_unit_dir = ToPxVec3(unit_direction);
//            physx::PxRaycastBufferN<16> hit_buffer; // Or use heap allocated if max_hits_param > 16
//            physx::PxHitFlags hit_flags = physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL | physx::PxHitFlag::eFACE_INDEX | physx::PxHitFlag::eMESH_MULTIPLE;
//            bool did_hit_anything = false;
//
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                did_hit_anything = m_scene->raycast(px_origin, px_unit_dir, max_distance, hit_buffer, hit_flags, filter_data_from_caller, filter_callback);
//            }
//
//            if (did_hit_anything) {
//                results.reserve(hit_buffer.getNbAnyHits());
//                for (physx::PxU32 i = 0; i < hit_buffer.getNbAnyHits(); ++i) {
//                    const physx::PxRaycastHit& touch = hit_buffer.getAnyHit(i);
//                    HitResult res;
//                    res.hit_actor = touch.actor;
//                    res.hit_shape = touch.shape;
//                    res.hit_entity_id = (touch.actor && touch.actor->userData) ? reinterpret_cast<uint64_t>(touch.actor->userData) : 0;
//                    res.hit_point = FromPxVec3(touch.position);
//                    res.hit_normal = FromPxVec3(touch.normal);
//                    res.distance = touch.distance;
//                    res.hit_face_index = touch.faceIndex;
//                    results.push_back(res);
//                }
//            }
//            return results;
//        }
//
//        std::vector<HitResult> PhysicsEngine::OverlapMultiple(
//            const physx::PxGeometry& geometry, const physx::PxTransform& pose,
//            uint32_t max_hits_alloc, const physx::PxQueryFilterData& filter_data_from_caller,
//            physx::PxQueryFilterCallback* filter_callback
//        ) {
//            std::vector<HitResult> results;
//            if (!m_scene || max_hits_alloc == 0) { return results; }
//
//            std::vector<physx::PxOverlapHit> touches_buffer_mem(max_hits_alloc);
//            physx::PxOverlapBuffer hit_buffer(touches_buffer_mem.data(), max_hits_alloc);
//
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                m_scene->overlap(geometry, pose, hit_buffer, filter_data_from_caller, filter_callback);
//            }
//
//            uint32_t nb_hits = hit_buffer.getNbAnyHits();
//            if (nb_hits > 0) {
//                results.reserve(nb_hits);
//                for (physx::PxU32 i = 0; i < nb_hits; ++i) {
//                    const physx::PxOverlapHit& touch = hit_buffer.getAnyHit(i);
//                    HitResult res;
//                    res.hit_actor = touch.actor;
//                    res.hit_shape = touch.shape;
//                    res.hit_entity_id = (touch.actor && touch.actor->userData) ? reinterpret_cast<uint64_t>(touch.actor->userData) : 0;
//                    res.distance = 0.0f;
//                    res.hit_face_index = touch.faceIndex;
//                    results.push_back(res);
//                }
//            }
//            return results;
//        }
//
//        void PhysicsEngine::ApplyForceToActor(physx::PxRigidBody* actor, const SharedVec3& force, physx::PxForceMode::Enum mode, bool wakeup) {
//            if (!actor) { RF_PHYSICS_WARN("PhysicsEngine::ApplyForceToActor: Null actor passed."); return; }
//            physx::PxVec3 px_force = ToPxVec3(force);
//            {
//                std::lock_guard<std::mutex> physics_lock(m_physicsMutex);
//                if (actor->getScene()) {
//                    actor->addForce(px_force, mode, wakeup);
//                }
//                else { /* RF_PHYSICS_WARN("Actor not in scene"); */ }
//            }
//        }
//
//        void PhysicsEngine::ApplyForceToActorById(uint64_t entity_id, const SharedVec3& force, physx::PxForceMode::Enum mode, bool wakeup) {
//            physx::PxRigidActor* actor_base = GetRigidActor(entity_id);
//            if (!actor_base) {
//                physx::PxController* controller = GetPlayerController(entity_id);
//                if (controller) { actor_base = controller->getActor(); }
//            }
//            if (actor_base && actor_base->is<physx::PxRigidBody>()) {
//                ApplyForceToActor(static_cast<physx::PxRigidBody*>(actor_base), force, mode, wakeup);
//            }
//            else { RF_PHYSICS_WARN("PhysicsEngine::ApplyForceToActorById: No PxRigidBody for entity ID {}.", entity_id); }
//        }
//
//        void PhysicsEngine::CreateRadialForceField(uint64_t instigator_id, const SharedVec3& center, float strength, float radius, float duration_sec, bool is_push, float falloff) { /* ... conceptual stub ... */ }
//        void PhysicsEngine::ApplyLocalizedGravity(const SharedVec3& center, float strength, float radius, float duration_sec, const SharedVec3& gravity_direction) { /* ... conceptual stub ... */ }
//        bool PhysicsEngine::DeformTerrainRegion(const SharedVec3& impact_point, float radius, float depth_or_intensity, int deformation_type) { /* ... conceptual stub ... */ return false; }
//

    //} // namespace Physics
//} // namespace RiftForged