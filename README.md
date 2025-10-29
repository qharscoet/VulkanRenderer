# VulkanRenderer

This is a simple project for learning vulkan and tackle writing a whole renderer by myself.
Will follow https://vulkan-tutorial.com/ at first, and we'll see what it becomes.


This is still very early and subject to lot of changes when I'll have the time to go back on it (no big progress since March 2025)

WHAT WORKS
----------

- Basic Vulkan setup
- Multiple render pass creation
- Compute should be working
- SPIR-V shader loading, allowing GLSL, HLSL or Slang
- Add mesh from OBJ and basic GTLF (not full compatibility yet)
- Basic lights (Directional/Point/Spotlight)
- Basic Phong lighting
- Basic PBR lighting
- Normal maps


TODO :
-------

- Better lighting (more advanced models, PBR...)
- Improve GLTF compatibility
- Refactor Pipeline management to be less tied to the Device and vulkan specific code to allow future multiple backend support
- Make a proper asset manager and material system.
- Anything than come to my mind while doing the above

External librairies used:
---------------------

- Vulkan SDK
- GLFW
- GLM for the math until I do one myself 
- stb_image : https://github.com/nothings/stb
- tinyobjloader : https://github.com/tinyobjloader/tinyobjloader
- Dear imgui : https://github.com/ocornut/imgui

