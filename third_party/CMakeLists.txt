# import header only libs

# stb image
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/stb_image")

# vma
add_library(vma INTERFACE)
target_compile_options(vma INTERFACE "-w")
target_include_directories(vma INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vma")

# nlohmann
add_library(nlohmann INTERFACE)
target_include_directories(nlohmann INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann/include")

# glm
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/glm")

# entt
add_library(entt INTERFACE)
target_include_directories(entt INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/entt")

# sol2
add_library(sol2 INTERFACE)
target_include_directories(sol2 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/sol2/include")

# import pre-built libs

# volk
add_library(volk STATIC IMPORTED GLOBAL)
set_target_properties(volk PROPERTIES
                      IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/volk/libvolk.a"
                      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/volk")

# vkbootstrap
add_library(vkbootstrap STATIC IMPORTED GLOBAL)
set_target_properties(vkbootstrap PROPERTIES
                      IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/vkbootstrap/libvkbootstrap.a"
                      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/vkbootstrap")
# target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)
target_link_libraries(vkbootstrap INTERFACE volk)

# fmt
add_library(fmt STATIC IMPORTED GLOBAL)
set_target_properties(fmt PROPERTIES
                      IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/fmt/libfmt.a"
                      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/fmt/include")

# sdl
add_library(SDL2 STATIC IMPORTED GLOBAL)
set_target_properties(SDL2 PROPERTIES
                      IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/SDL/libSDL2.a"
                      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/SDL/include")

# imgui
add_library(imgui STATIC IMPORTED GLOBAL)
set_target_properties(imgui PROPERTIES
                      IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/imgui/libimgui.a"
                      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/imgui")

target_link_libraries(imgui INTERFACE volk SDL2)

# fastgltf
add_library(fastgltf STATIC IMPORTED GLOBAL)
set_target_properties(fastgltf PROPERTIES
                      IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/fastgltf/libfastgltf.a"
                      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/fastgltf/include")

# spirv_reflect
add_library(spirv_reflect STATIC IMPORTED GLOBAL)
set_target_properties(spirv_reflect PROPERTIES
                      IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect/libspirv_reflect.a"
                      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect/include")

# lua
add_library(lua STATIC IMPORTED GLOBAL)
set_target_properties(lua PROPERTIES
                      IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lua/liblua.a"
                      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/lua/include")
