#include "gltf.hpp"
#include "file.hpp"
#include "simd.hpp"
#include "builtin_wrappers.h"
#include "math.hpp"

#if TEST
    #include "test.hpp"
#endif

/*
    WARNING!! This parser has very strict memory alignment rules!!
*/

// Notes on file implementation and old code at the bottom of the file
Gltf parse_gltf(const char *filename);

Gltf_Animation* gltf_parse_animations(const char *data, u64 *offset, int *animation_count);
Gltf_Animation_Channel* gltf_parse_animation_channels(const char *data, u64 *offset, int *channel_count);
Gltf_Animation_Sampler* gltf_parse_animation_samplers(const char *data, u64 *offset, int *sampler_count);

Gltf_Accessor* gltf_parse_accessors(const char *data, u64 *offset, int *accessor_count);
void gltf_parse_accessor_sparse(const char *data, u64 *offset, Gltf_Accessor *accessor);

Gltf_Buffer* gltf_parse_buffers(const char *data, u64 *offset, int *buffer_count);
Gltf_Buffer_View* gltf_parse_buffer_views(const char *data, u64 *offset, int *buffer_view_count);

Gltf_Camera* gltf_parse_cameras(const char *data, u64 *offset, int *camera_count);

Gltf_Image* gltf_parse_images(const char *data, u64 *offset, int *image_count);

Gltf parse_gltf(const char *filename) {
    //
    // Function Method:
    //     While there is a '"' before a closing brace in the file, jump to the '"' as '"' means a key;
    //     match the key and call its parser function.
    //
    //     Each parser function increments the file offset to point to the end of whatver it parsed, 
    //     so if a closing brace is ever found before a key, there must be no keys left in the file. 
    //
    u64 size;
    const char *data = (const char*)file_read_char_heap_padded(filename, &size, 16);
    Gltf gltf;
    char buf[16];
    u64 offset = 0;

    while (simd_find_char_interrupted(data + offset, '"', '}', &offset)) {
        offset++; // skip into key
        if (simd_strcmp_short(data + offset, "accessorsxxxxxxx", 7) == 0) {
            gltf.accessors = gltf_parse_accessors(data + offset, &offset, &gltf.accessor_count);
            continue;
        } else if (simd_strcmp_short(data + offset, "animationsxxxxxx", 6) == 0) {
            gltf.animations = gltf_parse_animations(data + offset, &offset, &gltf.animation_count);
            continue;
        } else if (simd_strcmp_short(data + offset, "buffersxxxxxxxxx", 9) == 0) {
            gltf.buffers = gltf_parse_buffers(data + offset, &offset, &gltf.buffer_count);
            continue;
        } else if (simd_strcmp_short(data + offset, "bufferViewsxxxxx", 5) == 0) {
            gltf.buffer_views = gltf_parse_buffer_views(data + offset, &offset, &gltf.buffer_view_count);
            continue;
        } else if (simd_strcmp_short(data + offset, "camerasxxxxxxxxx", 9) == 0) {
            gltf.cameras = gltf_parse_cameras(data + offset, &offset, &gltf.camera_count);
            continue;
        } else if (simd_strcmp_short(data + offset, "imagesxxxxxxxxxx", 10) == 0) {
            gltf.images = gltf_parse_images(data + offset, &offset, &gltf.image_count);
            continue;
        } else {
            ASSERT(false, "This is not a top level gltf key"); 
        }
    }

    memory_free_heap((void*)data);
    return gltf;
}

// helper algorithms start
static inline int gltf_match_int(char c) {
    switch(c) {
    case '0':
        return 0;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
        return 9;
    default:
        ASSERT(false && "not an int", "");
    }
}

inline int gltf_ascii_to_int(const char *data, u64 *offset) {
    u64 inc = 0;
    if (!simd_skip_to_int(data, &inc, 128))
        ASSERT(false, "Failed to find an integer in search range");

    int accum = 0;
    while(data[inc] >= '0' && data[inc] <= '9') {
        accum *= 10;
        accum += gltf_match_int(data[inc]);
        inc++;
    }
    *offset += inc;
    return accum;
}

float gltf_ascii_to_float(const char *data, u64 *offset) {
    u64 inc = 0;
    if (!simd_skip_to_int(data, &inc, Max_u64))
        ASSERT(false, "Failed to find an integer in search range");

    bool neg = data[inc - 1] == '-';
    bool seen_dot = false;
    int after_dot = 0;
    float accum = 0;
    int num;
    while((data[inc] >= '0' && data[inc] <= '9') || data[inc] == '.') {
        if (data[inc] == '.') {
            seen_dot = true;
            inc++;
        }

        if (seen_dot)
            after_dot++;

        num = gltf_match_int(data[inc]);
        accum *= 10;
        accum += num;

        inc++; // will point beyond the last int at the end of the loop, no need for +1
    }

    // The line below this comment used to say...
    //
    // for(int i = 0; i < after_dot; ++i)
    //     accum /= 10;
    // 
    // ...It is little pieces of code like this that make me worry my code is actually slow while
    // I am believing it to be fast lol
    accum /= pow(10, after_dot);

    *offset += inc;

    if (neg)
        accum = -accum;

    return accum;
}

inline int gltf_parse_float_array(const char *data, u64 *offset, float *array) {
    int i = 0;
    u64 inc = 0;
    while(simd_find_int_interrupted(data + inc, ']', &inc)) {
        array[i] = gltf_ascii_to_float(data + inc, &inc);
        i++;
    }
    *offset += inc + 1; // +1 go beyond the cloasing square bracket (prevent early exit at accessors list level)
    return i;
}
// algorithms end


// `Accessors
void gltf_parse_accessor_sparse(const char *data, u64 *offset, Gltf_Accessor *accessor) {
    u64 inc = 0;
    simd_find_char_interrupted(data + inc, '{', '}', &inc); // find sparse start
    while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
        inc++; // go beyond the '"'
        if (simd_strcmp_short(data + inc, "countxxxxxxxxxxx", 11) == 0)  {
            accessor->sparse_count = gltf_ascii_to_int(data + inc, &inc);
            continue;
        } else if (simd_strcmp_short(data + inc, "indicesxxxxxxxxx", 9) == 0) {
            simd_find_char_interrupted(data + inc, '{', '}', &inc); // find indices start
            while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
                inc++; // go passed the '"'
                if (simd_strcmp_short(data + inc, "bufferViewxxxxxx", 6) == 0) {
                    accessor->indices_buffer_view = gltf_ascii_to_int(data + inc, &inc);
                    continue;
                }
                if (simd_strcmp_short(data + inc, "byteOffsetxxxxxx", 6) == 0) {
                    accessor->indices_byte_offset = gltf_ascii_to_int(data + inc, &inc);
                    continue;
                }
                if (simd_strcmp_short(data + inc, "componentTypexxx", 3) == 0) {
                    accessor->indices_component_type = (Gltf_Type)gltf_ascii_to_int(data + inc, &inc);
                    continue;
                }
            }
            simd_find_char_interrupted(data + inc, '}', '{', &inc); // find indices end
            inc++; // go beyond
            continue;
        } else if (simd_strcmp_short(data + inc, "valuesxxxxxxxxx", 10) == 0) {
            simd_find_char_interrupted(data + inc, '{', '}', &inc); // find values start
            while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
                inc++; // go passed the '"'
                if (simd_strcmp_short(data + inc, "bufferViewxxxxxx", 6) == 0) {
                    accessor->values_buffer_view = gltf_ascii_to_int(data + inc, &inc);
                    continue;
                }
                if (simd_strcmp_short(data + inc, "byteOffsetxxxxxx", 6) == 0) {
                    accessor->values_byte_offset = gltf_ascii_to_int(data + inc, &inc);
                    continue;
                }
            }
            simd_find_char_interrupted(data + inc, '}', '{', &inc); // find indices end
            inc++; // go beyond
            continue;
        }
    }
    *offset += inc + 1; // +1 go beyond the last curly brace in sparse object
}

// @Todo check that all defaults are being properly set
Gltf_Accessor* gltf_parse_accessors(const char *data, u64 *offset, int *accessor_count) {
    u64 inc = 0; // track position in file
    simd_skip_passed_char(data, &inc, '[', Max_u64);

    int count = 0; // accessor count
    float max[16];
    float min[16];
    int min_max_len;
    int temp;
    bool min_found;
    bool max_found;

    // aligned pointer to return
    Gltf_Accessor *ret = (Gltf_Accessor*)memory_allocate_temp(0, 8);
    // pointer for allocating to in loops
    Gltf_Accessor *accessor; 

    //
    // Function Method:
    //     outer loop jumps through the objects in the list,
    //     inner loop jumps through the keys in the objects
    //

    while(simd_find_char_interrupted(data + inc, '{', ']', &inc)) {
        count++; // increment accessor count

        // Temp allocation made for every accessor struct. Keeps shit packed, linear allocators are fast...
        accessor = (Gltf_Accessor*)memory_allocate_temp(sizeof(Gltf_Accessor), 8);
        accessor->indices_component_type = GLTF_TYPE_NONE;
        accessor->type = GLTF_TYPE_NONE;
        min_max_len = 0;
        min_found = false;
        max_found = false;

        while (simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
            inc++; // go beyond the '"' found by find_char_inter...
            //simd_skip_passed_char(data + inc, &inc, '"', Max_u64); // skip to beginning of key
            
            //
            // I do not like all these branch misses, but I cant see a better way. Even if I make a system 
            // to remove options if they have already been chosen, there would still be at least one branch
            // with no improvement in predictability... (I think)
            //
            // ...I think this is just how parsing text goes...
            //

            // match keys to parse methods
            if (simd_strcmp_short(data + inc, "bufferViewxxxxxx",  6) == 0) {
                accessor->buffer_view = gltf_ascii_to_int(data + inc, &inc);
                continue; // go to next key
            }
            if (simd_strcmp_short(data + inc, "byteOffsetxxxxxx",  6) == 0) {
                accessor->byte_offset = gltf_ascii_to_int(data + inc, &inc);
                continue; // go to next key
            }
            if (simd_strcmp_short(data + inc, "countxxxxxxxxxxx", 11) == 0) {
                accessor->count = gltf_ascii_to_int(data + inc, &inc);
                continue; // go to next key
            }
            if (simd_strcmp_short(data + inc, "componentTypexxx",  3) == 0) {
                accessor->component_type = (Gltf_Type)gltf_ascii_to_int(data + inc, &inc);
                continue; // go to next key
            }

            // match type key to a Gltf_Type
            if (simd_strcmp_short(data + inc, "typexxxxxxxxxxxx", 12) == 0) {

                simd_skip_passed_char(data + inc, &inc, '"', Max_u64); // skip to end of "type" key
                simd_skip_passed_char(data + inc, &inc, '"', Max_u64); // skip to beginning of "type" value

                // Why are these types stored as strings?? Why are 'componentType's castable integers, 
                // but these are strings?? This file format is a little insane, there must be a better way 
                // to store this data, surely...???
                if (simd_strcmp_short(data + inc, "SCALARxxxxxxxxxx", 10) == 0)
                    accessor->type = GLTF_TYPE_SCALAR;
                else if (simd_strcmp_short(data + inc, "VEC2xxxxxxxxxxxx", 12) == 0)
                     accessor->type = GLTF_TYPE_VEC2;
                else if (simd_strcmp_short(data + inc, "VEC3xxxxxxxxxxxx", 12) == 0)
                     accessor->type = GLTF_TYPE_VEC3;
                else if (simd_strcmp_short(data + inc, "VEC4xxxxxxxxxxxx", 12) == 0)
                     accessor->type = GLTF_TYPE_VEC4;
                else if (simd_strcmp_short(data + inc, "MAT2xxxxxxxxxxxx", 12) == 0)
                     accessor->type = GLTF_TYPE_MAT2;
                else if (simd_strcmp_short(data + inc, "MAT3xxxxxxxxxxxx", 12) == 0)
                     accessor->type = GLTF_TYPE_MAT3;
                else if (simd_strcmp_short(data + inc, "MAT4xxxxxxxxxxxx", 12) == 0)
                    accessor->type = GLTF_TYPE_MAT4;

                simd_skip_passed_char(data + inc, &inc, '"', Max_u64); // skip passed the value string
                continue; // go to next key
            }

            // why the fuck are bools represented as ascii strings hahhahahahah wtf!!!
            // just use 1 or 0!!! Its just wasting space! Human readable, but so much dumber to parse!!!
            if (simd_strcmp_short(data + inc, "normalizedxxxxxx", 6) == 0) {
                simd_skip_passed_char(data + inc, &inc, ':', Max_u64);
                simd_skip_whitespace(data + inc, &inc); // go the beginning of 'true' || 'false' ascii string
                if (simd_strcmp_short(data + inc, "truexxxxxxxxxxxx", 12) == 0) {
                    accessor->normalized = 1;
                    inc += 5; // go passed the 'true' in the file (no quotation marks to skip)
                }
                else {
                    accessor->normalized = 0;
                    inc += 6; // go passed the 'false' in the file (no quotation marks to skip)
                }
                
                continue; // go to next key
            }

            // sparse object gets its own function
            if (simd_strcmp_short(data + inc, "sparsexxxxxxxxxx", 10) == 0) {
                gltf_parse_accessor_sparse(data + inc, &inc, accessor);
                continue; // go to next key
            }

            // @Lame type can be defined after min max arrays, (which is incredibly inefficient), so this
            // must be handled
            if (simd_strcmp_short(data + inc, "maxxxxxxxxxxxxxx", 13) == 0) {
                min_max_len = gltf_parse_float_array(data + inc, &inc, max);
                max_found = true;
                continue; // go to next key
            }
            if (simd_strcmp_short(data + inc, "minxxxxxxxxxxxxx", 13) == 0) {
                min_max_len = gltf_parse_float_array(data + inc, &inc, min);
                min_found = true;
                continue; // go to next key
            }

        }
        if (min_found && max_found && accessor->type != GLTF_TYPE_NONE) {
            // Have to be careful with memory alignment here: each accessor is allocated individually,
            // so if the temp allocator fragments or packs wrong, reading from the first allocation 
            // and treating the linear allocator as an array would break...
            temp = align(sizeof(float) * min_max_len * 2, 8);
            accessor->max = (float*)memory_allocate_temp(temp, 8); 
            accessor->min = accessor->max + min_max_len;

            memcpy(accessor->max, max, sizeof(float) * min_max_len);
            memcpy(accessor->min, min, sizeof(float) * min_max_len);

            accessor->stride = sizeof(Gltf_Accessor) + (temp);
        } else {
            accessor->stride = sizeof(Gltf_Accessor);
        }
    }
    // handle end...

    // sol - 23 Sept 2023
    // @Note @Memalign
    //
    // ** If accessor info is garbled shit, come back here **
    // -- Update: It seems to work fine, but I wont feel sure for a little bit. --
    //
    // The way the list is handled is I make an allocation for every accessor, and then decrement the final
    // accessor pointer to point to the first allocation, because the allocations are made in a linear allocator.
    // I think that this works despite alignment rules, as in indexing by accessor will work fine, because
    // there will not be alignment problems in the linear allocator.
    *accessor_count = count;
    *offset += inc;
    return ret; // point the returned pointer to the beginning of the list
} // function parse_accessors(..)

// `Animations
Gltf_Animation_Channel* gltf_parse_animation_channels(const char *data, u64 *offset, int *channel_count) {
    // Aligned pointer to return
    Gltf_Animation_Channel *channels = (Gltf_Animation_Channel*)memory_allocate_temp(0, 8);
    // pointer for allocating to in loops
    Gltf_Animation_Channel *channel;

    //
    // Function Method:
    //     outer loop to jump through the list of objects
    //     inner loop to jump through the keys in an object
    //

    u64 inc = 0;   // track pos in file
    int count = 0; // track object count

    while(simd_find_char_interrupted(data + inc, '{', ']', &inc)) {
        channel = (Gltf_Animation_Channel*)memory_allocate_temp(sizeof(Gltf_Animation_Channel), 8);
        count++;
        while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) { // channel loop
            inc++; // go beyond opening '"' in key
            if (simd_strcmp_short(data + inc, "samplerxxxxxxxxx",  9) == 0) {
                channel->sampler = gltf_ascii_to_int(data + inc, &inc);
                continue;
            } else if (simd_strcmp_short(data + inc, "targetxxxxxxxxxx", 10) == 0) {
                simd_skip_passed_char(data + inc, &inc, '{');
                // loop through 'target' object's keys
                while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
                    inc++;
                    if (simd_strcmp_short(data + inc, "nodexxxxxxxxxxxx", 12) == 0) {
                       channel->target_node = gltf_ascii_to_int(data + inc, &inc);
                       continue;
                    } else if (simd_strcmp_short(data + inc, "pathxxxxxxxxxxxx", 12) == 0) {

                        //
                        // string keys + string values are brutal for error proneness...
                        // have to consider everytime what you are skipping
                        //

                        // skip passed closing '"' in 'path' key, skip passed opening '"' in the value string
                        simd_skip_passed_char_count(data + inc, '"', 2, &inc);
                        if(simd_strcmp_short(data + inc, "translationxxxxx", 5) == 0) {
                            channel->path = GLTF_ANIMATION_PATH_TRANSLATION;
                        } else if(simd_strcmp_short(data + inc, "rotationxxxxxxxx", 8) == 0) {
                            channel->path = GLTF_ANIMATION_PATH_ROTATION;
                        } else if(simd_strcmp_short(data + inc, "scalexxxxxxxxxxx", 11) == 0) {
                            channel->path = GLTF_ANIMATION_PATH_SCALE;
                        } else if(simd_strcmp_short(data + inc, "weightsxxxxxxxxx", 9) == 0) {
                            channel->path = GLTF_ANIMATION_PATH_WEIGHTS;
                        }
                        simd_skip_passed_char(data + inc, &inc, '"');
                    }
                }
                inc++; // go passed closing brace of 'target' object to avoid early 'channel' loop exit
            }
        }
    }

    *channel_count = count;
    *offset += inc + 1; // go beyond array closing char
    return channels;
}
Gltf_Animation_Sampler* gltf_parse_animation_samplers(const char *data, u64 *offset, int *sampler_count) {
    //
    // Function Method:
    //     outer loop to jump through the list of objects
    //     inner loop to jump through the keys in an object
    //
    Gltf_Animation_Sampler *samplers = (Gltf_Animation_Sampler*)memory_allocate_temp(0, 8); // get pointer to beginning of sampler allocations
    Gltf_Animation_Sampler *sampler; // temp pointer to allocate to in loops

    u64 inc = 0;   // track file pos
    int count = 0; // track sampler count

    while(simd_find_char_interrupted(data + inc, '{', ']', &inc)) {
        sampler = (Gltf_Animation_Sampler*)memory_allocate_temp(sizeof(Gltf_Animation_Sampler), 8);
        count++;
        sampler->interp = GLTF_ANIMATION_INTERP_LINEAR;
        while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
            inc++; // go beyond opening '"' of key
            if (simd_strcmp_short(data + inc, "inputxxxxxxxxxxx", 11) == 0) {
                sampler->input = gltf_ascii_to_int(data + inc, &inc);
                continue; // Idk if continue and else if is destroying some branch predict algorithm, I hope not...
            } else if (simd_strcmp_short(data + inc, "outputxxxxxxxxxx", 10) == 0) {
                sampler->output = gltf_ascii_to_int(data + inc, &inc);
                continue;
            } else if (simd_strcmp_short(data + inc, "interpolationxxx", 3) == 0) {
                simd_skip_passed_char_count(data + inc, '"', 2, &inc); // jump into value string
                if (simd_strcmp_short(data + inc, "LINEARxxxxxxxxxx", 10) == 0) {
                    sampler->interp = GLTF_ANIMATION_INTERP_LINEAR;
                    simd_skip_passed_char(data + inc, &inc, '"'); // skip passed the end of the value
                    continue;
                } else if (simd_strcmp_short(data + inc, "STEPxxxxxxxxxxxx", 12) == 0) {
                    sampler->interp = GLTF_ANIMATION_INTERP_STEP;
                    simd_skip_passed_char(data + inc, &inc, '"'); // skip passed the end of the value
                    continue;
                } else if (simd_strcmp_short(data + inc, "CUBICSPLINExxxxx", 5) == 0) {
                    sampler->interp = GLTF_ANIMATION_INTERP_CUBICSPLINE;
                    simd_skip_passed_char(data + inc, &inc, '"'); // skip passed the end of the value
                    continue;
                } else {
                    ASSERT(false, "This is not a valid interpolation type");
                }
                simd_skip_passed_char(data + inc, &inc, '"');
                continue;
            }
        }
    }

    *sampler_count = count;
    *offset += inc + 1; // go beyond array closing char
    return samplers;
}
Gltf_Animation* gltf_parse_animations(const char *data, u64 *offset, int *animation_count) {
    //
    // Function Method:
    //     outer loop jumps through the list of animation objects
    //     inner loop jumps through the keys in each object
    //

    Gltf_Animation *animations = (Gltf_Animation*)memory_allocate_temp(0, 8); // get aligned pointer to return
    Gltf_Animation *animation; // pointer for allocating to in loops
    
    u64 inc = 0;   // track pos in file
    int count = 0; // track object count

    while(simd_find_char_interrupted(data + inc, '{', ']', &inc)) { // jump to object start
        ++count;
        animation = (Gltf_Animation*)memory_allocate_temp(sizeof(Gltf_Animation), 8);
        while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
            inc++; // enter the key
            if (simd_strcmp_short(data + inc, "namexxxxxxxxxxxx", 12) == 0) {
                // skip "name" key. Have to jump 3 quotation marks: key end, value both
                simd_skip_passed_char_count(data + inc, '"', 3, &inc);
                continue;
            } else if (simd_strcmp_short(data + inc, "channelsxxxxxxxx", 8) == 0) {
                animation->channels = gltf_parse_animation_channels(data + inc, &inc, &animation->channel_count);
                continue;
            } else if (simd_strcmp_short(data + inc, "samplersxxxxxxxx", 8) == 0) {
                animation->samplers = gltf_parse_animation_samplers(data + inc, &inc, &animation->sampler_count);
                continue;
            }
        }
        animation->stride =  sizeof(Gltf_Animation) + 
                            (sizeof(Gltf_Animation_Channel) * animation->channel_count) + 
                            (sizeof(Gltf_Animation_Sampler) * animation->sampler_count);
    }

    inc++; // go passed closing ']'
    *offset += inc;
    *animation_count = count;
    return animations;
}

// `Buffers
Gltf_Buffer* gltf_parse_buffers(const char *data, u64 *offset, int *buffer_count) {
    Gltf_Buffer *buffers = (Gltf_Buffer*)memory_allocate_temp(0, 8); // pointer to start of array to return
    Gltf_Buffer *buffer; // temp pointer to allocate to while parsing

    u64 inc = 0; // track file pos locally
    int count = 0; // track obj count locally
    int uri_len;

    while(simd_find_char_interrupted(data + inc, '{', ']', &inc)) {
        count++;
        buffer = (Gltf_Buffer*)memory_allocate_temp(sizeof(Gltf_Buffer), 8);
        while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
            inc++; // go beyond opening '"'
            if (simd_strcmp_short(data + inc, "byteLengthxxxxxx", 6) == 0) {
                buffer->byte_length = gltf_ascii_to_int(data + inc, &inc);
                continue;
            } else if (simd_strcmp_short(data + inc, "urixxxxxxxxxxxxx", 13) == 0) {
                simd_skip_passed_char_count(data + inc, '"', 2, &inc); // step inside value string
                uri_len = simd_strlen(data + inc, '"') + 1; // +1 for null termination
                buffer->uri = (char*)memory_allocate_temp(uri_len, 1);
                memcpy(buffer->uri, data + inc, uri_len);
                buffer->uri[uri_len - 1] = '\0';
                simd_skip_passed_char(data + inc, &inc, '"'); // step inside value string
                continue;
            }
        }
        buffer->stride = align(sizeof(Gltf_Buffer) + uri_len, 8);
    }

    *offset += inc;
    *buffer_count = count;
    return buffers;
}

Gltf_Buffer_View* gltf_parse_buffer_views(const char *data, u64 *offset, int *buffer_view_count) {
    Gltf_Buffer_View *buffer_views = (Gltf_Buffer_View*)memory_allocate_temp(0, 8);
    Gltf_Buffer_View *buffer_view;
    u64 inc = 0;
    int count = 0;

    while(simd_find_char_interrupted(data + inc, '{', ']', &inc)) {
        count++;
        buffer_view = (Gltf_Buffer_View*)memory_allocate_temp(sizeof(Gltf_Buffer_View), 8);
        while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
            inc++; // step beyond key's opening '"'
            if (simd_strcmp_short(data + inc, "bufferxxxxxxxxxx", 10) == 0) {
                buffer_view->buffer = gltf_ascii_to_int(data + inc, &inc);
                continue;
            } else if (simd_strcmp_short(data + inc, "byteOffsetxxxxxx",  6) == 0) {
                buffer_view->byte_offset = gltf_ascii_to_int(data + inc, &inc);
                continue;
            } else if (simd_strcmp_short(data + inc, "byteLengthxxxxxx",  6) == 0) {
                buffer_view->byte_length = gltf_ascii_to_int(data + inc, &inc);
                continue;
            } else if (simd_strcmp_short(data + inc, "byteStridexxxxxx",  6) == 0) {
                buffer_view->byte_stride = gltf_ascii_to_int(data + inc, &inc);
                continue;
            } else if (simd_strcmp_short(data + inc, "targetxxxxxxxxxx", 10) == 0) {
                buffer_view->buffer_type = (Gltf_Buffer_Type)gltf_ascii_to_int(data + inc, &inc);
                continue;
            }
        }
        buffer_view->stride = sizeof(Gltf_Buffer_View);
    }

    *offset += inc;
    *buffer_view_count = count;
    return buffer_views;
}

Gltf_Camera* gltf_parse_cameras(const char *data, u64 *offset, int *camera_count) {
    Gltf_Camera *cameras = (Gltf_Camera*)memory_allocate_temp(0, 8);
    Gltf_Camera *camera;

    u64 inc = 0;
    int count = 0;
    while(simd_find_char_interrupted(data + inc, '{', ']', &inc)) {
        camera = (Gltf_Camera*)memory_allocate_temp(sizeof(Gltf_Camera), 8);
        count++;
        while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
            inc++; // step into key
            if (simd_strcmp_short(data + inc, "typexxxxxxxxxxxx", 12) == 0) {
                simd_skip_passed_char_count(data + inc, '"', 2, &inc);
                if (simd_strcmp_short(data + inc, "orthographicxxxx", 4) == 0) {
                    camera->ortho = true;
                    simd_skip_passed_char(data + inc, &inc, '"');
                    continue;
                } else {
                    camera->ortho = false;
                    simd_skip_passed_char(data + inc, &inc, '"');
                    continue;
                } 
            } else if (simd_strcmp_short(data + inc, "orthographicxxxx", 4) == 0) {
                simd_skip_passed_char(data + inc, &inc, '"');
                while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
                    inc++;
                    if (simd_strcmp_short(data + inc, "xmagxxxxxxxxxxxx", 12) == 0) {
                        camera->x_factor = gltf_ascii_to_float(data + inc, &inc);
                        continue;
                    } else if (simd_strcmp_short(data + inc, "ymagxxxxxxxxxxxx", 12) == 0) {
                        camera->y_factor = gltf_ascii_to_float(data + inc, &inc);
                        continue;
                    } else if (simd_strcmp_short(data + inc, "zfarxxxxxxxxxxxx", 12) == 0) {
                        camera->zfar = gltf_ascii_to_float(data + inc, &inc);
                        continue;
                    } else if (simd_strcmp_short(data + inc, "znearxxxxxxxxxxx", 11) == 0) {
                        camera->znear = gltf_ascii_to_float(data + inc, &inc);
                        continue;
                    }
                }
            } else if (simd_strcmp_short(data + inc, "perspectivexxxxx", 5) == 0) {
                simd_skip_passed_char(data + inc, &inc, '"');
                while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
                    inc++;
                    if (simd_strcmp_short(data + inc, "aspectRatioxxxxx", 5) == 0) {
                        camera->x_factor = gltf_ascii_to_float(data + inc, &inc);
                        continue;
                    } else if (simd_strcmp_short(data + inc, "yfovxxxxxxxxxxxx", 12) == 0) {
                        camera->y_factor = gltf_ascii_to_float(data + inc, &inc);
                        continue;
                    } else if (simd_strcmp_short(data + inc, "zfarxxxxxxxxxxxx", 12) == 0) {
                        camera->zfar = gltf_ascii_to_float(data + inc, &inc);
                        continue;
                    } else if (simd_strcmp_short(data + inc, "znearxxxxxxxxxxx", 11) == 0) {
                        camera->znear = gltf_ascii_to_float(data + inc, &inc);
                        continue;
                    }
                }
            }
        }
        camera->stride = sizeof(Gltf_Camera);
    }

    *offset += inc;
    *camera_count = count;
    return cameras;
}

Gltf_Image* gltf_parse_images(const char *data, u64 *offset, int *image_count) {
    Gltf_Image *images = (Gltf_Image*)memory_allocate_temp(0, 8);
    Gltf_Image *image;

    u64 inc = 0;
    int count = 0;
    int uri_len;
    while(simd_find_char_interrupted(data + inc, '{', ']', &inc)) {
        count++;
        image = (Gltf_Image*)memory_allocate_temp(sizeof(Gltf_Image), 8);
        while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
            inc++;
            if (simd_strcmp_short(data + inc, "urixxxxxxxxxxxxx", 13) == 0) {
                simd_skip_passed_char_count(data + inc, '"', 2, &inc);
                uri_len = simd_strlen(data + inc, '"') + 1;
                image->uri = (char*)memory_allocate_temp(uri_len, 1);
                memcpy(image->uri, data + inc, uri_len);
                image->uri[uri_len - 1] = '\0';
                simd_skip_passed_char(data + inc, &inc, '"');
                continue;
            } else if (simd_strcmp_short(data + inc, "mimeTypexxxxxxxx", 8) == 0) {
                uri_len = 0;
                image->uri = NULL;
                simd_skip_passed_char_count(data + inc, '"', 2, &inc);
                if (simd_strcmp_short(data + inc, "image/jpegxxxxxx", 6) == 0) {
                    image->jpeg = 1;
                } else {
                    image->jpeg = 0;
                }
                simd_skip_passed_char(data + inc, &inc, '"');
                continue;
            } else if (simd_strcmp_short(data + inc, "bufferViewxxxxxx", 6) == 0) {
                image->buffer_view = gltf_ascii_to_int(data + inc, &inc);
                continue;
            }
        }
        image->stride = align(sizeof(Gltf_Image) + uri_len, 8);
    }
    *offset += inc;
    *image_count = count;
    return images;
}

#if TEST
static void test_accessors(Gltf_Accessor *accessor);
static void test_animations(Gltf_Animation *animation);
static void test_buffers(Gltf_Buffer *buffers);
static void test_buffer_views(Gltf_Buffer_View *buffer_views);
static void test_cameras(Gltf_Camera *cameras);
static void test_images(Gltf_Image *images);

void test_gltf() {
    Gltf gltf = parse_gltf("test_gltf.gltf");
    Gltf_Accessor *accessor = gltf.accessors;

    test_accessors(gltf.accessors);
    ASSERT(gltf.accessor_count == 3, "Incorrect Accessor Count");
    test_animations(gltf.animations);
    ASSERT(gltf.animation_count == 4, "Incorrect Animation Count");
    test_buffers(gltf.buffers);
    ASSERT(gltf.buffer_count == 5, "Incorrect Buffer Count");
    test_buffer_views(gltf.buffer_views);
    ASSERT(gltf.buffer_view_count == 4, "Incorrect Buffer View Count");
    test_cameras(gltf.cameras);
    ASSERT(gltf.camera_count == 3, "Incorrect Camera View Count");
    test_images(gltf.images);
    ASSERT(gltf.image_count == 3, "Incorrect Image Count");
}

static void test_accessors(Gltf_Accessor *accessor) {
    BEGIN_TEST_MODULE("Gltf_Accessor", true, false);   

    TEST_EQ("accessor[0].type", (int)accessor->type, 1, false);
    TEST_EQ("accessor[0].componentType", (int)accessor->component_type, 5123, false);
    TEST_EQ("accessor[0].buffer_view", accessor->buffer_view, 1, false);
    TEST_EQ("accessor[0].byte_offset", accessor->byte_offset, 100, false);
    TEST_EQ("accessor[0].count", accessor->count, 12636, false);
    TEST_EQ("accessor[0].max[0]", accessor->max[0], 4212, false);
    TEST_EQ("accessor[0].min[0]", accessor->min[0], 0, false);

    accessor = (Gltf_Accessor*)((u8*)accessor + accessor->stride);
    TEST_EQ("accessor[1].type", (int)accessor->type, 2, false);
    TEST_EQ("accessor[1].componentType", (int)accessor->component_type, 5126, false);
    TEST_EQ("accessor[1].buffer_view", accessor->buffer_view, 2, false);
    TEST_EQ("accessor[1].byte_offset", accessor->byte_offset, 200, false);
    TEST_EQ("accessor[1].count", accessor->count, 2399, false);

    float inaccuracy = 0.0000001;
    TEST_LT("accessor[1].max[0]", accessor->max[0] - 0.961799,  inaccuracy, false);
    TEST_LT("accessor[1].max[1]", accessor->max[1] - -1.6397,   inaccuracy, false);
    TEST_LT("accessor[1].max[2]", accessor->max[2] - 0.539252,  inaccuracy, false);
    TEST_LT("accessor[1].min[0]", accessor->min[0] - -0.692985, inaccuracy, false);
    TEST_LT("accessor[1].min[1]", accessor->min[1] - 0.0992937, inaccuracy, false);
    TEST_LT("accessor[1].min[2]", accessor->min[2] - -0.613282, inaccuracy, false);

    accessor = (Gltf_Accessor*)((u8*)accessor + accessor->stride);
    TEST_EQ("accessor[2].type", (int)accessor->type, 3, false);
    TEST_EQ("accessor[2].componentType", (int)accessor->component_type, 5123, false);
    TEST_EQ("accessor[2].buffer_view", accessor->buffer_view, 3, false);
    TEST_EQ("accessor[2].byte_offset", accessor->byte_offset, 300, false);
    TEST_EQ("accessor[2].count", accessor->count, 12001, false);

    TEST_EQ("accessor[2].sparse_count", accessor->sparse_count, 10, false);
    TEST_EQ("accessor[2].indices_comp_type", (int)accessor->indices_component_type, 5123, false);
    TEST_EQ("accessor[2].indices_buffer_view", accessor->indices_buffer_view, 7, false);
    TEST_EQ("accessor[2].values_buffer_view", accessor->values_buffer_view, 4, false);
    TEST_EQ("accessor[2].indices_byte_offset", accessor->indices_byte_offset, 8888, false);
    TEST_EQ("accessor[2].values_byte_offset", accessor->values_byte_offset, 9999, false);

    END_TEST_MODULE();   
}

static void test_animations(Gltf_Animation *animation) {
    BEGIN_TEST_MODULE("Gltf_Accessor", true, false);   

    TEST_EQ("animation[0].channels[0].sampler", animation->channels    [0].sampler,     0, false);
    TEST_EQ("animation[0].channels[0].target_node", animation->channels[0].target_node, 1, false);
    TEST_EQ("animation[0].channels[0].path", animation->channels       [0].path, GLTF_ANIMATION_PATH_ROTATION, false);
    TEST_EQ("animation[0].channels[1].sampler", animation->channels    [1].sampler,     1, false);
    TEST_EQ("animation[0].channels[1].target_node", animation->channels[1].target_node, 2, false);
    TEST_EQ("animation[0].channels[1].path", animation->channels       [1].path, GLTF_ANIMATION_PATH_SCALE, false);
    TEST_EQ("animation[0].channels[2].sampler", animation->channels    [2].sampler,     2, false);
    TEST_EQ("animation[0].channels[2].target_node", animation->channels[2].target_node, 3, false);
    TEST_EQ("animation[0].channels[2].path", animation->channels       [2].path, GLTF_ANIMATION_PATH_TRANSLATION,false);
    TEST_EQ("animation[0].samplers[0].input",  animation->samplers[0].input,  888, false);
    TEST_EQ("animation[0].samplers[0].output", animation->samplers[0].output, 5, false);
    TEST_EQ("animation[0].samplers[0].interp", animation->samplers[0].interp, GLTF_ANIMATION_INTERP_LINEAR, false);
    TEST_EQ("animation[0].samplers[1].input",  animation->samplers[1].input,  4, false);
    TEST_EQ("animation[0].samplers[1].output", animation->samplers[1].output, 6, false);
    TEST_EQ("animation[0].samplers[1].interp", animation->samplers[1].interp, GLTF_ANIMATION_INTERP_CUBICSPLINE, false);
    TEST_EQ("animation[0].samplers[2].input",  animation->samplers[2].input,  4, false);
    TEST_EQ("animation[0].samplers[2].output", animation->samplers[2].output, 7, false);
    TEST_EQ("animation[0].samplers[2].interp", animation->samplers[2].interp, GLTF_ANIMATION_INTERP_STEP, false);

    animation = (Gltf_Animation*)((u8*)animation + animation->stride);
    TEST_EQ("animation[1].channels[0].sampler", animation->channels    [0].sampler,     0, false);
    TEST_EQ("animation[1].channels[0].target_node", animation->channels[0].target_node, 0, false);
    TEST_EQ("animation[1].channels[0].path", animation->channels       [0].path, GLTF_ANIMATION_PATH_ROTATION, false);
    TEST_EQ("animation[1].channels[1].sampler", animation->channels    [1].sampler,     1, false);
    TEST_EQ("animation[1].channels[1].target_node", animation->channels[1].target_node, 1, false);
    TEST_EQ("animation[1].channels[1].path", animation->channels       [1].path, GLTF_ANIMATION_PATH_ROTATION, false);
    TEST_EQ("animation[1].samplers[0].input",  animation->samplers[0].input,  0, false);
    TEST_EQ("animation[1].samplers[0].output", animation->samplers[0].output, 1, false);
    TEST_EQ("animation[1].samplers[0].interp", animation->samplers[0].interp, GLTF_ANIMATION_INTERP_LINEAR, false);
    TEST_EQ("animation[1].samplers[1].input",  animation->samplers[1].input,  2, false);
    TEST_EQ("animation[1].samplers[1].output", animation->samplers[1].output, 3, false);
    TEST_EQ("animation[1].samplers[1].interp", animation->samplers[1].interp, GLTF_ANIMATION_INTERP_LINEAR, false);

    animation = (Gltf_Animation*)((u8*)animation + animation->stride);
    TEST_EQ("animation[2].channels[0].sampler", animation->channels    [0].sampler,     1000, false);
    TEST_EQ("animation[2].channels[0].target_node", animation->channels[0].target_node, 2000, false);
    TEST_EQ("animation[2].channels[0].path", animation->channels       [0].path, GLTF_ANIMATION_PATH_TRANSLATION,false);
    TEST_EQ("animation[2].channels[1].sampler", animation->channels    [1].sampler,     799, false);
    TEST_EQ("animation[2].channels[1].target_node", animation->channels[1].target_node, 899, false);
    TEST_EQ("animation[2].channels[1].path", animation->channels       [1].path, GLTF_ANIMATION_PATH_WEIGHTS, false);
    TEST_EQ("animation[2].samplers[0].input",  animation->samplers[0].input,  676, false);
    TEST_EQ("animation[2].samplers[0].output", animation->samplers[0].output, 472, false);
    TEST_EQ("animation[2].samplers[0].interp", animation->samplers[0].interp, GLTF_ANIMATION_INTERP_STEP, false);

    animation = (Gltf_Animation*)((u8*)animation + animation->stride);
    TEST_EQ("animation[3].channels[0].sampler", animation->channels    [0].sampler,     24, false);
    TEST_EQ("animation[3].channels[0].target_node", animation->channels[0].target_node, 27, false);
    TEST_EQ("animation[3].channels[0].path", animation->channels       [0].path, GLTF_ANIMATION_PATH_ROTATION, false);
    TEST_EQ("animation[3].channels[1].sampler", animation->channels    [1].sampler,     31, false);
    TEST_EQ("animation[3].channels[1].target_node", animation->channels[1].target_node, 36, false);
    TEST_EQ("animation[3].channels[1].path", animation->channels       [1].path, GLTF_ANIMATION_PATH_WEIGHTS, false);
    TEST_EQ("animation[3].samplers[0].input",  animation->samplers[0].input,  999, false);
    TEST_EQ("animation[3].samplers[0].output", animation->samplers[0].output, 753, false);
    TEST_EQ("animation[3].samplers[0].interp", animation->samplers[0].interp, GLTF_ANIMATION_INTERP_LINEAR, false);
    TEST_EQ("animation[3].samplers[1].input",  animation->samplers[1].input,  4, false);
    TEST_EQ("animation[3].samplers[1].output", animation->samplers[1].output, 6, false);
    TEST_EQ("animation[3].samplers[1].interp", animation->samplers[1].interp, GLTF_ANIMATION_INTERP_LINEAR, false);

    END_TEST_MODULE();
}
static void test_buffers(Gltf_Buffer *buffers) {
    BEGIN_TEST_MODULE("Gltf_Buffer", true, false);

    Gltf_Buffer *buffer = buffers;
       TEST_EQ("buffers[0].byteLength", buffer->byte_length, 10001, false);
    TEST_STREQ("buffers[0].uri", buffer->uri, "duck1.bin", false);

    buffer = (Gltf_Buffer*)((u8*)buffer + buffer->stride);
       TEST_EQ("buffers[1].byteLength", buffer->byte_length, 10002, false);
    TEST_STREQ("buffers[1].uri", buffer->uri, "duck2.bin", false);

    buffer = (Gltf_Buffer*)((u8*)buffer + buffer->stride);
       TEST_EQ("buffers[2].byteLength", buffer->byte_length, 10003, false);
    TEST_STREQ("buffers[2].uri", buffer->uri,   "duck3.bin", false);

    buffer = (Gltf_Buffer*)((u8*)buffer + buffer->stride);
       TEST_EQ("buffers[3].byteLength", buffer->byte_length, 10004, false);
    TEST_STREQ("buffers[3].uri", buffer->uri, "duck4.bin", false);

    buffer = (Gltf_Buffer*)((u8*)buffer + buffer->stride);
       TEST_EQ("buffers[4].byteLength", buffer->byte_length, 10005, false);
    TEST_STREQ("buffers[4].uri", buffer->uri, "duck5.bin", false);

    END_TEST_MODULE();
}
static void test_buffer_views(Gltf_Buffer_View *buffer_views) {
    BEGIN_TEST_MODULE("Gltf_Buffer_Views", true, false);

    Gltf_Buffer_View *view = buffer_views;
    TEST_EQ("buffer_views[0].buffer",           view->buffer, 1, false);
    TEST_EQ("buffer_views[0].byte_offset", view->byte_offset, 2, false);
    TEST_EQ("buffer_views[0].byte_length", view->byte_length, 25272, false);
    TEST_EQ("buffer_views[0].byte_stride", view->byte_stride, 0, false);
    TEST_EQ("buffer_views[0].buffer_type", view->buffer_type, 34963, false);

    view = (Gltf_Buffer_View*)((u8*)view + view->stride);
    TEST_EQ("buffer_views[1].buffer",           view->buffer, 6, false);
    TEST_EQ("buffer_views[1].byte_offset", view->byte_offset, 25272, false);
    TEST_EQ("buffer_views[1].byte_length", view->byte_length, 76768, false);
    TEST_EQ("buffer_views[1].byte_stride", view->byte_stride, 32, false);
    TEST_EQ("buffer_views[1].buffer_type", view->buffer_type, 34962, false);

    view = (Gltf_Buffer_View*)((u8*)view + view->stride);
    TEST_EQ("buffer_views[2].buffer",           view->buffer,  9999, false);
    TEST_EQ("buffer_views[2].byte_offset", view->byte_offset,  6969, false);
    TEST_EQ("buffer_views[2].byte_length", view->byte_length,  99907654, false);
    TEST_EQ("buffer_views[2].byte_stride", view->byte_stride, 0, false);
    TEST_EQ("buffer_views[2].buffer_type", view->buffer_type, 34962, false);

    view = (Gltf_Buffer_View*)((u8*)view + view->stride);
    TEST_EQ("buffer_views[3].buffer",           view->buffer, 9, false);
    TEST_EQ("buffer_views[3].byte_offset", view->byte_offset, 25272, false);
    TEST_EQ("buffer_views[3].byte_length", view->byte_length, 76768, false);
    TEST_EQ("buffer_views[3].byte_stride", view->byte_stride, 32, false);
    TEST_EQ("buffer_views[3].buffer_type", view->buffer_type, 34963, false);

    END_TEST_MODULE();
}
static void test_cameras(Gltf_Camera *cameras) {
    BEGIN_TEST_MODULE("Gltf_Camera", true, false);

    float inaccuracy = 0.0000001;

    Gltf_Camera *camera = cameras;
    TEST_LT("cameras[0].ortho", camera->ortho           - 0,        inaccuracy, false);
    TEST_LT("cameras[0].aspect_ratio", camera->x_factor - 1.5,      inaccuracy, false);
    TEST_LT("cameras[0].yfov", camera->y_factor         - 0.646464, inaccuracy, false);
    TEST_LT("cameras[0].zfar", camera->znear            - 100,      inaccuracy, false);
    TEST_LT("cameras[0].znear", camera->znear           - 0.01,     inaccuracy, false);

    camera = (Gltf_Camera*)((u8*)camera + camera->stride);
    TEST_LT("cameras[1].ortho", camera->ortho           - 0,        inaccuracy, false);
    TEST_LT("cameras[1].aspect_ratio", camera->x_factor - 1.9,      inaccuracy, false);
    TEST_LT("cameras[1].yfov", camera->y_factor         - 0.797979, inaccuracy, false);
    TEST_LT("cameras[1].zfar", camera->znear            - 100,      inaccuracy, false);
    TEST_LT("cameras[1].znear", camera->znear           - 0.02,     inaccuracy, false);

    camera = (Gltf_Camera*)((u8*)camera + camera->stride);
    TEST_LT("cameras[2].ortho", camera->ortho   - 1,     inaccuracy, false);
    TEST_LT("cameras[2].xmag", camera->x_factor - 1.822, inaccuracy, false);
    TEST_LT("cameras[2].ymag", camera->y_factor - 0.489, inaccuracy, false);
    TEST_LT("cameras[2].znear", camera->znear   - 0.01,  inaccuracy, false);

    END_TEST_MODULE();
}
static void test_images(Gltf_Image *images) {
    BEGIN_TEST_MODULE("Gltf_Image", true, false);

    Gltf_Image *image = images;
    TEST_STREQ("images[0].uri", image->uri, "duckCM.png", false);

    image = (Gltf_Image*)((u8*)image + image->stride); 
    TEST_EQ("images[1].jpeg", image->jpeg, 1, false);
    TEST_EQ("images[1].bufferView", image->buffer_view, 14, false);
    TEST_EQ("images[1].uri", image->uri, nullptr, false);

    image = (Gltf_Image*)((u8*)image + image->stride); 
    TEST_STREQ("images[2].uri", image->uri, "duck_but_better.jpeg", false);

    END_TEST_MODULE();
}
#endif

// This file is gltf file parser. It reads a gltf file and turns the information into usable C++.
// It does so in a potentially unorthodox way, in the interest of consistency, simplicity, speed and code size.
// There are not more general helper functions such as "find_key(..)". The reason for this is that for functions
// like these to work, much more jumping back and forth in the file takes place, for an equivalent number of
// compare and branch operations, if not more.
//
// The implementation instead uses helper functions for jumping to characters. In this way, the file never stops
// being traversed forwards. And as the file is traversed, the C++ structs are filled at the same time.
//
// The workflow takes the form: define a char of interest, define a break char, jump to the next char of interest
// while they come before the closing char in the file. This is more complicated and error prone than saying "find 
// me this key", but such function would be expensive, as you could not go through the whole file in one clean parse:
// you have to look ahead for the string matching the key, and then return it. Then to get the next key you have to
// do the same, then when all keys are collected, do a big jump passed all the keys. This can be a big jump for some
// objects.
//
// While my method is potentially error prone, it is super clean to debug and understand, as you know exactly
// where it is in the file at any point, how much progress it has made, what char or string has tripped it up etc.
// It is simple to understand as the same short clear functions are used everywhere: "jump to char x, unless y is
// in the way", simple. The call stack is super small, so you only have to check a few functions at any time for a bug.
//
// Finally it seems fast. One pass through the file, when you reach the end everything is done, no more work. And the
// work during the file is super cheap and cache friendly: match the current key against a small list of the possible
// keys that can exist in the current object, call the specific parse function for its specific value. Move forward.
// Zero Generality (which is what makes it error prone, as everything has to be considered its own thing, and then
// coded as such, but man up! GO FAST!)


/*** Below Comments: Notes On Design of this file (Often Rambling, Stream of thoughts) ***/

/*
    Final Solution:
        Loop the file once, parse as I go. No need to loop once to tokenize or get other info. Memory access
        and branching will never be that clean for this task regardless of the method, and since I expect that
        these files can get pretty large, only needing one pass seems like it would be fastest.
*/

/* ** Old Note See Above **
    After working on the problem more, I hav realised that a json parser would be very difficult. It would not be 
    hard to serialize the text data into a big block of mem with pointers into that memory mapped to keys. 
    Just iterate through keys or do a hash look up to find the matching key, deref its pointer to find its data.
    By just knowing what type of data the key is pointing to, it is trivial to interpret the data on the end of the
    pointer. 

    However I am going to use my original plan but just in one loop. Because this is much faster than the other option.
    To serialize all the text data into a memory block meaningful for C++ would be a waste of time, because I would
    then have to turn that memory block into each of the types of structs which meaningfully map to my application.
    This process is easier with the memory block than just the text data, but it would be interesting work to make the
    parser/serialiser, then tiresome boring work to turn that into Gltf_<> structs. It would be slow, as it is
    essentially two programs, one of which will incur minor cache minsses (the data will be parsed in order, but
    I will still have to jump around in look ups). As such I will turn the data into Gltf_<> structs straight from
    the text. This will be more difficult work, as a general task becomes more specific (each gltf property becomes
    its own task to serialize straight from text, rather than text being largely general (objects, arrays, numbers)
    and then specific to create from the serialized block).

    This will be much faster: I just pass through the data once, all in order. And at the end I have a bunch
    of C++ structs all mapped to gltf properties. Cool. Let's begin!
*/

/* ** Old Note, See Above **
    I am unsure how to implement this file. But I think I have come to a working solution:

    I have a working idea:
    
        Make a list of function pointers. This list is ordered by the order of the gltf keys 
        in the file. Then I do not have to branch for each key. I just loop through the file once 
        to get the memory required for each first level type. As I do this, I push the correct function 
        pointers to the array. Then I loop through this list calling each function, which will 
        use the file data in order, keeping it all in cache. 

    e.g.

    ~~~~~~~~~~~~~~~~~~~~

    Array<FnPtr> parse_functions;

    Key key = match_key(<str>);
    get_size_<key>() {
        gltf-><key>_count = ...
        gltf-><key>s = memory_allocate(...)
        parse_functions.push(&parse_<key>())
    }

    char *data;
    u64 file_offset;
    for(int i = 0; i < parse_functions) {
        // start at the beginning of the file again, offset get incremented to point at the start
        // of the next key inside the parse_<key> function
        parse_fucntions[i](data, offset);
    }

    ~~~~~~~~~~~
    
    This way I loop the entire file 2 times, and each time in entirety, so the prefectcher is only 
    interrupted once. I only have to match keys in order to dispatch correct fucntions once, as the
    second pass can just call the functions in order.
*/


/* Below is old code related to this file. I am preserving it because it is cool code imo, does some cool things... */

/* **** Code example for calculating the length of json arrays with simd *****

// string assumed 16 len
inline static u16 simd_match_char(const char *string, char c) {
    __m128i a = _mm_loadu_si128((__m128i*)string);
    __m128i b = _mm_set1_epi8(c);
    a = _mm_cmpeq_epi8(a, b);
    return _mm_movemask_epi8(a);
}

inline static int pop_count(u16 mask) {
    return __builtin_popcount(mask);
}
inline static int ctz(u16 mask) {
    return __builtin_ctz(mask);
}
inline static int clzs(u16 mask) {
    return __builtin_clzs(mask);
}

int resolve_depth(u16 mask1, u16 mask2, int current_depth) {
    u8 tz;
    int open;
    while(mask1) {
        tz = ctz(mask1); 
        open = pop_count(mask2 << (16 - tz));

        current_depth -= 1;
        if (current_depth + open == 0)
            return tz;

        mask1 ^= 1 << tz;
    }
    return -1;
}

int get_obj_array_len(const char *string, u64 *offset) {
    u16 mask1;
    u16 mask2;
    int array_depth = 0;
    int ret;
    while(true) {
        mask1 = simd_match_char(string + *offset, ']');
        mask2 = simd_match_char(string + *offset, '[');
        if (array_depth - pop_count(mask1) <= 0) {
            ret = resolve_depth(mask1, mask2, array_depth);
            if(ret != -1)
                return *offset + ret;
        }
        array_depth += pop_count(mask2) - pop_count(mask1);
        *offset += 16;
    }
}
int main() {
    const char *d = "[[[[xxxxxxxxxxxxx]]]xxxxxxxxxxx]";

    u64 offset = 0;
    int x = get_obj_array_len(d, &offset);
    printf("%i", x);

    return 0;
}
*/

/* 
    ** Template Key Pad **

    Key_Pad keys[] = {
        {"bufferViewxxxxxx",  6},
        {"byteOffsetxxxxxx",  6},
        {"componentTypexxx",  3},
        {"countxxxxxxxxxxx", 11},
        {"maxxxxxxxxxxxxxx", 13},
        {"minxxxxxxxxxxxxx", 13},
        {"typexxxxxxxxxxxx", 12},
        {"sparsexxxxxxxxxx", 10},
    };
    int key_count = 8; 


// Old but imo cool code, so I am preserving it
int resolve_depth(u16 mask1, u16 mask2, int current_depth, int *results) {
    u8 tz;
    int open;
    int index = 0;
    while(mask1) {
        tz = count_trailing_zeros_u16(mask1); 
        open = pop_count16(mask2 << (16 - tz));

        current_depth -= 1;
        if (current_depth + open == 0) {
            results[index] = tz; 
            ++index;
        }

        mask1 ^= 1 << tz;
    }
    return index;
}

int parse_object_array_len(const char *string, u64 *offset, u64 *object_offsets) {
    u64 inc = 0;
    int array_depth  = 0;
    int object_depth = 0;
    int object_count = 0;
    int rd;
    int rds[8];
    u16 mask1;
    u16 mask2;
    u16 temp_count;
    while(true) {
        // check array
        mask1 = simd_match_char(string + inc, ']');
        mask2 = simd_match_char(string + inc, '[');
        if (array_depth - pop_count16(mask1) <= 0) {
            rd = resolve_depth(mask1, mask2, array_depth, rds);
            if(rd != 0) {
                // check object within the end of the array
                mask1 = simd_match_char(string + inc, '}') << (16 - rds[0]);
                mask2 = simd_match_char(string + inc, '{') << (16 - rds[0]);
                if (object_depth - pop_count16(mask1) <= 0) {
                    temp_count = object_count;
                    object_count += resolve_depth(mask1, mask2, object_depth, rds);
                    for(int i = temp_count; i < object_count; ++i)
                        object_offsets[i] = inc + rds[i - temp_count];
                }
                inc += rds[0];
                *offset += inc; // deref offset only once just to keep cache as clean as possible
                return object_count;
            }
        }
        array_depth += pop_count16(mask2) - pop_count16(mask1);

        // check object
        mask1 = simd_match_char(string + inc, '}');
        mask2 = simd_match_char(string + inc, '{');
        if (object_depth - pop_count16(mask1) <= 0) {
            temp_count = object_count;
            object_count += resolve_depth(mask1, mask2, object_depth, rds);
            for(int i = temp_count; i < object_count; ++i)
                object_offsets[i] = inc + rds[i - temp_count];
        }
        object_depth += pop_count16(mask2) - pop_count16(mask1);

        inc += 16;
    }
}
enum Gltf_Key {
    GLTF_KEY_BUFFER_VIEW,
    GLTF_KEY_BYTE_OFFSET,
    GLTF_KEY_COMPONENT_TYPE,
    GLTF_KEY_COUNT,
    GLTF_KEY_MAX,
    GLTF_KEY_MIN,
    GLTF_KEY_TYPE,
    GLTF_KEY_SPARSE,
    GLTF_KEY_INDICES,
    GLTF_KEY_VALUES,
};
// increments beyond end char
void collect_string(const char *data, u64 *offset, char c, char *buf) {
    int i = 0;
    u64 inc = 0;
    while(data[inc] != c) {
        buf[i] = data[inc];
        ++inc;
        ++i;
    }
    buf[i] = '\0';
    *offset += inc + 1;
}


// string must be assumed 16 in len
static int find_string_in_list(const char *string, const Key_Pad *keys, int count) {
    for(int i = 0; i < count; ++i) {
        if(simd_strcmp_short(string, keys[i].key, keys[i].padding) == 0)
            return i;
    }
    return -1;
}

struct Key_Pad {
    const char *key;
    u8 padding;
};

*/
