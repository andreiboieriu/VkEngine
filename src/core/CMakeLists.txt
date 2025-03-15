file(GLOB_RECURSE CORE_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(core STATIC ${CORE_SOURCE_FILES})

target_compile_definitions(core PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS)
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(core spirv_reflect volk vma glm entt nlohmann fmt stb_image glfw vkbootstrap imgui fastgltf lua sol2)

target_precompile_headers(core PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp>)
