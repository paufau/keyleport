cmake_minimum_required(VERSION 3.21)
project(keyleport C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(kp_log MSG)
  string(TIMESTAMP _ts "%Y-%m-%d %H:%M:%S")
  message(STATUS "[${_ts}] ${MSG}")
endfunction()

kp_log("Configuring keyleport on ${CMAKE_SYSTEM_NAME} with CMake ${CMAKE_VERSION}; Generator: ${CMAKE_GENERATOR}")

file(GLOB_RECURSE KEYLEPORT_SOURCES CONFIGURE_DEPENDS
  src/*.cpp
)
list(LENGTH KEYLEPORT_SOURCES _kp_src_globbed)
kp_log("Globbed src/*.cpp -> ${_kp_src_globbed} sources")

# Platform-aware implementation selector
#
# For any header under src/, prefer an implementation in a platform subfolder
# then "cxx" subfolder, then same folder as the header. Exclude other
# implementations if a higher-preference one is present. This runs after the
# coarse platform folder filtering above, so non-target platform subfolders are
# already excluded from KEYLEPORT_SOURCES.
function(keyleport_select_platform_impl SOURCES_VAR)
  # Make a working copy of the list
  set(_all_src "${${SOURCES_VAR}}")

  # Collect headers to drive selection
  file(GLOB_RECURSE _hdrs CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/src/*.hpp
  )

  foreach(_h IN LISTS _hdrs)
    get_filename_component(_hdr_dir "${_h}" DIRECTORY)
    get_filename_component(_stem "${_h}" NAME_WE)

    # Candidate implementations (absolute paths)
    set(_cand_same    "${_hdr_dir}/${_stem}.cpp")
    set(_cand_cxx     "${_hdr_dir}/cxx/${_stem}.cpp")
    set(_cand_macos   "${_hdr_dir}/macos/${_stem}.cpp")
    set(_cand_windows "${_hdr_dir}/windows/${_stem}.cpp")

    # Build preference order for current platform
    set(_pref_list)
    if(APPLE)
      list(APPEND _pref_list "${_cand_macos}" "${_cand_cxx}" "${_cand_same}")
    elseif(WIN32)
      list(APPEND _pref_list "${_cand_windows}" "${_cand_cxx}" "${_cand_same}")
    else()
      list(APPEND _pref_list "${_cand_cxx}" "${_cand_same}")
    endif()

    # Determine which candidates actually exist and are currently part of the sources
    set(_present)
    foreach(_c IN LISTS _pref_list)
      if(EXISTS "${_c}")
        list(FIND _all_src "${_c}" _idx)
        if(NOT _idx EQUAL -1)
          list(APPEND _present "${_c}")
        endif()
      endif()
    endforeach()

    if(_present)
      # Keep the first in preference order; remove the others from the sources
      list(GET _present 0 _keep)
      foreach(_c IN LISTS _present)
        if(NOT _c STREQUAL _keep)
          list(REMOVE_ITEM _all_src "${_c}")
        endif()
      endforeach()
    endif()
  endforeach()

  # Return the filtered list
  set(${SOURCES_VAR} "${_all_src}" PARENT_SCOPE)
endfunction()

# Apply the per-header platform implementation selection
keyleport_select_platform_impl(KEYLEPORT_SOURCES)

# Device-name sources are self-gated with preprocessor guards per platform,
# so we don't need to filter them here.

# Windows: SDL3 is required; provide SDL3_DIR or CMAKE_PREFIX_PATH
if(WIN32)
  find_package(SDL3 CONFIG REQUIRED)
endif()

add_executable(keyleport ${KEYLEPORT_SOURCES})
list(LENGTH KEYLEPORT_SOURCES _kp_src_len)
kp_log("Target 'keyleport' created with ${_kp_src_len} sources")

target_include_directories(keyleport PRIVATE src)

if(APPLE OR UNIX)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    kp_log("Checking SDL3 via pkg-config")
    pkg_check_modules(SDL3 QUIET sdl3)
    if(SDL3_FOUND)
      list(LENGTH SDL3_INCLUDE_DIRS _sdl_inc_count)
      list(LENGTH SDL3_LINK_LIBRARIES _sdl_lib_count)
      kp_log("SDL3 found via pkg-config: ${_sdl_inc_count} include dirs, ${_sdl_lib_count} libs")
      target_include_directories(keyleport PRIVATE ${SDL3_INCLUDE_DIRS})
      target_link_libraries(keyleport PRIVATE ${SDL3_LINK_LIBRARIES})
    else()
      kp_log("SDL3 not found via pkg-config; continuing without it")
    endif()
  endif()
endif()

# Link Windows system libraries when building on Windows
if(WIN32)
  target_link_libraries(keyleport PRIVATE user32 ws2_32 SDL3::SDL3)
  # Copy all dependent runtime DLLs (incl. SDL3.dll) next to the executable
  add_custom_command(TARGET keyleport POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:keyleport>
      $<TARGET_FILE_DIR:keyleport>
    COMMAND_EXPAND_LISTS)
endif()

# Require nlohmann_json via FetchContent (header-only)
include(FetchContent)
kp_log("Preparing nlohmann_json (FetchContent)")
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)
kp_log("nlohmann_json available")
target_link_libraries(keyleport PRIVATE nlohmann_json::nlohmann_json enet)

# ENet (reliable/unreliable UDP library)
include(FetchContent)
kp_log("Preparing ENet (FetchContent)")
FetchContent_Declare(
  enet
  GIT_REPOSITORY https://github.com/lsalzman/enet.git
  GIT_TAG v1.3.17
)
FetchContent_Populate(enet)
kp_log("ENet populated at: ${enet_SOURCE_DIR}")
# Build ENet manually as a static lib to avoid upstream CMake policy issues
set(ENET_PLATFORM_SRC ${enet_SOURCE_DIR}/unix.c)
if(WIN32)
  set(ENET_PLATFORM_SRC ${enet_SOURCE_DIR}/win32.c)
endif()
add_library(enet STATIC
  ${enet_SOURCE_DIR}/callbacks.c
  ${enet_SOURCE_DIR}/compress.c
  ${enet_SOURCE_DIR}/host.c
  ${enet_SOURCE_DIR}/list.c
  ${enet_SOURCE_DIR}/packet.c
  ${enet_SOURCE_DIR}/peer.c
  ${enet_SOURCE_DIR}/protocol.c
  ${ENET_PLATFORM_SRC}
)
set_target_properties(enet PROPERTIES LINKER_LANGUAGE C LANGUAGE C)
target_include_directories(enet PUBLIC ${enet_SOURCE_DIR}/include)
if(APPLE OR UNIX)
  target_link_libraries(enet PUBLIC m)
elseif(WIN32)
  target_link_libraries(enet PUBLIC ws2_32 winmm)
endif()
target_link_libraries(keyleport PRIVATE enet)
kp_log("ENet static library added and linked")

# Dear ImGui (core + optional SDL3 backend)
option(IMGUI_BUILD_DEMO "Build Dear ImGui demo file" OFF)
include(FetchContent)
kp_log("Preparing Dear ImGui (FetchContent)")
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.0
)
FetchContent_MakeAvailable(imgui)
kp_log("Dear ImGui available")

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

# Install and package (bundle SDL3 on Windows)
install(TARGETS keyleport RUNTIME DESTINATION .)
if(WIN32)
  # Install all runtime DLLs alongside the executable
  install(CODE [[
    file(GET_RUNTIME_DEPENDENCIES
      RESOLVED_DEPENDENCIES_VAR deps
      EXECUTABLES $<TARGET_FILE:keyleport>)
    foreach(dll ${deps})
      get_filename_component(name "${dll}" NAME)
      file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}" TYPE FILE FILES "${dll}")
    endforeach()
  ]])
endif()

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "keyleport")
set(CPACK_PACKAGE_VERSION "0.1.0")
include(CPack)

# Formatting helper (optional; requires clang-format installed)
find_program(CLANG_FORMAT_BIN NAMES clang-format)
if(CLANG_FORMAT_BIN)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_BIN} -i ${KEYLEPORT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-format on project sources")
endif()