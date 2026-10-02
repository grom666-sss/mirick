cmake_minimum_required(VERSION 3.21)
project(MirickMenu LANGUAGES CXX)
if(NOT WIN32)
  message(FATAL_ERROR "MirickMenu is a Windows/DirectX 9 DLL and must be built on Windows")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "GTA San Andreas is 32-bit. Configure with -A Win32")
endif()
include(FetchContent)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.9b)
FetchContent_Declare(minhook GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git GIT_TAG v1.3.4)
FetchContent_MakeAvailable(imgui minhook)
add_library(MirickMenu SHARED
  src/dllmain.cpp src/overlay.cpp src/menu.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx9.cpp)
target_include_directories(MirickMenu PRIVATE include ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(MirickMenu PRIVATE minhook d3d9)
target_compile_features(MirickMenu PRIVATE cxx_std_20)
target_compile_definitions(MirickMenu PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX IMGUI_DEFINE_MATH_OPERATORS)
set_target_properties(MirickMenu PROPERTIES OUTPUT_NAME "MirickMenu" SUFFIX ".dll")
