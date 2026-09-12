//============================================================================
// Phase 4 guard test -- the 16-bit legacy reader.
//
// Everything here is pinned to GENUINE 1995 ARTIFACTS, not to values this port
// produced.  tests/fixtures/legacy holds three files copied unmodified out of
// Research/BBA, all dated 19 April 1995 and all written by BORG.EXE:
//
//     1.SIM     a 6192-byte 16-bit snapshot, simulation 1 of the BBA batch
//     1_1.P     the separately-written position dump for that same simulation
//     BBA.RUL   the rule file that batch was seeded from
//
// The point of testing against these rather than against a file written here is
// that there is no way to make the reader and the fixture agree by accident.  A
// round trip through a file this port wrote would pass with a consistently WRONG
// schema; 1.SIM cannot.
//
// The four checks are ordered by how much they would cost to fake:
//
//   CHECK 1  decoded field values, against numbers read out of a hex dump
//   CHECK 2  the cursor lands exactly on EOF, and the size arithmetic closes
//   CHECK 3  .SIM and .P -- two files, two code paths -- hold the same floats
//   CHECK 4  convert -> borgLoadSim -> borgSaveSim reproduces itself byte-exactly
//   CHECK 5  malformed input is refused rather than half-read
//   CHECK 6  .RUL counting, and the trap that counting exists to catch
//
// Run:  ctest --test-dir build --output-on-failure
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "task.h"
#include <host.h>
#include <simfile.h>
#include <legacy.h>

//----------------------------------------------------------------------------
// Errors are expected in CHECK 5, so they are counted and printed rather than
// treated as failures.  `task` itself comes from host/globals.cpp.
//----------------------------------------------------------------------------
static int errorsSeen = 0;
static int errorsQuiet = 0;

int hostDisplayError(int num, const char *str)
{
  ++errorsSeen;
  if (!errorsQuiet)
    fprintf(stderr,"  [borg error %d] %s\n",num,str ? str : "");
  return 0;
}

//----------------------------------------------------------------------------
static int failures = 0;

static void ok(int cond, const char *what)
{
  printf("  %s  %s\n", cond ? "PASS" : "FAIL", what);
  if (!cond) ++failures;
}

static void okEqI(long got, long want, const char *what)
{
  int good = (got == want);
  printf("  %s  %-46s %ld\n", good ? "PASS" : "FAIL", what, got);
  if (!good)
  { printf("        expected %ld\n",want); ++failures; }
}

static void okEqD(double got, double want, const char *what)
{
  int good = (got == want);
  printf("  %s  %-46s %g\n", good ? "PASS" : "FAIL", what, got);
  if (!good)
  { printf("        expected %g\n",want); ++failures; }
}

static void okEqS(const char *got, const char *want, const char *what)
{
  int good = (strcmp(got,want) == 0);
  printf("  %s  %-46s \"%s\"\n", good ? "PASS" : "FAIL", what, got);
  if (!good)
  { printf("        expected \"%s\"\n",want); ++failures; }
}

//----------------------------------------------------------------------------
static long fileSize(const char *p)
{
  FILE *f = fopen(p,"rb");
  if (!f) return -1;
  fseek(f,0,SEEK_END);
  long n = ftell(f);
  fclose(f);
  return n;
}

static int filesIdentical(const char *a, const char *b, long *lenOut)
{
  FILE *fa = fopen(a,"rb"), *fb = fopen(b,"rb");
  if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return 0; }

  long n = 0;
  int same = 1, ca, cb;
  do
  {
    ca = fgetc(fa);  cb = fgetc(fb);
    if (ca != cb) { same = 0; break; }
    if (ca != EOF) ++n;
  } while (ca != EOF);

  fclose(fa);  fclose(fb);
  if (lenOut) *lenOut = n;
  return same;
}

// Copy a fixture, optionally altering one byte, so CHECK 5 can damage a file
// without touching the fixture itself.
static int copyAltered(const char *src, const char *dst, long dropBytes,
                       long patchAt, int patchTo)
{
  FILE *in = fopen(src,"rb");
  if (!in) return 0;
  fseek(in,0,SEEK_END);
  long n = ftell(in);
  fseek(in,0,SEEK_SET);

  unsigned char *buf = (unsigned char *)malloc((size_t)n);
  if (!buf || fread(buf,1,(size_t)n,in) != (size_t)n)
  { fclose(in); free(buf); return 0; }
  fclose(in);

  if (patchAt >= 0 && patchAt < n) buf[patchAt] = (unsigned char)patchTo;

  FILE *out = fopen(dst,"wb");
  if (!out) { free(buf); return 0; }
  fwrite(buf,1,(size_t)(n - dropBytes),out);
  fclose(out);
  free(buf);
  return 1;
}

//============================================================================
int main(int argc, char **argv)
{
  const char *dir = (argc > 1) ? argv[1] : "tests/fixtures/legacy";
  char simPath[1024], posPath[1024], rulPath[1024];

  snprintf(simPath,sizeof(simPath),"%s/1.SIM",dir);
  snprintf(posPath,sizeof(posPath),"%s/1_1.P",dir);
  snprintf(rulPath,sizeof(rulPath),"%s/BBA.RUL",dir);

  printf("\nBorg 16-bit legacy reader guard\n");
  printf("==============================\n");
  printf("fixtures: %s\n\n",dir);

  borgLegacySim sim;
  memset(&sim,0,sizeof(sim));

  //--------------------------------------------------------------------------
  // CHECK 1 -- the decoded values.
  //
  // Every number below was read out of a hex dump of 1.SIM by hand before the
  // reader existed, so this check compares the code against the file, not
  // against itself.  Several are the interesting ones: bidCap is 30, not the
  // DEF_BID_CAP of 100, and goalReward/dirReward are 20, not the defaults of
  // 2 and 5 -- this run was configured, and a reader that silently fell back to
  // defaults would be caught here.
  //--------------------------------------------------------------------------
  printf("CHECK 1: decode Research/BBA/1.SIM\n");
  if (!borgLegacyReadSim(simPath,&sim))
  {
    printf("  FAIL  could not read %s -- nothing further can run\n",simPath);
    return 1;
  }

  okEqS(sim.version,"2.1",                        "version string");
  okEqI(sim.network,            1,                "file has the DLCS (NETWORK) layout");
  okEqI(sim.msgLen,             9,                "message length");
  okEqI(sim.seed,               100,              "seed");
  okEqI(sim.geneticInterval,    4000,             "geneticInterval");
  okEqI(sim.maxCount,           500,              "maxCount");
  okEqI(sim.networkEnabled,     0,                "networkEnabled (DLCS built in, turned off)");
  okEqI(sim.classListLen,       32,               "classifier list length");
  okEqI(sim.BBAenabled,         1,                "BBA enabled");
  okEqI(sim.maxActionsToPost,   1,                "maxActionsToPost");
  okEqI(sim.CDOenabled,         1,                "CDO enabled");
  okEqI(sim.CEOenabled,         1,                "CEO enabled");
  okEqD(sim.bidConstant,        0.125,            "bidConstant");
  okEqD(sim.sexualProb,         0.7,              "sexualProb");
  okEqD(sim.mutationProb,       0.1,              "mutationProb");
  okEqD(sim.strengthCap,        100.0,            "strengthCap");
  okEqD(sim.bidCap,             30.0,             "bidCap (not the default 100)");
  okEqS(sim.condMask,           "XXXXXXXXX",      "condition mask (COND_MASK_BBA)");
  okEqS(sim.actionMask,         "00BBBBBBB",      "action mask (ACTION_MASK_BBA)");
  okEqI(sim.msgBoardLen,        1,                "message board length");
  okEqD(sim.goalRange,          2000.0,           "goalRange");
  okEqD(sim.obstRange,          50.0,             "obstRange");
  okEqD(sim.goalReward,         20.0,             "goalReward (not the default 2)");
  okEqD(sim.dirReward,          20.0,             "dirReward (not the default 5)");
  okEqD(sim.crashPenalty,       5.0,              "crashPenalty");
  okEqI(sim.classPassing,       1,                "classifier passing enabled");
  okEqD(sim.TxStrenThresh,      0.8,              "TxStrenThresh");
  okEqI(sim.envType,            task.CONCAVE_OBST,"environment type is CONCAVE_OBST");
  okEqI(sim.x1,                 -400,             "playing field x1 (a negative 16-bit int)");
  okEqI(sim.y1,                 -300,             "playing field y1");
  okEqI(sim.x2,                 400,              "playing field x2");
  okEqI(sim.y2,                 500,              "playing field y2");
  okEqI(sim.numObst,            401,              "obstacle count");
  okEqI(sim.numAgents,          1,                "agent count");
  okEqI(sim.globalClock,        23,               "global clock");
  okEqD(sim.goalX,              0.0,              "goal x");
  okEqD(sim.goalY,              200.0,            "goal y");

  ok(sim.numAgents == 1 && sim.agent != 0, "one agent record was allocated");
  if (sim.numAgents >= 1 && sim.agent)
  {
    const borgLegacyAgent *a = &sim.agent[0];
    okEqI(a->agentID,       1,  "agent 1 ID");
    okEqI(a->clockCount,    23, "agent 1 iteration count");
    okEqI(a->ptCount,       24, "agent 1 saved positions (clock + the start)");
    okEqI(a->finished,      1,  "agent 1 is marked finished");
    okEqI(a->numRandCalls,  535,"agent 1 numRandCalls");
    okEqI(a->numCrashes,    0,  "agent 1 crashes");
    okEqI(a->numMutations,  0,  "agent 1 mutations (geneticInterval never fired)");
    okEqI(a->numCDOs,       0,  "agent 1 CDOs");
    okEqI(a->numCEOs,       0,  "agent 1 CEOs");

    // The first and last positions, to 6 significant figures, as stored.
    printf("         start (%.5f, %.5f) heading %.5f\n",
           a->x[0],a->y[0],a->dir[0]);
    printf("         end   (%.5f, %.5f) heading %.5f\n",
           a->x[a->ptCount-1],a->y[a->ptCount-1],a->dir[a->ptCount-1]);
    ok(a->x[0] == 0.0f && a->y[0] == 0.0f, "agent 1 starts at the origin");
    ok(a->dir[0] == 1.0f,                  "agent 1 starts heading 1.0 rad");
  }

  //--------------------------------------------------------------------------
  // CHECK 2 -- the cursor closes on EOF.
  //
  // This is the check that makes the whole schema trustworthy.  The widths in a
  // 1995 file are implied by the C types the save() functions used, and nothing
  // in the file states them; if a single field were read at the wrong width the
  // walk would finish somewhere other than the last byte.  It does not.
  //
  // The size arithmetic is restated here independently of the reader:
  //
  //     4                           version string
  //   + 216                         task.saveStatic()   (measured, DLCS layout)
  //   + 16 + 2*A + 8 + 8*O + 2*A    task.save() header, goal, obstacles, counts
  //   + sum over agents of 12*P     positions (coord) and headings (float)
  //   + 32*A + 20                   eight sensor arrays, two masks
  //   + A * (72*L + 24*B + 68)      classifierSystem::save() per agent
  //
  // with A=1 agent, O=401 obstacles, P=24 positions, L=32 rules, B=1 board slot,
  // which is 6192 -- the size of the file on disk.
  //--------------------------------------------------------------------------
  printf("\nCHECK 2: the walk accounts for every byte\n");
  {
    const long A = sim.numAgents, O = sim.numObst;
    const long L = sim.classListLen, B = sim.msgBoardLen;
    long expect = 4 + 216
                + 16 + 2*A + 8 + 8*O + 2*A
                + 32*A + 20
                + A * (72*L + 24*B + 68);
    for (int i = 0; i < sim.numAgents; i++) expect += 12*sim.agent[i].ptCount;

    printf("         file %ld bytes, walk consumed %ld, arithmetic predicts %ld\n",
           sim.fileSize,sim.bytesConsumed,expect);
    okEqI(sim.fileSize,      6192,          "1.SIM is 6192 bytes");
    ok(sim.bytesConsumed == sim.fileSize,   "the walk ended exactly at EOF");
    ok(expect == sim.fileSize,              "the 16-bit size arithmetic closes");

    // Widening int 2->4 and long 4->8 can at most double the file; char, float
    // and double are untouched, so in practice the growth is modest.
    printf("         widened image is %ld bytes (%.1f%% larger)\n",
           sim.modernSize,
           100.0*((double)sim.modernSize/(double)sim.fileSize - 1.0));
    ok(sim.modernSize > sim.fileSize && sim.modernSize <= 2*sim.fileSize,
       "the widened image grew, but by less than 2x");
  }

  //--------------------------------------------------------------------------
  // CHECK 3 -- .SIM and .P agree.
  //
  // taskEnvironment::save() and saveAgentPosBinary() wrote the same position
  // history into two different files through two different functions, and this
  // reader decodes them through two different walks.  Comparing them is as close
  // to an independent confirmation as a dead format allows -- and the comparison
  // is exact, not approximate: these are the identical float32 bit patterns, so
  // anything but equality means a decode error, never rounding.
  //--------------------------------------------------------------------------
  printf("\nCHECK 3: 1.SIM and 1_1.P hold the same trajectory\n");
  borgLegacyPos pos;
  memset(&pos,0,sizeof(pos));
  if (!borgLegacyReadPos(posPath,&pos))
    ok(0,"could not read the .P fixture");
  else
  {
    okEqI(pos.fileSize,      294, "1_1.P is 294 bytes");
    ok(pos.bytesConsumed == pos.fileSize, "the .P walk ended exactly at EOF");
    okEqI(pos.numAgents,     sim.numAgents, "same agent count as the .SIM");
    okEqI(pos.maxPositions,  sim.maxCount+1,
          ".P's position cap is the .SIM's maxCount + 1");

    int mismatch = 0, compared = 0;
    for (int i = 0; i < pos.numAgents && i < sim.numAgents; i++)
    {
      if (pos.agent[i].ptCount != sim.agent[i].ptCount) { mismatch = 1; continue; }
      for (int j = 0; j < pos.agent[i].ptCount; j++, compared++)
        if (pos.agent[i].x[j]   != sim.agent[i].x[j]   ||
            pos.agent[i].y[j]   != sim.agent[i].y[j]   ||
            pos.agent[i].dir[j] != sim.agent[i].dir[j]) ++mismatch;
    }
    printf("         compared %d positions and headings\n",compared);
    okEqI(compared, 24, "all 24 positions were compared");
    ok(mismatch == 0,
       "every float32 is bit-identical between the two 1995 files");
  }

  //--------------------------------------------------------------------------
  // CHECK 4 -- the converted file round-trips through the real loader.
  //
  // borgLegacyWriteModernSim() writes what the reader widened; borgLoadSim() is
  // the ordinary Phase 3 loader, with no knowledge of 1995 at all; borgSaveSim()
  // writes what the live objects now hold.  If those two files are byte-identical
  // then the widened image was, field for field, exactly what this build writes
  // -- which is a much stronger statement than "it loaded without crashing".
  //
  // This is also the moment the 1995 data becomes usable: after this load, the
  // live taskEnvironment IS simulation 1 of the April 1995 BBA batch.
  //--------------------------------------------------------------------------
  printf("\nCHECK 4: convert -> load -> save is byte-exact\n");
  {
    const char *conv = "legacy16-converted.sim";
    const char *again = "legacy16-resaved.sim";
    remove(conv);  remove(again);

    if (!borgLegacyWriteModernSim(&sim,conv))
      ok(0,"could not write the converted .sim");
    else
    {
      okEqI(fileSize(conv), sim.modernSize, "converted .sim is the widened size");

      if (!borgLoadSim(conv))
        ok(0,"the converted .sim did not load");
      else
      {
        ok(1,"the converted .sim loaded through the ordinary borgLoadSim()");
        okEqI(task.getNumAgents(),    sim.numAgents,   "live agent count");
        okEqI(task.readGlobalClock(), sim.globalClock, "live global clock");
        okEqI(task.readClock(0),      sim.agent[0].clockCount, "live agent clock");

        unsigned seed;
        int genInterval, maxCount, netOn;
        classifierSystem::getSettings(&seed,&genInterval,&maxCount,&netOn);
        okEqI(seed,        sim.seed,           "live seed");
        okEqI(maxCount,    sim.maxCount,       "live maxCount");
        okEqI(genInterval, sim.geneticInterval,"live geneticInterval");

        // The position the 1995 run ended on, now held by a live coord object.
        const coord & c = task.getCurrentLoc(1);
        ok(c.getX() == sim.agent[0].x[sim.agent[0].ptCount-1] &&
           c.getY() == sim.agent[0].y[sim.agent[0].ptCount-1],
           "the live agent sits where the 1995 run left it");

        if (!borgSaveSim(again)) ok(0,"could not re-save");
        else
        {
          long n = 0;
          int same = filesIdentical(conv,again,&n);
          printf("         %s and %s: %ld bytes compared\n",conv,again,n);
          ok(same,"re-saving reproduces the converted file byte-for-byte");
        }
      }
    }
    remove(conv);  remove(again);
  }

  //--------------------------------------------------------------------------
  // CHECK 5 -- damaged input is refused.
  //
  // Both of these would have been accepted by a reader that trusted its input,
  // and both produce a file that looks loadable.  The first drops two bytes from
  // the end, which only the exact-EOF rule catches.  The second leaves the length
  // alone and corrupts one byte of the condition mask, which only the mask
  // alphabet catches -- and which, left unchecked, would hand the simulator a
  // mask the classifier list cannot satisfy.
  //--------------------------------------------------------------------------
  printf("\nCHECK 5: malformed files are refused\n");
  {
    const char *shortPath = "legacy16-short.sim";
    const char *maskPath  = "legacy16-badmask.sim";
    borgLegacySim bad;

    errorsQuiet = 1;     // the errors below are the point of the test

    if (copyAltered(simPath,shortPath,2,-1,0))
    {
      int before = errorsSeen;
      ok(!borgLegacyReadSim(shortPath,&bad), "a .SIM truncated by 2 bytes is refused");
      ok(errorsSeen > before, "and the refusal was reported through error()");
      borgLegacyFreeSim(&bad);
    }
    else ok(0,"could not build the truncated fixture copy");

    // Offset 0x72 is the first byte of condMask; 'Q' is not in the 01#XB
    // alphabet CLSSLIST.CPP documents.
    if (copyAltered(simPath,maskPath,0,0x72,'Q'))
    {
      ok(!borgLegacyReadSim(maskPath,&bad), "a .SIM with a corrupt mask is refused");
      borgLegacyFreeSim(&bad);
    }
    else ok(0,"could not build the corrupt-mask fixture copy");

    // A .P is not a .SIM, and has no version string to say so.
    ok(!borgLegacyReadSim(posPath,&bad), "a .P offered as a .SIM is refused");
    borgLegacyFreeSim(&bad);

    // And the reverse: a .SIM read as a .P must not be accepted either.
    borgLegacyPos badPos;
    ok(!borgLegacyReadPos(simPath,&badPos), "a .SIM offered as a .P is refused");
    borgLegacyFreePos(&badPos);

    errorsQuiet = 0;
    remove(shortPath);  remove(maskPath);
  }

  //--------------------------------------------------------------------------
  // CHECK 6 -- .RUL counting.
  //
  // .RUL files need no widening at all: classifierList::saveRules/loadRules are
  // plain ASCII.  What they need is the arithmetic loadRules() does not do.
  // BBA.RUL carries one "; Agent 1" header and 33 rule lines, while the
  // simulation it fed had a 32-rule classifier list -- so the 33rd line was
  // never read, and nothing anywhere said so.
  //--------------------------------------------------------------------------
  printf("\nCHECK 6: BBA.RUL structure\n");
  {
    int agents = 0, rules = 0, msgLenSeen = 0;
    ok(borgLegacyCheckRules(rulPath,&agents,&rules,&msgLenSeen),
       "BBA.RUL could be read");
    okEqI(agents,     1, "\"; Agent N\" headers (one per agent, skipped on load)");
    okEqI(rules,      33,"well-formed rule lines");
    okEqI(msgLenSeen, 9, "all rules are the same width, and it is the message length");
    ok(rules > sim.classListLen,
       "BBA.RUL holds more rules than the 32-slot list loadRules() fills");
  }

  borgLegacyFreePos(&pos);
  borgLegacyFreeSim(&sim);

  printf("\n==============================\n");
  if (failures == 0) printf("All checks passed.\n\n");
  else               printf("%d CHECK(S) FAILED.\n\n",failures);
  return failures == 0 ? 0 : 1;
}
