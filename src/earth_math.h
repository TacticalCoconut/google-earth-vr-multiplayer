#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace EarthMath {

// Exact Earth radius in Keyhole Space from Earth.exe RVA 0xfe13d0 (6371010.0 meters)
constexpr double kEarthRadiusMeters = 6371010.0;
constexpr double kPi = 3.14159265358979323846;

struct Vec3f {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3f() = default;
    Vec3f(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct Vec3d {
    double x = 0.0, y = 0.0, z = 0.0;
    Vec3d() = default;
    Vec3d(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vec3d operator+(const Vec3d& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3d operator-(const Vec3d& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3d operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3d operator/(double s) const { return {x / s, y / s, z / s}; }

    double Dot(const Vec3d& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3d Cross(const Vec3d& o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }
    double LengthSq() const { return x * x + y * y + z * z; }
    double Length() const { return std::sqrt(LengthSq()); }
    Vec3d Normalized() const {
        double len = Length();
        return (len > 1e-12) ? (*this / len) : Vec3d{0.0, 1.0, 0.0};
    }
    Vec3f ToFloat() const { return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)}; }
};

// Matches Earth.exe's ion::math::Rotationd / Quaternion layout: (qx, qy, qz, qw)
struct Quatd {
    double x = 0.0, y = 0.0, z = 0.0, w = 1.0;
    Quatd() = default;
    Quatd(double x_, double y_, double z_, double w_) : x(x_), y(y_), z(z_), w(w_) {}

    Quatd Conjugate() const { return {-x, -y, -z, w}; }

    double NormSq() const { return x * x + y * y + z * z + w * w; }

    Quatd Normalized() const {
        double n = std::sqrt(NormSq());
        if (n < 1e-12) return {0.0, 0.0, 0.0, 1.0};
        return {x / n, y / n, z / n, w / n};
    }

    Quatd Inverse() const {
        double n2 = NormSq();
        if (n2 < 1e-12) return {0.0, 0.0, 0.0, 1.0};
        return {-x / n2, -y / n2, -z / n2, w / n2};
    }

    // Hamilton product: this * o
    Quatd operator*(const Quatd& o) const {
        return {
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w,
            w * o.w - x * o.x - y * o.y - z * o.z
        };
    }

    // Rotate 3D vector v by unit quaternion q: q * v * q^-1
    Vec3d Rotate(const Vec3d& v) const {
        Vec3d u{x, y, z};
        Vec3d uv = u.Cross(v);
        Vec3d uuv = u.Cross(uv);
        return v + (uv * w + uuv) * 2.0;
    }

    // Slerp for smooth remote avatar interpolation
    static Quatd Slerp(const Quatd& a, const Quatd& b, double t) {
        Quatd b_adj = b;
        double cos_half = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        if (cos_half < 0.0) {
            b_adj = {-b.x, -b.y, -b.z, -b.w};
            cos_half = -cos_half;
        }
        if (cos_half > 0.9995) {
            return Quatd{
                a.x + t * (b_adj.x - a.x),
                a.y + t * (b_adj.y - a.y),
                a.z + t * (b_adj.z - a.z),
                a.w + t * (b_adj.w - a.w)
            }.Normalized();
        }
        double half_angle = std::acos(std::clamp(cos_half, -1.0, 1.0));
        double sin_half = std::sin(half_angle);
        double ra = std::sin((1.0 - t) * half_angle) / sin_half;
        double rb = std::sin(t * half_angle) / sin_half;
        return {
            a.x * ra + b_adj.x * rb,
            a.y * ra + b_adj.y * rb,
            a.z * ra + b_adj.z * rb,
            a.w * ra + b_adj.w * rb
        };
    }

    // Convert 3x3 upper-left of a row-major 4x4 float matrix to Quatd
    static Quatd FromRowMajorMat4f(const float* m) {
        double m00 = m[0], m01 = m[1], m02 = m[2];
        double m10 = m[4], m11 = m[5], m12 = m[6];
        double m20 = m[8], m21 = m[9], m22 = m[10];
        double trace = m00 + m11 + m22;
        Quatd q;
        if (trace > 0.0) {
            double s = 0.5 / std::sqrt(trace + 1.0);
            q.w = 0.25 / s;
            q.x = (m21 - m12) * s;
            q.y = (m02 - m20) * s;
            q.z = (m10 - m01) * s;
        } else if (m00 > m11 && m00 > m22) {
            double s = 2.0 * std::sqrt(1.0 + m00 - m11 - m22);
            q.w = (m21 - m12) / s;
            q.x = 0.25 * s;
            q.y = (m01 + m10) / s;
            q.z = (m02 + m20) / s;
        } else if (m11 > m22) {
            double s = 2.0 * std::sqrt(1.0 + m11 - m00 - m22);
            q.w = (m02 - m20) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25 * s;
            q.z = (m12 + m21) / s;
        } else {
            double s = 2.0 * std::sqrt(1.0 + m22 - m00 - m11);
            q.w = (m10 - m01) / s;
            q.x = (m02 + m20) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25 * s;
        }
        return q.Normalized();
    }

    // Write 3x3 rotation into a row-major 4x4 float matrix (with scale and translation)
    void ToRowMajorMat4f(float* out, const Vec3d& translation, double scale = 1.0) const {
        double xx = x * x, yy = y * y, zz = z * z;
        double xy = x * y, xz = x * z, yz = y * z;
        double wx = w * x, wy = w * y, wz = w * z;

        out[0]  = static_cast<float>((1.0 - 2.0 * (yy + zz)) * scale);
        out[1]  = static_cast<float>((2.0 * (xy - wz)) * scale);
        out[2]  = static_cast<float>((2.0 * (xz + wy)) * scale);
        out[3]  = static_cast<float>(translation.x);

        out[4]  = static_cast<float>((2.0 * (xy + wz)) * scale);
        out[5]  = static_cast<float>((1.0 - 2.0 * (xx + zz)) * scale);
        out[6]  = static_cast<float>((2.0 * (yz - wx)) * scale);
        out[7]  = static_cast<float>(translation.y);

        out[8]  = static_cast<float>((2.0 * (xz - wy)) * scale);
        out[9]  = static_cast<float>((2.0 * (yz + wx)) * scale);
        out[10] = static_cast<float>((1.0 - 2.0 * (xx + yy)) * scale);
        out[11] = static_cast<float>(translation.z);

        out[12] = 0.0f;
        out[13] = 0.0f;
        out[14] = 0.0f;
        out[15] = 1.0f;
    }
};

// Exact 64-byte Sim3d structure from Earth.exe (TransformManager + 0x98):
// start_space_from_keyhole_space_
//   +0x00: double translation[3] (planet center in Start Space)
//   +0x18: double rotation[4]    (qx, qy, qz, qw: Keyhole -> Start Space)
//   +0x38: double scale          (Start Space meters per Keyhole meter = 1.0 / viewer_scale)
#pragma pack(push, 1)
struct Sim3d {
    Vec3d translation;
    Quatd rotation;
    double scale = 1.0;

    // Matches Earth.exe 0x2ae130: p_start = t + s * (q * p_keyhole * q^-1)
    Vec3d TransformPointFromKeyholeToStart(const Vec3d& p_keyhole) const {
        return translation + rotation.Rotate(p_keyhole) * scale;
    }

    // Matches Earth.exe 0x2aea90: p_keyhole = (1/s) * (q^-1 * (p_start - t) * q)
    Vec3d TransformPointFromStartToKeyhole(const Vec3d& p_start) const {
        double inv_s = (std::abs(scale) > 1e-30) ? (1.0 / scale) : 1.0;
        return rotation.Conjugate().Rotate(p_start - translation) * inv_s;
    }

    Quatd TransformRotFromKeyholeToStart(const Quatd& q_keyhole) const {
        return (rotation * q_keyhole).Normalized();
    }

    Quatd TransformRotFromStartToKeyhole(const Quatd& q_start) const {
        return (rotation.Conjugate() * q_start).Normalized();
    }

    // Viewer scale relative to real Earth (1.0 = human size, 1e6 = giant in space)
    double GetViewerScale() const {
        return (std::abs(scale) > 1e-30) ? (1.0 / scale) : 1.0;
    }

    bool IsValid() const {
        if (!std::isfinite(scale) || scale <= 1e-20 || scale > 1e10) return false;
        if (!std::isfinite(translation.x) || !std::isfinite(translation.y) || !std::isfinite(translation.z)) return false;
        double n2 = rotation.NormSq();
        return std::isfinite(n2) && n2 > 0.5 && n2 < 1.5;
    }
};
#pragma pack(pop)
static_assert(sizeof(Sim3d) == 64, "Sim3d must be exactly 64 bytes to match Earth.exe");

// Matches Earth.exe 0x2ad5d0: Keyhole (X, Y, Z) -> (Latitude deg, Longitude deg, Altitude m)
inline void KeyholeToLatLonAlt(const Vec3d& p_kh, double& out_lat_deg, double& out_lon_deg, double& out_alt_m) {
    double r = p_kh.Length();
    if (r < 1e-6) {
        out_lat_deg = 0.0;
        out_lon_deg = 0.0;
        out_alt_m = -kEarthRadiusMeters;
        return;
    }
    out_alt_m = r - kEarthRadiusMeters;
    double sin_lat = std::clamp(p_kh.y / r, -1.0, 1.0);
    out_lat_deg = std::asin(sin_lat) * (180.0 / kPi);
    double lon_rad = -std::atan2(p_kh.z, p_kh.x) - (kPi * 0.5);
    while (lon_rad <= -kPi) lon_rad += 2.0 * kPi;
    while (lon_rad > kPi) lon_rad -= 2.0 * kPi;
    out_lon_deg = lon_rad * (180.0 / kPi);
}

// Inverse of 0x2ad5d0: (Latitude deg, Longitude deg, Altitude m) -> Keyhole (X, Y, Z)
inline Vec3d LatLonAltToKeyhole(double lat_deg, double lon_deg, double alt_m) {
    double lat = lat_deg * (kPi / 180.0);
    double lon = lon_deg * (kPi / 180.0);
    double r = kEarthRadiusMeters + alt_m;
    double angle = -lon - (kPi * 0.5);
    double cos_lat = std::cos(lat);
    return {
        r * cos_lat * std::cos(angle),
        r * std::sin(lat),
        r * cos_lat * std::sin(angle)
    };
}

// Analytical test: does the line segment between two Keyhole-Space positions pass through the Earth sphere?
inline bool IsOccludedByEarthSphere(const Vec3d& eye_kh, const Vec3d& target_kh) {
    Vec3d ab = target_kh - eye_kh;
    double ab_len_sq = ab.LengthSq();
    if (ab_len_sq < 1e-6) return false;
    double t = -(eye_kh.Dot(ab)) / ab_len_sq;
    if (t <= 0.01 || t >= 0.99) return false;
    Vec3d closest = eye_kh + ab * t;
    // Slightly below mean sphere (0.9985 * R ~ 9.5 km margin for ocean/valleys)
    constexpr double kOcclusionRadius = kEarthRadiusMeters * 0.9985;
    return closest.LengthSq() < (kOcclusionRadius * kOcclusionRadius);
}

// Row-major 4x4 float matrix multiplication: out = A * B
inline void Mat4fMultiplyRowMajor(float* out, const float* A, const float* B) {
    float tmp[16];
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            tmp[r * 4 + c] =
                A[r * 4 + 0] * B[0 * 4 + c] +
                A[r * 4 + 1] * B[1 * 4 + c] +
                A[r * 4 + 2] * B[2 * 4 + c] +
                A[r * 4 + 3] * B[3 * 4 + c];
        }
    }
    std::memcpy(out, tmp, sizeof(tmp));
}

} // namespace EarthMath
