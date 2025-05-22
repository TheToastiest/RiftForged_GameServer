#pragma once

// Assuming Vec3 and Quaternion structs are in SharedProtocols/Generated/ and included there.
// This file provides utility functions.
#include "../FlatBuffers/V0.0.1/riftforged_common_types_generated.h" // For Vec3 & Quaternion
#include <cmath> // For sqrt, sin, cos, acos

// For M_PI if not defined on Windows with <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace RiftForged {
    namespace Utilities {
        namespace Math {

            using Networking::Shared::Vec3; // Type alias for convenience
            using Networking::Shared::Quaternion; // Type alias

            const float PI_F = static_cast<float>(M_PI);
            const float DEG_TO_RAD_FACTOR = PI_F / 180.0f;
            const float RAD_TO_DEG_FACTOR = 180.0f / PI_F;
            const float QUATERNION_NORMALIZATION_EPSILON = 0.00001f;
            const float VECTOR_NORMALIZATION_EPSILON = 0.00001f;


            // Normalizes a Vec3
            inline Vec3 NormalizeVector(const Vec3& v) {
                float mag_sq = v.x() * v.x() + v.y() * v.y() + v.z() * v.z();
                if (mag_sq > VECTOR_NORMALIZATION_EPSILON * VECTOR_NORMALIZATION_EPSILON) { // Check against squared epsilon to avoid sqrt if possible
                    float mag = std::sqrt(mag_sq);
                    return Vec3(v.x() / mag, v.y() / mag, v.z() / mag);
                }
                return Vec3(0.0f, 0.0f, 0.0f); // Or return original if very small, or a default forward
            }

            // Normalizes a Quaternion
            inline Quaternion NormalizeQuaternion(const Quaternion& q) {
                float mag_sq = q.x() * q.x() + q.y() * q.y() + q.z() * q.z() + q.w() * q.w();
                if (mag_sq > QUATERNION_NORMALIZATION_EPSILON * QUATERNION_NORMALIZATION_EPSILON &&
                    std::abs(mag_sq - 1.0f) > QUATERNION_NORMALIZATION_EPSILON) { // Avoid sqrt if already normalized
                    float mag = std::sqrt(mag_sq);
                    return Quaternion(q.x() / mag, q.y() / mag, q.z() / mag, q.w() / mag);
                }
                return q; // Return original if too small to normalize or already normalized
            }

            // Creates a Quaternion representing a rotation around an axis
            inline Quaternion FromAngleAxis(float angle_degrees, const Vec3& axis) {
                float angle_radians = angle_degrees * DEG_TO_RAD_FACTOR;
                float half_angle = angle_radians * 0.5f;
                float s = std::sin(half_angle);
                Vec3 normalized_axis = NormalizeVector(axis);
                return NormalizeQuaternion(Quaternion(
                    normalized_axis.x() * s,
                    normalized_axis.y() * s,
                    normalized_axis.z() * s,
                    std::cos(half_angle)
                ));
            }

            // Multiplies two quaternions (q_result = q_first * q_second)
            // Order matters: q_first represents the initial orientation, q_second is the rotation to apply.
            // Or q_result = q_rotation * q_original_orientation for applying world rotation to local.
            // Let's define q_new = q_rotation_to_apply * q_current_orientation
            inline Quaternion MultiplyQuaternions(const Quaternion& q_rot, const Quaternion& q_current) {
                return NormalizeQuaternion(Quaternion(
                    q_rot.w() * q_current.x() + q_rot.x() * q_current.w() + q_rot.y() * q_current.z() - q_rot.z() * q_current.y(),
                    q_rot.w() * q_current.y() - q_rot.x() * q_current.z() + q_rot.y() * q_current.w() + q_rot.z() * q_current.x(),
                    q_rot.w() * q_current.z() + q_rot.x() * q_current.y() - q_rot.y() * q_current.x() + q_rot.z() * q_current.w(),
                    q_rot.w() * q_current.w() - q_rot.x() * q_current.x() - q_rot.y() * q_current.y() - q_rot.z() * q_current.z()
                ));
            }

            // Rotates a vector by a quaternion (v_rotated = q * v * q_conjugate)
            inline Vec3 RotateVectorByQuaternion(const Vec3& v, const Quaternion& q) {
                // Create a pure quaternion from the vector
                Quaternion p(v.x(), v.y(), v.z(), 0.0f);

                // Conjugate of q
                Quaternion q_conj(-q.x(), -q.y(), -q.z(), q.w()); // Assuming q is normalized

                // v_rotated_quat = q * p * q_conj
                Quaternion temp = MultiplyQuaternions(q, p);
                Quaternion rotated_p_quat = MultiplyQuaternions(temp, q_conj);

                return Vec3(rotated_p_quat.x(), rotated_p_quat.y(), rotated_p_quat.z());
            }

            // Gets the world-space forward vector (+Y local) from an orientation quaternion
            inline Vec3 GetWorldForwardVector(const Quaternion& orientation) {
                // Rotate local forward vector (0, 1, 0) by the orientation quaternion
                return RotateVectorByQuaternion(Vec3(0.0f, 1.0f, 0.0f), orientation);
            }

            // Gets the world-space right vector (+X local) from an orientation quaternion
            inline Vec3 GetWorldRightVector(const Quaternion& orientation) {
                // Rotate local right vector (1, 0, 0) by the orientation quaternion
                return RotateVectorByQuaternion(Vec3(1.0f, 0.0f, 0.0f), orientation);
            }

            // Gets the world-space up vector (+Z local) from an orientation quaternion
            inline Vec3 GetWorldUpVector(const Quaternion& orientation) {
                // Rotate local up vector (0, 0, 1) by the orientation quaternion
                return RotateVectorByQuaternion(Vec3(0.0f, 0.0f, 1.0f), orientation);
            }

        } // namespace Math
    } // namespace Utilities
} // namespace RiftForged