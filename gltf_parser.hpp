#pragma once

#include "nlohmann/json.hpp"
#define V_LAYERS true
#include "Array.hpp"
#include "String.hpp"

#include <cstdint>
#include <limits>

namespace Sol {

extern const int GLTF_INVALID_INDEX;
extern const uint32_t GLTF_INVALID_COUNT;
extern const float GLTF_INVALID_FLOAT;
extern const uint8_t GLTF_PNG_BYTE_PATTERN[8];
extern const uint8_t GLTF_JPG_BYTE_PATTERN[3];
extern const int GLTF_NEAREST_FALLBACK;
extern const int GLTF_LINEAR_FALLBACK;

bool gltf_read_json(const char *file, nlohmann::json *json);
struct glTF {

    // Asset
    struct Asset {
        // TODO:: Add minVerison support
        StringBuffer version;
        StringBuffer copyright;

        void fill(nlohmann::json json);
    };

    // Scenes
    struct Scene {
        Array<int32_t> nodes;
        StringBuffer name;

        void fill(nlohmann::json json);
    };
    struct Scenes {
        Array<Scene> scenes;
        int scene = GLTF_INVALID_INDEX;
        void fill(nlohmann::json json);
    };

    // Nodes
    struct Node {
        Array<float> rotation;
        Array<float> scale;
        Array<float> translation;
        Array<float> matrix;
        Array<float> weights;

        Array<int32_t> children;
        StringBuffer name;
        int mesh = GLTF_INVALID_INDEX;
        int skin = GLTF_INVALID_INDEX;
        int camera = GLTF_INVALID_INDEX;

        void fill(nlohmann::json json);
    };
    struct Nodes {
        Array<Node> nodes;
        void fill(nlohmann::json json);
    };

    // Buffers & BufferViews
    struct Buffer {
        uint32_t byte_length;
        StringBuffer uri;
        void fill(nlohmann::json json);
    };
    struct Buffers {
        Array<Buffer> buffers;
        void fill(nlohmann::json json);
    };
    struct BufferView {
        enum Target {
            NONE = 0,
            ARRAY_BUFFER = 34962,
            ELEMENT_ARRAY_BUFFER = 34963,
        };
        uint32_t byte_length = GLTF_INVALID_COUNT;
        uint32_t byte_offset = GLTF_INVALID_COUNT;
        uint32_t byte_stride = GLTF_INVALID_COUNT;
        int buffer = GLTF_INVALID_INDEX;
        Target target = NONE;

        void fill(nlohmann::json json);
    };
    struct BufferViews {
        Array<BufferView> views;
        void fill(nlohmann::json json);
    };

    // Accessors
    struct Accessor {
        enum Type {
            SCALAR,
            VEC2,
            VEC3,
            VEC4,
            MAT2,
            MAT3,
            MAT4,
        };
        enum ComponentType {
            NONE = 0,
            INT8 = 5120,
            UINT8 = 5121,
            INT16 = 5122,
            UINT16 = 5123,
            UINT32 = 5125,
            FLOAT = 5126,
        };

        struct Sparse {
            struct Indices {
                uint32_t byte_offset = GLTF_INVALID_COUNT;
                int buffer_view = GLTF_INVALID_INDEX;
                ComponentType component_type = NONE;
                void fill(nlohmann::json json);
            };
            struct Values {
                uint32_t byte_offset = GLTF_INVALID_COUNT;
                int buffer_view = GLTF_INVALID_INDEX;
                void fill(nlohmann::json json);
            };
            Indices indices;
            Values values;
            uint32_t count = GLTF_INVALID_COUNT;
            void fill(nlohmann::json json);
        };

        Sparse sparse;
        Array<float> max;
        Array<float> min;
        Type type;
        ComponentType component_type = NONE;

        uint32_t byte_offset = GLTF_INVALID_COUNT;
        uint32_t count = GLTF_INVALID_COUNT;
        int buffer_view = GLTF_INVALID_INDEX;

        void fill(nlohmann::json json);
    };
    struct Accessors {
        Array<Accessor> accessors;
        void fill(nlohmann::json json);
    };

    // Meshes
    struct Mesh {
        struct Primitive {
            struct Attribute {
                StringBuffer key;
                int accessor = GLTF_INVALID_INDEX;
            };
            struct Target {
                Array<Attribute> attributes;
                void fill(nlohmann::json json);
            };

            Array<Attribute> attributes;
            Array<Target> targets;
            int indices = GLTF_INVALID_INDEX;
            int material = GLTF_INVALID_INDEX;
            int mode = GLTF_INVALID_INDEX;

            static void fill_attrib_array(nlohmann::json json, Array<Attribute> *attributes);
            void fill(nlohmann::json json);
        };
        struct Extras {
            Array<StringBuffer> target_names;
            void fill(nlohmann::json json);
        };

        Array<Primitive> primitives;
        Array<float> weights;
        Extras extras;

        void fill(nlohmann::json json);
    };
    struct Meshes {
        Array<Mesh> meshes;
        void fill(nlohmann::json json);
    };

    // Skins
    struct Skin {
        Array<int32_t> joints;
        int i_bind_matrices = GLTF_INVALID_INDEX;
        int skeleton = GLTF_INVALID_INDEX;
        void fill(nlohmann::json json);
    };
    struct Skins {
        Array<Skin> skins;
        void fill(nlohmann::json json);
    };

    // Textures
    struct Texture {
        int sampler = GLTF_INVALID_INDEX;
        int source = GLTF_INVALID_INDEX;
        void fill(nlohmann::json json);
    };
    struct Textures {
        Array<Texture> textures;
        void fill(nlohmann::json json);
    };

    // Images
    struct Image {
        enum MimeType {
            NONE,
            PNG,
            JPG,
        };
        StringBuffer uri;
        MimeType mime_type = NONE;
        int buffer_view = GLTF_INVALID_INDEX;

        void fill(nlohmann::json json);
    };
    struct Images {
        Array<Image> images;
        void fill(nlohmann::json json);
    };

    // Samplers
    struct Sampler {
        enum class Filter {
            NONE = 0,
            NEAREST = 9728,
            LINEAR = 9729,
            NEAREST_MIPMAP_NEAREST = 9984,
            LINEAR_MIPMAP_NEAREST = 9985,
            NEAREST_MIPMAP_LINEAR = 9986,
            LINEAR_MIPMAP_LINEAR = 9987,
        };
        enum class Wrap {
            NONE = 0,
            CLAMP_TO_EDGE = 33071,
            MIRRORED_REPEAT = 33648,
            REPEAT = 10497,
        };
        Filter mag_filter = Filter::NONE;
        Filter min_filter = Filter::NONE;
        Wrap wrap_s = Wrap::NONE;
        Wrap wrap_t = Wrap::NONE;

        void fill(nlohmann::json json);
    };
    struct Samplers {
        Array<Sampler> samplers;
        void fill(nlohmann::json json);
    };

    // Materials
    struct Material {
        struct MatTexture {
            float scale = GLTF_INVALID_FLOAT;
            int index = GLTF_INVALID_INDEX;
            int tex_coord = GLTF_INVALID_INDEX;

            void fill_tex(nlohmann::json json, const char *key);
        };
        struct PbrMetallicRoughness {
            Array<float> base_color_factor;
            MatTexture base_color_texture;
            MatTexture metallic_roughness_texture;
            float metallic_factor = GLTF_INVALID_FLOAT;
            float roughness_factor = GLTF_INVALID_FLOAT;

            void fill(nlohmann::json json);
        };
        enum AlphaMode {
            OPAQUE,
            MASK,
            BLEND,
        };

        PbrMetallicRoughness pbr_metallic_roughness;
        Array<float> emissive_factor;
        StringBuffer name;
        MatTexture normal_texture;
        MatTexture occlusion_texture;
        MatTexture emissive_texture;
        AlphaMode alpha_mode = OPAQUE;
        float alpha_cutoff = GLTF_INVALID_FLOAT;
        bool double_sided = false;

        void fill(nlohmann::json json);
    };
    struct Materials {
        Array<Material> materials;
        void fill(nlohmann::json json);
    };

    // Cameras
    struct Camera {
        enum Type {
            UNKNOWN,
            ORTHO,
            PERSPECTIVE,
        };
        StringBuffer name;
        Type type = UNKNOWN;
        float aspect_ratio = GLTF_INVALID_FLOAT;
        float yfov = GLTF_INVALID_FLOAT;
        float xmag = GLTF_INVALID_FLOAT;
        float ymag = GLTF_INVALID_FLOAT;
        float zfar = GLTF_INVALID_FLOAT;
        float znear = GLTF_INVALID_FLOAT;

        void fill(nlohmann::json json);
    };
    struct Cameras {
        Array<Camera> cameras;
        void fill(nlohmann::json json);
    };

    // Animations
    struct Animation {
        struct Channel {
            struct Target {
                enum Path {
                    NONE,
                    TRANSLATION,
                    ROTATION,
                    SCALE,
                    WEIGHTS,
                }; // Path

                int node = GLTF_INVALID_INDEX;
                Path path = NONE;
            }; // Target

            Target target;
            int sampler = GLTF_INVALID_INDEX;

            void fill(nlohmann::json json);
        }; // Channel

        struct Sampler {
            enum Interpolation {
                LINEAR,
                STEP,
                CUBICSPLINE,
            };
            Interpolation interpolation;
            int input = GLTF_INVALID_INDEX;
            int output = GLTF_INVALID_INDEX;

            void fill(nlohmann::json json);
        }; // Sampler

        // NOTE: Accessor comp_type normalisation rules (see spec, right before
        // "Specifying Extensions"...)
        Array<Channel> channels;
        Array<Sampler> samplers;
        StringBuffer name;

        void fill(nlohmann::json json);
    };
    struct Animations {
        Array<Animation> animations;
        void fill(nlohmann::json json);
    };

// glTF
    static bool get(const char* file_name, glTF *gltf);
    Asset asset;
    Scenes scenes;
    Nodes nodes;
    Buffers buffers;
    BufferViews buffer_views;
    Accessors accessors;
    Meshes meshes;
    Skins skins;
    Textures textures;
    Images images;
    Samplers samplers;
    Materials materials;
    Cameras cameras;
    Animations animations;

#if TEST
static void run_tests(); 
#endif
};
} // namespace Sol
