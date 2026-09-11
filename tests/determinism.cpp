//============================================================================
// Phase 1 guard test.
//
// This test exists because of one specific, silent failure mode.  getRandom()
// in CFS.H divides by RAND_MAX+1.  Borland's RAND_MAX was 32767; a modern
// toolchain's is 2147483647, which overflows int and makes every random draw 0
// or negative.  Nothing throws, nothing warns, the simulation runs happily --
// and the animat never moves, the rule list never diversifies, and every
// number the program reports is meaningless.
//
// So the checks below deliberately test OBSERVABLE SIMULATION BEHAVIOUR, not
// just the generator in isolation.  A port that breaks the RNG again will fail
// CHECK 3 and CHECK 4 loudly instead of producing quiet nonsense.
//
// Run:  ctest --test-dir build --output-on-failure
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#include "task.h"
#include <host.h>

//----------------------------------------------------------------------------
// In 1995 these two globals lived in BORG.CPP (Layer 3) and had to be
// constructed in the order G, task, UI.  With the GUI gone, the host owns the
// taskEnvironment.  Phase 3 should decide whether Layer 2 ought to define its
// own global instead of making every front end remember to.
//----------------------------------------------------------------------------
taskEnvironment task;

int hostDisplayError(int num, const char *str)
{
  fprintf(stderr, "  [borg error %d] %s\n", num, str ? str : "");
  return 0;
}

//----------------------------------------------------------------------------
static int failures = 0;

static void ok(int cond, const char *what)
{
  printf("  %s  %s\n", cond ? "PASS" : "FAIL", what);
  if (!cond) ++failures;
}

//----------------------------------------------------------------------------
// FNV-1a over the raw bytes of every position and heading the run produced.
// Any change to the draw sequence, the kinematics, or the payoff shaping moves
// this value.
//----------------------------------------------------------------------------
// uint32_t, not unsigned long: `long` is 32-bit on Windows and 64-bit here, so
// a golden value computed in unsigned long would not travel between targets.
static uint32_t trajectoryHash(int maxTicks, int *ticksRun,
                               double *finalX, double *finalY)
{
  uint32_t h = 2166136261u;
  int done = 0, t = 0;

  for (t = 0; t < maxTicks && !done; t++)
  {
    done = task.clockTick();
    for (int a = 0; a < task.getNumAgents(); a++)
    {
      coord c = task.getCurrentLoc(a + 1);
      float v[3];
      v[0] = c.getX();  v[1] = c.getY();  v[2] = task.getCurrentDir(a + 1);
      const unsigned char *p = (const unsigned char *)v;
      for (unsigned k = 0; k < sizeof(v); k++)
      { h ^= p[k];  h *= 16777619u; }
    }
  }

  coord last = task.getCurrentLoc(1);
  *ticksRun = t;
  *finalX = last.getX();
  *finalY = last.getY();
  return h;
}

//============================================================================
int main(void)
{
  printf("\nBorg determinism and RNG-fidelity guard\n");
  printf("======================================\n\n");

  //--------------------------------------------------------------------------
  // CHECK 1 -- the generator's constants.
  //--------------------------------------------------------------------------
  printf("CHECK 1: Borland RAND_MAX and generator sequence\n");
  ok(RAND_MAX == 32767, "RAND_MAX is Borland's 32767");
  ok((long)RAND_MAX + 1L == 32768L, "RAND_MAX+1 does not overflow");

  // Turbo C's LCG from seed 100.  Hard-coded so a change to the generator is a
  // test failure rather than a silent change to every result in the thesis.
  {
    static const int expected[6] = { 1862, 11548, 3973, 4846, 9095, 16503 };
    srand(100);
    int allMatch = 1;
    printf("         seq:");
    for (int i = 0; i < 6; i++)
    {
      int r = rand();
      printf(" %d", r);
      if (r != expected[i]) allMatch = 0;
    }
    printf("\n");
    ok(allMatch, "rand() reproduces Turbo C's sequence from seed 100");
  }

  //--------------------------------------------------------------------------
  // CHECK 2 -- getRandom() is not degenerate.  This is the direct assertion
  // that the RAND_MAX+1 overflow is not present.
  //--------------------------------------------------------------------------
  printf("\nCHECK 2: getRandom() distribution\n");
  {
    srand(100);
    int counts[2] = { 0, 0 }, negative = 0, outOfRange = 0;
    for (int i = 0; i < 2000; i++)
    {
      int r = getRandom(2);
      if (r < 0) ++negative;
      else if (r > 1) ++outOfRange;
      else ++counts[r];
    }
    printf("         getRandom(2) over 2000 draws: [0]=%d [1]=%d negative=%d\n",
           counts[0], counts[1], negative);
    ok(negative == 0,    "getRandom() never returns a negative value");
    ok(outOfRange == 0,  "getRandom(2) stays inside [0,2)");
    ok(counts[1] > 800 && counts[1] < 1200, "getRandom(2) is roughly uniform");
  }

  //--------------------------------------------------------------------------
  // CHECK 3 -- the animat actually moves.  Under the overflow bug this is the
  // check that fails: every draw was 0, so both wheels stayed off and the
  // agent sat at its start position indefinitely.
  //--------------------------------------------------------------------------
  printf("\nCHECK 3: simulation is alive\n");
  task.reset();
  {
    coord start = task.getCurrentLoc(1);
    double sx = start.getX(), sy = start.getY();
    double moved = 0.0;
    for (int t = 0; t < 200; t++)
    {
      task.clockTick();
      coord c = task.getCurrentLoc(1);
      double d = fabs(c.getX() - sx) + fabs(c.getY() - sy);
      if (d > moved) moved = d;
    }
    printf("         start=(%.2f,%.2f)  max displacement over 200 ticks=%.2f\n",
           sx, sy, moved);
    ok(moved > 1.0, "agent 1 moves away from its start position");
  }

  //--------------------------------------------------------------------------
  // CHECK 4 -- a full seeded run is reproducible, and matches the recorded
  // golden state.  GOLDEN_* were captured on arm64 macOS with Apple clang 21
  // at the defaults in CFS.H / TASK.H (seed 100, 2 agents, STANDARD field).
  //--------------------------------------------------------------------------
  printf("\nCHECK 4: run-to-run determinism\n");
  {
    // Golden values recorded 11 Sep 2026, arm64 macOS, Apple clang 21.0.0,
    // RelWithDebInfo, at the defaults in CFS.H and TASK.H: seed 100, 2 agents,
    // STANDARD field, geneticInterval 4000, classListLength 32.  Agent 1 ends
    // at (288.90, 293.18) -- it reached the goal, which sits at (300, 300).
    const int      GOLDEN_TICKS  = 636;
    const uint32_t GOLDEN_HASH   = 3658847264u;
    const double   GOLDEN_X      = 288.90, GOLDEN_Y = 293.18;

    int tA = 0, tB = 0;
    double xA = 0, yA = 0, xB = 0, yB = 0;

    task.reset();
    uint32_t hA = trajectoryHash(9000, &tA, &xA, &yA);
    task.reset();
    uint32_t hB = trajectoryHash(9000, &tB, &xB, &yB);

    printf("         run A: ticks=%d final=(%.2f,%.2f) hash=%u\n", tA, xA, yA, hA);
    printf("         run B: ticks=%d final=(%.2f,%.2f) hash=%u\n", tB, xB, yB, hB);
    printf("         golden: ticks=%d final=(%.2f,%.2f) hash=%u\n",
           GOLDEN_TICKS, GOLDEN_X, GOLDEN_Y, GOLDEN_HASH);

    ok(hA == hB && tA == tB, "two seeded runs produce an identical trajectory");
    ok(tA == GOLDEN_TICKS,   "run length matches the recorded golden value");
    ok(hA == GOLDEN_HASH,    "trajectory matches the recorded golden hash");
    ok(fabs(xA - GOLDEN_X) < 0.01 && fabs(yA - GOLDEN_Y) < 0.01,
       "agent 1 ends at the recorded golden position");

    // The default run ends when the agent reaches the goal, not by exhaustion.
    ok(tA > 1 && tA < 9000, "run terminates on its own, not by hitting the tick cap");
  }

  printf("\n======================================\n");
  if (failures == 0) printf("All checks passed.\n\n");
  else               printf("%d CHECK(S) FAILED.\n\n", failures);
  return failures == 0 ? 0 : 1;
}
