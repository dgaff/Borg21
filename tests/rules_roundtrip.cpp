//============================================================================
// Guard test for the .RUL round trip -- and specifically for the plain-LCS
// (non-NETWORK) path, where it was broken.
//
// THE BUG THIS EXISTS TO CATCH
//
// taskEnvironment::saveRules() wrote a "; Agent N" header line per agent, but
// only #ifdef NETWORK.  taskEnvironment::loadRules() skipped one line per agent
// unconditionally.  In a plain-LCS build the two therefore disagreed: reloading a
// .RUL that had just been saved consumed the first RULE as the header, shifting
// every rule up one slot and losing the last.  Nothing complained --
// classifierList::loadRules()'s own comment says it does no error checking -- so
// the simulation just ran on a silently wrong rule base.
//
// WHY IT SURVIVED, AND WHY THIS FILE IS COMPILED TWICE
//
// The #else branches in this codebase are invisible to the compiler in a DLCS
// build, so no amount of testing the default build could ever reach the broken
// line.  CMake now builds this test against both variants of borgcore
// (-DBORG_NO_NETWORK for the plain one) and tests/run_rules.sh compares what the
// two produce.  That cross-variant comparison is the real check: before the fix
// the two builds wrote different files, and only one of them could read its own.
//
// Everything below uses only API that exists in both variants, so there is not a
// single #ifdef in this file.  That is deliberate -- a test for a bug that lives
// in an #ifdef branch should not be able to drift between the two.
//
// Run:  ctest --test-dir build --output-on-failure
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "task.h"
#include <host.h>

int hostDisplayError(int num, const char *str)
{
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
  printf("  %s  %-52s %ld\n", good ? "PASS" : "FAIL", what, got);
  if (!good) { printf("        expected %ld\n",want); ++failures; }
}

//----------------------------------------------------------------------------
static int saveRulesTo(const char *path)
{
  FILE *f = fopen(path,"w");
  if (!f) return 0;
  task.saveRules(f);
  fclose(f);
  return 1;
}

static int loadRulesFrom(const char *path)
{
  FILE *f = fopen(path,"r");
  if (!f) return 0;
  task.loadRules(f);
  fclose(f);
  return 1;
}

static long lineCount(const char *path)
{
  FILE *f = fopen(path,"r");
  if (!f) return -1;
  long n = 0;
  int c, last = '\n';
  while ((c = fgetc(f)) != EOF) { if (c == '\n') ++n; last = c; }
  if (last != '\n' && n >= 0) ++n;        // count a final unterminated line
  fclose(f);
  return n;
}

static int filesIdentical(const char *a, const char *b)
{
  FILE *fa = fopen(a,"rb"), *fb = fopen(b,"rb");
  if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return 0; }
  int ca, cb, same = 1;
  do
  {
    ca = fgetc(fa);  cb = fgetc(fb);
    if (ca != cb) { same = 0; break; }
  } while (ca != EOF);
  fclose(fa);  fclose(fb);
  return same;
}

// Read one line, stripping CR and LF, so a comparison does not depend on whether
// the file came from DOS.  Research/BBA/BBA.RUL happens to be LF-terminated while
// most 1995 files are CRLF, and neither should matter here.
static int readTrimmedLine(FILE *f, char *buf, int size)
{
  if (!fgets(buf,size,f)) return 0;
  int n = (int)strlen(buf);
  while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
  return 1;
}

//============================================================================
int main(int argc, char **argv)
{
  const char *rulPath = (argc > 1) ? argv[1] : "tests/fixtures/legacy/BBA.RUL";
  const char *outPath = (argc > 2) ? argv[2] : "rules-roundtrip-out.RUL";

  char pathA[1024], pathB[1024];
  snprintf(pathA,sizeof(pathA),"%s.a",outPath);
  snprintf(pathB,sizeof(pathB),"%s.b",outPath);

  printf("\nBorg .RUL round-trip guard\n");
  printf("==========================\n");
#ifdef NETWORK
  printf("variant: DLCS (NETWORK defined)\n");
#else
  printf("variant: plain LCS (NETWORK not defined)\n");
#endif
  printf("fixture: %s\n\n",rulPath);

  const int agents = 2;
  task.setNumAgents(agents);
  task.reset();

  const int ruleCount = classifierList::size();

  //--------------------------------------------------------------------------
  // CHECK 1 -- saveRules writes a header line per agent in BOTH variants.
  //
  // This is the assertion that fails on the old code in a plain-LCS build: the
  // file came out numAgents lines short because no header was written at all.
  //--------------------------------------------------------------------------
  printf("CHECK 1: saveRules writes one header line per agent\n");
  ok(saveRulesTo(pathA), "rules saved");
  okEqI(lineCount(pathA), agents * (1 + ruleCount),
        "lines written (1 header + rules, per agent)");

  {
    FILE *f = fopen(pathA,"r");
    ok(f != 0, "saved file can be reopened");
    if (f)
    {
      char line[256];
      int headers = 0, rules = 0, badHeader = 0;
      for (int a = 0; a < agents; a++)
      {
        if (!readTrimmedLine(f,line,sizeof(line))) { badHeader = 1; break; }
        char want[64];
        // reset() assigns agent IDs as i+1 (TASK.CPP:166), so this is the text
        // both variants must produce.
        snprintf(want,sizeof(want),"; Agent %d",a+1);
        if (strcmp(line,want) != 0) { badHeader = 1; printf("         got \"%s\", wanted \"%s\"\n",line,want); }
        else ++headers;

        for (int r = 0; r < ruleCount; r++)
          if (readTrimmedLine(f,line,sizeof(line))) ++rules;
      }
      fclose(f);
      okEqI(headers, agents, "\"; Agent N\" header lines, numbered from 1");
      okEqI(rules, agents * ruleCount, "rule lines between the headers");
      ok(!badHeader, "every header line is exactly what loadRules expects to skip");
    }
  }

  //--------------------------------------------------------------------------
  // CHECK 2 -- save, load, save again, and compare.
  //
  // THE REGRESSION.  On the old code in a plain-LCS build, loading the file from
  // CHECK 1 consumed rule 0 as a header, so the second save differed from the
  // first: every rule shifted up a slot and the last slot held whatever had been
  // left there.  Byte-identity is the whole property that was missing.
  //--------------------------------------------------------------------------
  printf("\nCHECK 2: save -> load -> save is byte-identical\n");
  ok(loadRulesFrom(pathA), "rules loaded back");
  ok(saveRulesTo(pathB),   "rules saved again");
  ok(filesIdentical(pathA,pathB),
     "the two saves are byte-for-byte the same file");

  // And again, to catch a shift that happens to be stable after one pass.
  ok(loadRulesFrom(pathB) && saveRulesTo(pathA), "a second round trip ran");
  ok(filesIdentical(pathA,pathB), "a second round trip is also stable");

  //--------------------------------------------------------------------------
  // CHECK 3 -- a genuine 1995 .RUL loads correctly in this variant.
  //
  // Every surviving .RUL in Research/ carries the "; Agent N" header, which is
  // why the fix writes the header in both variants rather than making the loader
  // conditional: a loader that skipped it only under NETWORK would no longer be
  // able to read the real data in a plain build.  This check is that claim.
  //
  // BBA.RUL holds one agent's worth of rules, so the agent count drops to 1.
  // Its strengths are all inside the default strength cap, so checkStrengths()
  // leaves them alone and every value must come back exactly.
  //
  // The comparison is on VALUES, not on text.  BBA.RUL was hand-edited at some
  // point and spells its strengths four different ways -- "10.000000", "10.0",
  // "0.000000" and a bare "1".  sscanf("%lf") reads all four, while saveRules()
  // always writes "%lf", so a text comparison would fail on the file's
  // formatting rather than on anything this code does.
  //--------------------------------------------------------------------------
  printf("\nCHECK 3: the 1995 Research/BBA/BBA.RUL loads in this variant\n");
  task.setNumAgents(1);
  task.reset();

  if (!loadRulesFrom(rulPath))
    ok(0,"BBA.RUL could be opened");
  else
  {
    ok(1,"BBA.RUL loaded");
    ok(saveRulesTo(pathA),"and saved back out");

    FILE *orig = fopen(rulPath,"r");
    FILE *mine = fopen(pathA,"r");
    ok(orig != 0 && mine != 0,"both files open for comparison");

    if (orig && mine)
    {
      char o[256], m[256];

      // Skip each file's header line.  They need not be textually equal -- the
      // 1995 file's header is whatever the operator's build wrote -- but both
      // must BE headers.
      int haveO = readTrimmedLine(orig,o,sizeof(o));
      int haveM = readTrimmedLine(mine,m,sizeof(m));
      ok(haveO && o[0] == ';', "the 1995 file starts with a ';' header");
      ok(haveM && m[0] == ';', "our file starts with a ';' header");

      int compared = 0, differing = 0, firstAt = -1, unparsed = 0;
      for (int r = 0; r < ruleCount; r++)
      {
        if (!readTrimmedLine(orig,o,sizeof(o))) break;
        if (!readTrimmedLine(mine,m,sizeof(m))) break;

        char oc[64], oa[64], mc[64], ma[64];
        double os, ms;
        if (sscanf(o,"%63s %63s %lf",oc,oa,&os) != 3 ||
            sscanf(m,"%63s %63s %lf",mc,ma,&ms) != 3)
        { ++unparsed; continue; }

        ++compared;
        // Exact equality on the strength: loadRules parses the text and
        // saveRules prints the double back, and with values like 10.0 and 0.0
        // that is lossless.  A tolerance here would hide a real shift.
        if (strcmp(oc,mc) != 0 || strcmp(oa,ma) != 0 || os != ms)
        {
          ++differing;
          if (firstAt < 0)
          {
            firstAt = r;
            printf("         rule %d: 1995 \"%s\"\n",r,o);
            printf("                  ours \"%s\"\n",m);
          }
        }
      }
      fclose(orig);  fclose(mine);

      okEqI(unparsed,0,"every line on both sides parsed as a rule");
      okEqI(compared,ruleCount,"rule lines compared against the 1995 file");
      ok(differing == 0,
         "every rule comes back with the condition, action and strength 1995 wrote");
    }
  }

  //--------------------------------------------------------------------------
  // Leave pathA in place when asked to, so run_rules.sh can compare the file
  // this variant produced against the other variant's.
  //--------------------------------------------------------------------------
  remove(pathB);
  if (argc > 2)
    printf("\n  left %s for cross-variant comparison\n",pathA);
  else
    remove(pathA);

  printf("\n==========================\n");
  if (failures == 0) printf("All checks passed.\n\n");
  else               printf("%d CHECK(S) FAILED.\n\n",failures);
  return failures == 0 ? 0 : 1;
}
