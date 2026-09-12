#=============================================================================
# Borg 2.1 -- Learning Classifier System / Distributed LCS simulator
#
# Originally built with Borland C++ 4.02 for 16-bit Windows 3.x, driven by the
# IDE project file BORG.IDE.  That file is NOT the manifest here: it is stale,
# still listing GLOBAL.CPP and ENVIRON.CPP, neither of which exists any more
# (ENVIRON.CPP became EI.CPP).  The source list below is explicit.
#
# WHAT THIS BUILDS
#   borgcore          Layer 1 (classifier system) + Layer 2 (task environment),
#                     plus the host/ support code that used to live in Layer 3:
#                     the .sim and .P reader/writer, the .b batch runner, and the
#                     16-bit reader for the 1995 files themselves.
#   borg              Headless command-line front end.  cli/main.cpp.
#   borg_determinism  Phase 1 guard test.  tests/determinism.cpp.
#   borg_batchformat  Phase 3 guard test.  tests/batch_format.cpp.
#   borg_legacy16     Phase 4 guard test.  tests/legacy16.cpp.
#   borg_rules        .RUL round-trip guard, built TWICE -- see "THE TWO
#   borg_rules_plain  VARIANTS" below.
#   borg_determinism_plain  Phase 1 guard against the plain-LCS library.
#
# WHAT THIS DOES NOT BUILD
#   UI.CPP, GRAPH.CPP, BORG.CPP, BORG.RC -- Layer 3, the Win16 + BWCC user
#   interface.  Those stay in the tree as the preserved 1995 artifact and are
#   replaced by a Dear ImGui front end in a later phase.
#
# THE TWO VARIANTS
#   #define NETWORK in CFS.H switches DLCS code in and out of every module, and
#   the two branches are parallel overloads rather than additions.  That means
#   the #else branches are INVISIBLE TO THE COMPILER in a normal build: they are
#   never parsed, never type-checked, never run.  A real bug lived in one of them
#   for thirty-one years because of it (taskEnvironment::saveRules wrote the
#   "; Agent N" header only under NETWORK while loadRules always skipped a line,
#   so a plain-LCS build could not reload a rule file it had just written).
#
#   So borgcore is built twice: borgcore as the DLCS build, and borgcore_plain
#   with BORG_NO_NETWORK defined, which the guard added to CFS.H turns into a
#   plain-LCS build.  Only the .RUL guard test is currently built against both --
#   cli/main.cpp is DLCS-only, since it reports agent IDs -- but the library
#   itself is compiled and linked both ways, so the non-NETWORK branches can no
#   longer rot unnoticed.  Turn it off with -DBORG_BUILD_PLAIN_LCS=OFF.
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
set(BORG_SOURCES
    MESSAGE.CPP
    MSGBOARD.CPP
    QUEUE.CPP
    CLSSLIST.CPP
    NI.CPP
    CFS.CPP
    EI.CPP
    TASK.CPP
    compat/borland_rand.cpp

    # 2026 port support.  host/ is platform-neutral code that every front end
    # needs -- it is not a second GUI.  globals.cpp holds the taskEnvironment
    # that BORG.CPP used to define; simfile and batch are lifted out of UI.CPP;
    # legacy.cpp is new code with no 1995 counterpart -- it reads the 16-bit
    # files BORG.EXE wrote, which no build of this source could ever read back.
    host/globals.cpp
    host/simfile.cpp
    host/batch.cpp
    host/legacy.cpp
)

add_library(borgcore STATIC ${BORG_SOURCES})

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
# The plain-LCS variant of the same library.  See "THE TWO VARIANTS" above: this
# exists so the #else branches get compiled, linked and exercised instead of
# merely existing.  It is the same source list, the same flags, one extra define.
#-----------------------------------------------------------------------------
option(BORG_BUILD_PLAIN_LCS
       "Also build borgcore without NETWORK, so the plain-LCS branches are tested"
       ON)

if(BORG_BUILD_PLAIN_LCS)
  add_library(borgcore_plain STATIC ${BORG_SOURCES})
  target_include_directories(borgcore_plain PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/compat
      ${CMAKE_CURRENT_SOURCE_DIR}/host
  )
  target_compile_definitions(borgcore_plain PUBLIC huge= BORG_NO_NETWORK)
  target_compile_options(borgcore_plain PUBLIC -Wall -Wno-nonportable-include-path)
endif()

#-----------------------------------------------------------------------------
# The headless front end.  DLCS only: it prints agent IDs, which do not exist
# without NETWORK.
#-----------------------------------------------------------------------------
add_executable(borg cli/main.cpp)
target_link_libraries(borg PRIVATE borgcore)

#-----------------------------------------------------------------------------
# Guard tests.  Run with: ctest --test-dir build --output-on-failure
#-----------------------------------------------------------------------------
enable_testing()

add_executable(borg_determinism tests/determinism.cpp)
target_link_libraries(borg_determinism PRIVATE borgcore)
add_test(NAME determinism COMMAND borg_determinism)

# The same guard against the plain-LCS library.  It carries its own golden values
# -- the two variants genuinely differ, because a DLCS build has the network on by
# default and its agents share rules.  See CHECK 4 in tests/determinism.cpp.
if(BORG_BUILD_PLAIN_LCS)
  add_executable(borg_determinism_plain tests/determinism.cpp)
  target_link_libraries(borg_determinism_plain PRIVATE borgcore_plain)
  add_test(NAME determinism_plain COMMAND borg_determinism_plain)
endif()

add_executable(borg_batchformat tests/batch_format.cpp)
target_link_libraries(borg_batchformat PRIVATE borgcore)
add_test(NAME batch_format COMMAND borg_batchformat)

# Phase 4: decode the genuine 1995 16-bit files in tests/fixtures/legacy and
# check the decoded values, the .SIM/.P agreement, and the round trip through
# the modern loader.  The fixture path is passed in rather than compiled in.
add_executable(borg_legacy16 tests/legacy16.cpp)
target_link_libraries(borg_legacy16 PRIVATE borgcore)
add_test(NAME legacy16
         COMMAND borg_legacy16 ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/legacy)

# End-to-end: run the genuine 1995 SAMPLE.B through the CLI and check the .O
# table, the .sim snapshot, and the rejection of a malformed spec line.  The
# script writes only inside its own scratch directory under the build tree.
add_test(NAME batch_run
         COMMAND ${CMAKE_COMMAND} -E env
                 sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_batch.sh
                    $<TARGET_FILE:borg>
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/sample.b
                    ${CMAKE_CURRENT_BINARY_DIR}/batch-scratch)

# The .RUL round trip, in both variants.  The plain-LCS one is the reason this
# test exists; the DLCS one is there so the two can be compared.  Each is given a
# scratch output path, which makes it leave its file behind for run_rules.sh.
add_executable(borg_rules tests/rules_roundtrip.cpp)
target_link_libraries(borg_rules PRIVATE borgcore)
add_test(NAME rules_dlcs
         COMMAND borg_rules
                 ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/legacy/BBA.RUL
                 ${CMAKE_CURRENT_BINARY_DIR}/rules-dlcs.RUL)

if(BORG_BUILD_PLAIN_LCS)
  add_executable(borg_rules_plain tests/rules_roundtrip.cpp)
  target_link_libraries(borg_rules_plain PRIVATE borgcore_plain)
  add_test(NAME rules_plain
           COMMAND borg_rules_plain
                   ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/legacy/BBA.RUL
                   ${CMAKE_CURRENT_BINARY_DIR}/rules-plain.RUL)

  # The cross-variant comparison: a .RUL written by the plain build and one
  # written by the DLCS build must be the same bytes.  Depends on both tests
  # above having run, so it is ordered after them.
  add_test(NAME rules_cross_variant
           COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_rules.sh
                      ${CMAKE_CURRENT_BINARY_DIR}/rules-dlcs.RUL.a
                      ${CMAKE_CURRENT_BINARY_DIR}/rules-plain.RUL.a
                      ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/legacy/BBA.RUL)
  set_tests_properties(rules_cross_variant PROPERTIES DEPENDS "rules_dlcs;rules_plain")
endif()

# End-to-end for Phase 4: push the 1995 files through the CLI -- decode, convert,
# cross-check, replay -- and confirm the conversion round-trips byte-for-byte.
add_test(NAME legacy_run
         COMMAND ${CMAKE_COMMAND} -E env
                 sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_legacy.sh
                    $<TARGET_FILE:borg>
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/legacy
                    ${CMAKE_CURRENT_BINARY_DIR}/legacy-scratch)
