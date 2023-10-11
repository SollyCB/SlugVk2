## TODOs ##

1. Renderpass creation is soooooo verbose and time consuming to type.
   STATUS: Resolved for basic rendering (Todo deferred rendering setup functions + broader system)

2. Resource creation list should be spat out from renderer stage one. In the same way that it
   is done for the descriptor sets after spirv parsing.
   STATUS: Complete for vertex attribute and index data (Todo textures, samplers, uniform buffers)

3. VkSpec section 33.7 Sparse Resources. Looks like useful information, and likely vital
   for using the gltf sparse accessors...

4. Intel Opti guide threading ; gltf parser
    Have a look ahead thread which is running the file finding key boundaries and marking tokens,
    then have a second thread behind which chews through this info.

5. Create Thread Lib.
    Windows: _Interlocked.. <intrin.h>
    Linux: __sync_..

6. Descriptor Set Allocation ; duplicate descriptor sets.
    Current system reads spirv for a group of shaders, and creates + allocates all of these
    decriptors. Improvement: before allocating and creating any descriptorsets, note duplicate
    sets and set set layouts, between shader groupings and avoid allocating the duplicates. 
    (Note that this could impede threading, as certain pipelines would not be distinct as they
    share set allocations...)
