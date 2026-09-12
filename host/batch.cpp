//============================================================================
// Batch runner.  See host/batch.h for how this maps onto UI.CPP.
//============================================================================

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "task.h"
#include "cfs.h"
#include "ei.h"
#include "host.h"
#include "simfile.h"
#include "batch.h"

//----------------------------------------------------------------------------
// 1995 value, preserved.  It is tighter than it looks: the longest line in the
// surviving SAMPLE.B is 265 characters and a spec line is 263, so there are
// barely 30 characters of headroom.  Adding one more parameter column to the
// format would overflow it, and fgets() would then split a spec line in two and
// parse the tail as a fresh line.  nextDataLine() detects that instead.
//----------------------------------------------------------------------------
#define MAX_CHARS_PER_LINE  300

// The 42 fields of a .b spec line: 20 system, 9 agent, 13 network.  The column
// meanings are documented in the two ';' header lines of BORG/Source/SAMPLE.B.
#define BATCH_FIELD_COUNT   42

//============================================================================
// The .O table.  Both of these reproduce 1995 byte-for-byte.
//
// The ONLY difference between a .O written here and one written by BORG.EXE is
// the line terminator: DOS text mode turned every '\n' into CRLF, and this does
// not.  Column positions and field widths are identical, which is what the
// format actually specifies.  To diff a new .O against a surviving 1995 one,
// use `diff --strip-trailing-cr`.
//============================================================================

const char *borgStatsHeader(void)
{
  // userInterface::startBatch(), UI.CPP:2264.
  return "                    Number of   Sexual     Asexual    Number of  Number of  Number of  Number of\n"
         "Sim #  Agent  Seed  Iterations  Crossover  Crossover  Mutations  CDO's      CEO's      Crashes\n\n";
}

void borgFormatStatsRow(char *buf, int simNum, unsigned long agentID,
                        unsigned seed, int iterations,
                        int numSexual, int numAsexual, int numMutations,
                        int numCDOs, int numCEOs, int numCrashes)
{
  // userInterface::saveStats(), UI.CPP:2240.  The format string is copied
  // character for character, including the irregular run of spaces before the
  // last column, because the fixed columns ARE the file format -- the MATLAB and
  // Excel post-processing in Research/ reads them by position.
  //
  // 2026 PORT: the original passed task.getAgentID(), which returns
  // `unsigned long`, to a `%4ld` conversion expecting a signed `long`.  Both were
  // 32 bits under Borland so it printed correctly; the cast below makes that
  // explicit rather than leaving a type mismatch that happens to work.  Agent IDs
  // are small positive integers, so the printed text is unchanged.
  //
  // snprintf, not sprintf: a width of 4 is a minimum, not a maximum, so a wide
  // value widens the line rather than truncating, and nothing here bounds
  // iterations or the operator counts.
  snprintf(buf, BORG_STATS_ROW_MAX,
           "%4d   %4ld   %4u     %4d       %4d       %4d       %4d       "
           "%4d       %4d      %4d\n",
           simNum, (long)agentID, seed, iterations, numSexual,
           numAsexual, numMutations, numCDOs, numCEOs, numCrashes);
}

//============================================================================
batchRunner::batchRunner()
  : batchFp(0), simNum(0), completed(0), sawTerminator(0)
{
  batchPath[0] = '\0';
  outputFile[0] = '\0';
  simName[0] = '\0';
}

batchRunner::~batchRunner()
{
  close();
}

void batchRunner::close()
{
  if (batchFp) { fclose(batchFp); batchFp = 0; }
}

//============================================================================
// Read the next line that carries data: not a comment, not blank.
//
// 1995 did this with
//     do { fgets(temp,MAX_CHARS_PER_LINE,fptr); } while(temp[0] == ';');
// which has no EOF test.  When fgets() fails it leaves the buffer untouched, so
// a .b file missing its '*' terminator re-examined the previous line forever.
// That was invisible behind a modal message loop; in a CLI it is a hang.
//
// Returns 1 on a data line, 0 at end of file, -1 if the line was too long to
// fit -- which must be an error rather than a silent split, since half a spec
// line still parses into something.
//============================================================================
int batchRunner::nextDataLine(char *line, int size)
{
  while (fgets(line,size,batchFp))
  {
    size_t len = strlen(line);

    if (len == (size_t)(size-1) && line[len-1] != '\n' && !feof(batchFp))
      return -1;

    if (line[0] == ';') continue;

    // Skip blank lines.  A bare CRLF line would otherwise fall through to the
    // parser as neither a comment nor the '*' terminator -- and the 1995 sources
    // are CRLF, so on this host fgets() hands back the '\r' as content.
    const char *p = line;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (!*p) continue;

    return 1;
  }

  return 0;
}

//============================================================================
int batchRunner::open(const char *batchFilePath)
{
  char line[MAX_CHARS_PER_LINE+1];
  char msg[600];

  close();
  completed = 0;
  sawTerminator = 0;

  snprintf(batchPath,sizeof(batchPath),"%s",batchFilePath);

  //--------------------------------------------------------------------------
  // The output filename is the batch filename with its extension replaced by
  // ".O".  startBatch() got this for free: the common-dialog struct handed back
  // nFileExtension, and it cut the string there before strcat(outputFile,".O").
  //--------------------------------------------------------------------------
  snprintf(outputFile,sizeof(outputFile),"%s",batchFilePath);
  char *slash = strrchr(outputFile,'/');
  char *dot   = strrchr(slash ? slash : outputFile,'.');
  if (dot) *dot = '\0';
  strncat(outputFile,".O",sizeof(outputFile)-strlen(outputFile)-1);

  //--------------------------------------------------------------------------
  // Delete any previous output and write the table header.
  //--------------------------------------------------------------------------
  remove(outputFile);
  FILE *out = fopen(outputFile,"a+");
  if (!out)
  {
    snprintf(msg,sizeof(msg),"Cannot open batch output file '%s' for writing",
             outputFile);
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }
  fputs(borgStatsHeader(),out);
  fclose(out);

  //--------------------------------------------------------------------------
  batchFp = fopen(batchPath,"r");
  if (!batchFp)
  {
    snprintf(msg,sizeof(msg),"Cannot open batch file '%s'",batchPath);
    error(BORG_ERR_BATCH_SCRIPT,msg);
    return 0;
  }

  // The first data line is the starting simulation number.
  int got = nextDataLine(line,sizeof(line));
  if (got <= 0 || sscanf(line,"%d",&simNum) != 1)
  {
    snprintf(msg,sizeof(msg),
             "Batch file '%s' does not begin with a starting simulation number",
             batchPath);
    error(BORG_ERR_BATCH_SCRIPT,msg);
    close();
    return 0;
  }

  return 1;
}

//============================================================================
// userInterface::runBatch(), UI.CPP:2290, from the "Load next batch simulation"
// comment onward.  The save-and-record half of that function is finishSim().
//
// One structural simplification: 1995 reopened the .b file on every call and
// used fgetpos/fsetpos to get back to where it had stopped reading, because the
// FILE* was a local.  Holding the stream open across calls reads exactly the
// same lines in exactly the same order.
//============================================================================
int batchRunner::startNextSim()
{
  char line[MAX_CHARS_PER_LINE+1];
  char msg[600];

  char BBAenabledStr, softResetStr, netOnStr, classPassOnStr, actPassOnStr,
       CDOstr, CEOstr, eliteStr;

  unsigned seed;
  float goalRange, obstRange, sensorRange, timeConst, antWidth,
        goalReward, dirReward, obstReward, crashPenalty;
  int numAgents, envType,
      geneticInterval, maxCount, maxMsgBoardLength,
      classListLength, numConditions, BBAenabled,  BBAstyle,
      elitismEnabled, producerTaxInterval, CDOenabled, CEOenabled,
      TCOenabled, maxActionsToPost, actionPassingEnabled,
      classifierPassingEnabled, classifierTxInterval,
      maxClassifiersToTx, maxClassifiersToRx, actionTxInterval,
      maxActionsToTx, maxActionsToRx, netOn;
  double bidConstant, sexualProb, mutationProb, startStrength, headTax, bidTax,
        producerTax, eliteThresh, strengthCap, bidCap, TxStrenThresh,
        RxStrenThresh, TxBidThresh, RxBidThresh;

  if (!batchFp) return -1;

  int got = nextDataLine(line,sizeof(line));
  if (got < 0)
  {
    snprintf(msg,sizeof(msg),
             "A line in '%s' is longer than %d characters and was truncated",
             batchPath,MAX_CHARS_PER_LINE-1);
    error(BORG_ERR_BATCH_SCRIPT,msg);
    return -1;
  }

  if (got == 0)
  {
    // 2026 PORT: 1995 relied entirely on the '*' terminator and would have spun
    // forever without one.  Reaching EOF is treated as the end of the batch, but
    // it is reported, because a .b file that stops early is more likely to be
    // truncated than deliberate.
    snprintf(msg,sizeof(msg),
             "Batch file '%s' ended without a '*' terminator line",batchPath);
    error(BORG_ERR_BATCH_SCRIPT,msg);
    return 0;
  }

  if (line[0] == '*') { sawTerminator = 1; return 0; }

  //--------------------------------------------------------------------------
  // Load the current settings.  Not all of them appear in the batch file, so
  // this read supplies the values the spec line does not overwrite -- notably
  // numConditions, BBAstyle, TCOenabled, producerTax, bidTax and
  // producerTaxInterval, the parameters CLAUDE.md records as declared but not
  // used, which must keep their documented values.
  //--------------------------------------------------------------------------
#ifdef NETWORK
  classifierSystem::getSettings(&seed, &geneticInterval, &maxCount, &netOn,
       &maxMsgBoardLength,
       &classListLength, &numConditions, &BBAenabled,  &BBAstyle,
       &elitismEnabled, &producerTaxInterval, &CDOenabled, &CEOenabled,
       &TCOenabled, &maxActionsToPost, &bidConstant, &sexualProb,
       &mutationProb, &startStrength, &headTax, &bidTax, &producerTax,
       &eliteThresh, &strengthCap, &bidCap,
       &actionPassingEnabled, &classifierPassingEnabled, &classifierTxInterval,
       &maxClassifiersToTx, &maxClassifiersToRx, &actionTxInterval,
       &maxActionsToTx, &maxActionsToRx, &TxStrenThresh, &RxStrenThresh,
       &TxBidThresh, &RxBidThresh);
#else
  // The network columns are still parsed below -- the field order is the format
  // and must not shift -- but a non-NETWORK build has nowhere to put them.
  netOn = 0;
  actionPassingEnabled = classifierPassingEnabled = 0;
  classifierTxInterval = maxClassifiersToTx = maxClassifiersToRx = 0;
  actionTxInterval = maxActionsToTx = maxActionsToRx = 0;
  TxStrenThresh = RxStrenThresh = TxBidThresh = RxBidThresh = 0.0;
  classifierSystem::getSettings(&seed, &geneticInterval, &maxCount,
       &maxMsgBoardLength,
       &classListLength, &numConditions, &BBAenabled,  &BBAstyle,
       &elitismEnabled, &producerTaxInterval, &CDOenabled, &CEOenabled,
       &TCOenabled, &maxActionsToPost, &bidConstant, &sexualProb,
       &mutationProb, &startStrength, &headTax, &bidTax, &producerTax,
       &eliteThresh, &strengthCap, &bidCap);
#endif

  //--------------------------------------------------------------------------
  // Read new parameters.  This sscanf is the entire specification of the .b
  // format; there is no other.  Copied verbatim from UI.CPP:2367 -- do not
  // reorder, regroup or reformat the conversions.
  //--------------------------------------------------------------------------
  int fields = sscanf(line,
         "%u %d %d %c %c %c %c %c %lf %lf %d %d %d %d %lf %lf %lf %lf %lf %lf "  // system
         " %f %f %f %f %f %f %f %f %f"                           // agent
         " %c %c %d %d %d %lf %lf %c %d %d %d %lf %lf",          // network
         &seed, &numAgents, &envType, &BBAenabledStr, &softResetStr,
         &CDOstr, &CEOstr, &eliteStr, &eliteThresh, &headTax, &maxCount,
         &classListLength, &maxActionsToPost, &geneticInterval, &bidConstant,
         &startStrength, &strengthCap, &bidCap, &sexualProb, &mutationProb,

         &goalReward, &dirReward, &obstReward, &crashPenalty,
         &goalRange, &obstRange, &sensorRange, &timeConst, &antWidth,

         &netOnStr, &classPassOnStr, &classifierTxInterval,
         &maxClassifiersToTx, &maxClassifiersToRx, &TxStrenThresh,
         &RxStrenThresh, &actPassOnStr, &actionTxInterval,
         &maxActionsToTx, &maxActionsToRx, &TxBidThresh, &RxBidThresh);

  //--------------------------------------------------------------------------
  // 2026 PORT: 1995 ignored the return value.  A short line therefore left the
  // unmatched parameters holding the PREVIOUS simulation's values, read back by
  // the getSettings() call above -- so a batch file with one missing column ran
  // quietly with the wrong settings and wrote results that looked fine.  That is
  // the same class of silent-wrong failure as the RAND_MAX overflow, so it is an
  // error here.  A .b file from an earlier Borg version has fewer columns and
  // will be caught by this.
  //--------------------------------------------------------------------------
  if (fields != BATCH_FIELD_COUNT)
  {
    snprintf(msg,sizeof(msg),
             "Simulation %d in '%s': expected %d parameter columns, matched %d. "
             "See the two ';' header lines in SAMPLE.B for the column order.",
             simNum,batchPath,BATCH_FIELD_COUNT,fields);
    error(BORG_ERR_BATCH_SCRIPT,msg);
    return -1;
  }

  if (BBAenabledStr == 'Y') BBAenabled = 1;
  else BBAenabled = 0;
  if (netOnStr == 'Y') netOn = 1;
  else netOn = 0;
  if (classPassOnStr == 'Y') classifierPassingEnabled = 1;
  else classifierPassingEnabled = 0;
  if (actPassOnStr == 'Y') actionPassingEnabled = 1;
  else actionPassingEnabled = 0;
  if (CDOstr == 'Y') CDOenabled = 1;
  else CDOenabled = 0;
  if (CEOstr == 'Y') CEOenabled = 1;
  else CEOenabled = 0;
  if (eliteStr == 'Y') elitismEnabled = 1;
  else elitismEnabled = 0;
  if (envType == 0) envType = task.STANDARD;
  else envType = task.CONCAVE_OBST;

  // Set the maximum length of the message board
  if (netOn && actionPassingEnabled)
    maxMsgBoardLength = maxActionsToPost + maxActionsToRx;
  else
    maxMsgBoardLength = maxActionsToPost;

  // Make the simulation filename
  snprintf(simName,sizeof(simName),"%d.sim",simNum);

  // Set the number of agents and the environment type
  task.setNumAgents(numAgents);
  task.setEnvType(envType);

  // Set new parameters
  environmentInterface::setEnvSettings(goalReward, dirReward, obstReward,
                                       crashPenalty);
  environmentInterface::setAgentSettings(goalRange, obstRange, sensorRange,
                                         timeConst, antWidth);
#ifdef NETWORK
  classifierSystem::setSettings(seed, geneticInterval, maxCount, netOn,
       maxMsgBoardLength,
       classListLength, numConditions, BBAenabled,  BBAstyle,
       elitismEnabled, producerTaxInterval, CDOenabled, CEOenabled,
       TCOenabled, maxActionsToPost, bidConstant, sexualProb,
       mutationProb, startStrength, headTax, bidTax, producerTax,
       eliteThresh, strengthCap, bidCap,
       actionPassingEnabled, classifierPassingEnabled, classifierTxInterval,
       maxClassifiersToTx, maxClassifiersToRx, actionTxInterval,
       maxActionsToTx, maxActionsToRx, TxStrenThresh, RxStrenThresh,
       TxBidThresh, RxBidThresh);
#else
  classifierSystem::setSettings(seed, geneticInterval, maxCount,
       maxMsgBoardLength,
       classListLength, numConditions, BBAenabled,  BBAstyle,
       elitismEnabled, producerTaxInterval, CDOenabled, CEOenabled,
       TCOenabled, maxActionsToPost, bidConstant, sexualProb,
       mutationProb, startStrength, headTax, bidTax, producerTax,
       eliteThresh, strengthCap, bidCap);
#endif

  //--------------------------------------------------------------------------
  // Reset the system.  A soft reset keeps the rules the previous simulation
  // learned and only restores the starting conditions, which is how a batch
  // studies continued learning; it performs no allocation, so a batch that soft
  // resets must not change any parameter that affects array sizes.
  //--------------------------------------------------------------------------
  if (softResetStr=='Y') task.softReset();
  else task.reset();

  return 1;
}

//============================================================================
int batchRunner::tick()
{
  return task.clockTick();
}

//============================================================================
// The first half of runBatch()'s non-firstRun branch, UI.CPP:2327-2331:
//     save(filename);  saveStats(outputFile);  fsetpos(...);  ++simNum;
//============================================================================
int batchRunner::finishSim()
{
  if (!borgSaveSim(simName)) return 0;
  if (!appendStats())        return 0;

  ++simNum;
  ++completed;
  return 1;
}

//============================================================================
// userInterface::saveStats(), UI.CPP:2219.
//
// Opened and closed per simulation, as the original was, so a batch that dies
// partway through still leaves every completed run on disk.
//============================================================================
int batchRunner::appendStats()
{
  int numCrossovers, numSexual, numMutations, numCDOs, numCEOs,
      geneticInterval, maxCount, numCrashes,
      numAgents = task.getNumAgents();
  unsigned seed;
  char row[BORG_STATS_ROW_MAX];
  char msg[600];

#ifdef NETWORK
  int netOn;
  classifierSystem::getSettings(&seed, &geneticInterval, &maxCount, &netOn);
#else
  classifierSystem::getSettings(&seed, &geneticInterval, &maxCount);
#endif
  (void)geneticInterval;  (void)maxCount;

  FILE *fptr = fopen(outputFile,"a+");
  if (!fptr)
  {
    snprintf(msg,sizeof(msg),"Cannot append to batch output file '%s'",
             outputFile);
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }

  for (int i=0; i<numAgents; i++)
  {
    task.readStats(i, &numCrossovers, &numSexual, &numMutations, &numCDOs,
                   &numCEOs, &numCrashes);
#ifdef NETWORK
    unsigned long agentID = task.getAgentID(i);
#else
    // getAgentID() is declared only inside TASK.H's #ifdef NETWORK block, which
    // is why 1995's saveStats() did not compile without NETWORK.  IDs are handed
    // out as 1..numAgents (see getCurrentLoc, which indexes with ID-1), so the
    // printed column is the same either way.
    unsigned long agentID = (unsigned long)(i+1);
#endif
    borgFormatStatsRow(row, simNum, agentID, seed, task.readClock(i),
                       numSexual, numCrossovers-numSexual, numMutations,
                       numCDOs, numCEOs, numCrashes);
    fputs(row,fptr);
  }

  int writeFailed = ferror(fptr);
  if (fclose(fptr) != 0 || writeFailed)
  {
    snprintf(msg,sizeof(msg),"Failed while writing '%s'",outputFile);
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }

  return 1;
}
