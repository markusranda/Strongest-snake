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
