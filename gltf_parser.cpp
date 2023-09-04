#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "glTF.hpp"
#include "nlohmann/json.hpp"

#if TEST
#include "test/test.hpp"
#endif

namespace Sol {
const int32_t GLTF_INVALID_INDEX = -1;
const uint32_t GLTF_INVALID_COUNT = UINT32_MAX;
const float GLTF_INVALID_FLOAT = std::numeric_limits<float>::max();

const uint8_t GLTF_PNG_BYTE_PATTERN[8] = {0x89, 0x50, 0x4E, 0x47,
                                     0x0D, 0x0A, 0x1A, 0x0A};
const uint8_t GLTF_JPG_BYTE_PATTERN[3] = {0xFF, 0xD8, 0xFF};

const int32_t GLTF_NEAREST_FALLBACK = 9728;
const int32_t GLTF_LINEAR_FALLBACK = 9729;

bool gltf_read_json(const char *file, nlohmann::json *json)
{
    std::ifstream f(file);
    if (!f.is_open())
        return false;

    *json = nlohmann::json::parse(f);
    f.close();
    return true;
}

bool glTF::get(const char* file_name, glTF *gltf)
{
    nlohmann::json json;
    bool ok = gltf_read_json(file_name, &json);
    if (!ok)
        return ok;

    gltf->asset.fill(json);
    gltf->scenes.fill(json);
    gltf->nodes.fill(json);
    gltf->buffers.fill(json);
    gltf->buffer_views.fill(json);
    gltf->accessors.fill(json);
    gltf->meshes.fill(json);
    gltf->skins.fill(json);
    gltf->textures.fill(json);
    gltf->images.fill(json);
    gltf->samplers.fill(json);
    gltf->materials.fill(json);
    gltf->cameras.fill(json);
    gltf->animations.fill(json);

    return ok;
}

namespace {
template <typename T> static bool load_T(nlohmann::json json, const char *key, T *obj)
{
    auto tmp = json.find(key);
    if (tmp == json.end())
        return false;

    *obj = tmp.value();
    return true;
}
static bool load_string(nlohmann::json json, const char *key, StringBuffer *str)
{
    auto obj = json.find(key);
    if (obj == json.end())
        return false;

    std::string tmp = json.value(key, "");
    str->init(tmp.length());
    str->copy_here(tmp, tmp.length());
    return true;
}
template <typename T>
static bool load_array(nlohmann::json json, const char *key, Array<T> *array)
{
    auto obj = json.find(key);
    if (obj == json.end())
        return false;

    size_t size = obj.value().size();
    array->init(size);
    return true;
}
template <typename T>
static void fill_obj_array(nlohmann::json json, const char *key, Array<T> *array)
{
    uint32_t i = 0;
    for (auto item : json[key]) {
        (*array)[i].fill(item);
        ++i;
    }
}
static void fill_str_array(nlohmann::json json, const char *key,
                           Array<StringBuffer> *array)
{
    std::string str;
    for (auto i : json[key]) {
        str = i;
        (*array)[i] = StringBuffer::get(str, str.length());
    }
}
} // namespace

// Asset /////////////////////////
void glTF::Asset::fill(nlohmann::json json)
{
    auto asset = json.find("asset");
    ASSERT(asset != json.end(), "glTF has no 'asset' obj");

    load_string(asset.value(), "version", &version);
    ASSERT(version.data, "glTF asset has no 'version' field");

    load_string(asset.value(), "copyright", &copyright);
}

// Scenes ///////////////////////
void glTF::Scenes::fill(nlohmann::json json)
{
    load_T(json, "scene", &scene);
    load_array(json, "scenes", &scenes);
    fill_obj_array(json, "scenes", &scenes);
}
void glTF::Scene::fill(nlohmann::json json)
{
    load_string(json, "name", &name);
    load_array(json, "nodes", &nodes);
    uint32_t i = 0;
    for (auto item : json["nodes"]) {
        nodes[i] = item;
        ++i;
    }
}

// Nodes ////////////////////////
void glTF::Nodes::fill(nlohmann::json json)
{
    load_array(json, "nodes", &nodes);
    fill_obj_array(json, "nodes", &nodes);
}
void glTF::Node::fill(nlohmann::json json)
{
    load_string(json, "name", &name);
    load_T(json, "mesh", &mesh);
    load_T(json, "camera", &camera);
    load_T(json, "skin", &skin);

    load_array(json, "rotation", &rotation);
    uint32_t i = 0;
    for (auto item : json["rotation"]) {
        rotation[i] = std::move(item);
        ++i;
    }
    i = 0;
    load_array(json, "scale", &scale);
    for (auto item : json["scale"]) {
        scale[i] = std::move(item);
        ++i;
    }
    i = 0;
    load_array(json, "translation", &translation);
    for (auto item : json["translation"]) {
        translation[i] = std::move(item);
        ++i;
    }
    i = 0;
    load_array(json, "weights", &weights);
    for (auto item : json["weights"]) {
        weights[i] = std::move(item);
        ++i;
    }
    i = 0;
    load_array(json, "matrix", &matrix);
    for (auto item : json["matrix"]) {
        matrix[i] = std::move(item);
        ++i;
    }
    i = 0;
    load_array(json, "children", &children);
    for (auto item : json["children"]) {
        children[i] = std::move(item);
        ++i;
    }
}

// Buffers & BufferViews //////////////////////
void glTF::Buffers::fill(nlohmann::json json)
{
    load_array(json, "buffers", &buffers);
    fill_obj_array(json, "buffers", &buffers);
}
void glTF::Buffer::fill(nlohmann::json json)
{
    load_T(json, "byteLength", &byte_length);
    load_string(json, "uri", &uri);
}

void glTF::BufferViews::fill(nlohmann::json json)
{
    load_array(json, "bufferViews", &views);
    fill_obj_array(json, "bufferViews", &views);
}
void glTF::BufferView::fill(nlohmann::json json)
{
    load_T(json, "buffer", &buffer);
    load_T(json, "byteLength", &byte_length);
    load_T(json, "byteOffset", &byte_offset);
    load_T(json, "byteStride", &byte_stride);
    load_T(json, "target", &target);
}

// Accessors ///////////////////////
void glTF::Accessors::fill(nlohmann::json json)
{
    load_array(json, "accessors", &accessors);
    fill_obj_array(json, "accessors", &accessors);
}
void glTF::Accessor::fill(nlohmann::json json)
{
    load_array(json, "max", &max);
    uint32_t i = 0;
    for (auto item : json["max"]) {
        max[i] = std::move(item);
        ++i;
    }
    i = 0;
    load_array(json, "min", &min);
    for (auto item : json["min"]) {
        min[i] = std::move(item);
        ++i;
    }
    i = 0;

    StringBuffer tmp;
    load_string(json, "type", &tmp);
    if (strcmp("SCALAR", tmp.as_cstr()) == 0)
        type = SCALAR;
    if (strcmp("VEC2", tmp.as_cstr()) == 0)
        type = VEC2;
    if (strcmp("VEC3", tmp.as_cstr()) == 0)
        type = VEC3;
    if (strcmp("VEC4", tmp.as_cstr()) == 0)
        type = VEC4;
    if (strcmp("MAT2", tmp.as_cstr()) == 0)
        type = MAT2;
    if (strcmp("MAT3", tmp.as_cstr()) == 0)
        type = MAT3;
    if (strcmp("MAT4", tmp.as_cstr()) == 0)
        type = MAT4;

    load_T(json, "componentType", &component_type);
    load_T(json, "byteOffset", &byte_offset);
    load_T(json, "count", &count);
    load_T(json, "bufferView", &buffer_view);

    nlohmann::json json_sparse;
    if (load_T(json, "sparse", &json_sparse)) {
        sparse.fill(json_sparse);
    }
}
void glTF::Accessor::Sparse::fill(nlohmann::json json)
{
    load_T(json, "count", &count);

    nlohmann::json json_indices;
    if (load_T(json, "indices", &json_indices)) {
        indices.fill(json_indices);
    }
    nlohmann::json json_values;
    if (load_T(json, "values", &json_values)) {
        values.fill(json_values);
    }
}
void glTF::Accessor::Sparse::Indices::fill(nlohmann::json json)
{
    load_T(json, "bufferView", &buffer_view);
    load_T(json, "byteOffset", &byte_offset);
    load_T(json, "componentType", &component_type);
}
void glTF::Accessor::Sparse::Values::fill(nlohmann::json json)
{
    load_T(json, "bufferView", &buffer_view);
    load_T(json, "byteOffset", &byte_offset);
}

// Meshes ////////////////////
void glTF::Meshes::fill(nlohmann::json json)
{
    load_array(json, "meshes", &meshes);
    uint32_t i = 0;
    for (auto item : json["meshes"]) {
        Mesh mesh;
        mesh.fill(item);
        meshes[i] = std::move(mesh);
        ++i;
    }
}
void glTF::Mesh::fill(nlohmann::json json)
{
    load_array(json, "primitives", &primitives);
    fill_obj_array(json, "primitives", &primitives);

    load_array(json, "weights", &weights);
    uint32_t i = 0;
    for (auto item : json["weights"]) {
        weights[i] = std::move(item);
        ++i;
    }

    extras.fill(json);
}
void glTF::Mesh::Primitive::fill(nlohmann::json json)
{
    load_T(json, "indices", &indices);
    load_T(json, "material", &material);
    load_T(json, "mode", &mode);

    if (load_array(json, "attributes", &attributes))
        fill_attrib_array(json["attributes"], &attributes);
    if (load_array(json, "targets", &targets))
        fill_obj_array(json, "targets", &targets);
}

void glTF::Mesh::Primitive::Target::fill(nlohmann::json json)
{
    attributes.init(json.size());
    if (attributes.cap)
        fill_attrib_array(json, &attributes);
}
void glTF::Mesh::Primitive::fill_attrib_array(nlohmann::json json, Array<Attribute> *attributes)
{
    uint32_t i = 0;
    for (auto item : json.items()) {
        Attribute attrib;
        std::string str = item.key();
        attrib.key = StringBuffer::get(str, str.length());
        attrib.accessor = item.value();
        (*attributes)[i] = std::move(attrib);
        ++i;
    }
}
void glTF::Mesh::Extras::fill(nlohmann::json json)
{
    load_array(json, "targetNames", &target_names);
    fill_str_array(json, "targetNames", &target_names);
}

// Skins ////////////////////
void glTF::Skins::fill(nlohmann::json json)
{
    load_array(json, "skins", &skins);
    fill_obj_array(json, "skins", &skins);
}
void glTF::Skin::fill(nlohmann::json json)
{
    load_T(json, "inverseBindMatrices", &i_bind_matrices);
    load_T(json, "skeleton", &skeleton);
    load_array(json, "joints", &joints);
    uint32_t i = 0;
    for (auto item : json["joints"]) {
        joints[i] = std::move(item);
        ++i;
    }
}

// Textures ////////////////
void glTF::Textures::fill(nlohmann::json json)
{
    load_array(json, "textures", &textures);
    fill_obj_array(json, "textures", &textures);
}
void glTF::Texture::fill(nlohmann::json json)
{
    load_T(json, "sampler", &sampler);
    load_T(json, "source", &source);
}

// Images ////////////////
void glTF::Images::fill(nlohmann::json json)
{
    load_array(json, "images", &images);
    fill_obj_array(json, "images", &images);
}
void glTF::Image::fill(nlohmann::json json)
{
    load_string(json, "uri", &uri);
    load_T(json, "bufferView", &buffer_view);

    // Without the if, this will segfault, as load_string() can 
    // return without initializing StringBuffer
    StringBuffer tmp;
    if (load_string(json, "mimeType", &tmp)) {
        if (strcmp(tmp.as_cstr(), "image/jpeg") == 0)
            mime_type = JPG;
        if (strcmp(tmp.as_cstr(), "image/png") == 0)
            mime_type = PNG;
    }
}

// Samplers //////////////
void glTF::Samplers::fill(nlohmann::json json)
{
    load_array(json, "samplers", &samplers);
    fill_obj_array(json, "samplers", &samplers);
}
void glTF::Sampler::fill(nlohmann::json json)
{
    load_T(json, "magFilter", &mag_filter);
    load_T(json, "minFilter", &min_filter);
    load_T(json, "wrapS", &wrap_s);
    load_T(json, "wrapT", &wrap_t);
}

// Materials ///////////////////
void glTF::Materials::fill(nlohmann::json json)
{
    load_array(json, "materials", &materials);
    fill_obj_array(json, "materials", &materials);
}
void glTF::Material::fill(nlohmann::json json)
{
    load_string(json, "name", &name);
    load_T(json, "alphaCutoff", &alpha_cutoff);
    load_T(json, "doubleSided", &double_sided);
    load_array(json, "emissiveFactor", &emissive_factor);
    uint32_t i = 0;
    for (auto item : json["emissiveFactor"]) {
        emissive_factor[i] = std::move(item);
    }

    StringBuffer tmp;
    load_string(json, "alphaMode", &tmp);
    if (strcmp(tmp.as_cstr(), "OPAQUE") == 0)
        alpha_mode = OPAQUE;
    if (strcmp(tmp.as_cstr(), "MASK") == 0)
        alpha_mode = MASK;
    if (strcmp(tmp.as_cstr(), "BLEND") == 0)
        alpha_mode = BLEND;

    pbr_metallic_roughness.fill(json);
    normal_texture.fill_tex(json, "normalTexture");
    emissive_texture.fill_tex(json, "emissiveTexture");
    occlusion_texture.fill_tex(json, "occlusionTexture");
}
void glTF::Material::MatTexture::fill_tex(nlohmann::json json, const char *key)
{
    auto tmp = json.find(key);
    if (tmp == json.end())
        return;
    else
        json = tmp.value();

    load_T(json, "scale", &scale);
    load_T(json, "strength", &scale);
    load_T(json, "index", &index);
    load_T(json, "texCoord", &tex_coord);
}
void glTF::Material::PbrMetallicRoughness::fill(nlohmann::json json)
{
    auto tmp = json.find("pbrMetallicRoughness");
    if (tmp == json.end())
        return;
    else
        json = tmp.value();

    load_array(json, "baseColorFactor", &base_color_factor);
    uint32_t i = 0;
    for (auto item : json["baseColorFactor"]) {
        base_color_factor[i] = std::move(item);
        ++i;
    }
    base_color_texture.fill_tex(json, "baseColorTexture");
    metallic_roughness_texture.fill_tex(json, "metallicRoughnessTexture");
    load_T(json, "metallicFactor", &metallic_factor);
    load_T(json, "roughnessFactor", &roughness_factor);
}

// Cameras /////////////////////
void glTF::Cameras::fill(nlohmann::json json)
{
    load_array(json, "cameras", &cameras);
    fill_obj_array(json, "cameras", &cameras);
}
void glTF::Camera::fill(nlohmann::json json)
{
    load_string(json, "name", &name);

    StringBuffer tmp;
    load_string(json, "type", &tmp);
    if (strcmp(tmp.as_cstr(), "perspective") == 0)
        type = PERSPECTIVE;
    if (strcmp(tmp.as_cstr(), "orthographic") == 0)
        type = ORTHO;
    ASSERT(type != UNKNOWN, "glTF model camera type must be defined");

    load_T(json, "aspectRatio", &aspect_ratio);
    load_T(json, "yfov", &yfov);
    load_T(json, "xmag", &xmag);
    load_T(json, "ymag", &ymag);
    load_T(json, "zfar", &zfar);
    load_T(json, "znear", &znear);

    if (type == ORTHO) {
        load_T(json["orthographic"], "xmag", &xmag);
        load_T(json["orthographic"], "ymag", &ymag);
        load_T(json["orthographic"], "zfar", &zfar);
        load_T(json["orthographic"], "znear", &znear);

        ASSERT(xmag != GLTF_INVALID_FLOAT,
              "glTF model Ortho camera must have XMAG defined");
        ASSERT(ymag != GLTF_INVALID_FLOAT,
              "glTF model Ortho camera must have YMAG defined");
        ASSERT(zfar != GLTF_INVALID_FLOAT,
              "glTF model Ortho camera must have ZFAR defined");
        ASSERT(znear != GLTF_INVALID_FLOAT,
              "glTF model Ortho camera must have ZNEAR defined");
    }
    if (type == PERSPECTIVE) {
        load_T(json["perspective"], "aspectRatio", &aspect_ratio);
        load_T(json["perspective"], "yfov", &yfov);
        load_T(json["perspective"], "zfar", &zfar);
        load_T(json["perspective"], "znear", &znear);

        ASSERT(yfov != GLTF_INVALID_FLOAT,
              "glTF model Perspective camera must have YFOV defined");
        ASSERT(znear != GLTF_INVALID_FLOAT,
              "glTF model Perspective camera must have ZNEAR defined");
    }
}

// Animations ////////////////////
void glTF::Animations::fill(nlohmann::json json)
{
    load_array(json, "animations", &animations);
    fill_obj_array(json, "animations", &animations);
}
void glTF::Animation::fill(nlohmann::json json)
{
    load_string(json, "name", &name);

    load_array(json, "channels", &channels);
    fill_obj_array(json, "channels", &channels);
    load_array(json, "samplers", &samplers);
    fill_obj_array(json, "samplers", &samplers);
}
void glTF::Animation::Channel::fill(nlohmann::json json)
{
    load_T(json, "sampler", &sampler);
    load_T(json["target"], "node", &target.node);

    StringBuffer tmp;
    load_string(json["target"], "path", &tmp);
    if (strcmp(tmp.as_cstr(), "rotation") == 0)
        target.path = Target::ROTATION;
    if (strcmp(tmp.as_cstr(), "translation") == 0)
        target.path = Target::TRANSLATION;
    if (strcmp(tmp.as_cstr(), "scale") == 0)
        target.path = Target::SCALE;
    if (strcmp(tmp.as_cstr(), "weights") == 0)
        target.path = Target::WEIGHTS;
}
void glTF::Animation::Sampler::fill(nlohmann::json json)
{
    load_T(json, "input", &input);
    load_T(json, "output", &output);

    StringBuffer tmp;
    load_string(json, "interpolation", &tmp);
    if (strcmp(tmp.as_cstr(), "LINEAR") == 0)
        interpolation = LINEAR;
    if (strcmp(tmp.as_cstr(), "STEP") == 0)
        interpolation = STEP;
    if (strcmp(tmp.as_cstr(), "CUBICSPLINE") == 0)
        interpolation = CUBICSPLINE;
}

#if TEST
void test1(bool skip) {
    glTF gltf;
    bool ok = glTF::get("test/test_1.gltf", &gltf);
    ASSERT(ok == true, "FAILED to read gltf file");
    TEST_EQ("nodes[3]_translation", gltf.nodes.nodes[3].translation[0], -17.7082f, skip);
    TEST_EQ("bufferviews[1]", gltf.buffer_views.views[1].byte_length, static_cast<uint32_t>(76768), skip);
    TEST_EQ("accessors[0]", gltf.accessors.accessors[0].type, glTF::Accessor::VEC3, skip);
    TEST_EQ("meshes[0].primitives[0]", gltf.meshes.meshes[0].primitives[0].indices, 21, skip);
    TEST_STR_EQ("images[0].uri", gltf.images.images[0].uri.as_cstr(), "duckCM.png", skip);
}
void glTF::run_tests() {
    TEST_MODULE_BEGIN("glTF", true, false);
	size_t mark = SCRATCH->get_mark();
    test1(false);

	SCRATCH->cut_diff(mark);
    TEST_MODULE_END();
}
#endif
} // namespace Sol
