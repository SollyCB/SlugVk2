#include "gltf.hpp"
#include "file.hpp"

// omfg I even have to use std::string for this lib!!! WTF!!!
// Reminder: this is only to bake model files into a c++ struct. It shouldnt be too slow for this use case. (I hope)
#include <string> 
#include <fstream> // this parser will not be part of the real final engine...

Gltf create_gltf(const char *gltf_filename) {
    std::ifstream f(gltf_filename);
    Json json = Json::parse(f);

    Gltf gltf;

    // Accessors
    /* start here */

    return gltf;
}

// Here I use a reference cos this is a dumb lib that uses 'modern c++'. I would rather use a pointer 
// to be explicit about what the function wants to do but that wont play nice with the lib...
void fill_accessor(Gltf_Accessor *accessor, Json &json) { 

    // re do taking into account required fields and the shittiness of the api...
    // shittiness: different access methods fail in different ways, with terrible error reporting

    return;
}
