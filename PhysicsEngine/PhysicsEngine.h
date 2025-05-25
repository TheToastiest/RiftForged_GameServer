// File: PhysicsEngine/PhysicsEngine.h
// Copyright (c) 2023-2025 RiftForged Game Development Team
#pragma once

#include "physx/PxPhysicsAPI.h"
#include "physx/PxPhysics.h"
#include "physx/foundation/PxMath.h"
#include "physx/extensions/PxDefaultCpuDispatcher.h"
#include "physx/characterkinematic/PxController.h"
#include "physx/cooking/PxCooking.h"
#include "physx/PxQueryFiltering.h"
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include "../FlatBuffers/V0.0.3/riftforged_common_types_generated.h"
#include "../Utils/Logger.h"

namespace physx {
    class PxShape;
    class PxGeometry;
    class PxRigidActor;
    class PxRigidBody;
    class PxRigidStatic;
    class PxRigidDynamic;
    class PxMaterial;
    class PxScene;
    class PxPhysics;
    class PxDefaultCpuDispatcher;
    class PxFoundation;
    class PxPvd;
    class PxPvdTransport;
    class PxControllerManager;
    struct PxFilterData;
    class PxQueryFilterCallback;
    class PxController;
    class PxTriangleMesh;
    class PxConvexMesh;
    class PxHeightField;
    class PxCooking;
    class PxController;
    class PxExtendedVector;
	class PxControllerManager;
	struct PxQueryFilterData;
}

namespace RiftForged {
    namespace Physics {

        using SharedVec3 = RiftForged::Networking::Shared::Vec3;
        using SharedQuaternion = RiftForged::Networking::Shared::Quaternion;

        inline physx::PxVec3 ToPxVec3(const SharedVec3& v) { return physx::PxVec3(v.x(), v.y(), v.z()); }
        inline SharedVec3 FromPxVec3(const physx::PxVec3& pv) { return SharedVec3(pv.x, pv.y, pv.z); }
        inline physx::PxQuat ToPxQuat(const SharedQuaternion& q) { return physx::PxQuat(q.x(), q.y(), q.z(), q.w()); }
        inline SharedQuaternion FromPxQuat(const physx::PxQuat& pq) { return SharedQuaternion(pq.x, pq.y, pq.z, pq.w); }

        struct HitResult {
            uint64_t hit_entity_id = 0;
            physx::PxActor* hit_actor = nullptr;
            physx::PxShape* hit_shape = nullptr;
            SharedVec3 hit_point;
            SharedVec3 hit_normal;
            float distance = -1.0f;
            uint32_t hit_face_index = physx::PxHitFlag::eFACE_INDEX;
        };

        struct CollisionFilterData {
            uint32_t word0 = 0;
            uint32_t word1 = 0;
            uint32_t word2 = 0;
            uint32_t word3 = 0;
        };

        class PhysicsEngine {
        public:
            PhysicsEngine();
            ~PhysicsEngine();

            PhysicsEngine(const PhysicsEngine&) = delete;
            PhysicsEngine& operator=(const PhysicsEngine&) = delete;

            bool Initialize(const SharedVec3& gravity = SharedVec3(0.0f, 0.0f, -9.81f), bool connect_to_pvd = true);
            void Shutdown();
            void StepSimulation(float delta_time_sec);

            physx::PxMaterial* CreateMaterial(float static_friction, float dynamic_friction, float restitution);

            physx::PxController* CreateCharacterController(uint64_t player_id, const SharedVec3& initial_position, float radius, float height, physx::PxMaterial* material = nullptr, void* user_data_for_controller_actor = nullptr);
            void RegisterPlayerController(uint64_t player_id, physx::PxController* controller);
            void UnregisterPlayerController(uint64_t player_id);
            physx::PxController* GetPlayerController(uint64_t player_id) const;

            // Sets the world orientation of the character controller's actor associated with player_id.
            bool SetCharacterControllerOrientation(uint64_t player_id, const SharedQuaternion& orientation);

            uint32_t MoveCharacterController(physx::PxController* controller, const SharedVec3& world_space_displacement, float delta_time_sec, const std::vector<physx::PxController*>& other_controllers_to_ignore = {});
            void SetCharacterControllerPose(physx::PxController* controller, const SharedVec3& world_position);
            SharedVec3 GetCharacterControllerPosition(physx::PxController* controller) const;
            void SetActorUserData(physx::PxActor* actor, void* userData);

            bool CapsuleSweepSingle(
                const SharedVec3& start_pos,
                const SharedQuaternion& orientation,
                float radius,
                float half_height,
                const SharedVec3& unit_direction,
                float max_distance,
                HitResult& out_hit_result,
                physx::PxRigidActor* actor_to_ignore,
                const physx::PxQueryFilterData& filter_data = physx::PxQueryFilterData(
                    physx::PxQueryFlags(physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER | physx::PxQueryFlag::ePOSTFILTER)
                ),
                physx::PxQueryFilterCallback* filter_callback = nullptr
            );

            physx::PxRigidStatic* CreateStaticBox(uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation, const SharedVec3& half_extents, physx::PxMaterial* material = nullptr, const CollisionFilterData& filter_data = {}, void* user_data = nullptr);
            physx::PxRigidStatic* CreateStaticSphere(uint64_t entity_id, const SharedVec3& position, float radius, physx::PxMaterial* material = nullptr, const CollisionFilterData& filter_data = {}, void* user_data = nullptr);
            physx::PxRigidStatic* CreateStaticCapsule(uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation, float radius, float half_height, physx::PxMaterial* material = nullptr, const CollisionFilterData& filter_data = {}, void* user_data = nullptr);
            physx::PxRigidStatic* CreateStaticPlane(const SharedVec3& normal, float distance, physx::PxMaterial* material = nullptr, const CollisionFilterData& filter_data = {});
            physx::PxRigidStatic* CreateStaticTriangleMesh(uint64_t entity_id, const std::vector<SharedVec3>& vertices, const std::vector<uint32_t>& indices, const SharedVec3& scale = SharedVec3(1.0f, 1.0f, 1.0f), physx::PxMaterial* material = nullptr, const CollisionFilterData& filter_data = {}, void* user_data = nullptr);

            physx::PxRigidDynamic* CreateDynamicBox(uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation, const SharedVec3& half_extents, float density, physx::PxMaterial* material = nullptr, const CollisionFilterData& filter_data = {}, void* user_data = nullptr);
            physx::PxRigidDynamic* CreateDynamicSphere(uint64_t entity_id, const SharedVec3& position, float radius, float density, physx::PxMaterial* material = nullptr, const CollisionFilterData& filter_data = {}, void* user_data = nullptr);
            physx::PxRigidDynamic* CreateDynamicCapsule(uint64_t entity_id, const SharedVec3& position, const SharedQuaternion& orientation, float radius, float half_height, float density, physx::PxMaterial* material = nullptr, const CollisionFilterData& filter_data = {}, void* user_data = nullptr);

            void RegisterRigidActor(uint64_t entity_id, physx::PxRigidActor* actor);
            void UnregisterRigidActor(uint64_t entity_id);
            physx::PxRigidActor* GetRigidActor(uint64_t entity_id) const;

            bool RaycastSingle(const SharedVec3& start, const SharedVec3& unit_direction, float max_distance, HitResult& out_hit, const physx::PxQueryFilterData& filter_data = physx::PxQueryFilterData(physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER), physx::PxQueryFilterCallback* filter_callback = nullptr);
            std::vector<HitResult> RaycastMultiple(const SharedVec3& start, const SharedVec3& unit_direction, float max_distance, uint32_t max_hits, const physx::PxQueryFilterData& filter_data = physx::PxQueryFilterData(physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER | physx::PxQueryFlag::eNO_BLOCK), physx::PxQueryFilterCallback* filter_callback = nullptr);
            std::vector<HitResult> OverlapMultiple(const physx::PxGeometry& geometry, const physx::PxTransform& pose, uint32_t max_hits, const physx::PxQueryFilterData& filter_data = physx::PxQueryFilterData(physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER | physx::PxQueryFlag::eNO_BLOCK | physx::PxQueryFlag::eANY_HIT), physx::PxQueryFilterCallback* filter_callback = nullptr);

            void ApplyForceToActor(physx::PxRigidBody* actor, const SharedVec3& force, physx::PxForceMode::Enum mode, bool wakeup = true);
            void ApplyForceToActorById(uint64_t entity_id, const SharedVec3& force, physx::PxForceMode::Enum mode, bool wakeup = true);

            void CreateRadialForceField(uint64_t instigator_id, const SharedVec3& center, float strength, float radius, float duration_sec, bool is_push, float falloff = 1.0f);
            void ApplyLocalizedGravity(const SharedVec3& center, float strength, float radius, float duration_sec, const SharedVec3& gravity_direction);

            bool DeformTerrainRegion(const SharedVec3& impact_point, float radius, float depth_or_intensity, int deformation_type);

            physx::PxScene* GetScene() const { return m_scene; }
            physx::PxPhysics* GetPhysics() const { return m_physics; }
            //physx::PxCooking* GetCooking() const { return m_cooking; }
            physx::PxFoundation* GetFoundation() const { return m_foundation; }
            void SetPvdTransport(physx::PxPvdTransport* transport) { m_pvd_transport = transport; }

        private:
            physx::PxFoundation* m_foundation = nullptr;
            physx::PxPhysics* m_physics = nullptr;
            physx::PxDefaultCpuDispatcher* m_dispatcher = nullptr;
            physx::PxScene* m_scene = nullptr;
            physx::PxMaterial* m_default_material = nullptr;
            physx::PxCooking* m_cooking = nullptr;
            physx::PxControllerManager* m_controller_manager = nullptr;
            std::map<uint64_t, physx::PxController*> m_playerControllers;
            mutable std::mutex m_playerControllersMutex;
            std::map<uint64_t, physx::PxRigidActor*> m_entityActors;
            mutable std::mutex m_entityActorsMutex;
            physx::PxPvd* m_pvd = nullptr;
            physx::PxPvdTransport* m_pvd_transport = nullptr;
			physx::PxQueryFilterData m_default_filter_data; // Default filter data for queries

            void SetupShapeFiltering(physx::PxShape* shape, const CollisionFilterData& filter_data);
        };

    } // namespace Physics
} // namespace RiftForged
