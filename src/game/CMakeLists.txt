file(GLOB_RECURSE GAME_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(game ${GAME_SOURCE_FILES})

target_link_libraries(game core)
target_include_directories(game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# target_precompile_headers(game PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp>)
