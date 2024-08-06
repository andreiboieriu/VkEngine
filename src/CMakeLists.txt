# Add source to this project's executable.
add_executable (engine 
  main.cpp
  vk_types.h
  vk_initializers.cpp
  vk_initializers.h
  vk_images.h
  vk_images.cpp 
  vk_descriptors.h
  vk_descriptors.cpp
  vk_pipelines.h
  vk_pipelines.cpp
  vk_engine.h
  vk_engine.cpp
  vk_loader.h
  vk_loader.cpp
  vk_window.h
  vk_window.cpp
  vk_swapchain.h
  vk_swapchain.cpp
  vk_enum_string_helper.h
  camera.cpp
  camera.h
  deletion_queue.h
  deletion_queue.cpp
  vk_materials.cpp
  vk_materials.h
  vk_scene.cpp
  vk_scene.h
  ecs_components/gltf.h
  ecs_components/transform.h
  ecs_components/components.h
  game_object.cpp
  game_object.h
  ecs_systems/render_system.cpp
  ecs_systems/render_system.h
  compute_effects/compute_effect.cpp
  compute_effects/compute_effect.h
  compute_effects/fxaa.cpp
  compute_effects/fxaa.h
  skybox.cpp
  skybox.h
)

set_property(TARGET engine PROPERTY CXX_STANDARD 20)
set( CMAKE_CXX_FLAGS "-g")

target_compile_definitions(engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS)
target_include_directories(engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(engine PUBLIC volk vma glm entt fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)

target_precompile_headers(engine PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp>)
