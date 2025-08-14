cmake_minimum_required(VERSION 3.20)
project(keyleport CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE KEYLEPORT_SOURCES CONFIGURE_DEPENDS
  src/*.cpp
)

# Generic platform filtering: only compile sources under the active platform dir
if(APPLE)
  # On macOS include everything except windows-specific sources
  list(FILTER KEYLEPORT_SOURCES EXCLUDE REGEX ".*/windows/.*\\.cpp$")
elseif(WIN32)
  # On Windows include everything except macOS-specific sources
  list(FILTER KEYLEPORT_SOURCES EXCLUDE REGEX ".*/macos/.*\\.cpp$")
else()
  # On other platforms exclude both macOS and Windows specific trees
  list(FILTER KEYLEPORT_SOURCES EXCLUDE REGEX ".*/(macos|windows)/.*\\.cpp$")
endif()

# Remove platform-specific networking folders; use cxx (Asio) implementation
list(FILTER KEYLEPORT_SOURCES EXCLUDE REGEX ".*/networking/server/(macos|windows)/.*\\.cpp$")
# Remove platform-specific discovery folders; use cxx implementation
list(FILTER KEYLEPORT_SOURCES EXCLUDE REGEX ".*/networking/discovery/(macos|windows)/.*\\.cpp$")

add_executable(keyleport ${KEYLEPORT_SOURCES})

target_include_directories(keyleport PRIVATE src)

# Asio (standalone) header-only via FetchContent
include(FetchContent)
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG asio-1-28-0
)
FetchContent_MakeAvailable(asio)
# Provide include dir and define ASIO_STANDALONE
if(DEFINED asio_SOURCE_DIR)
  target_include_directories(keyleport PRIVATE ${asio_SOURCE_DIR}/asio/include)
endif()
target_compile_definitions(keyleport PRIVATE ASIO_STANDALONE)

if(APPLE OR UNIX)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(SDL3 QUIET sdl3)
    if(SDL3_FOUND)
      target_include_directories(keyleport PRIVATE ${SDL3_INCLUDE_DIRS})
      target_link_libraries(keyleport PRIVATE ${SDL3_LINK_LIBRARIES})
    endif()
  endif()
endif()

# SDL3 on Windows: prefer CMake package, fallback to FetchContent
if(WIN32)
  # Try a CMake-config package (e.g., via vcpkg)
  find_package(SDL3 QUIET CONFIG)
  if(NOT SDL3_FOUND AND NOT TARGET SDL3::SDL3)
    include(FetchContent)
    FetchContent_Declare(
      SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.0.0
    )
    # Trim features for faster CI builds
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3)
  endif()
  if(TARGET SDL3::SDL3)
    target_link_libraries(keyleport PRIVATE SDL3::SDL3)
  endif()
endif()

# Link Windows system libraries when building on Windows
if(WIN32)
  target_link_libraries(keyleport PRIVATE user32 ws2_32)
endif()

# Require nlohmann_json via FetchContent (header-only)
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)
target_link_libraries(keyleport PRIVATE nlohmann_json::nlohmann_json)

# Dear ImGui (core + optional SDL3 backend)
option(IMGUI_BUILD_DEMO "Build Dear ImGui demo file" OFF)
include(FetchContent)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.0
)
FetchContent_MakeAvailable(imgui)

# Create a small static lib for core ImGui sources
if(DEFINED imgui_SOURCE_DIR)
  set(IMGUI_CORE_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  )
  if(IMGUI_BUILD_DEMO)
    list(APPEND IMGUI_CORE_SOURCES ${imgui_SOURCE_DIR}/imgui_demo.cpp)
  endif()

  add_library(imgui STATIC ${IMGUI_CORE_SOURCES})
  target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  # Silence some deprecation warnings on macOS, keep tidy builds
  if(APPLE)
    target_compile_definitions(imgui PRIVATE IMGUI_DISABLE_OSX_FUNCTION_OBSOLETE)
  endif()

  # Optional SDL3 backend (platform + renderer using SDL_Renderer)
  if(TARGET keyleport AND ((DEFINED SDL3_FOUND AND SDL3_FOUND) OR TARGET SDL3::SDL3))
    add_library(imgui_backend_sdl3 STATIC
      ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
      ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
    )
    if(TARGET SDL3::SDL3)
      target_link_libraries(imgui_backend_sdl3 PUBLIC imgui SDL3::SDL3)
    else()
      target_link_libraries(imgui_backend_sdl3 PUBLIC imgui ${SDL3_LINK_LIBRARIES})
      target_include_directories(imgui_backend_sdl3 PUBLIC ${SDL3_INCLUDE_DIRS})
    endif()
  # ImGui's SDL3 backend still uses old names in some places; enable SDL old names for compatibility
  target_compile_definitions(imgui_backend_sdl3 PRIVATE SDL_ENABLE_OLD_NAMES)
    # Consumers can just link this one target to get ImGui + SDL3 backend
    add_library(imgui::sdl3 ALIAS imgui_backend_sdl3)
  endif()

  # Alias for the core target for cleaner linking
  add_library(imgui::imgui ALIAS imgui)
endif()

# Link ImGui (core) to our executable; backend is optional and can be linked by users of SDL3 code
target_link_libraries(keyleport PRIVATE imgui::imgui)

# If SDL3 is available, link the app with the ImGui SDL3 backend as well
if(TARGET imgui_backend_sdl3)
  target_link_libraries(keyleport PRIVATE imgui_backend_sdl3)
endif()

# Formatting helper (optional; requires clang-format installed)
find_program(CLANG_FORMAT_BIN NAMES clang-format)
if(CLANG_FORMAT_BIN)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_BIN} -i ${KEYLEPORT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-format on project sources")
endif()