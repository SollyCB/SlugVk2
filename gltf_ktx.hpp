#ifndef SOL_GLTF_KTX_HPP_INCLUDE_GUARD_
#define SOL_GLTF_KTX_HPP_INCLUDE_GUARD_

#include "basic.h"
struct Ktx {
    
};
Ktx load_ktx(const char *file_name); 
void free_ktx(Ktx *ktx);

struct Gltf {
};
Gltf load_gltf(const char *file_name);

#endif
