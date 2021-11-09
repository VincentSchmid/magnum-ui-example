cmake_minimum_required(VERSION 3.0.0)
project(uiTest VERSION 0.1.0)

INCLUDE_DIRECTORIES(
    c:/dev/vcpkg/installed/x64-windows/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

LINK_DIRECTORIES(
    C:/dev/vcpkg/installed/x64-windows/lib
)

find_package(Corrade REQUIRED 
    Main
    Utility
)

find_package(Magnum CONFIG REQUIRED 
    GL
    MeshTools
    Primitives
    Shaders
    Sdl2Application
)

find_package(MagnumExtras REQUIRED Ui)

set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS ON)

add_executable(uiTest src/UIExample.cpp)

target_link_libraries(uiTest
    Corrade::Main
    Corrade::Utility
    Magnum::Magnum
    Magnum::GL
    Magnum::Application
    Magnum::Shaders
    Magnum::MeshTools
    Magnum::Primitives
)

target_link_libraries(uiTest MagnumExtras::Ui)
