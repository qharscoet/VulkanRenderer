find_program(GLSL_VALIDATOR glslc HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/glsl/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/glsl/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/glsl/*.comp"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  message(STATUS ${FILE_NAME})
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/spv/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)


find_program(HLSL_VALIDATOR dxc HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
file(GLOB_RECURSE HLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/hlsl/*.hlsl"
    )

foreach(HLSL ${HLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME  ${HLSL} NAME_WE)
  message(STATUS ${FILE_NAME})
  set(SPIRV_VS "${PROJECT_SOURCE_DIR}/shaders/spv/${FILE_NAME}.vs.spv")
  set(SPIRV_PS "${PROJECT_SOURCE_DIR}/shaders/spv/${FILE_NAME}.ps.spv")
  message(STATUS ${HLSL})
  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_custom_command(
		OUTPUT ${SPIRV_VS} ${SPIRV_PS}
		COMMAND ${HLSL_VALIDATOR} ${HLSL} -spirv -T vs_6_0 -E VSMain -Fo ${SPIRV_VS} -fspv-extension=SPV_KHR_non_semantic_info -fspv-debug=vulkan-with-source
		COMMAND ${HLSL_VALIDATOR} ${HLSL} -spirv -T ps_6_0 -E PSMain -Fo ${SPIRV_PS} -fspv-extension=SPV_KHR_non_semantic_info -fspv-debug=vulkan-with-source
		DEPENDS ${HLSL})
  else()
	add_custom_command(
		OUTPUT ${SPIRV_VS} ${SPIRV_PS}
		COMMAND ${HLSL_VALIDATOR} ${HLSL} -spirv -T vs_6_0 -E VSMain -Fo ${SPIRV_VS}
		COMMAND ${HLSL_VALIDATOR} ${HLSL} -spirv -T ps_6_0 -E PSMain -Fo ${SPIRV_PS}
		DEPENDS ${HLSL})
  endif()

  list(APPEND SPIRV_BINARY_FILES ${SPIRV_VS} ${SPIRV_PS})
 endforeach(HLSL)



 find_program(SLANG_VALIDATOR slangc HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
 file(GLOB_RECURSE SLANG_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/slang/*.slang"
    )

foreach(SLANG ${SLANG_SOURCE_FILES})
  get_filename_component(FILE_NAME  ${SLANG} NAME_WE)
  message(STATUS ${FILE_NAME})
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/spv/${FILE_NAME}.slang.spv")
  message(STATUS "BUILDING SLANG SHADER")
  message(STATUS ${SLANG})
  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_custom_command(
		OUTPUT ${SPIRV}
		COMMAND ${SLANG_VALIDATOR} ${SLANG} -target spirv -profile spirv_1_4 -g -o ${SPIRV}
		DEPENDS ${SLANG})
  else()
	add_custom_command(
		OUTPUT ${SPIRV}
		COMMAND ${SLANG_VALIDATOR} ${SLANG} -target spirv -profile spirv_1_4  -o ${SPIRV}
		DEPENDS ${SLANG})
  endif()

  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
 
endforeach(SLANG)


add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )