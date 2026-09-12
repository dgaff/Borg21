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
//
// The batch subcommand is the one that matters: it is how every result in the
// thesis was produced, and the .O table it writes is what the MATLAB and Excel
// post-processing in Research/ reads.
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
    "           [-o out.sim] [-q]\n"
    "  borg info <file.sim>\n"
    "\n"
    "  -C dir   change to dir first, so N.sim and the .O land there\n"
    "  -q       quiet: no per-simulation progress on stdout\n"
    "\n"
    "batch writes <file>.O and one <N>.sim per simulation, numbered from the\n"
    "first data line of the script.  Column meanings for a .b spec line are in\n"
    "the two ';' header lines of SAMPLE.B.\n",
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
    else if (strcmp(argv[i],"-o") == 0 && i+1 < argc) outFile = argv[++i];
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
int main(int argc, char **argv)
{
  if (argc < 2) { usage(); return 2; }

  const char *cmd = argv[1];

  if (strcmp(cmd,"batch") == 0) return cmdBatch(argc-2, argv+2);
  if (strcmp(cmd,"run")   == 0) return cmdRun  (argc-2, argv+2);
  if (strcmp(cmd,"info")  == 0) return cmdInfo (argc-2, argv+2);

  if (strcmp(cmd,"-h") == 0 || strcmp(cmd,"--help") == 0) { usage(); return 0; }

  fprintf(stderr,"borg: unknown command '%s'\n",cmd);
  usage();
  return 2;
}
