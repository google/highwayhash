
project(highwayhash C CXX)

cmake_minimum_required(VERSION 3.18)

# BUILD_SHARED_LIBS is a standard CMake variable, but we declare it here to make
# it prominent in the GUI.
option(BUILD_SHARED_LIBS "Build library as shared." OFF)

# Force PIC on unix when building shared libs
# see: https://en.wikipedia.org/wiki/Position-independent_code
if(BUILD_SHARED_LIBS AND UNIX)
  option(CMAKE_POSITION_INDEPENDENT_CODE "Build with Position Independant Code." ON)
endif()


set(PROCESSOR_IS_ARM FALSE)
set(PROCESSOR_IS_AARCH64 FALSE)
set(PROCESSOR_IS_X86 FALSE)
set(PROCESSOR_IS_POWER FALSE)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
  set(PROCESSOR_IS_AARCH64 TRUE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
  set(PROCESSOR_IS_ARM TRUE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64)|(AMD64|amd64)|(^i.86$)")
  set(PROCESSOR_IS_X86 TRUE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)")
  set(PROCESSOR_IS_POWER TRUE)
endif()


if(CMAKE_COMPILER_IS_GNUCXX OR CLANG)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -O3 -fPIC -pthread -Wno-maybe-uninitialized")
  if(PROCESSOR_IS_ARM)
    # aarch64 and ARM use the same code, although ARM usually needs an extra flag for NEON.
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=hard -march=armv7-a -mfpu=neon")
  endif()
endif()


#
# library : highwayhash
#

set(HH_INCLUDES
  ${PROJECT_SOURCE_DIR}/highwayhash/c_bindings.h
  ${PROJECT_SOURCE_DIR}/highwayhash/highwayhash.h
)

set(HH_SOURCES
  ${PROJECT_SOURCE_DIR}/highwayhash/c_bindings.cc
  ${PROJECT_SOURCE_DIR}/highwayhash/hh_portable.cc
  ${PROJECT_SOURCE_DIR}/highwayhash/arch_specific.cc

  ${PROJECT_SOURCE_DIR}/highwayhash/scalar_sip_tree_hash.cc
  ${PROJECT_SOURCE_DIR}/highwayhash/sip_hash.cc
  ${PROJECT_SOURCE_DIR}/highwayhash/sip_tree_hash.cc

  ${PROJECT_SOURCE_DIR}/highwayhash/hh_portable.h
  ${PROJECT_SOURCE_DIR}/highwayhash/state_helpers.h

  ${PROJECT_SOURCE_DIR}/highwayhash/arch_specific.h
  ${PROJECT_SOURCE_DIR}/highwayhash/compiler_specific.h
  ${PROJECT_SOURCE_DIR}/highwayhash/load3.h
  ${PROJECT_SOURCE_DIR}/highwayhash/vector128.h
  ${PROJECT_SOURCE_DIR}/highwayhash/vector256.h
  ${PROJECT_SOURCE_DIR}/highwayhash/endianess.h
  ${PROJECT_SOURCE_DIR}/highwayhash/iaca.h
  ${PROJECT_SOURCE_DIR}/highwayhash/hh_types.h
  ${PROJECT_SOURCE_DIR}/highwayhash/hh_buffer.h

  ${PROJECT_SOURCE_DIR}/highwayhash/scalar_sip_tree_hash.h
  ${PROJECT_SOURCE_DIR}/highwayhash/sip_hash.h
  ${PROJECT_SOURCE_DIR}/highwayhash/sip_tree_hash.h
)

if(PROCESSOR_IS_ARM OR PROCESSOR_IS_AARCH64)
  list(APPEND HH_SOURCES  ${PROJECT_SOURCE_DIR}/highwayhash/hh_neon.cc)
  list(APPEND HH_SOURCES  ${PROJECT_SOURCE_DIR}/highwayhash/hh_neon.h)

elseif(PROCESSOR_IS_POWER)
  list(APPEND HH_SOURCES  ${PROJECT_SOURCE_DIR}/highwayhash/hh_vsx.cc)
  list(APPEND HH_SOURCES  ${PROJECT_SOURCE_DIR}/highwayhash/hh_vsx.h)

  set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/highwayhash/benchmark.cc
    PROPERTIES COMPILE_FLAGS -mvsx)

  set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/highwayhash/hh_vsx.cc
    PROPERTIES COMPILE_FLAGS -mvsx)

elseif(PROCESSOR_IS_X86)
  list(APPEND HH_SOURCES  ${PROJECT_SOURCE_DIR}/highwayhash/hh_avx2.cc)
  list(APPEND HH_SOURCES  ${PROJECT_SOURCE_DIR}/highwayhash/hh_sse41.cc)
  list(APPEND HH_SOURCES  ${PROJECT_SOURCE_DIR}/highwayhash/hh_avx2.h)
  list(APPEND HH_SOURCES  ${PROJECT_SOURCE_DIR}/highwayhash/hh_sse41.h)

  # TODO: Portability: Have AVX2 be optional so benchmarking can be done on older machines.
  set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/highwayhash/benchmark.cc
    PROPERTIES COMPILE_FLAGS  -mavx2)

  set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/highwayhash/sip_tree_hash.cc
    PROPERTIES COMPILE_FLAGS  -mavx2)

  set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/highwayhash/hh_avx2.cc
    PROPERTIES COMPILE_FLAGS  -mavx2)

  set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/highwayhash/hh_sse41.cc
    PROPERTIES COMPILE_FLAGS  -msse4.1)

  set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/highwayhash/hh_portable.cc
    PROPERTIES COMPILE_FLAGS  -DHH_TARGET_NAME=Portable)

else()
  # Unknown architecture.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHH_DISABLE_TARGET_SPECIFIC")
endif()


add_library(highwayhash ${HH_INCLUDES} ${HH_SOURCES})
set_target_properties(highwayhash PROPERTIES PUBLIC_HEADER "${HH_INCLUDES}")

target_include_directories(highwayhash
  PUBLIC  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
)
target_include_directories(highwayhash
  PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/highwayhash>
)

if(NOT WIN32 AND NOT ANDROID)
  target_link_libraries(highwayhash pthread)
endif()

add_library(highwayhash::highwayhash ALIAS highwayhash)


#
# Tests & Similar
#

add_library(nanobenchmark OBJECT
   ${PROJECT_SOURCE_DIR}/highwayhash/nanobenchmark.h
   ${PROJECT_SOURCE_DIR}/highwayhash/nanobenchmark.cc

   ${PROJECT_SOURCE_DIR}/highwayhash/instruction_sets.h
   ${PROJECT_SOURCE_DIR}/highwayhash/os_specific.h
   ${PROJECT_SOURCE_DIR}/highwayhash/profiler.h
   ${PROJECT_SOURCE_DIR}/highwayhash/tsc_timer.h

   ${PROJECT_SOURCE_DIR}/highwayhash/instruction_sets.cc
   ${PROJECT_SOURCE_DIR}/highwayhash/os_specific.cc
)
target_include_directories(nanobenchmark PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


add_executable(highwayhash_test)
target_sources(highwayhash_test PRIVATE

  ${PROJECT_SOURCE_DIR}/highwayhash/highwayhash_test.cc
  ${PROJECT_SOURCE_DIR}/highwayhash/highwayhash_test_portable.cc
  ${PROJECT_SOURCE_DIR}/highwayhash/highwayhash_test_target.h
)
target_link_libraries(highwayhash_test highwayhash nanobenchmark)


add_executable(vector_test)
target_sources(vector_test PRIVATE
   ${PROJECT_SOURCE_DIR}/highwayhash/vector_test.cc
   ${PROJECT_SOURCE_DIR}/highwayhash/vector_test_portable.cc
   ${PROJECT_SOURCE_DIR}/highwayhash/vector_test_target.h
)
target_link_libraries(vector_test highwayhash nanobenchmark)


if(PROCESSOR_IS_ARM OR PROCESSOR_IS_AARCH64)
  target_sources(highwayhash_test PRIVATE
    ${PROJECT_SOURCE_DIR}/highwayhash/highwayhash_test_neon.cc
  )
  target_sources(vector_test PRIVATE
    ${PROJECT_SOURCE_DIR}/highwayhash/vector_test_neon.cc
  )

elseif(PROCESSOR_IS_X86)
  target_sources(highwayhash_test PRIVATE
    ${PROJECT_SOURCE_DIR}/highwayhash/highwayhash_test_avx2.cc
    ${PROJECT_SOURCE_DIR}/highwayhash/highwayhash_test_sse41.cc
  )
  target_sources(vector_test PRIVATE
    ${PROJECT_SOURCE_DIR}/highwayhash/vector_test_avx2.cc
    ${PROJECT_SOURCE_DIR}/highwayhash/vector_test_sse41.cc
  )

  set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/highwayhash/highwayhash_test_avx2.cc
    PROPERTIES COMPILE_FLAGS  -mavx2)

  set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/highwayhash/highwayhash_test_sse41.cc
    PROPERTIES COMPILE_FLAGS  -msse4.1)

  set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/highwayhash/vector_test_avx2.cc
    PROPERTIES COMPILE_FLAGS  -mavx2)

  set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/highwayhash/vector_test_sse41.cc
    PROPERTIES COMPILE_FLAGS  -msse4.1)

elseif(PROCESSOR_IS_POWER)
  target_sources(highwayhash_test PRIVATE
    ${PROJECT_SOURCE_DIR}/highwayhash/highwayhash_test_vsx.cc
  )

  set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/highwayhash/highwayhash_test_vsx.cc
    PROPERTIES COMPILE_FLAGS -mvsx)

  set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/highwayhash/vector_test.cc
    PROPERTIES COMPILE_FLAGS  -DHH_DISABLE_TARGET_SPECIFIC)

endif()


add_executable(sip_hash_test)
target_sources(sip_hash_test PRIVATE
    ${PROJECT_SOURCE_DIR}/highwayhash/sip_hash_test.cc
)
target_link_libraries(sip_hash_test highwayhash)


add_executable(example)
target_sources(example PRIVATE
    ${PROJECT_SOURCE_DIR}/highwayhash/example.cc
 )
target_link_libraries(example highwayhash)

