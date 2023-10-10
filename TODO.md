## TODOs ##

1. Renderpass creation is soooooo verbose and time consuming to type.
   STATUS: Resolved for basic rendering (Todo deferred rendering setup functions + broader system)

2. Resource creation list should be spat out from renderer stage one. In the same way that it
   is done for the descriptor sets after spirv parsing.
   STATUS: Complete for vertex attribute and index data (Todo textures, samplers, uniform buffers)

3. VkSpec section 33.7 Sparse Resources. Looks like useful information, and likely vital
   for using the gltf sparse accessors...
