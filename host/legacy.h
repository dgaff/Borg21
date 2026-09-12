//============================================================================
// Readers for the 1995 16-bit binary files: .SIM snapshots and .P position
// dumps.
//
// WHY THESE CANNOT JUST BE fread() BACK
//
// Every save/load function in Layers 1 and 2 is a raw fwrite of scalars --
// "field order IS the format" (CLAUDE.md).  That makes the format portable in
// field ORDER but not in field WIDTH, and Borland C++ 4.02 targeting 16-bit
// Windows had different widths from a 64-bit Unix compiler:
//
//                 Borland 16-bit        arm64 macOS / LP64
//     char                      1                       1
//     int                       2                       4
//     unsigned                  2                       4
//     long                      4                       8
//     unsigned long             4                       8
//     float                     4                       4   IEEE-754 single
//     double                    8                       8   IEEE-754 double
//     coord (two floats)        8                       8
//
// So a 1995 .SIM read with today's fread() desynchronises on the very first
// field.  host/simfile.cpp deliberately does not try; this is where the widening
// happens.  Nothing in the 1995 sources is touched to make it work: the reader
// walks the same field order those save() functions wrote and re-emits it at
// native widths, producing a byte stream that the ordinary borgLoadSim() path
// then reads.
//
// No long double is ever written to a Borg file, which is the one place Borland
// would have been genuinely incompatible (its long double was the x87 80-bit
// format).  Every float and double in these files is IEEE-754 and survives the
// move exactly.
//
// VERIFIED AGAINST THE REAL ARTIFACTS
//
// The schema below predicts the size of a 1995 .SIM from its own header fields:
//
//     4 + 216 + (task.save) + numAgents*(classifierSystem::save)
//
// For Research/BBA/1.SIM -- message length 9, classifier list 32, message board
// 1, 1 agent, 401 obstacles, 24 saved positions -- that arithmetic gives exactly
// 6192 bytes, which is the size of the file.  All 50 .SIM files in Research/BBA
// parse with the cursor landing exactly on EOF, and the position block inside
// 1.SIM is byte-identical to the separately-written 1_1.P.  Exact-EOF is
// therefore enforced, not advisory: it is the check that catches a schema drift.
//
// THE NETWORK LAYOUT
//
// #define NETWORK in CFS.H adds fields to almost every record (agent IDs
// threaded through the classifier list, the message board, and the network
// interface), so a DLCS .SIM and a plain-LCS .SIM have different layouts and
// nothing in the file says which it is before those fields are needed.  The
// reader resolves this by trying the DLCS layout first and the plain layout
// second, and accepting only a parse that consumes the file exactly and passes
// every sanity check along the way.  borgLegacySim::network reports which won.
//
// .RUL FILES NEED NONE OF THIS.  classifierList::saveRules/loadRules are plain
// ASCII ("%s %s %lf" per line), so the 1995 files load directly -- see
// borgLegacyCheckRules() for the one trap they do carry.
//============================================================================

#ifndef BORG_HOST_LEGACY_H
#define BORG_HOST_LEGACY_H

#define BORG_LEGACY_MAX_MSG 64      // generous; 1995 files all use 9

//----------------------------------------------------------------------------
// Per-agent state worth reporting or comparing.  The trajectory and the rule
// list are kept because they are the two things anyone actually wants out of a
// 1995 snapshot: where the animat went, and what it had learned.  The rest of
// the bulk data (bids, specificity, support, supplier sets, queued network
// messages) is transcoded straight into the modern byte image without being
// modelled -- nothing here needs to reason about it field by field, and
// borgLoadSim() unpacks it into the real objects.
//----------------------------------------------------------------------------
struct borgLegacyAgent
{
  unsigned long agentID;        // DLCS layout only; 0 otherwise
  long          numRandCalls;   // how far the RNG had been advanced
  int           clockCount;     // this agent's own iteration count
  int           finished;       // taskEnvironment::agentFinished[i]

  int           ptCount;        // saved positions, = clockCount + 1
  float        *x, *y, *dir;    // ptCount entries each

  // The classifier list as it stood: classListLen entries.  cond[i] and
  // action[i] are NUL-terminated allele strings of msgLen characters.
  char        **cond, **action;
  double       *strength;

  int numCrossovers, numSexual, numMutations, numCDOs, numCEOs, numTCOs,
      numClassifiers;
  int numCrashes, crashFlag, goalFlag;
};

//----------------------------------------------------------------------------
// A decoded 1995 .SIM.  Field names and grouping follow the save() functions
// that wrote them, so this struct reads as a map of the file.
//----------------------------------------------------------------------------
struct borgLegacySim
{
  char version[4];
  int  network;                 // 1 = the file has the DLCS layout

  // classifierSystem::saveStatic
  int      msgLen;
  unsigned seed;
  int      geneticInterval, maxCount, networkEnabled;

  // classifierList::saveStatic
  int    classListLen, numConditions, BBAenabled, BBAstyle, elitismEnabled,
         producerTaxInterval, maxActionsToPost, CDOenabled, CEOenabled,
         TCOenabled;
  double bidConstant, sexualProb, mutationProb, startStrength, headTax,
         bidTax, producerTax, eliteThresh, strengthCap, bidCap;
  char   condMask[BORG_LEGACY_MAX_MSG+1], actionMask[BORG_LEGACY_MAX_MSG+1];

  // messageBoard::saveStatic
  int msgBoardLen;

  // environmentInterface::saveStatic
  float goalRange, obstRange, sensorRange, timeConst, antWidth,
        goalReward, dirReward, obstReward, crashPenalty;

  // networkInterface::saveStatic (DLCS layout only)
  int    actionPassing, classPassing, classTxInterval, maxClassTx, maxClassRx,
         actionTxInterval, maxActTx, maxActRx;
  double TxStrenThresh, RxStrenThresh, TxBidThresh, RxBidThresh;

  // taskEnvironment::save
  int   envType, x1, y1, x2, y2, numObst, numAgents, globalClock;
  float goalX, goalY;

  borgLegacyAgent *agent;       // numAgents entries

  // Provenance
  long fileSize, bytesConsumed; // equal on success, by construction
  long modernSize;              // size of the widened image

  // The widened byte image, ready for borgLegacyWriteModernSim().  Opaque.
  unsigned char *modern;
};

//----------------------------------------------------------------------------
// True when no rule-discovery operator ran during the simulation the snapshot
// came from.  That matters for one specific reason: if nothing rewrote the rule
// list, then the condition/action pairs in the snapshot are still the ones the
// run STARTED with, so a .RUL can be matched against them.  Once a mutation or a
// CDO has fired, the snapshot's rules are an outcome, not an input.
//----------------------------------------------------------------------------
int borgLegacyRulesUnchanged(const borgLegacySim *sim);

//----------------------------------------------------------------------------
// A decoded 1995 .P (taskEnvironment::saveAgentPosBinary).  Positions only:
// no version string, no statics, and no NETWORK dependence, so this one is
// unambiguous.
//----------------------------------------------------------------------------
struct borgLegacyPos
{
  int numAgents;
  int maxPositions;             // what 1995 wrote: maxCount + 1
  struct Agent { int ptCount; float *x, *y, *dir; } *agent;

  long fileSize, bytesConsumed, modernSize;
  unsigned char *modern;
};

//----------------------------------------------------------------------------
// All four readers return 1 on success and 0 on failure, reporting the reason
// through error() / hostDisplayError() exactly as the rest of the port does.
// On failure the struct is left safe to pass to the matching free().
//
// Every struct returned by a Read must be handed to the matching Free.
//----------------------------------------------------------------------------
int  borgLegacyReadSim(const char *path, borgLegacySim *sim);
void borgLegacyFreeSim(borgLegacySim *sim);

int  borgLegacyReadPos(const char *path, borgLegacyPos *pos);
void borgLegacyFreePos(borgLegacyPos *pos);

//----------------------------------------------------------------------------
// Write the widened image as a modern .sim / .P that this build can load.
//
// A .sim is refused when the file's layout does not match this build's NETWORK
// setting: CLAUDE.md's warning that the two are not interchangeable applies to
// the converted file just as it did in 1995, and writing one that borgLoadSim()
// would then misread is worse than refusing.
//----------------------------------------------------------------------------
int borgLegacyWriteModernSim(const borgLegacySim *sim, const char *path);
int borgLegacyWriteModernPos(const borgLegacyPos *pos, const char *path);

//----------------------------------------------------------------------------
// Report on a 1995 .RUL without loading it, because two things about that
// format bite silently:
//
//   * taskEnvironment::loadRules() (TASK.CPP) discards one line per agent before
//     handing the file to classifierList::loadRules(), to skip the "; Agent N"
//     header that saveRules() writes.  But saveRules() writes that header only
//     #ifdef NETWORK, while loadRules() skips it unconditionally -- so in a
//     plain-LCS build a .RUL it just wrote loses its first rule when reloaded.
//   * classifierList::loadRules() reads exactly `length` lines and does no error
//     checking whatsoever (its own comment says so).  A file with fewer rules
//     than the configured classifier list leaves the remaining slots holding
//     whatever reset() randomly generated, with no complaint.
//
// Fills in the counts so a caller can say something useful before either
// happens.  *agents is the number of "; Agent N" headers found, *rules the
// number of well-formed rule lines.  Returns 1 if the file could be read.
//----------------------------------------------------------------------------
int borgLegacyCheckRules(const char *path, int *agents, int *rules,
                         int *msgLenSeen);

#endif
