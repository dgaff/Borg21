//============================================================================
// borg -- headless front end for the 1995 Borg 2.1 simulator.
//
// This is Layer 3 replaced rather than ported.  It links the unmodified Layer 1
// and Layer 2 code in borgcore and drives it the way UI.CPP's message loop did,
// minus the message loop.  Three things it can do:
//
//   borg batch <file.b>    run a batch script, writing <file>.O and N.sim
//   borg run               run one simulation and report
//   borg info <file.sim>   load a snapshot and report what is in it
//   borg legacy ...        read, convert and replay the 1995 16-bit files
//
// The batch subcommand is the one that matters for new work: it is how every
// result in the thesis was produced, and the .O table it writes is what the
// MATLAB and Excel post-processing in Research/ reads.  The legacy subcommands
// matter for the old work: they are the only way to open the .SIM and .P files
// BORG.EXE left behind, which no build of this source can read directly.
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "task.h"
#include "cfs.h"
#include "ei.h"
#include "host.h"
#include "simfile.h"
#include "batch.h"
#include "legacy.h"

//----------------------------------------------------------------------------
// The host side of the one seam.  In 1995 this was userInterface::displayError
// (UI.CPP:2063), which put up a BWCC message box and, for most codes, called
// PostQuitMessage -- the errors it reports are nearly all fatal.  The numbering
// is documented in host/host.h.
//----------------------------------------------------------------------------
static int errorCount = 0;

int hostDisplayError(int num, const char *str)
{
  ++errorCount;
  fflush(stdout);          // keep the diagnostic in sequence with the progress

  if (!str) str = "";

  // The 1995 table, reproduced so that a core error still reads as English.
  // Codes from 100 up are this port's own and carry a complete sentence, which
  // is also how displayError's default case treated an unknown number.
  switch (num)
  {
    case 0:  fprintf(stderr,"borg: memory allocation failed in %s\n",str);    break;
    case 1:  fprintf(stderr,"borg: array index out of range in %s\n",str);    break;
    case 2:  fprintf(stderr,"borg: infinite loop detected in %s\n",str);      break;
    case 3:
    case 12: fprintf(stderr,"borg: '%s' is not a Borg 2.1 file\n",str);       break;
    case 10: fprintf(stderr,"borg: window creation failed in %s\n",str);      break;
    case 11: fprintf(stderr,"borg: process creation failed in %s\n",str);     break;
    case 13: fprintf(stderr,"borg: cannot do that during a batch run\n");     break;
    default: fprintf(stderr,"borg: %s\n",str);                               break;
  }
  return 0;
}

//----------------------------------------------------------------------------
static void usage(void)
{
  fputs(
    "borg -- Borg 2.1 learning classifier system, headless\n"
    "\n"
    "Usage:\n"
    "  borg batch <file.b> [-C dir] [-q]\n"
    "  borg run [-s seed] [-a agents] [-m maxticks] [-e standard|concave]\n"
    "           [-r rules.RUL] [-o out.sim] [-p out.P] [-q]\n"
    "  borg info <file.sim>\n"
    "  borg pos <file.P>\n"
    "\n"
    "  -C dir   change to dir first, so N.sim and the .O land there\n"
    "  -r file  load a rule list (.RUL) after reset, before running\n"
    "  -p file  write the agent position history as a binary .P\n"
    "  -q       quiet: no per-simulation progress on stdout\n"
    "\n"
    "batch writes <file>.O and one <N>.sim per simulation, numbered from the\n"
    "first data line of the script.  Column meanings for a .b spec line are in\n"
    "the two ';' header lines of SAMPLE.B.\n"
    "\n"
    "The 1995 16-bit files -- these cannot be read by plain borg info, because\n"
    "int was 2 bytes in 1995 and is 4 bytes now:\n"
    "\n"
    "  borg legacy info    <file.SIM|file.P> [--rules]\n"
    "  borg legacy convert <file.SIM|file.P> <out>\n"
    "  borg legacy csv     <file.SIM|file.P> [-o out.csv]\n"
    "  borg legacy check   <file.SIM> <file.P>\n"
    "  borg legacy replay  <file.SIM> [-r rules.RUL]\n"
    "  borg legacy rules   <file.RUL>\n"
    "\n"
    "convert writes a snapshot this build can load; check compares the two\n"
    "independently-written copies of a trajectory; replay re-runs the 1995\n"
    "configuration and says whether this build reproduces it.\n",
    stderr);
}

//----------------------------------------------------------------------------
// The per-agent table both `run` and `info` print.  Same columns as the .O file
// so the two are directly comparable.
//----------------------------------------------------------------------------
static void reportAgents(int simNum)
{
  char row[BORG_STATS_ROW_MAX];
  int numCrossovers, numSexual, numMutations, numCDOs, numCEOs, numCrashes;
  int geneticInterval, maxCount, netOn;
  unsigned seed;

  classifierSystem::getSettings(&seed,&geneticInterval,&maxCount,&netOn);

  fputs(borgStatsHeader(),stdout);
  for (int i=0; i<task.getNumAgents(); i++)
  {
    task.readStats(i,&numCrossovers,&numSexual,&numMutations,&numCDOs,
                   &numCEOs,&numCrashes);
    borgFormatStatsRow(row,simNum,task.getAgentID(i),seed,task.readClock(i),
                       numSexual,numCrossovers-numSexual,numMutations,
                       numCDOs,numCEOs,numCrashes);
    fputs(row,stdout);
  }
}

//============================================================================
static int cmdBatch(int argc, char **argv)
{
  const char *batchFile = 0;
  const char *dir = 0;
  int quiet = 0;

  for (int i=0; i<argc; i++)
  {
    if (strcmp(argv[i],"-q") == 0) quiet = 1;
    else if (strcmp(argv[i],"-C") == 0 && i+1 < argc) dir = argv[++i];
    else if (argv[i][0] == '-') { usage(); return 2; }
    else if (!batchFile) batchFile = argv[i];
    else { usage(); return 2; }
  }

  if (!batchFile) { usage(); return 2; }

  // Resolve the script before chdir, so -C and a relative script path compose
  // the way a shell user expects.
  char resolved[1024];
  if (dir && batchFile[0] != '/')
  {
    char cwd[512];
    if (!getcwd(cwd,sizeof(cwd))) { perror("borg: getcwd"); return 1; }
    snprintf(resolved,sizeof(resolved),"%s/%s",cwd,batchFile);
    batchFile = resolved;
  }

  if (dir && chdir(dir) != 0)
  { fprintf(stderr,"borg: cannot chdir to '%s': ",dir); perror(""); return 1; }

  batchRunner batch;
  if (!batch.open(batchFile)) return 1;

  if (!quiet)
    printf("batch  %s\noutput %s\n\n",batchFile,batch.outputPath());

  //--------------------------------------------------------------------------
  // The 1995 state machine, with the Windows idle loop removed.  See
  // host/batch.h: startNextSim, then tick until the simulation says it is done,
  // then finishSim.
  //--------------------------------------------------------------------------
  int status;
  while ((status = batch.startNextSim()) == 1)
  {
    int simNum = batch.currentSimNum();
    int ticks  = 0;

    // Count the tick that reports "done" as well, so this matches the
    // iterations column the .O file records from task.readClock().
    do { ++ticks; } while (!batch.tick());

    if (!batch.finishSim()) return 1;

    if (!quiet)
    {
      printf("  sim %-4d  %5d ticks  -> %s\n",simNum,ticks,batch.simPath());
      fflush(stdout);
    }
  }

  batch.close();

  if (status < 0) return 1;

  if (!quiet)
    printf("\n%d simulation(s) completed.  Results appended to %s\n",
           batch.simsCompleted(),batch.outputPath());

  return errorCount ? 1 : 0;
}

//============================================================================
static int cmdRun(int argc, char **argv)
{
  const char *outFile = 0;
  const char *posFile = 0;
  const char *ruleFile = 0;
  int quiet = 0;
  int haveSeed = 0, haveAgents = 0, haveMax = 0, haveEnv = 0;
  unsigned newSeed = 0;
  int newAgents = 0, newMax = 0, newEnv = 0;

  for (int i=0; i<argc; i++)
  {
    if      (strcmp(argv[i],"-q") == 0) quiet = 1;
    else if (strcmp(argv[i],"-s") == 0 && i+1 < argc)
    { newSeed = (unsigned)strtoul(argv[++i],0,10); haveSeed = 1; }
    else if (strcmp(argv[i],"-a") == 0 && i+1 < argc)
    { newAgents = atoi(argv[++i]); haveAgents = 1; }
    else if (strcmp(argv[i],"-m") == 0 && i+1 < argc)
    { newMax = atoi(argv[++i]); haveMax = 1; }
    else if (strcmp(argv[i],"-o") == 0 && i+1 < argc) outFile  = argv[++i];
    else if (strcmp(argv[i],"-p") == 0 && i+1 < argc) posFile  = argv[++i];
    else if (strcmp(argv[i],"-r") == 0 && i+1 < argc) ruleFile = argv[++i];
    else if (strcmp(argv[i],"-e") == 0 && i+1 < argc)
    {
      ++i;
      if      (strcmp(argv[i],"standard") == 0) newEnv = task.STANDARD;
      else if (strcmp(argv[i],"concave")  == 0) newEnv = task.CONCAVE_OBST;
      else { fprintf(stderr,"borg: -e takes 'standard' or 'concave'\n"); return 2; }
      haveEnv = 1;
    }
    else { usage(); return 2; }
  }

  unsigned seed;
  int geneticInterval, maxCount, netOn;
  classifierSystem::getSettings(&seed,&geneticInterval,&maxCount,&netOn);
  if (haveSeed) seed = newSeed;
  if (haveMax)  maxCount = newMax;
  classifierSystem::setSettings(seed,geneticInterval,maxCount,netOn);

  if (haveAgents) task.setNumAgents(newAgents);
  if (haveEnv)    task.setEnvType(newEnv);

  // Plain reset(), never reset(1): the constructor already did the first one.
  task.reset();

  // Rules go in after reset and before the first tick -- the order the 1995 GUI
  // enforced, because reset() fills the list with random rules that loading then
  // replaces.  Note that loadRules() discards one line per agent as a
  // "; Agent N" header; see `borg legacy rules`.
  if (ruleFile)
  {
    FILE *rf = fopen(ruleFile,"r");
    if (!rf) { fprintf(stderr,"borg: cannot open '%s'\n",ruleFile); return 1; }
    task.loadRules(rf);
    fclose(rf);
  }

  int ticks = 0;
  do { ++ticks; } while (!task.clockTick());

  if (!quiet)
  {
    printf("seed %u   agents %d   maxCount %d   network %s\n\n",
           seed,task.getNumAgents(),maxCount,netOn ? "on" : "off");
    reportAgents(0);
    printf("\n%d ticks\n",ticks);
    for (int i=0; i<task.getNumAgents(); i++)
    {
      const coord & c = task.getCurrentLoc((unsigned long)(i+1));
      printf("  agent %d final position (%.2f, %.2f) heading %.3f rad\n",
             i+1,c.getX(),c.getY(),task.getCurrentDir((unsigned long)(i+1)));
    }
  }

  if (outFile && !borgSaveSim(outFile)) return 1;
  if (posFile && !borgSavePositions(posFile)) return 1;

  return errorCount ? 1 : 0;
}

//============================================================================
static int cmdInfo(int argc, char **argv)
{
  if (argc != 1) { usage(); return 2; }

  if (!borgLoadSim(argv[0]))
  {
    // borgLoadSim stays quiet when the file is simply absent, matching 1995.
    if (!errorCount)
      fprintf(stderr,"borg: cannot open '%s'\n",argv[0]);
    return 1;
  }

  unsigned seed;
  int geneticInterval, maxCount, netOn;
  classifierSystem::getSettings(&seed,&geneticInterval,&maxCount,&netOn);

  printf("%s\n", argv[0]);
  printf("  version          2.1\n");
  printf("  agents           %d\n", task.getNumAgents());
  printf("  seed             %u\n", seed);
  printf("  maxCount         %d\n", maxCount);
  printf("  geneticInterval  %d\n", geneticInterval);
  printf("  network          %s\n", netOn ? "on" : "off");
  printf("  global clock     %d\n\n", task.readGlobalClock());
  reportAgents(0);

  return errorCount ? 1 : 0;
}

//============================================================================
// The `borg legacy` subcommands -- everything that touches a 1995 16-bit file.
//
// These are the Phase 4 front end for host/legacy.h.  Nothing below knows the
// file layout; it all goes through borgLegacyReadSim()/ReadPos().
//============================================================================

//----------------------------------------------------------------------------
// Tell a .SIM from a .P without trusting the filename.  A .SIM opens with the
// 4-byte version stamp userInterface::save() writes; a .P has no header at all,
// so "starts with 2.1\0" is the whole discriminator -- and it is the same test
// borgLoadSim() applies.
//----------------------------------------------------------------------------
static int looksLikeSim(const char *path)
{
  char head[4];
  FILE *f = fopen(path,"rb");
  if (!f) return -1;
  size_t n = fread(head,1,4,f);
  fclose(f);
  if (n != 4) return 0;
  return (head[0]=='2' && head[1]=='.' && head[2]=='1' && head[3]=='\0');
}

//----------------------------------------------------------------------------
static void printSimInfo(const char *path, const borgLegacySim *s, int showRules)
{
  printf("%s\n",path);
  printf("  1995 16-bit .SIM, version \"%s\", %s layout\n",
         s->version, s->network ? "DLCS (NETWORK)" : "plain LCS");
  printf("  %ld bytes in, %ld bytes after widening to native types\n\n",
         s->fileSize, s->modernSize);

  printf("  classifier system\n");
  printf("    seed                %u\n",   s->seed);
  printf("    geneticInterval     %d\n",   s->geneticInterval);
  printf("    maxCount            %d\n",   s->maxCount);
  printf("    message length      %d\n",   s->msgLen);
  printf("    message board       %d\n",   s->msgBoardLen);
  printf("    network             %s\n",   s->networkEnabled ? "on" : "off");

  printf("\n  classifier list\n");
  printf("    length              %d\n",   s->classListLen);
  printf("    BBA                 %s\n",   s->BBAenabled ? "on" : "off");
  printf("    elitism             %s\n",   s->elitismEnabled ? "on" : "off");
  printf("    CDO / CEO           %s / %s\n",
         s->CDOenabled ? "on" : "off", s->CEOenabled ? "on" : "off");
  printf("    maxActionsToPost    %d\n",   s->maxActionsToPost);
  printf("    bid constant        %g\n",   s->bidConstant);
  printf("    sexual / mutation   %g / %g\n", s->sexualProb, s->mutationProb);
  printf("    start strength      %g\n",   s->startStrength);
  printf("    head tax            %g\n",   s->headTax);
  printf("    strength / bid cap  %g / %g\n", s->strengthCap, s->bidCap);
  printf("    condition mask      %s\n",   s->condMask);
  printf("    action mask         %s\n",   s->actionMask);

  printf("\n  animat and environment\n");
  printf("    goal / obst range   %g / %g\n", s->goalRange, s->obstRange);
  printf("    sensor half-angle   %g rad\n", s->sensorRange);
  printf("    time constant       %g\n",   s->timeConst);
  printf("    animat width        %g\n",   s->antWidth);
  printf("    goal / dir reward   %g / %g\n", s->goalReward, s->dirReward);
  printf("    obst reward         %g\n",   s->obstReward);
  printf("    crash penalty       %g\n",   s->crashPenalty);

  if (s->network)
  {
    printf("\n  network interface\n");
    printf("    action passing      %s (interval %d, Tx %d, Rx %d)\n",
           s->actionPassing ? "on" : "off",
           s->actionTxInterval, s->maxActTx, s->maxActRx);
    printf("    classifier passing  %s (interval %d, Tx %d, Rx %d)\n",
           s->classPassing ? "on" : "off",
           s->classTxInterval, s->maxClassTx, s->maxClassRx);
    printf("    strength Tx / Rx    %g / %g\n",
           s->TxStrenThresh, s->RxStrenThresh);
    printf("    bid Tx / Rx         %g / %g\n",
           s->TxBidThresh, s->RxBidThresh);
  }

  printf("\n  task environment\n");
  printf("    type                %s\n",
         s->envType == 1 ? "CONCAVE_OBST" : "STANDARD");
  printf("    playing field       x %d..%d, y %d..%d\n",
         s->x1, s->x2, s->y1, s->y2);
  printf("    goal                (%.2f, %.2f)\n", s->goalX, s->goalY);
  printf("    obstacles           %d\n", s->numObst);
  printf("    agents              %d\n", s->numAgents);
  printf("    global clock        %d\n", s->globalClock);

  printf("\n");
  fputs(borgStatsHeader(),stdout);
  for (int i = 0; i < s->numAgents; i++)
  {
    const borgLegacyAgent *a = &s->agent[i];
    char row[BORG_STATS_ROW_MAX];
    borgFormatStatsRow(row,0,a->agentID ? a->agentID : (unsigned long)(i+1),
                       s->seed,a->clockCount,
                       a->numSexual,a->numCrossovers - a->numSexual,
                       a->numMutations,a->numCDOs,a->numCEOs,a->numCrashes);
    fputs(row,stdout);
  }

  for (int i = 0; i < s->numAgents; i++)
  {
    const borgLegacyAgent *a = &s->agent[i];
    printf("\n  agent %lu: %d positions, %ld random draws consumed, %s\n",
           a->agentID ? a->agentID : (unsigned long)(i+1),
           a->ptCount, a->numRandCalls,
           a->finished ? "finished" : "still running");
    printf("    start (%.2f, %.2f) heading %.3f rad\n",
           a->x[0], a->y[0], a->dir[0]);
    printf("    end   (%.2f, %.2f) heading %.3f rad\n",
           a->x[a->ptCount-1], a->y[a->ptCount-1], a->dir[a->ptCount-1]);
  }

  if (!showRules) return;

  //--------------------------------------------------------------------------
  // The rule list.  Worth saying out loud whether these are the rules the run
  // STARTED with or the ones it produced -- see borgLegacyRulesUnchanged().
  //--------------------------------------------------------------------------
  int pristine = borgLegacyRulesUnchanged(s);
  for (int i = 0; i < s->numAgents; i++)
  {
    const borgLegacyAgent *a = &s->agent[i];
    printf("\n  agent %lu classifier list (%d rules, strengths as saved)\n",
           a->agentID ? a->agentID : (unsigned long)(i+1), s->classListLen);
    printf("  %s\n", pristine
           ? "no rule discovery ran, so these are also the rules it started with"
           : "rule discovery ran, so these are an outcome, not an input");
    printf("      #  condition   action      strength\n");
    for (int j = 0; j < s->classListLen; j++)
      printf("    %3d  %-10s  %-10s  %9.4f\n",
             j, a->cond[j], a->action[j], a->strength[j]);
  }
}

//----------------------------------------------------------------------------
static void printPosInfo(const char *path, const borgLegacyPos *p)
{
  printf("%s\n",path);
  printf("  1995 16-bit .P position dump\n");
  printf("  %ld bytes in, %ld bytes after widening to native types\n",
         p->fileSize, p->modernSize);
  printf("  agents              %d\n",p->numAgents);
  printf("  position cap        %d  (the run's maxCount was %d)\n",
         p->maxPositions, p->maxPositions - 1);
  for (int i = 0; i < p->numAgents; i++)
  {
    const borgLegacyPos::Agent *a = &p->agent[i];
    printf("\n  agent %d: %d positions\n",i+1,a->ptCount);
    printf("    start (%.2f, %.2f) heading %.3f rad\n",
           a->x[0], a->y[0], a->dir[0]);
    printf("    end   (%.2f, %.2f) heading %.3f rad\n",
           a->x[a->ptCount-1], a->y[a->ptCount-1], a->dir[a->ptCount-1]);
  }
}

//============================================================================
static int legacyInfo(int argc, char **argv)
{
  int showRules = 0;
  const char *path = 0;

  for (int i = 0; i < argc; i++)
  {
    if (strcmp(argv[i],"--rules") == 0) showRules = 1;
    else if (argv[i][0] == '-') { usage(); return 2; }
    else if (!path) path = argv[i];
    else { usage(); return 2; }
  }
  if (!path) { usage(); return 2; }

  int kind = looksLikeSim(path);
  if (kind < 0) { fprintf(stderr,"borg: cannot open '%s'\n",path); return 1; }

  if (kind)
  {
    borgLegacySim sim;
    if (!borgLegacyReadSim(path,&sim)) return 1;
    printSimInfo(path,&sim,showRules);
    borgLegacyFreeSim(&sim);
  }
  else
  {
    borgLegacyPos pos;
    if (!borgLegacyReadPos(path,&pos)) return 1;
    printPosInfo(path,&pos);
    borgLegacyFreePos(&pos);
  }
  return errorCount ? 1 : 0;
}

//============================================================================
static int legacyConvert(int argc, char **argv)
{
  if (argc != 2) { usage(); return 2; }
  const char *in = argv[0], *out = argv[1];

  int kind = looksLikeSim(in);
  if (kind < 0) { fprintf(stderr,"borg: cannot open '%s'\n",in); return 1; }

  if (kind)
  {
    borgLegacySim sim;
    if (!borgLegacyReadSim(in,&sim)) return 1;
    int good = borgLegacyWriteModernSim(&sim,out);
    if (good)
      printf("%s -> %s\n  .SIM, %s layout, %ld bytes -> %ld bytes\n",
             in,out, sim.network ? "DLCS" : "plain LCS",
             sim.fileSize, sim.modernSize);
    borgLegacyFreeSim(&sim);
    if (!good) return 1;
  }
  else
  {
    borgLegacyPos pos;
    if (!borgLegacyReadPos(in,&pos)) return 1;
    int good = borgLegacyWriteModernPos(&pos,out);
    if (good)
      printf("%s -> %s\n  .P, %ld bytes -> %ld bytes\n",
             in,out,pos.fileSize,pos.modernSize);
    borgLegacyFreePos(&pos);
    if (!good) return 1;
  }

  return errorCount ? 1 : 0;
}

//============================================================================
// Dump a trajectory as CSV.  This is what the 1995 .P files were FOR: the
// MATLAB scripts in Research/MATLAB plotted agent paths, and taskEnvironment::
// saveAgentPositions() wrote the same thing as agentN.pos text files.  Columns
// are named so a spreadsheet or a plotting script can read the header.
//============================================================================
static int legacyCsv(int argc, char **argv)
{
  const char *path = 0, *out = 0;

  for (int i = 0; i < argc; i++)
  {
    if (strcmp(argv[i],"-o") == 0 && i+1 < argc) out = argv[++i];
    else if (argv[i][0] == '-') { usage(); return 2; }
    else if (!path) path = argv[i];
    else { usage(); return 2; }
  }
  if (!path) { usage(); return 2; }

  int kind = looksLikeSim(path);
  if (kind < 0) { fprintf(stderr,"borg: cannot open '%s'\n",path); return 1; }

  FILE *dst = stdout;
  if (out)
  {
    dst = fopen(out,"w");
    if (!dst) { fprintf(stderr,"borg: cannot write '%s'\n",out); return 1; }
  }

  fputs("agent,step,x,y,heading\n",dst);

  int rows = 0;
  if (kind)
  {
    borgLegacySim sim;
    if (!borgLegacyReadSim(path,&sim)) { if (out) fclose(dst); return 1; }
    for (int i = 0; i < sim.numAgents; i++)
    {
      const borgLegacyAgent *a = &sim.agent[i];
      unsigned long id = a->agentID ? a->agentID : (unsigned long)(i+1);
      for (int j = 0; j < a->ptCount; j++, rows++)
        fprintf(dst,"%lu,%d,%.6f,%.6f,%.6f\n",id,j,a->x[j],a->y[j],a->dir[j]);
    }
    borgLegacyFreeSim(&sim);
  }
  else
  {
    borgLegacyPos pos;
    if (!borgLegacyReadPos(path,&pos)) { if (out) fclose(dst); return 1; }
    for (int i = 0; i < pos.numAgents; i++)
    {
      const borgLegacyPos::Agent *a = &pos.agent[i];
      for (int j = 0; j < a->ptCount; j++, rows++)
        fprintf(dst,"%d,%d,%.6f,%.6f,%.6f\n",i+1,j,a->x[j],a->y[j],a->dir[j]);
    }
    borgLegacyFreePos(&pos);
  }

  if (out) { fclose(dst); printf("%s -> %s  (%d rows)\n",path,out,rows); }
  return errorCount ? 1 : 0;
}

//============================================================================
// Cross-check a .SIM against its matching .P.
//
// taskEnvironment::save() and saveAgentPosBinary() wrote the agent position
// history independently, through different functions, into different files.  For
// the surviving batches they were written moments apart from the same live
// objects, so they must agree exactly -- and because these are the same float32
// bit patterns on both sides, "exactly" means bit-for-bit, not within a
// tolerance.  A disagreement means one of the two files is damaged, or they are
// not from the same run.
//============================================================================
static int legacyCheck(int argc, char **argv)
{
  if (argc != 2) { usage(); return 2; }

  borgLegacySim sim;
  borgLegacyPos pos;

  if (!borgLegacyReadSim(argv[0],&sim)) return 1;
  if (!borgLegacyReadPos(argv[1],&pos)) { borgLegacyFreeSim(&sim); return 1; }

  printf("%s  vs  %s\n\n",argv[0],argv[1]);

  int bad = 0;

  if (sim.numAgents != pos.numAgents)
  {
    printf("  MISMATCH  agent count: .SIM has %d, .P has %d\n",
           sim.numAgents,pos.numAgents);
    bad = 1;
  }
  if (!bad && pos.maxPositions != sim.maxCount + 1)
  {
    printf("  MISMATCH  .P allows %d positions, .SIM's maxCount implies %d\n",
           pos.maxPositions, sim.maxCount + 1);
    bad = 1;
  }

  int total = 0;
  for (int i = 0; !bad && i < sim.numAgents; i++)
  {
    const borgLegacyAgent     *a = &sim.agent[i];
    const borgLegacyPos::Agent *b = &pos.agent[i];

    if (a->ptCount != b->ptCount)
    {
      printf("  MISMATCH  agent %d position count: .SIM %d, .P %d\n",
             i+1,a->ptCount,b->ptCount);
      bad = 1;
      break;
    }

    int differing = 0, firstAt = -1;
    for (int j = 0; j < a->ptCount; j++, total++)
      if (a->x[j] != b->x[j] || a->y[j] != b->y[j] || a->dir[j] != b->dir[j])
      { ++differing; if (firstAt < 0) firstAt = j; }

    if (differing)
    {
      printf("  MISMATCH  agent %d: %d of %d positions differ, first at step %d\n",
             i+1,differing,a->ptCount,firstAt);
      printf("              .SIM (%.6f, %.6f) heading %.6f\n",
             a->x[firstAt],a->y[firstAt],a->dir[firstAt]);
      printf("              .P   (%.6f, %.6f) heading %.6f\n",
             b->x[firstAt],b->y[firstAt],b->dir[firstAt]);
      bad = 1;
    }
    else
      printf("  agent %d: %d positions and headings, every float identical\n",
             i+1,a->ptCount);
  }

  if (!bad)
    printf("\n  AGREE -- %d positions checked, bit-for-bit\n",total);

  borgLegacyFreeSim(&sim);
  borgLegacyFreePos(&pos);
  return bad ? 1 : (errorCount ? 1 : 0);
}

//============================================================================
// Re-run a 1995 simulation and compare it to what 1995 recorded.
//
// This is the sharpest question Phase 4 can ask: does the ported simulator, on
// arm64 with clang, reproduce what Borland produced on an 80386 in April 1995?
//
// The setup has to be exactly right for the question to be meaningful:
//
//   1. Convert and load the snapshot.  That installs every static parameter the
//      1995 operator had dialled in -- seed, caps, masks, rewards, the works --
//      along with the environment type and agent count.
//   2. reset().  This is where the seeded draw sequence begins: reset() calls
//      srand(seed) and then generates a random rule for every slot, and the
//      masks decide how many draws that takes.  Get the masks wrong and every
//      subsequent draw is off by some amount, so step 1 is not optional.
//   3. Load the .RUL, which overwrites those random rules with the ones the run
//      actually used.  Loading rules consumes no draws, so the RNG is still
//      where 1995 left it at this point.
//   4. Tick to completion and compare.
//
// Without -r the rule list stays random and the trajectory will not match; that
// is reported rather than silently producing a divergence report.  The numRandCalls
// the snapshot recorded is the check on whether steps 2 and 3 landed correctly:
// it counts every draw the 1995 run made, so if it agrees, the draw sequence was
// reproduced, and any remaining difference in the trajectory is arithmetic.
//============================================================================
static int legacyReplay(int argc, char **argv)
{
  const char *path = 0, *rules = 0;
  const char *tmpSim = "borg-replay-tmp.sim";

  for (int i = 0; i < argc; i++)
  {
    if (strcmp(argv[i],"-r") == 0 && i+1 < argc) rules = argv[++i];
    else if (argv[i][0] == '-') { usage(); return 2; }
    else if (!path) path = argv[i];
    else { usage(); return 2; }
  }
  if (!path) { usage(); return 2; }

  borgLegacySim sim;
  if (!borgLegacyReadSim(path,&sim)) return 1;

  printf("%s\n",path);
  printf("  1995: %d agent(s), %d iterations, seed %u, %s field\n\n",
         sim.numAgents, sim.globalClock, sim.seed,
         sim.envType == 1 ? "concave" : "standard");

  //-- step 1: install the 1995 configuration -------------------------------
  int ready = borgLegacyWriteModernSim(&sim,tmpSim) && borgLoadSim(tmpSim);
  remove(tmpSim);
  if (!ready) { borgLegacyFreeSim(&sim); return 1; }

  //-- step 2: restart from it ----------------------------------------------
  task.reset();

  // How many draws reset() itself consumed, before a single tick.  Every slot in
  // the classifier list gets a random condition and action, and the masks decide
  // how many alleles are actually drawn for each -- with COND_MASK_BBA and
  // ACTION_MASK_BBA over a 32-rule list that is 32*(9 + 7) = 512 draws.  This
  // number is what makes it possible to tell a replayable snapshot from a
  // mid-batch one: see the verdict on random draws below.
  long drawsAtReset = numRandCalls;

  //-- step 3: the rules the run actually used ------------------------------
  if (rules)
  {
    FILE *rf = fopen(rules,"r");
    if (!rf)
    {
      fprintf(stderr,"borg: cannot open '%s'\n",rules);
      borgLegacyFreeSim(&sim);
      return 1;
    }
    task.loadRules(rf);
    fclose(rf);
    printf("  rules loaded from %s\n",rules);

    // Did that .RUL actually belong to this snapshot?  When no rule-discovery
    // operator ran, the snapshot's rules are still the loaded ones, so the two
    // can be compared directly -- which catches the common mistake of replaying
    // against the wrong rule file.
    if (borgLegacyRulesUnchanged(&sim))
    {
      int differing = 0;
      for (int i = 0; i < sim.numAgents; i++)
      {
        message cond, act;
        double stren, spec, sup, ruleBid;
        int match, sel, supplier, elite, ruleNum;
        unsigned long supID;
        char cbuf[BORG_LEGACY_MAX_MSG+1], abuf[BORG_LEGACY_MAX_MSG+1];

        for (int j = 0; j < sim.classListLen; j++)
        {
          task.getClassifier(i,j,cond,act,&stren,&spec,&sup,&ruleBid,
                             &match,&sel,&supplier,&supID,&elite,&ruleNum);
          cond.msgToStr(cbuf);
          act.msgToStr(abuf);
          if (strcmp(cbuf,sim.agent[i].cond[j]) != 0 ||
              strcmp(abuf,sim.agent[i].action[j]) != 0) ++differing;
        }
      }
      if (differing)
        printf("  WARNING: %d of %d loaded rules differ from the ones in the "
               "snapshot --\n           this .RUL is probably not the one this "
               "run used.\n",differing,sim.classListLen*sim.numAgents);
      else
        printf("  all %d rules match the ones the snapshot ended with\n",
               sim.classListLen*sim.numAgents);
    }
    else
      printf("  (rule discovery ran in this snapshot, so its rules cannot be\n"
             "   compared against the .RUL)\n");
  }
  else
    printf("  NO -r GIVEN: the rule list is whatever reset() generated at\n"
           "  random, so the trajectory below will not match 1995.  Pass the\n"
           "  .RUL this batch was seeded from for a real comparison.\n");

  //-- step 4: run and compare ---------------------------------------------
  int ticks = 0;
  do { ++ticks; } while (!task.clockTick() && ticks < sim.maxCount + 1);

  printf("\n  1995 ran %d iterations; this build ran %d\n",
         sim.globalClock,ticks);

  // Three separate questions, kept separate on purpose.  A trajectory that
  // matches and a draw counter that does not is a completely different finding
  // from a trajectory that does not match, and collapsing them into one verdict
  // would hide which one happened.
  int badTrajectory = (ticks != sim.globalClock);
  int badStats      = 0;
  int badDraws      = 0;
  int drawsExplained = 1;

  for (int i = 0; i < sim.numAgents; i++)
  {
    const borgLegacyAgent *a = &sim.agent[i];
    unsigned long id = (unsigned long)(i+1);

    int cross, sexual, mutations, CDOs, CEOs, crashes;
    task.readStats(i,&cross,&sexual,&mutations,&CDOs,&CEOs,&crashes);

    // The live position history, not just the endpoint.  A matching endpoint
    // after 23 ticks already implies a matching path -- the kinematics have no
    // way back from a divergence -- but comparing every step is what turns that
    // inference into a measurement, and it reports WHERE a divergence began.
    coord *livePts;
    float *liveDir;
    int liveCount;
    float gDist, gDir, oDist, oDir;
    task.getAgent(i,&livePts,&liveDir,&liveCount,&gDist,&gDir,&oDist,&oDir);

    printf("\n  agent %lu trajectory\n",id);
    printf("    1995 saved %d positions, this build saved %d\n",
           a->ptCount,liveCount);

    int differing = 0, firstAt = -1;
    int n = (liveCount < a->ptCount) ? liveCount : a->ptCount;
    for (int j = 0; j < n; j++)
      if (livePts[j].getX() != a->x[j] || livePts[j].getY() != a->y[j] ||
          liveDir[j]        != a->dir[j])
      { ++differing; if (firstAt < 0) firstAt = j; }

    if (liveCount != a->ptCount) badTrajectory = 1;

    if (differing == 0 && liveCount == a->ptCount)
      printf("    all %d positions and headings IDENTICAL, "
             "to the last bit of the float32\n",n);
    else
    {
      badTrajectory = 1;
      printf("    %d of %d positions DIFFER, first at step %d\n",
             differing,n,firstAt);
      if (firstAt >= 0)
      {
        printf("      1995       (%.6f, %.6f) heading %.6f\n",
               a->x[firstAt],a->y[firstAt],a->dir[firstAt]);
        printf("      this build (%.6f, %.6f) heading %.6f\n",
               livePts[firstAt].getX(),livePts[firstAt].getY(),
               liveDir[firstAt]);
        printf("      delta      (%.3e, %.3e)\n",
               (double)livePts[firstAt].getX() - a->x[firstAt],
               (double)livePts[firstAt].getY() - a->y[firstAt]);
      }
    }

    printf("    final: 1995 (%.5f, %.5f), here (%.5f, %.5f)\n",
           a->x[a->ptCount-1],a->y[a->ptCount-1],
           livePts[liveCount-1].getX(),livePts[liveCount-1].getY());

    printf("    stats: 1995 crossovers %d/%d mutations %d CDOs %d CEOs %d "
           "crashes %d\n",
           a->numSexual,a->numCrossovers - a->numSexual,
           a->numMutations,a->numCDOs,a->numCEOs,a->numCrashes);
    printf("           here crossovers %d/%d mutations %d CDOs %d CEOs %d "
           "crashes %d\n",
           sexual,cross - sexual,mutations,CDOs,CEOs,crashes);
    if (sexual != a->numSexual || cross - sexual != a->numCrossovers - a->numSexual ||
        mutations != a->numMutations || CDOs != a->numCDOs ||
        CEOs != a->numCEOs || crashes != a->numCrashes)
      badStats = 1;

    //------------------------------------------------------------------------
    // numRandCalls says how far the RNG had been advanced when the snapshot was
    // written, and it is the one field that can distinguish a simulation that
    // began with a full reset from one that did not.
    //
    // runBatch() does NOT hard-reset between simulations in a batch; it
    // softResets, which keeps the learned rules and does not regenerate them --
    // so it draws nothing, and the RNG simply carries on from where the previous
    // simulation left it.  A snapshot from simulation 7 of a batch therefore
    // records a draw count that no single replay can reproduce, because
    // reproducing it would mean replaying simulations 1 through 6 first.
    //
    // The test is whether the recorded count is smaller than what reset() alone
    // consumes.  If it is, the snapshot cannot have started from a reset, and the
    // mismatch is expected rather than a defect in the port.
    //------------------------------------------------------------------------
    printf("    random draws: 1995 %ld, here %ld (reset alone used %ld)\n",
           a->numRandCalls,numRandCalls,drawsAtReset);
    if (numRandCalls != a->numRandCalls)
    {
      badDraws = 1;
      if (a->numRandCalls < drawsAtReset)
        printf("      1995 drew fewer than a reset costs, so this simulation was\n"
               "      a softReset continuation inside a batch -- its rule list was\n"
               "      inherited, not generated.  Replaying it on its own cannot\n"
               "      reach the same point in the draw sequence.\n");
      else
        drawsExplained = 0;
    }
  }

  //--------------------------------------------------------------------------
  // The verdict.  Exit status follows the trajectory and the statistics, which
  // are what "does this port reproduce 1995" actually means; an explained draw
  // count is reported but is not a failure.
  //--------------------------------------------------------------------------
  printf("\n");
  if (!badTrajectory && !badStats && !badDraws)
    printf("  REPLAY REPRODUCED THE 1995 RUN EXACTLY\n");
  else if (!badTrajectory && !badStats && drawsExplained)
    printf("  REPLAY REPRODUCED THE 1995 TRAJECTORY AND STATISTICS EXACTLY\n"
           "  (the random-draw counter differs for the reason above)\n");
  else
    printf("  REPLAY DIVERGED\n");

  borgLegacyFreeSim(&sim);

  int failed = badTrajectory || badStats || !drawsExplained;
  return failed ? 1 : (errorCount ? 1 : 0);
}

//============================================================================
static int legacyRules(int argc, char **argv)
{
  if (argc != 1) { usage(); return 2; }

  int agents = 0, rules = 0, msgLenSeen = 0;
  if (!borgLegacyCheckRules(argv[0],&agents,&rules,&msgLenSeen)) return 1;

  printf("%s\n",argv[0]);
  printf("  \"; Agent N\" header lines   %d\n",agents);
  printf("  well-formed rule lines     %d\n",rules);
  if (msgLenSeen < 0)
    printf("  rule width                 INCONSISTENT -- the file mixes widths, "
           "and message::operator=\n                             "
           "raises error 1 on every line that does not match\n");
  else
    printf("  rule width                 %d alleles\n",msgLenSeen);

  printf("\n  .RUL files are plain ASCII and need no conversion -- "
         "classifierList::loadRules\n  reads them directly.  Two things to know "
         "before loading one:\n\n");
  printf("    * taskEnvironment::loadRules() discards one line per agent to skip "
         "the\n      \"; Agent N\" header.  saveRules() writes that header only in a "
         "DLCS\n      build, so a plain-LCS .RUL loses its first rule when "
         "reloaded.\n");
  printf("    * loadRules() reads exactly as many lines as the classifier list is "
         "long\n      and checks nothing.  Extra lines are ignored; missing ones "
         "leave slots\n      holding whatever reset() generated at random.\n");

  if (agents == 0)
    printf("\n  This file has NO header line, so loading it will consume its "
           "first rule\n  line as one.\n");

  return errorCount ? 1 : 0;
}

//============================================================================
// Load a native .P back into the live simulator and report it.
//
// This exists to close the loop that `borg legacy convert` opens: converting a
// 1995 .P is only worth doing if something can then read it.  It also makes
// explicit what loadAgentPosBinary() does to the live state -- it derives
// maxCount from the file and resets -- which is easy to miss in the 1995 code
// and is why the 1995 GUI repainted everything afterwards.
//============================================================================
static int cmdPos(int argc, char **argv)
{
  if (argc != 1) { usage(); return 2; }

  if (!borgLoadPositions(argv[0]))
  {
    if (!errorCount) fprintf(stderr,"borg: cannot open '%s'\n",argv[0]);
    return 1;
  }

  unsigned seed;
  int geneticInterval, maxCount, netOn;
  classifierSystem::getSettings(&seed,&geneticInterval,&maxCount,&netOn);

  printf("%s\n",argv[0]);
  printf("  agents           %d\n",task.getNumAgents());
  printf("  maxCount         %d   (derived from the file, and now the live "
         "setting)\n",maxCount);

  for (int i = 0; i < task.getNumAgents(); i++)
  {
    coord *pts;
    float *dirs;
    int count;
    float gDist, gDir, oDist, oDir;
    task.getAgent(i,&pts,&dirs,&count,&gDist,&gDir,&oDist,&oDir);

    printf("\n  agent %d: %d positions\n",i+1,count);
    printf("    start (%.2f, %.2f) heading %.3f rad\n",
           pts[0].getX(),pts[0].getY(),dirs[0]);
    printf("    end   (%.2f, %.2f) heading %.3f rad\n",
           pts[count-1].getX(),pts[count-1].getY(),dirs[count-1]);
  }

  return errorCount ? 1 : 0;
}

//============================================================================
static int cmdLegacy(int argc, char **argv)
{
  if (argc < 1) { usage(); return 2; }

  const char *sub = argv[0];
  if (strcmp(sub,"info")    == 0) return legacyInfo   (argc-1,argv+1);
  if (strcmp(sub,"convert") == 0) return legacyConvert(argc-1,argv+1);
  if (strcmp(sub,"csv")     == 0) return legacyCsv    (argc-1,argv+1);
  if (strcmp(sub,"check")   == 0) return legacyCheck  (argc-1,argv+1);
  if (strcmp(sub,"replay")  == 0) return legacyReplay (argc-1,argv+1);
  if (strcmp(sub,"rules")   == 0) return legacyRules  (argc-1,argv+1);

  fprintf(stderr,"borg: unknown legacy subcommand '%s'\n",sub);
  usage();
  return 2;
}

//============================================================================
int main(int argc, char **argv)
{
  if (argc < 2) { usage(); return 2; }

  const char *cmd = argv[1];

  if (strcmp(cmd,"batch") == 0) return cmdBatch(argc-2, argv+2);
  if (strcmp(cmd,"run")   == 0) return cmdRun  (argc-2, argv+2);
  if (strcmp(cmd,"info")  == 0) return cmdInfo (argc-2, argv+2);
  if (strcmp(cmd,"pos")   == 0) return cmdPos   (argc-2, argv+2);
  if (strcmp(cmd,"legacy")== 0) return cmdLegacy(argc-2, argv+2);

  if (strcmp(cmd,"-h") == 0 || strcmp(cmd,"--help") == 0) { usage(); return 0; }

  fprintf(stderr,"borg: unknown command '%s'\n",cmd);
  usage();
  return 2;
}
