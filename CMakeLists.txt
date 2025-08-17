cmake_minimum_required(VERSION 3.15)

project(strongest_snake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Allows w_char
add_definitions(-DUNICODE -D_UNICODE)

# Grab all source files in src/
file(GLOB SRC_FILES CONFIGURE_DEPENDS src/*.cpp src/*.h)

add_executable(strongest_snake ${SRC_FILES})

# Tell MSVC this is a Windows (GUI) app, not console
set_target_properties(strongest_snake PROPERTIES
    WIN32_EXECUTABLE TRUE
)

set(Vulkan_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/include")
set(Vulkan_LIBRARY "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/vulkan-1.lib")
find_package(Vulkan REQUIRED)
target_link_libraries(strongest_snake PRIVATE Vulkan::Vulkan)

# Make sure the compiler can find your headers in src/
target_include_directories(strongest_snake PRIVATE src)
