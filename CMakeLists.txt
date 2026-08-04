cmake_minimum_required(VERSION 3.20)
project(longshot VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FSIC_BUILD_TESTS "Build tests" ON)
option(FSIC_ENABLE_SYNTHETIC_BACKEND "Build synthetic (replay-from-disk) backend" ON)

include(cmake/FsicWarnings.cmake)

add_library(fsic_core STATIC
    src/core/log.cpp
    src/core/image.cpp
    src/core/image_io.cpp
    src/core/rowsig.cpp
    src/core/stitch.cpp
    src/core/backend_factory.cpp
    src/core/probe.cpp
    src/core/session.cpp
)
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/stb/stb_image_write.h")
  target_sources(fsic_core PRIVATE third_party/stb/stb_impl.c)
  target_compile_definitions(fsic_core PUBLIC FSIC_HAVE_STB=1)
else()
  message(WARNING "stb_image_write.h not vendored; PNG output disabled, BMP still available")
endif()
target_include_directories(fsic_core PUBLIC include third_party ${CMAKE_SOURCE_DIR}/src)
fsic_set_warnings(fsic_core)

include(cmake/FsicPlatform.cmake)
add_library(fsic_platform STATIC ${FSIC_PLATFORM_SOURCES})
target_link_libraries(fsic_platform PUBLIC fsic_core PRIVATE ${FSIC_PLATFORM_LIBS})
fsic_set_warnings(fsic_platform)

add_executable(longshot
  src/cli/main.cpp
  src/cli/args.cpp
  src/cli/cmd_stitch.cpp
  src/cli/cmd_list.cpp
  src/cli/cmd_capture.cpp
  src/cli/cmd_doctor.cpp
  src/cli/prompt.cpp
)
target_link_libraries(longshot PRIVATE fsic_platform)
fsic_set_warnings(longshot)
if(MSVC AND DEFINED FSIC_WIN32_MANIFEST AND EXISTS "${FSIC_WIN32_MANIFEST}")
  target_sources(longshot PRIVATE "${FSIC_WIN32_MANIFEST}")
endif()

add_executable(fsic_gen_testdata tools/gen_testdata.cpp)
target_link_libraries(fsic_gen_testdata PRIVATE fsic_core)
fsic_set_warnings(fsic_gen_testdata)

if(FSIC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
