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
      target_compile_definitions(keyleport PRIVATE USE_SDL3)
      target_include_directories(keyleport PRIVATE ${SDL3_INCLUDE_DIRS})
      target_link_libraries(keyleport PRIVATE ${SDL3_LINK_LIBRARIES})
    endif()
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

# Formatting helper (optional; requires clang-format installed)
find_program(CLANG_FORMAT_BIN NAMES clang-format)
if(CLANG_FORMAT_BIN)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_BIN} -i ${KEYLEPORT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-format on project sources")
endif()