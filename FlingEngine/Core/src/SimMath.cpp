#include "SimMath.h"

#include <cmath>
#include <limits>

namespace Fling::SimMath
{
    namespace
    {
        // Every constant is a double written in hex, so no compiler rounds a decimal literal its own way. Series
        // coefficients are exact rationals (1/n!, 1/(2n+1)) rounded once to the nearest double. Only +, -, *, /,
        // sqrt, floor, fmod, frexp and ldexp are used below: IEEE 754 defines each of them exactly.

        constexpr double Pi = 0x1.921fb54442d18p+1;
        constexpr double HalfPi = 0x1.921fb54442d18p+0;
        constexpr double QuarterPi = 0x1.921fb54442d18p-1;
        constexpr double SixthPi = 0x1.0c152382d7366p-1;
        constexpr double TwoOverPi = 0x1.45f306dc9c883p-1;

        /** pi/2 in two parts (Cody-Waite): the first 33 bits, so k * HalfPiHi is exact for |k| < 2^20, and the rest. */
        constexpr double HalfPiHi = 0x1.921fb54400000p+0;
        constexpr double HalfPiLo = 0x1.0b4611a626331p-34;

        constexpr double Sqrt3 = 0x1.bb67ae8584caap+0;
        constexpr double TanTwelfthPi = 0x1.126145e9ecd56p-2;
        constexpr double SqrtHalf = 0x1.6a09e667f3bcdp-1;

        constexpr double Log2E = 0x1.71547652b82fep+0;
        /** ln 2 in two parts: the first 32 bits, so k * Ln2Hi is exact for |k| < 2^21, and the rest. */
        constexpr double Ln2Hi = 0x1.62e42fee00000p-1;
        constexpr double Ln2Lo = 0x1.a39ef35793c76p-33;

        /** The float nearest a double, with overflow made explicit (the cast is undefined out of range). */
        float ToFloat(const double t_Value)
        {
            // The midpoint between FLT_MAX and the next power of two rounds to infinity (FLT_MAX is odd)
            if (std::fabs(t_Value) >= 0x1.ffffffp+127)
            {
                return t_Value < 0.0 ? -std::numeric_limits<float>::infinity() : std::numeric_limits<float>::infinity();
            }
            return static_cast<float>(t_Value);
        }

        constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
        constexpr double Infinity = std::numeric_limits<double>::infinity();

        // sin(r) = r + r^3 * (S3 + r^2 * (S5 + ...)): the first left-out term is below 1e-19 for |r| <= pi/4
        double SinKernel(const double r)
        {
            // sin(-0) is -0, but -0 + (+0) below would be +0
            if (r == 0.0)
            {
                return r;
            }
            const double r2 = r * r;
            const double Poly =
                -0x1.5555555555555p-3 +
                r2 * (0x1.1111111111111p-7 +
                      r2 * (-0x1.a01a01a01a01ap-13 +
                            r2 * (0x1.71de3a556c734p-19 +
                                  r2 * (-0x1.ae64567f544e4p-26 +
                                        r2 * (0x1.6124613a86d09p-33 +
                                              r2 * (-0x1.ae7f3e733b81fp-41 + r2 * 0x1.952c77030ad4ap-49))))));
            return r + r * r2 * Poly;
        }

        // cos(r) = 1 + r^2 * (C2 + r^2 * (C4 + ...)): the first left-out term is below 1e-20 for |r| <= pi/4
        double CosKernel(const double r)
        {
            const double r2 = r * r;
            const double Poly =
                -0x1.0000000000000p-1 +
                r2 * (0x1.5555555555555p-5 +
                      r2 * (-0x1.6c16c16c16c17p-10 +
                            r2 * (0x1.a01a01a01a01ap-16 +
                                  r2 * (-0x1.27e4fb7789f5cp-22 +
                                        r2 * (0x1.1eed8eff8d898p-29 +
                                              r2 * (-0x1.93974a8c07c9dp-37 +
                                                    r2 * (0x1.ae7f3e733b81fp-45 + r2 * -0x1.6827863b97d97p-53)))))));
            return 1.0 + r2 * Poly;
        }

        /** sin and cos of a finite x, in double. */
        void SinCosD(const double x, double& t_OutSin, double& t_OutCos)
        {
            const double k = std::floor(x * TwoOverPi + 0.5);
            const double r = (x - k * HalfPiHi) - k * HalfPiLo;
            // k is a whole number, so this is exact; for k beyond 2^53 it is 0, which is still the same everywhere
            const int Quadrant = static_cast<int>(k - 4.0 * std::floor(k * 0.25));
            const double s = SinKernel(r);
            const double c = CosKernel(r);
            switch (Quadrant)
            {
            case 0:
                t_OutSin = s;
                t_OutCos = c;
                break;
            case 1:
                t_OutSin = c;
                t_OutCos = -s;
                break;
            case 2:
                t_OutSin = -s;
                t_OutCos = -c;
                break;
            default:
                t_OutSin = -c;
                t_OutCos = s;
                break;
            }
        }

        // atan(t) = t + t^3 * (A3 + t^2 * (A5 + ...)): the first left-out term is below 1e-20 for |t| <= tan(pi/12)
        double AtanSeries(const double t)
        {
            const double t2 = t * t;
            constexpr double A[] = {-0x1.5555555555555p-2,
                                    0x1.999999999999ap-3,
                                    -0x1.2492492492492p-3,
                                    0x1.c71c71c71c71cp-4,
                                    -0x1.745d1745d1746p-4,
                                    0x1.3b13b13b13b14p-4,
                                    -0x1.1111111111111p-4,
                                    0x1.e1e1e1e1e1e1ep-5,
                                    -0x1.af286bca1af28p-5,
                                    0x1.8618618618618p-5,
                                    -0x1.642c8590b2164p-5,
                                    0x1.47ae147ae147bp-5,
                                    -0x1.2f684bda12f68p-5,
                                    0x1.1a7b9611a7b96p-5,
                                    -0x1.0842108421084p-5};
            constexpr int Count = static_cast<int>(sizeof(A) / sizeof(A[0]));
            double Poly = A[Count - 1];
            for (int i = Count - 2; i >= 0; --i)
            {
                Poly = A[i] + t2 * Poly;
            }
            return t + t * t2 * Poly;
        }

        /** atan of t >= 0 (infinity allowed), in double. */
        double AtanPositive(const double t)
        {
            if (t > 1.0)
            {
                // atan(t) = pi/2 - atan(1/t)
                return HalfPi - AtanPositive(1.0 / t);
            }
            if (t > TanTwelfthPi)
            {
                // atan(t) = pi/6 + atan(u) with u = (t * sqrt3 - 1) / (t + sqrt3), and |u| <= tan(pi/12)
                return SixthPi + AtanSeries((t * Sqrt3 - 1.0) / (t + Sqrt3));
            }
            return AtanSeries(t);
        }

        /** atan2 in double, with std::atan2's results for zeros and infinities. */
        double Atan2D(const double y, const double x)
        {
            if (std::isnan(x) || std::isnan(y))
            {
                return std::numeric_limits<double>::quiet_NaN();
            }
            const bool YNegative = std::signbit(y);
            const bool XNegative = std::signbit(x);
            const double AbsY = std::fabs(y);
            const double AbsX = std::fabs(x);
            double Angle = 0.0;
            if (AbsY == 0.0)
            {
                Angle = XNegative ? Pi : 0.0;
            }
            else if (AbsX == 0.0)
            {
                Angle = HalfPi;
            }
            else if (std::isinf(AbsY) && std::isinf(AbsX))
            {
                Angle = XNegative ? 3.0 * QuarterPi : QuarterPi;
            }
            else
            {
                // Inside the first quadrant, then moved to x's side
                const double First = std::isinf(AbsY) ? HalfPi : AtanPositive(AbsY / AbsX);
                Angle = XNegative ? Pi - First : First;
            }
            return YNegative ? -Angle : Angle;
        }

        /** e^x in double. */
        double ExpD(const double x)
        {
            if (std::isnan(x))
            {
                return x;
            }
            if (x > 709.0)
            {
                return Infinity;
            }
            if (x < -746.0)
            {
                return 0.0;
            }
            // x = k ln2 + r with |r| <= ln2/2, then e^x = 2^k e^r
            const double k = std::floor(x * Log2E + 0.5);
            const double r = (x - k * Ln2Hi) - k * Ln2Lo;
            // e^r by Taylor terms to r^16: the first left-out term is below 1e-22 for |r| <= ln2/2
            constexpr double E[] = {0x1.0000000000000p-1,
                                    0x1.5555555555555p-3,
                                    0x1.5555555555555p-5,
                                    0x1.1111111111111p-7,
                                    0x1.6c16c16c16c17p-10,
                                    0x1.a01a01a01a01ap-13,
                                    0x1.a01a01a01a01ap-16,
                                    0x1.71de3a556c734p-19,
                                    0x1.27e4fb7789f5cp-22,
                                    0x1.ae64567f544e4p-26,
                                    0x1.1eed8eff8d898p-29,
                                    0x1.6124613a86d09p-33,
                                    0x1.93974a8c07c9dp-37,
                                    0x1.ae7f3e733b81fp-41,
                                    0x1.ae7f3e733b81fp-45};
            constexpr int Count = static_cast<int>(sizeof(E) / sizeof(E[0]));
            double Poly = E[Count - 1];
            for (int i = Count - 2; i >= 0; --i)
            {
                Poly = E[i] + r * Poly;
            }
            const double ExpR = 1.0 + r + r * r * Poly;
            return std::ldexp(ExpR, static_cast<int>(k));
        }

        /** ln x in double. */
        double LogD(const double x)
        {
            if (std::isnan(x) || x < 0.0)
            {
                return std::numeric_limits<double>::quiet_NaN();
            }
            if (x == 0.0)
            {
                return -Infinity;
            }
            if (std::isinf(x))
            {
                return x;
            }
            // x = m 2^e with m in [sqrt(1/2), sqrt(2)), then ln x = e ln2 + ln m
            int e = 0;
            double m = std::frexp(x, &e);
            if (m < SqrtHalf)
            {
                m *= 2.0;
                --e;
            }
            // ln m = 2 (s + s^3/3 + s^5/5 + ...) with s = (m - 1) / (m + 1), |s| <= 0.1716
            const double s = (m - 1.0) / (m + 1.0);
            const double s2 = s * s;
            constexpr double L[] = {0x1.5555555555555p-2,
                                    0x1.999999999999ap-3,
                                    0x1.2492492492492p-3,
                                    0x1.c71c71c71c71cp-4,
                                    0x1.745d1745d1746p-4,
                                    0x1.3b13b13b13b14p-4,
                                    0x1.1111111111111p-4,
                                    0x1.e1e1e1e1e1e1ep-5,
                                    0x1.af286bca1af28p-5,
                                    0x1.8618618618618p-5,
                                    0x1.642c8590b2164p-5,
                                    0x1.47ae147ae147bp-5};
            constexpr int Count = static_cast<int>(sizeof(L) / sizeof(L[0]));
            double Poly = L[Count - 1];
            for (int i = Count - 2; i >= 0; --i)
            {
                Poly = L[i] + s2 * Poly;
            }
            const double LnM = 2.0 * (s + s * s2 * Poly);
            const double E = static_cast<double>(e);
            return E * Ln2Hi + (E * Ln2Lo + LnM);
        }

        bool IsWhole(const double t_Value)
        {
            return std::floor(t_Value) == t_Value;
        }

        bool IsOddWhole(const double t_Value)
        {
            return IsWhole(t_Value) && std::fmod(std::fabs(t_Value), 2.0) == 1.0;
        }
    } // namespace

    void SinCos(const float t_Radians, float& t_OutSin, float& t_OutCos)
    {
        if (!std::isfinite(t_Radians))
        {
            t_OutSin = NaN;
            t_OutCos = NaN;
            return;
        }
        double s = 0.0;
        double c = 0.0;
        SinCosD(static_cast<double>(t_Radians), s, c);
        t_OutSin = static_cast<float>(s);
        t_OutCos = static_cast<float>(c);
    }

    float Sin(const float t_Radians)
    {
        float Sine = 0.0f;
        float Cosine = 0.0f;
        SinCos(t_Radians, Sine, Cosine);
        return Sine;
    }

    float Cos(const float t_Radians)
    {
        float Sine = 0.0f;
        float Cosine = 0.0f;
        SinCos(t_Radians, Sine, Cosine);
        return Cosine;
    }

    float Tan(const float t_Radians)
    {
        if (!std::isfinite(t_Radians))
        {
            return NaN;
        }
        double s = 0.0;
        double c = 0.0;
        SinCosD(static_cast<double>(t_Radians), s, c);
        return ToFloat(s / c);
    }

    float Asin(const float t_Value)
    {
        const double x = static_cast<double>(t_Value);
        if (!(std::fabs(x) <= 1.0))
        {
            return NaN;
        }
        // (1 - x)(1 + x) is exact in double for a float x, unlike 1 - x * x near |x| = 1
        return static_cast<float>(Atan2D(x, std::sqrt((1.0 - x) * (1.0 + x))));
    }

    float Acos(const float t_Value)
    {
        const double x = static_cast<double>(t_Value);
        if (!(std::fabs(x) <= 1.0))
        {
            return NaN;
        }
        return static_cast<float>(Atan2D(std::sqrt((1.0 - x) * (1.0 + x)), x));
    }

    float Atan(const float t_Value)
    {
        if (std::isnan(t_Value))
        {
            return NaN;
        }
        const double x = static_cast<double>(t_Value);
        const double Angle = AtanPositive(std::fabs(x));
        return static_cast<float>(std::signbit(x) ? -Angle : Angle);
    }

    float Atan2(const float t_Y, const float t_X)
    {
        return static_cast<float>(Atan2D(static_cast<double>(t_Y), static_cast<double>(t_X)));
    }

    float Exp(const float t_Value)
    {
        return ToFloat(ExpD(static_cast<double>(t_Value)));
    }

    float Log(const float t_Value)
    {
        return static_cast<float>(LogD(static_cast<double>(t_Value)));
    }

    float Pow(const float t_Base, const float t_Exponent)
    {
        const double x = static_cast<double>(t_Base);
        const double y = static_cast<double>(t_Exponent);
        // The cases std::pow defines whatever the other argument is
        if (y == 0.0 || x == 1.0)
        {
            return 1.0f;
        }
        if (std::isnan(x) || std::isnan(y))
        {
            return NaN;
        }
        double Sign = 1.0;
        double AbsX = x;
        if (std::signbit(x))
        {
            if (!std::isinf(y) && !IsWhole(y))
            {
                return NaN;
            }
            Sign = IsOddWhole(y) ? -1.0 : 1.0;
            AbsX = -x;
        }
        if (AbsX == 0.0)
        {
            return static_cast<float>(Sign * (y > 0.0 ? 0.0 : Infinity));
        }
        if (std::isinf(AbsX))
        {
            return static_cast<float>(Sign * (y > 0.0 ? Infinity : 0.0));
        }
        if (std::isinf(y))
        {
            // |x| = 1 (x = -1 here) gives 1; else 0 or infinity by which side of 1 |x| is on
            if (AbsX == 1.0)
            {
                return 1.0f;
            }
            return static_cast<float>((AbsX > 1.0) == (y > 0.0) ? Infinity : 0.0);
        }
        return ToFloat(Sign * ExpD(y * LogD(AbsX)));
    }

    glm::quat AngleAxis(const float t_Radians, const glm::vec3& t_Axis)
    {
        float SinHalf = 0.0f;
        float CosHalf = 0.0f;
        SinCos(t_Radians * 0.5f, SinHalf, CosHalf);
        return glm::quat(CosHalf, t_Axis * SinHalf);
    }

    glm::quat FromYawPitchRoll(const float t_Yaw, const float t_Pitch, const float t_Roll)
    {
        // glm::yawPitchRoll's matrix, term for term. quat_cast uses only arithmetic and sqrt.
        float SinYaw = 0.0f;
        float CosYaw = 0.0f;
        float SinPitch = 0.0f;
        float CosPitch = 0.0f;
        float SinRoll = 0.0f;
        float CosRoll = 0.0f;
        SinCos(t_Yaw, SinYaw, CosYaw);
        SinCos(t_Pitch, SinPitch, CosPitch);
        SinCos(t_Roll, SinRoll, CosRoll);

        glm::mat3 M;
        M[0][0] = CosYaw * CosRoll + SinYaw * SinPitch * SinRoll;
        M[0][1] = SinRoll * CosPitch;
        M[0][2] = -SinYaw * CosRoll + CosYaw * SinPitch * SinRoll;
        M[1][0] = -CosYaw * SinRoll + SinYaw * SinPitch * CosRoll;
        M[1][1] = CosRoll * CosPitch;
        M[1][2] = SinRoll * SinYaw + CosYaw * SinPitch * CosRoll;
        M[2][0] = SinYaw * CosPitch;
        M[2][1] = -SinPitch;
        M[2][2] = CosYaw * CosPitch;
        return glm::quat_cast(M);
    }

    void ToYawPitchRoll(const glm::mat3& t_Rotation, float& t_OutYaw, float& t_OutPitch, float& t_OutRoll)
    {
        // glm::extractEulerAngleYXZ, term for term
        const glm::mat3& M = t_Rotation;
        const float Yaw = Atan2(M[2][0], M[2][2]);
        const float CosPitch = std::sqrt(M[0][1] * M[0][1] + M[1][1] * M[1][1]);
        const float Pitch = Atan2(-M[2][1], CosPitch);
        float SinYaw = 0.0f;
        float CosYaw = 0.0f;
        SinCos(Yaw, SinYaw, CosYaw);
        t_OutYaw = Yaw;
        t_OutPitch = Pitch;
        t_OutRoll = Atan2(SinYaw * M[1][2] - CosYaw * M[1][0], CosYaw * M[0][0] - SinYaw * M[0][2]);
    }

    glm::quat Slerp(const glm::quat& t_From, const glm::quat& t_To, const float t_Amount)
    {
        // glm::slerp, step for step
        glm::quat To = t_To;
        float CosTheta = glm::dot(t_From, t_To);
        if (CosTheta < 0.0f)
        {
            To = -t_To;
            CosTheta = -CosTheta;
        }
        if (CosTheta > 1.0f - std::numeric_limits<float>::epsilon())
        {
            return glm::quat(glm::mix(t_From.w, To.w, t_Amount),
                             glm::mix(t_From.x, To.x, t_Amount),
                             glm::mix(t_From.y, To.y, t_Amount),
                             glm::mix(t_From.z, To.z, t_Amount));
        }
        const float Angle = Acos(CosTheta);
        return (Sin((1.0f - t_Amount) * Angle) * t_From + Sin(t_Amount * Angle) * To) / Sin(Angle);
    }
} // namespace Fling::SimMath
