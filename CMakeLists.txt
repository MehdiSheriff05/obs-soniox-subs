cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

include(FetchContent)

set(USE_TLS ON CACHE BOOL "" FORCE)
set(USE_WS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG v11.4.6
  EXCLUDE_FROM_ALL
)

# IXWebSocket and its vendored third-party sources (spdlog, msgpack11) are not
# warning-clean under the strict flags this project enables for its own
# target. cmake/macos/xcode.cmake bakes CMAKE_COMPILE_WARNING_AS_ERROR into
# CMAKE_XCODE_ATTRIBUTE_GCC_TREAT_WARNINGS_AS_ERRORS, which the Xcode
# generator captures per-target at the point each target is defined, so scope
# that variable off just for these fetched targets.
set(_soniox_saved_werror_attr ${CMAKE_XCODE_ATTRIBUTE_GCC_TREAT_WARNINGS_AS_ERRORS})
set(CMAKE_XCODE_ATTRIBUTE_GCC_TREAT_WARNINGS_AS_ERRORS NO)
FetchContent_MakeAvailable(ixwebsocket)
set(CMAKE_XCODE_ATTRIBUTE_GCC_TREAT_WARNINGS_AS_ERRORS ${_soniox_saved_werror_attr})

# Belt-and-suspenders: directly silence -Werror on the known fetched targets,
# regardless of which mechanism actually injected it into the Xcode project.
# COMPILE_WARNING_AS_ERROR is the portable (all-compilers) way to do this;
# the raw -Wno-error flag is GCC/Clang-only, so it's skipped on MSVC where
# it isn't a recognized option.
foreach(_soniox_tgt IN ITEMS ixwebsocket spdlog msgpack11 zlib)
  if(TARGET ${_soniox_tgt})
    if(NOT MSVC)
      target_compile_options(${_soniox_tgt} PRIVATE -Wno-error)
    endif()
    set_target_properties(
      ${_soniox_tgt}
      PROPERTIES COMPILE_WARNING_AS_ERROR OFF XCODE_ATTRIBUTE_GCC_TREAT_WARNINGS_AS_ERRORS NO
    )
  endif()
endforeach()

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ixwebsocket)

# IXWebSocket uses mbedtls for TLS on Windows by default; mbedtls's entropy
# source calls BCryptGenRandom (Windows CNG API), which isn't linked
# automatically. Confirmed via a real failed Windows CI build: "unresolved
# external symbol BCryptGenRandom".
if(WIN32)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE bcrypt)
endif()

# Our own target keeps -Werror, but ixwebsocket's headers (which we #include
# directly) aren't warning-clean; mark its include dirs as SYSTEM so clang
# suppresses warnings originating from headers found there.
get_target_property(_soniox_ix_includes ixwebsocket INTERFACE_INCLUDE_DIRECTORIES)
if(_soniox_ix_includes)
  target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PRIVATE ${_soniox_ix_includes})
endif()

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core Network REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/CaptionsDock.cpp
    src/CaptionsDock.h
    src/AudioBridge.cpp
    src/AudioBridge.h
    src/SonioxClient.cpp
    src/SonioxClient.h
    src/UpdateChecker.cpp
    src/UpdateChecker.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)

# Qt's static macOS build declares a link-time dependency on AGL.framework
# (legacy pre-Metal OpenGL API) via its .prl metadata. As of the macOS 26.2
# SDK, AGL.framework has no linkable binary left anywhere on the system (SDK
# and live /System/Library/Frameworks both lack the Mach-O), so the link
# fails with "framework 'AGL' not found". Nothing in Qt6/libobs's actual
# Cocoa/Metal rendering path calls into AGL at runtime on this OS version, so
# a symbol-less stub framework satisfies the linker safely. macOS/clang-only;
# meaningless on Windows/Linux, where AGL doesn't exist as a concept at all.
if(APPLE)
  set(_soniox_stub_framework_dir "${CMAKE_BINARY_DIR}/stub-frameworks")
  set(_soniox_agl_stub "${_soniox_stub_framework_dir}/AGL.framework/AGL")
  if(NOT EXISTS "${_soniox_agl_stub}")
    file(MAKE_DIRECTORY "${_soniox_stub_framework_dir}/AGL.framework")
    file(WRITE "${CMAKE_BINARY_DIR}/agl_stub.c" "")
    if(NOT CMAKE_OSX_SYSROOT)
      execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-path
        OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    endif()
    execute_process(
      COMMAND
        ${CMAKE_C_COMPILER} -dynamiclib -arch arm64 -arch x86_64 -isysroot ${CMAKE_OSX_SYSROOT}
        -mmacosx-version-min=12.0 -install_name
        /System/Library/Frameworks/AGL.framework/Versions/A/AGL -o "${_soniox_agl_stub}"
        "${CMAKE_BINARY_DIR}/agl_stub.c"
      RESULT_VARIABLE _soniox_agl_stub_result
    )
    if(NOT _soniox_agl_stub_result EQUAL 0)
      message(FATAL_ERROR "Failed to build stub AGL.framework")
    endif()
  endif()
  target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-F${_soniox_stub_framework_dir}")
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
