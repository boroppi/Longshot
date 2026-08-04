set(FSIC_PLATFORM_SOURCES "")
set(FSIC_PLATFORM_LIBS "")

if(FSIC_ENABLE_SYNTHETIC_BACKEND AND EXISTS "${CMAKE_SOURCE_DIR}/src/platform/synthetic/synthetic_backend.cpp")
  list(APPEND FSIC_PLATFORM_SOURCES src/platform/synthetic/synthetic_backend.cpp)
  add_compile_definitions(FSIC_HAVE_SYNTHETIC=1)
endif()

if(WIN32)
  set(FSIC_PLATFORM win32)
  if(EXISTS "${CMAKE_SOURCE_DIR}/src/platform/win32/win32_backend.cpp")
    list(APPEND FSIC_PLATFORM_SOURCES
         src/platform/win32/win32_backend.cpp src/platform/win32/win32_capture.cpp
         src/platform/win32/win32_windows.cpp src/platform/win32/win32_input.cpp
         src/platform/win32/win32_dpi.cpp)
    list(APPEND FSIC_PLATFORM_LIBS user32 gdi32 dwmapi shcore)
    add_compile_definitions(FSIC_PLATFORM_WIN32=1 WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    set(FSIC_WIN32_MANIFEST "${CMAKE_SOURCE_DIR}/src/platform/win32/fsic.manifest")
  endif()
elseif(APPLE)
  set(FSIC_PLATFORM macos)
elseif(UNIX)
  set(FSIC_PLATFORM x11)
endif()
