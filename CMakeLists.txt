cmake_minimum_required(VERSION 4.1)

project(strongest_snake LANGUAGES CXX)
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/share")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUNICODE -D_UNICODE)

# Grab all source files in src/
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS src/*.cpp src/*.h)

add_executable(strongest_snake ${SRC_FILES})

set_target_properties(strongest_snake PROPERTIES
    WIN32_EXECUTABLE OFF
)

# ============================
# Tracy Profiler
# ============================
set(TRACY_DIR "${CMAKE_SOURCE_DIR}/../tracy")

target_include_directories(strongest_snake PRIVATE
    ${TRACY_DIR}/public
)

target_sources(strongest_snake PRIVATE
    ${TRACY_DIR}/public/TracyClient.cpp
)

target_compile_definitions(strongest_snake PRIVATE TRACY_ENABLE)
# ============================


set(Vulkan_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/include")
set(Vulkan_LIBRARY "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/vulkan-1.lib")
target_include_directories(strongest_snake PRIVATE ${Vulkan_INCLUDE_DIR})
target_link_libraries(strongest_snake PRIVATE ${Vulkan_LIBRARY})

find_package(glm CONFIG REQUIRED)
target_link_libraries(strongest_snake PRIVATE glm::glm)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(strongest_snake PRIVATE glfw)

# Project includes
target_include_directories(strongest_snake PRIVATE src)

# Find glslc (should come with Vulkan SDK)
find_program(GLSLC_EXECUTABLE NAMES glslc HINTS "$ENV{VULKAN_SDK}/Bin")

if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found! Make sure Vulkan SDK is installed and VULKAN_SDK is set.")
endif()

# List all shader sources
set(SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/shaders/vert.vert
    ${CMAKE_SOURCE_DIR}/shaders/vert_font.vert
    ${CMAKE_SOURCE_DIR}/shaders/frag_flat.frag
    ${CMAKE_SOURCE_DIR}/shaders/frag_border.frag
    ${CMAKE_SOURCE_DIR}/shaders/frag_font.frag
    ${CMAKE_SOURCE_DIR}/shaders/frag_texture.frag
    ${CMAKE_SOURCE_DIR}/shaders/frag_texture_scrolling.frag
)

set(SHADER_OUTPUTS "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${SHADER} NAME_WE)   # NAME_WE = name without extension
    set(SPIRV ${CMAKE_SOURCE_DIR}/shaders/${FILE_NAME}.spv)  # write into source shaders/
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${FILE_NAME} to SPIR-V"
    )
    list(APPEND SHADER_OUTPUTS ${SPIRV})
endforeach()

# Add a custom target so shaders build along with your project
add_custom_target(Shaders ALL DEPENDS ${SHADER_OUTPUTS})

