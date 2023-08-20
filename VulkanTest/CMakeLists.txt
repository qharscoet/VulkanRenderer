# CMakeList.txt : projet CMake pour VulkanTest, incluez la source et définissez
# la logique spécifique au projet ici.
#

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

# Ajoutez une source à l'exécutable de ce projet.
add_executable (VulkanTest "VulkanTest.cpp" "VulkanTest.h")
target_link_libraries(VulkanTest PRIVATE glfw Vulkan::Vulkan )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET VulkanTest PROPERTY CXX_STANDARD 20)
endif()

# TODO: Ajoutez des tests et installez des cibles si nécessaire.
