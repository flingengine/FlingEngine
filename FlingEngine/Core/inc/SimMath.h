#pragma once

#include "CoreAPI.h"
#include "FlingMath.h"

/**
 * Math for simulation code that gives the same bits on every platform and compiler, so a
 * Linux server and a Windows client simulate the same ticks.
 *
 * The rule. Exact in IEEE 754 on every platform: +, -, *, /, sqrt, floor, fmod, ldexp,
 * frexp and comparisons (with FMA contraction and fast math off, which
 * fling_set_deterministic_math does for every first-party target). Not exact: sin, cos,
 * tan, asin, acos, atan, atan2, exp, log, pow and the rest of <cmath>; glibc and the
 * Microsoft runtime round them differently. So simulation code (fixed systems, what they
 * call, and level loading) uses this file for those and never std:: or glm:: versions,
 * nor the glm helpers built on them (glm::angleAxis, glm::rotate, glm::yawPitchRoll,
 * glm::eulerAngles, glm::slerp, ...). scripts/check_sim_math.py, run by ctest, fails the
 * build's tests when simulation code names one. Presentation code (frame systems, the
 * renderer, the editor) may use std and glm freely.
 *
 * Every function uses only the exact operations, in a fixed order, on doubles, and rounds
 * once to float at the end. Results are within one ulp of the true value (for Sin, Cos and
 * Tan when |x| is below about 10^5; larger inputs give the same bits everywhere, with less
 * accuracy). The bits of each result are pinned in FlingTests/Core/SimMathTests.cpp.
 */
namespace Fling::SimMath
{
    CORE_API float Sin(float t_Radians);
    CORE_API float Cos(float t_Radians);

    /** Both at once, from one range reduction: the same values as Sin and Cos. */
    CORE_API void SinCos(float t_Radians, float& t_OutSin, float& t_OutCos);

    CORE_API float Tan(float t_Radians);

    /** In [-pi/2, pi/2]. NaN outside [-1, 1]. */
    CORE_API float Asin(float t_Value);

    /** In [0, pi]. NaN outside [-1, 1]. */
    CORE_API float Acos(float t_Value);

    /** In [-pi/2, pi/2]. */
    CORE_API float Atan(float t_Value);

    /** The angle of (t_X, t_Y), in [-pi, pi], with the signs of zero as in std::atan2. */
    CORE_API float Atan2(float t_Y, float t_X);

    CORE_API float Exp(float t_Value);

    /** Natural log. NaN below 0, -infinity at 0. */
    CORE_API float Log(float t_Value);

    /** t_Base to the t_Exponent. A negative base needs a whole exponent (else NaN). */
    CORE_API float Pow(float t_Base, float t_Exponent);

    /** glm::angleAxis with SimMath: a turn of t_Radians around t_Axis (which must be unit length). */
    CORE_API glm::quat AngleAxis(float t_Radians, const glm::vec3& t_Axis);

    /** glm::quat_cast(glm::yawPitchRoll(...)) with SimMath: Ry(yaw) * Rx(pitch) * Rz(roll). */
    CORE_API glm::quat FromYawPitchRoll(float t_Yaw, float t_Pitch, float t_Roll);

    /** glm::extractEulerAngleYXZ with SimMath: the inverse of FromYawPitchRoll, in radians. */
    CORE_API void ToYawPitchRoll(const glm::mat3& t_Rotation, float& t_OutYaw, float& t_OutPitch, float& t_OutRoll);

    /** glm::slerp with SimMath: the same steps (the short way around, linear when the two are very close). */
    CORE_API glm::quat Slerp(const glm::quat& t_From, const glm::quat& t_To, float t_Amount);
} // namespace Fling::SimMath
