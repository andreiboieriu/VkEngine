# Add source to this project's executable.
file(GLOB_RECURSE CXX_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(engine 
  ${CXX_SOURCE_FILES}
)

# set( CMAKE_CXX_FLAGS "-g")

target_compile_definitions(engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS)
target_include_directories(engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(engine spirv_reflect volk vma glm entt nlohmann fmt stb_image SDL2 vkbootstrap imgui fastgltf lua sol2)

target_precompile_headers(engine PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp>)
