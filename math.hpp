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

#endif // include guard
