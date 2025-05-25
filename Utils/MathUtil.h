#pragma once

// Assuming Vec3 and Quaternion structs are in SharedProtocols/Generated/ and included there.
// This file provides utility functions.
#include "../FlatBuffers/V0.0.3/riftforged_common_types_generated.h" // For Vec3 & Quaternion
#include <cmath> // For sqrt, sin, cos, acos

// For M_PI if not defined on Windows with <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace RiftForged {
    namespace Utilities {
        namespace Math {

            // Type aliases for convenience (if not already present or preferred)
            using Vec3 = RiftForged::Networking::Shared::Vec3;
            using Quaternion = RiftForged::Networking::Shared::Quaternion;
            // Magnitude
			inline float Magnitude(const Vec3& v) {
				return std::sqrt(v.x() * v.x() + v.y() * v.y() + v.z() * v.z());
			}

            const float PI_F = static_cast<float>(M_PI);
            const float DEG_TO_RAD_FACTOR = PI_F / 180.0f;
            const float RAD_TO_DEG_FACTOR = 180.0f / PI_F;
            const float QUATERNION_NORMALIZATION_EPSILON = 0.00001f;
            const float VECTOR_NORMALIZATION_EPSILON = 0.00001f;


            // --- Vector Operations ---

            // Normalizes a Vec3
            inline Vec3 NormalizeVector(const Vec3& v) {
                float mag_sq = v.x() * v.x() + v.y() * v.y() + v.z() * v.z();
                if (mag_sq > VECTOR_NORMALIZATION_EPSILON * VECTOR_NORMALIZATION_EPSILON) {
                    float mag = std::sqrt(mag_sq);
                    return Vec3(v.x() / mag, v.y() / mag, v.z() / mag);
                }
                return Vec3(0.0f, 0.0f, 0.0f); // Return zero vector if magnitude is too small
            }

            // ***** NEWLY ADDED: AddVectors *****
            inline Vec3 AddVectors(const Vec3& v1, const Vec3& v2) {
                return Vec3(v1.x() + v2.x(), v1.y() + v2.y(), v1.z() + v2.z());
            }
            // ***********************************

            // ***** NEWLY ADDED: ScaleVector *****
            inline Vec3 ScaleVector(const Vec3& v, float scalar) {
                return Vec3(v.x() * scalar, v.y() * scalar, v.z() * scalar);
            }
            // ************************************

            // ***** NEWLY ADDED (Optional but useful): SubtractVectors *****
            inline Vec3 SubtractVectors(const Vec3& v1, const Vec3& v2) {
                return Vec3(v1.x() - v2.x(), v1.y() - v2.y(), v1.z() - v2.z());
            }
            // *************************************************************

            // ***** NEWLY ADDED (Optional but useful): DotProduct *****
            inline float DotProduct(const Vec3& v1, const Vec3& v2) {
                return v1.x() * v2.x() + v1.y() * v2.y() + v1.z() * v2.z();
            }
            // ********************************************************

            // ***** NEWLY ADDED (Optional but useful): DistanceSquared *****
            inline float DistanceSquared(const Vec3& v1, const Vec3& v2) {
                float dx = v1.x() - v2.x();
                float dy = v1.y() - v2.y();
                float dz = v1.z() - v2.z();
                return dx * dx + dy * dy + dz * dz;
            }
            // *************************************************************

            // ***** NEWLY ADDED (Optional but useful): Distance *****
            inline float Distance(const Vec3& v1, const Vec3& v2) {
                return std::sqrt(DistanceSquared(v1, v2));
            }
            // *****************************************************


            // --- Quaternion Operations ---

            // Normalize a Quaternion
            inline Quaternion NormalizeQuaternion(const Quaternion& q) {
                float mag_sq = q.x() * q.x() + q.y() * q.y() + q.z() * q.z() + q.w() * q.w();
                if (mag_sq > QUATERNION_NORMALIZATION_EPSILON * QUATERNION_NORMALIZATION_EPSILON) {
                    float mag = std::sqrt(mag_sq);
                    return Quaternion(q.x() / mag, q.y() / mag, q.z() / mag, q.w() / mag);
                }
                return Quaternion(0.0f, 0.0f, 0.0f, 1.0f); // Identity quaternion for zero magnitude
            }

            // Create Quaternion from Angle-Axis
            inline Quaternion FromAngleAxis(float angle_degrees, const Vec3& axis) {
                float angle_rad = angle_degrees * DEG_TO_RAD_FACTOR;
                float half_angle = angle_rad * 0.5f;
                float s = std::sin(half_angle);
                Vec3 norm_axis = NormalizeVector(axis);
                return Quaternion(norm_axis.x() * s, norm_axis.y() * s, norm_axis.z() * s, std::cos(half_angle));
            }

            // Multiply Quaternions (q2 * q1 applies q1's rotation then q2's)
            inline Quaternion MultiplyQuaternions(const Quaternion& q1, const Quaternion& q2) {
                return Quaternion(
                    q1.w() * q2.x() + q1.x() * q2.w() + q1.y() * q2.z() - q1.z() * q2.y(),
                    q1.w() * q2.y() - q1.x() * q2.z() + q1.y() * q2.w() + q1.z() * q2.x(),
                    q1.w() * q2.z() + q1.x() * q2.y() - q1.y() * q2.x() + q1.z() * q2.w(),
                    q1.w() * q2.w() - q1.x() * q2.x() - q1.y() * q2.y() - q1.z() * q2.z()
                );
            }

            // Rotate a Vector by a Quaternion
            inline Vec3 RotateVectorByQuaternion(const Vec3& v, const Quaternion& q) {
                Quaternion p(v.x(), v.y(), v.z(), 0.0f); // Pure quaternion for the vector
                Quaternion q_conj(-q.x(), -q.y(), -q.z(), q.w()); // Conjugate of q
                Quaternion result_q = MultiplyQuaternions(MultiplyQuaternions(q, p), q_conj);
                return Vec3(result_q.x(), result_q.y(), result_q.z());
            }

            // Get World Forward Vector (assuming +Y is forward in local object space)
            inline Vec3 GetWorldForwardVector(const Quaternion& orientation) {
                return RotateVectorByQuaternion(Vec3(0.0f, 1.0f, 0.0f), orientation);
            }

            // Get World Right Vector (assuming +X is right in local object space)
            inline Vec3 GetWorldRightVector(const Quaternion& orientation) {
                return RotateVectorByQuaternion(Vec3(1.0f, 0.0f, 0.0f), orientation);
            }

            // Get World Up Vector (assuming +Z is up in local object space)
            inline Vec3 GetWorldUpVector(const Quaternion& orientation) {
                return RotateVectorByQuaternion(Vec3(0.0f, 0.0f, 1.0f), orientation);
            }

            // TODO: Add other math utilities as needed (e.g., matrix operations, Lerp, Slerp)

        } // namespace Math
    } // namespace Utilities
} // namespace RiftForged