#=============================================================================
# Borg 2.1 -- Learning Classifier System / Distributed LCS simulator
#
# Originally built with Borland C++ 4.02 for 16-bit Windows 3.x, driven by the
# IDE project file BORG.IDE.  That file is NOT the manifest here: it is stale,
# still listing GLOBAL.CPP and ENVIRON.CPP, neither of which exists any more
# (ENVIRON.CPP became EI.CPP).  The source list below is explicit.
#
# WHAT THIS BUILDS
#   borgcore          Layer 1 (classifier system) + Layer 2 (task environment).
#   borg_determinism  The Phase 1 guard test.  See tests/determinism.cpp.
#
# WHAT THIS DOES NOT BUILD
#   UI.CPP, GRAPH.CPP, BORG.CPP, BORG.RC -- Layer 3, the Win16 + BWCC user
#   interface.  Those stay in the tree as the preserved 1995 artifact and are
#   replaced by a Dear ImGui front end in a later phase.
#=============================================================================

cmake_minimum_required(VERSION 3.20)
project(Borg VERSION 2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

#-----------------------------------------------------------------------------
# Layer 1 + Layer 2, in dependency order.  The 1995 sources are unmodified
# except for the portability fixes recorded in git; every one of them carries a
# "2026 PORT" comment at the point of change.
#-----------------------------------------------------------------------------
add_library(borgcore STATIC
    MESSAGE.CPP
    MSGBOARD.CPP
    QUEUE.CPP
    CLSSLIST.CPP
    NI.CPP
    CFS.CPP
    EI.CPP
    TASK.CPP
    compat/borland_rand.cpp
)

target_include_directories(borgcore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}          # the 1995 headers
    ${CMAKE_CURRENT_SOURCE_DIR}/compat   # except.h, values.h, borland_rand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host     # host.h
)

#-----------------------------------------------------------------------------
# `huge` was a 16-bit far-pointer qualifier (coord huge **agentPts, and 8 more
# sites in TASK.CPP/TASK.H).  It has no meaning on a flat 64-bit address space.
# Defining it away keeps those declarations byte-identical to the 1995 source
# instead of editing them.
#-----------------------------------------------------------------------------
target_compile_definitions(borgcore PUBLIC huge=)

#-----------------------------------------------------------------------------
# The sources spell their includes in lower case (#include"cfs.h") while the
# files on disk are 8.3 upper case (CFS.H) -- an artifact of DOS, where case
# did not exist.  macOS's default APFS volume is case-insensitive, so this
# resolves correctly and the warning is noise here.
#
# IF YOU EVER BUILD ON LINUX, OR ON A CASE-SENSITIVE APFS VOLUME, this is a
# real failure, not a warning.  Two ways out at that point, neither taken now:
#   - rewrite the #include lines to match the upper-case filenames, or
#   - have CMake generate a directory of lower-case symlinks into the build
#     tree and put it first on the include path, leaving the sources untouched.
#-----------------------------------------------------------------------------
target_compile_options(borgcore PUBLIC
    -Wall
    -Wno-nonportable-include-path
)

#-----------------------------------------------------------------------------
# Phase 1 guard.  Run with: ctest --test-dir build --output-on-failure
#-----------------------------------------------------------------------------
enable_testing()

add_executable(borg_determinism tests/determinism.cpp)
target_link_libraries(borg_determinism PRIVATE borgcore)

add_test(NAME determinism COMMAND borg_determinism)
