# Build the C sources from the pinned CSPICE source mirror as one static target.
# The mirror's CMake project is intentionally not added: it mutates the parent
# cache and installs data files, neither of which is suitable for lmspace.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cspice/src/cspice")
  message(FATAL_ERROR "third_party/cspice missing: run scripts/sync-deps.sh")
endif()

file(GLOB CSPICE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cspice/src/cspice/*.c")
if(NOT CSPICE_SOURCES)
  message(FATAL_ERROR "No CSPICE C sources found")
endif()

add_library(cspice STATIC ${CSPICE_SOURCES})
target_include_directories(cspice PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cspice/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cspice/src/cspice")
# dtime_.c ships a compatibility sys/times.h whose guard collides with glibc's
# header on modern Linux. CSPICE's documented clock() path avoids that collision
# and is also the portable path for MinGW.
target_compile_definitions(cspice PRIVATE USE_CLOCK)
set_target_properties(cspice PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
  target_compile_options(cspice PRIVATE -Wno-implicit-int
    -Wno-incompatible-pointer-types -Wno-pointer-to-int-cast -Wno-parentheses
    -Wno-tautological-compare -Wno-format)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(cspice PRIVATE -Wno-shift-op-parentheses
      -Wno-logical-op-parentheses -Wno-unsequenced)
  endif()
endif()
if(UNIX AND NOT APPLE)
  # Expose the POSIX declarations needed by CSPICE's legacy C sources on glibc.
  target_compile_definitions(cspice PRIVATE _DEFAULT_SOURCE _XOPEN_SOURCE=700)
endif()
