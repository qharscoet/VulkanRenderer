# CMakeList.txt : projet CMake pour VulkanTest, incluez la source et définissez
# la logique spécifique au projet ici.
#

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

find_package(Stb REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)    


file(GLOB SOURCES "./*.cpp")
 
# Ajoutez une source à l'exécutable de ce projet.
add_executable (VulkanTest ${SOURCES})
target_include_directories(VulkanTest PRIVATE ${Stb_INCLUDE_DIR})
target_link_libraries(VulkanTest PRIVATE glfw Vulkan::Vulkan tinyobjloader::tinyobjloader )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET VulkanTest PROPERTY CXX_STANDARD 20)
endif()

# TODO: Ajoutez des tests et installez des cibles si nécessaire.
