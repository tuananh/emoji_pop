cmake_minimum_required(VERSION 3.16)
project(emoji_pop CXX)

set(EMOJI_POP_FONT_DIR "${CMAKE_SOURCE_DIR}/assets/fonts" CACHE STRING "Font directory path baked into the binary")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5
)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(imgui glfw)

find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)

add_executable(emoji_pop
    src/main.cpp
    src/instance.cpp
    src/config.cpp
    src/fonts.cpp
    src/tone_icons.cpp
    src/emoji_pop.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_compile_definitions(emoji_pop PRIVATE
    IMGUI_USER_CONFIG="imconfig_user.h"
    "EMOJI_POP_FONT_DIR=\"${EMOJI_POP_FONT_DIR}\""
)

target_include_directories(emoji_pop PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/misc/freetype
)

target_link_libraries(emoji_pop PRIVATE glfw OpenGL::GL Freetype::Freetype PkgConfig::HARFBUZZ)
