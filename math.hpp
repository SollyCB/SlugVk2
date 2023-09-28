#ifndef SOL_MATH_HPP_INCLUDE_GUARD_
#define SOL_MATH_HPP_INCLUDE_GUARD_

static inline int pow(int num, int exp) {
    int accum = 1;
    for(int i = 0; i < exp; ++i) {
        accum *= num;
    }
    return accum;
}

struct Vec4 {
    float x;
    float y;
    float z;
    float w;
};
inline static Vec4 get_vec4(float x, float y, float z, float w) {
    return {x, y, z, w};
}

struct Vec3 {
    float x;
    float y;
    float z;
};
inline static Vec3 get_vec3(float x, float y, float z) {
    return {x, y, z};
}
inline static float sum_vec3(Vec3 vec3) {
     return vec3.x + vec3.y + vec3.z;
}

struct Vec2 {
    float x;
    float y;
};
inline static Vec2 get_vec2(float x, float y) {
    return {x, y};
}

struct Mat4 {
    Vec4 row0;
    Vec4 row1;
    Vec4 row2;
    Vec4 row3;
};

inline static Vec4 mul_mat4_vec4(Mat4 mat, Vec4 vec) {
    Vec4 ret;
    ret.x = vec.x * mat.row0.x + vec.y * mat.row0.y + vec.z * mat.row0.z + vec.w * mat.row0.w;
    ret.y = vec.x * mat.row1.x + vec.y * mat.row1.y + vec.z * mat.row1.z + vec.w * mat.row1.w;
    ret.z = vec.x * mat.row2.x + vec.y * mat.row2.y + vec.z * mat.row2.z + vec.w * mat.row2.w;
    ret.w = vec.x * mat.row3.x + vec.y * mat.row3.y + vec.z * mat.row3.z + vec.w * mat.row3.w;
    return ret;
}

#endif // include guard
