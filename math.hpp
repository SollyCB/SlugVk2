#ifndef SOL_MATH_HPP_INCLUDE_GUARD_
#define SOL_MATH_HPP_INCLUDE_GUARD_

struct Vec3 {
    float x;
    float y;
    float z;
};
Vec3 get_vec3(float x, float y, float z) {
    return {x, y, z};
}

struct Vec2 {
    float x;
    float y;
    float z;
};
Vec3 get_vec2(float x, float y) {
    return {x, y};
}

#endif // include guard
