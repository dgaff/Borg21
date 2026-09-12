//============================================================================
// Batch runner -- Layer 3's batch machinery, extracted and de-Windowsed.
//
// The 1995 original is split across three functions in UI.CPP:
//
//   userInterface::startBatch()  UI.CPP:2250   pick the .b, open the .O, begin
//   userInterface::runBatch()    UI.CPP:2290   read one spec line, apply, reset
//   userInterface::saveStats()   UI.CPP:2219   append one .O row per agent
//
// plus the message-pump state machine in processRequests() (UI.CPP:388-418),
// which drove it: batchRequest -> runBatch(), runStartRequest -> runRequest ->
// one task.clockTick() per idle message, and on the tick that returned "done",
// batchRequest again.  Running one tick per Windows idle message is what kept
// the display live on a 1993 machine; it is not part of the simulation.
//
// This class keeps that structure rather than collapsing it into one loop, so
// the ImGui front end can drive a batch a tick at a time and repaint between
// ticks exactly as BORG.EXE did, while the CLI just spins tick() to completion.
// The call order is the same either way, and it is the same order runBatch used:
//
//     open()                       read the starting sim number, write the .O header
//     while (startNextSim())       read and apply one spec line
//     {  while (!tick()) ;         run that simulation to its own end
//        finishSim();              write <N>.sim, append the .O rows, ++simNum
//     }
//     close()
//
// WHAT IS DELIBERATELY NOT REPRODUCED
//
// startBatch() saved the user's in-progress simulation to a mktemp() file and
// reloaded it when the batch finished, so a batch did not destroy unsaved work.
// A headless run has no unsaved work to protect, so there is no temp file.
//============================================================================

#ifndef BORG_HOST_BATCH_H
#define BORG_HOST_BATCH_H

#include <stdio.h>

//----------------------------------------------------------------------------
// One .O row.  Pulled out of saveStats() as a free function so the exact 1995
// column layout can be regression-tested against a surviving 1995 .O file
// without running a simulation.  See tests/batch_format.cpp.
//
// `buf` must hold at least BORG_STATS_ROW_MAX bytes.  The result ends in '\n'.
//----------------------------------------------------------------------------
#define BORG_STATS_ROW_MAX 256

void borgFormatStatsRow(char *buf, int simNum, unsigned long agentID,
                        unsigned seed, int iterations,
                        int numSexual, int numAsexual, int numMutations,
                        int numCDOs, int numCEOs, int numCrashes);

// The two header lines startBatch() wrote above the table, newline-terminated
// and ending in the blank line the original emitted.
const char *borgStatsHeader(void);

//----------------------------------------------------------------------------
class batchRunner
{
  public:
    batchRunner();
    ~batchRunner();

    // Opens the .b script, reads the starting simulation number, and creates
    // the .O output file with its header.  Returns 1 on success, 0 on failure.
    int open(const char *batchFilePath);

    // Reads and applies the next simulation spec, then resets the task
    // environment.  Returns 1 if a simulation is now set up and ready to tick,
    // 0 if the batch is complete, and -1 if the script is malformed.
    int startNextSim();

    // One simulation step.  Nonzero means this simulation has finished; the
    // simulation decides that for itself, either by reaching the goal or by
    // hitting maxCount (CFS.CPP:197).
    int tick();

    // Writes <simNum>.sim, appends one .O row per agent, and advances simNum.
    int finishSim();

    void close();

    int         currentSimNum(void) const { return simNum; }
    int         simsCompleted(void) const { return completed; }
    const char *outputPath(void)    const { return outputFile; }
    const char *simPath(void)       const { return simName; }

  private:
    int  nextDataLine(char *line, int size);
    int  appendStats(void);

    FILE *batchFp;
    char  batchPath[520];
    char  outputFile[520];
    char  simName[64];
    int   simNum;
    int   completed;
    int   sawTerminator;
};

#endif
