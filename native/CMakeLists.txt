# Standalone build of RubiconNarrativeCore (std-only C++17) for cross-language parity checks.
# The sources are the exact files the UE module compiles; nothing here is a copy.
#
#   npm run golden && npm run parity
#   (or) cmake -S native -B native/build -DCMAKE_BUILD_TYPE=Release && cmake --build native/build && native/build/rubicon_parity golden content

cmake_minimum_required(VERSION 3.16)
project(RubiconNarrativeCoreParity CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../unreal/Rubicon/Source/RubiconNarrativeCore)
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS ${CORE_DIR}/Private/NarrativeCore/*.cpp)

add_library(rubicon_core STATIC ${CORE_SOURCES})
target_include_directories(rubicon_core PUBLIC ${CORE_DIR}/Public PRIVATE ${CORE_DIR}/Private/NarrativeCore)

if(MSVC)
  target_compile_options(rubicon_core PRIVATE /W4 /WX /fp:precise /EHs-c-)
else()
  # -ffp-contract=off: no FMA contraction (JavaScript rounds every operation). -Wshadow mirrors UE5's
  # ShadowVariableWarningLevel=Error. -fno-exceptions mirrors UE's default.
  target_compile_options(rubicon_core PRIVATE -Wall -Wextra -Wshadow -Wpedantic -Werror -ffp-contract=off -fno-exceptions)
endif()

add_executable(rubicon_parity parity_main.cpp)
target_link_libraries(rubicon_parity PRIVATE rubicon_core)

add_executable(rubicon_core_tests core_tests.cpp)
target_link_libraries(rubicon_core_tests PRIVATE rubicon_core)
if(NOT MSVC)
  target_compile_options(rubicon_parity PRIVATE -Wall -Wextra -Werror -ffp-contract=off)
  target_compile_options(rubicon_core_tests PRIVATE -Wall -Wextra -Werror -ffp-contract=off)
endif()

# Exercise the fallback number formatting/parsing used on standard libraries without floating-point
# <charconv> (older libc++, e.g. some Android NDKs): cmake -DRUBICON_NO_TO_CHARS=ON
option(RUBICON_NO_TO_CHARS "Use the locale-independent stream fallback instead of std::to_chars/from_chars" OFF)
if(RUBICON_NO_TO_CHARS)
  target_compile_definitions(rubicon_core PRIVATE RUBICON_NO_TO_CHARS=1)
endif()
