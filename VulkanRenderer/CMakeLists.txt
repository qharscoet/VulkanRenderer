# CMakeList.txt : projet CMake pour VulkanRenderer, incluez la source et définissez
# la logique spécifique au projet ici.
#

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

find_package(Stb REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)    
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)

find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")


file(GLOB SOURCES "./*.cpp")
 
# Ajoutez une source à l'exécutable de ce projet.
add_executable (VulkanRenderer ${SOURCES})
target_include_directories(VulkanRenderer PRIVATE ${Stb_INCLUDE_DIR} ${TINYGLTF_INCLUDE_DIRS})
target_link_libraries(VulkanRenderer PRIVATE glfw Vulkan::Vulkan tinyobjloader::tinyobjloader  imgui::imgui imguizmo::imguizmo)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET VulkanRenderer PROPERTY CXX_STANDARD 20)
endif()

# TODO: Ajoutez des tests et installez des cibles si nécessaire.
