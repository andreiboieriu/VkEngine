file(GLOB_RECURSE EDITOR_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(editor ${EDITOR_SOURCE_FILES})

target_link_libraries(editor core)
target_include_directories(editor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# target_precompile_headers(editor PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp>)
