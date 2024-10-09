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

# glfw
add_library(glfw STATIC IMPORTED GLOBAL)
set_target_properties(glfw PROPERTIES
                      IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/GLFW/libglfw3.a"
                      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/GLFW/include")

# imgui
# add_library(imgui STATIC IMPORTED GLOBAL)
# set_target_properties(imgui PROPERTIES
#                       IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/imgui/libimgui.a"
#                       INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
add_library(imgui STATIC)

target_include_directories(imgui PUBLIC imgui)

target_sources(imgui PRIVATE
    imgui/imgui.h
    imgui/imgui.cpp

    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_impl_vulkan.cpp
    imgui/imgui_impl_glfw.cpp
)

target_link_libraries(imgui PUBLIC volk glfw)

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
