cmake_minimum_required(VERSION 3.30)

project(lost-engines-launcher)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT(CMAKE_BUILD_TYPE OR DEFINED ENV{CMAKE_BUILD_TYPE}))
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Default build type is Debug")
  set(CMAKE_BUILD_TYPE "Debug")
  message(STATUS "Build type defaulted to ${CMAKE_BUILD_TYPE}")
else()
  message(STATUS "Build type explicitly configured as ${CMAKE_BUILD_TYPE}")
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type)
if(build_type STREQUAL "debug")
  message(STATUS "Build type is debug - building with disabled optimisation and debugging symbols")
  set(opt_and_debug_compiler_options
    # optimisations
    -O0
    # debugging
    -g3
    -gsource-map
  )
  set(opt_and_debug_linker_options
    # optimisations
    -O0
    # debugging
    -g3
    -gsource-map
    --profiling-funcs
  )
elseif(build_type STREQUAL "release")
  message(STATUS "Build type is release - building with optimisation & minifying")
  set(opt_and_debug_compiler_options
    # optimisations
    -O3
    -flto
    # debugging disabled
    -g0
  )
  set(opt_and_debug_linker_options
    # optimisations - extra options for release build: https://emscripten.org/docs/tools_reference/emcc.html
    -O3
    --no-demangle
    -flto
    # debugging disabled
    -g0
  )
  set(opt_and_debug_compile_definitions
    IMGUI_DISABLE_DEBUG_TOOLS
  )
else()
  message(FATAL_ERROR "Invalid build type \"${CMAKE_BUILD_TYPE}\"")
endif()


set(Boost_NO_WARN_NEW_VERSIONS 1)
set(Boost_NO_BOOST_CMAKE TRUE)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Threads REQUIRED)
find_package(Boost 1.90.0 COMPONENTS url REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(BEFORE SYSTEM include)
include_directories(BEFORE .)

add_executable(lost-engines-launcher
  main.cpp
  init_graphics.cpp
  load_config.cpp
  platform_paths.cpp
  resource_loader.cpp
  include/imgui/imgui.cpp
  #include/imgui/imgui_demo.cpp
  include/imgui/imgui_draw.cpp
  include/imgui/imgui_tables.cpp
  include/imgui/imgui_widgets.cpp
  include/imgui/imgui_impl_glfw.cpp
  include/imgui/imgui_impl_opengl3.cpp
)

target_link_libraries(lost-engines-launcher PRIVATE
  glfw
  OpenGL::GL
  Boost::url
  ${CMAKE_DL_LIBS}
)

set(compile_options
  ${opt_and_debug_compiler_options}
  # errors
  -Wfatal-errors
  # warnings
  -Wall
  -Warray-bounds
  -Wcast-align
  -Wconversion
  -Wcovered-switch-default
  -Wdisabled-optimization
  -Wdocumentation
  -Wdouble-promotion
  -Wextra
  -Wextra-semi-stmt
  -Wfloat-equal
  -Wformat
  -Winit-self
  -Wimplicit-fallthrough
  -Winconsistent-missing-destructor-override
  -Winvalid-pch
  -Wlong-long
  -Wmissing-braces
  -Wmissing-declarations
  -Wmissing-include-dirs
  -Wmissing-prototypes
  -Wnon-virtual-dtor
  -Wold-style-cast
  -Woverloaded-virtual
  -Wpacked
  #-Wpadded                                                                      # useful to turn on occasionally until split - see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52981 and https://bugs.llvm.org/show_bug.cgi?id=22442
  -Wpointer-arith
  -Wrange-loop-analysis
  -Wredundant-decls
  -Wredundant-move
  -Wshadow
  -Wshorten-64-to-32
  -Wsuggest-override
  -Wswitch-enum
  -Wthread-safety-analysis
  -Wundefined-reinterpret-cast
  -Wuninitialized
  -Wunused
  -Wzero-as-null-pointer-constant
)
