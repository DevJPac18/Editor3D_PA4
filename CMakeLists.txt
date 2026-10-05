cmake_minimum_required(VERSION 3.10)
project(Editor3D_PA4 CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS ON)

file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)
add_executable(Editor3D WIN32 ${SOURCES})
target_include_directories(Editor3D PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(Editor3D PRIVATE _WIN32_WINNT=0x0601 WINVER=0x0601)
target_link_libraries(Editor3D opengl32 glu32 gdi32 comdlg32 shell32)

if(MINGW)
  # Evita un error de GCC 6.x en MinGW de 32 bits (clones IPA-CP de métodos thiscall)
  target_compile_options(Editor3D PRIVATE -fno-ipa-cp)
endif()

set_target_properties(Editor3D PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
