//============================================================================
// Phase 3 guard test: the .O table and the .b parser.
//
// The .O file is a fixed-column text table, and the columns ARE the format --
// the MATLAB and Excel post-processing that produced the thesis figures reads
// fields by position, not by delimiter.  So the rows this port writes have to
// land on exactly the byte offsets BORG.EXE used in 1995.
//
// CHECK 1 pins that against a surviving artifact.  The expected string is the
// first data row of Research/BBA/1_.O, a real batch output file written by
// BORG.EXE under Windows 3.1, with its trailing CR removed.  The values it
// encodes -- simulation 1, agent 1, seed 100, 23 iterations, all operator counts
// zero -- are fed back through the production formatter here, so any drift in a
// field width or in that irregular run of spaces before the last column shows up
// as a failing byte comparison.
//
// (1_.O itself has no header block.  It was stripped when the table was pulled
// into 1_.XLS, which sits next to it.  The header in borgStatsHeader() is
// character-identical to the one in UI.CPP:2264 instead, and to Borg 2.0's.)
//
// Run:  ctest --test-dir build --output-on-failure
//============================================================================

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "batch.h"
#include <host.h>

//----------------------------------------------------------------------------
// borgcore defines the taskEnvironment global (host/globals.cpp) and its
// constructor can call error(), so every host has to supply this hook even when,
// as here, the test never runs a simulation.
//----------------------------------------------------------------------------
int hostDisplayError(int num, const char *str)
{
  fprintf(stderr, "  [borg error %d] %s\n", num, str ? str : "");
  return 0;
}

static int failures = 0;

static void ok(int cond, const char *what)
{
  printf("  %s  %s\n", cond ? "PASS" : "FAIL", what);
  if (!cond) ++failures;
}

//----------------------------------------------------------------------------
static void showMismatch(const char *expected, const char *got)
{
  printf("         expected |%s|\n", expected);
  printf("         got      |%s|\n", got);
  size_t i = 0;
  while (expected[i] && got[i] && expected[i] == got[i]) ++i;
  printf("         first difference at byte %zu\n", i);
}

//============================================================================
int main(void)
{
  printf("\nBorg .O format and .b parser guard\n");
  printf("==================================\n\n");

  //--------------------------------------------------------------------------
  // CHECK 1 -- one row, byte for byte against 1995 output.
  //--------------------------------------------------------------------------
  printf("CHECK 1: .O data row matches Research/BBA/1_.O line 1\n");
  {
    // Research/BBA/1_.O, first line, CR stripped.  Written by BORG.EXE, 1995.
    const char *expected =
      "   1      1    100       23          0          0          0          0          0         0\n";

    char row[BORG_STATS_ROW_MAX];
    borgFormatStatsRow(row, /*simNum*/ 1, /*agentID*/ 1UL, /*seed*/ 100u,
                       /*iterations*/ 23, /*sexual*/ 0, /*asexual*/ 0,
                       /*mutations*/ 0, /*CDOs*/ 0, /*CEOs*/ 0, /*crashes*/ 0);

    int same = strcmp(row, expected) == 0;
    if (!same) showMismatch(expected, row);
    ok(same, "row is byte-identical to the 1995 artifact");
    ok(strlen(row) == strlen(expected), "row length matches");
  }

  //--------------------------------------------------------------------------
  // CHECK 2 -- what happens when a count outgrows its column.
  //
  // Every field is "%4d", and in printf a width is a MINIMUM, not a maximum.  So
  // a count of 10000 or more does not truncate -- it pushes every column to its
  // right one place over.  Nothing in the 1995 code bounds the mutation or
  // crossover totals, and a real run reaches five digits easily: SAMPLE.B's own
  // spec line (geneticInterval 20, maxCount 4000) produces around 10,000
  // mutations, so a .O table from it genuinely has shifted columns.
  //
  // 1_.O does not show this because those runs ended after 23 ticks with every
  // counter still at zero.  This check records the behaviour rather than
  // asserting it away: the first four columns -- simulation, agent, seed,
  // iterations -- are the ones the MATLAB post-processing reads by position, and
  // they are the ones that cannot shift, because everything that can overflow
  // sits to their right.
  //--------------------------------------------------------------------------
  printf("\nCHECK 2: column behaviour when a count outgrows 4 digits\n");
  {
    char narrow[BORG_STATS_ROW_MAX], wide[BORG_STATS_ROW_MAX];
    borgFormatStatsRow(narrow, 1, 1UL, 100u, 23, 0, 0,  9999, 0, 0, 0);
    borgFormatStatsRow(wide,   1, 1UL, 100u, 23, 0, 0, 10000, 0, 0, 0);

    printf("         4-digit mutations: %zu bytes\n", strlen(narrow));
    printf("         5-digit mutations: %zu bytes\n", strlen(wide));

    ok(strlen(narrow) == 93, "a row of 4-digit counts is 93 bytes incl. newline");
    ok(strlen(wide) == 94,   "a 5-digit count widens the row by one, as in 1995");

    // The four leading columns are fixed because nothing to their left can grow:
    // simulation 1-4, agent 8-11, seed 15-18, iterations 24-27.
    ok(strncmp(narrow, wide, 27) == 0,
       "the first four columns hold their byte offsets regardless");
  }

  //--------------------------------------------------------------------------
  // CHECK 3 -- the header block, byte for byte.
  //
  // The literal below is transcribed from userInterface::startBatch (UI.CPP:2264)
  // and is character-identical to Borg 2.0's (BORG/Borg20/UI.CPP:2108), so it is
  // the text every surviving .O was written with.  The one header file in
  // Research/ had its header stripped for import into Excel, so unlike CHECK 1
  // this is pinned to the source rather than to an output artifact.
  //--------------------------------------------------------------------------
  printf("\nCHECK 3: .O header block matches UI.CPP:2264\n");
  {
    const char *expected =
      "                    Number of   Sexual     Asexual    Number of  Number of  Number of  Number of\n"
      "Sim #  Agent  Seed  Iterations  Crossover  Crossover  Mutations  CDO's      CEO's      Crashes\n\n";

    const char *got = borgStatsHeader();
    int same = strcmp(got, expected) == 0;
    if (!same) showMismatch(expected, got);
    ok(same, "header is byte-identical to the 1995 text");
    ok(strlen(got) == 193, "header is 193 bytes, ending in a blank line");
  }

  printf("\n==================================\n");
  if (failures == 0) printf("All checks passed.\n\n");
  else               printf("%d CHECK(S) FAILED.\n\n", failures);
  return failures == 0 ? 0 : 1;
}
