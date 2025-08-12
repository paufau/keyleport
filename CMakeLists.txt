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

# Always include all cross-platform C++ sources from any subdirectory under src
file(GLOB_RECURSE EXTRA_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/src/*/cxx/*.cpp
)

# If a platform-specific implementation exists (macos on APPLE, windows on WIN32)
# remove the cxx fallback with the same filename from EXTRA_SOURCES.
set(PLATFORM_DIR "")
if(APPLE)
  set(PLATFORM_DIR "macos")
elseif(WIN32)
  set(PLATFORM_DIR "windows")
endif()
if(PLATFORM_DIR)
  file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*/${PLATFORM_DIR}/*.cpp)
  set(PLATFORM_BASENAMES)
  foreach(PSRC IN LISTS PLATFORM_SOURCES)
    get_filename_component(_base ${PSRC} NAME)
    list(APPEND PLATFORM_BASENAMES ${_base})
  endforeach()
  foreach(CXXSRC IN LISTS EXTRA_SOURCES)
    get_filename_component(_cxxbase ${CXXSRC} NAME)
    if(_cxxbase IN_LIST PLATFORM_BASENAMES)
      list(REMOVE_ITEM EXTRA_SOURCES ${CXXSRC})
    endif()
  endforeach()
endif()

list(APPEND KEYLEPORT_SOURCES ${EXTRA_SOURCES})
list(REMOVE_DUPLICATES KEYLEPORT_SOURCES)

add_executable(keyleport ${KEYLEPORT_SOURCES})

target_include_directories(keyleport PRIVATE src)

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

# Require nlohmann_json via FetchContent (header-only)
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)
target_link_libraries(keyleport PRIVATE nlohmann_json::nlohmann_json)