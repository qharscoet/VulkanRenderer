# VulkanRenderer

This is a simple project for learning vulkan and tackle writing a whole renderer by myself.
Will follow https://vulkan-tutorial.com/ at first, and we'll see what it becomes.
For now is has become a model viewer by taking https://github.khronos.org/glTF-Sample-Viewer-Release/ as reference.

This is still very early and subject to lot of changes in the future. Current goal is to settle all the basic features I may need for future experiments

This is what it looks like as November 2025.

<img width="2560" height="1528" alt="image" src="https://github.com/user-attachments/assets/ec1a00f8-5377-40d4-aace-21e85a18e38f" />


WHAT WORKS
----------

- Basic Vulkan setup
- Multiple render pass creation
- Compute should be working
- SPIR-V shader loading, allowing GLSL, HLSL or Slang
- Add mesh from OBJ and basic GTLF (not full compatibility yet but most features needed for static models)
- Basic lights (Directional/Point/Spotlight)
- Basic Phong lighting
- Basic PBR lighting
- Normal maps
- PBR

NEXT
------

- Refactor Pipeline management to be less tied to the Device and vulkan specific code to allow future multiple backend support
- General Cleanup
- Adding Shadows

BACKLOG :
-------

- Improve GLTF compatibility
- Make a proper asset manager and material system.
- Anything than come to my mind while doing the above

External librairies used:
---------------------

- Vulkan SDK
- GLFW
- GLM for the math until I do one myself 
- stb_image : https://github.com/nothings/stb
- tinyobjloader : https://github.com/tinyobjloader/tinyobjloader
- tinygltf : https://github.com/syoyo/tinygltf
- Dear imgui : https://github.com/ocornut/imgui

