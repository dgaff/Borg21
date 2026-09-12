//============================================================================
// 16-bit legacy reader.  See host/legacy.h for the format and for how the
// schema was verified against the surviving 1995 files.
//
// HOW THIS IS STRUCTURED, AND WHY
//
// There is exactly ONE walk of the .SIM schema, in walkSim() below, and it both
// decodes fields into a borgLegacySim and re-emits them at native widths into a
// byte buffer.  The alternative -- a validating pass followed by a transcoding
// pass -- would state the field order twice, and a format whose only
// specification is the order of its fields is the last place to duplicate it.
//
// Each cp*() helper does one legacy read and one native write, so the walk reads
// as a transcription of the 1995 save() functions it mirrors.  Every one of them
// is a no-op once cursor->bad is set, which is what lets the walk run straight
// through without an error test after every field.
//
// Integers are assembled from bytes rather than memcpy'd, so the reader does not
// depend on the host being little-endian.  Borland wrote little-endian because
// the 8086 was; that is a property of the FILE, not of this machine.
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include "coord.h"
#include "cfs.h"
#include "ei.h"
#include "host.h"
#include "legacy.h"

//----------------------------------------------------------------------------
// The widths this reader assumes on the writing side.  Named rather than
// inlined as 2/4/8 so the walk below says what each field's 1995 type was.
//----------------------------------------------------------------------------
#define L16_INT     2     // int, unsigned
#define L16_LONG    4     // long, unsigned long
#define L16_FLOAT   4
#define L16_DOUBLE  8
#define L16_COORD   8     // class coord { float x, y; }

//----------------------------------------------------------------------------
// Native-side assumptions.  If any of these ever stops holding, the widened
// image would not be what task.save() writes, and the conversion would produce
// a file that loads as plausible garbage.  Fail at compile time instead.
//----------------------------------------------------------------------------
static_assert(sizeof(int)    == 4, "this reader widens 1995 int to a 4-byte int");
static_assert(sizeof(float)  == 4, "float must be IEEE-754 single");
static_assert(sizeof(double) == 8, "double must be IEEE-754 double");
static_assert(sizeof(coord)  == 2*sizeof(float),
              "coord must still be two packed floats -- task.save() writes it "
              "with sizeof(coord), so any padding would change the format");

//----------------------------------------------------------------------------
// Queue message kinds.  These are queueList's anonymous enum (QUEUE.H); the
// values are what queue::save() writes into the type field.  Repeated here
// because the enum is inside a struct that only exists #ifdef NETWORK.
//----------------------------------------------------------------------------
#define LQ_NO_TYPE      0
#define LQ_ACTION       1
#define LQ_CLASSIFIER   2
#define LQ_PAYOFF       3

//============================================================================
// The cursor
//============================================================================

struct LC
{
  const unsigned char *in;
  long inLen, inPos;

  unsigned char *out;      // 0 for a decode-only walk
  long outLen, outCap;

  int net;                 // walk the DLCS layout?
  int bad;                 // first failure wins
  char why[200];
};

// Records the FIRST failure and stops the walk.  Keeping the first reason
// rather than the last is deliberate: once the cursor is out of step every
// later field is nonsense, and the earliest complaint is the one that points at
// where the layout actually diverged.
static void fail(LC *c, const char *fmt, ...)
{
  if (c->bad) return;
  c->bad = 1;

  va_list ap;
  va_start(ap,fmt);
  vsnprintf(c->why,sizeof(c->why),fmt,ap);
  va_end(ap);
}

//----------------------------------------------------------------------------
static const unsigned char *take(LC *c, long n)
{
  if (c->bad) return 0;
  if (n < 0 || c->inPos + n > c->inLen)
  {
    fail(c,"file ends early: wanted %ld more byte(s) at offset %ld",
         n,c->inPos);
    return 0;
  }
  const unsigned char *p = c->in + c->inPos;
  c->inPos += n;
  return p;
}

static void put(LC *c, const void *src, long n)
{
  if (c->bad || !c->out) return;
  if (c->outLen + n > c->outCap)
  { fail(c,"internal: widened image exceeded its buffer at offset %ld",c->inPos);
    return; }
  memcpy(c->out + c->outLen, src, (size_t)n);
  c->outLen += n;
}

//----------------------------------------------------------------------------
// Little-endian assembly out of the file's bytes.
//----------------------------------------------------------------------------
static uint64_t leBits(const unsigned char *p, int n)
{
  uint64_t v = 0;
  for (int i = n - 1; i >= 0; --i) v = (v << 8) | p[i];
  return v;
}

//----------------------------------------------------------------------------
// One legacy field in, one native field out.  Each returns the decoded value so
// the walk can both record it and branch on it.
//----------------------------------------------------------------------------
static int cpInt(LC *c)
{
  const unsigned char *p = take(c,L16_INT);
  if (!p) return 0;
  int v = (int)(int16_t)leBits(p,L16_INT);   // sign-extend the 1995 int
  put(c,&v,sizeof(v));
  return v;
}

static unsigned cpUnsigned(LC *c)
{
  const unsigned char *p = take(c,L16_INT);
  if (!p) return 0;
  unsigned v = (unsigned)(uint16_t)leBits(p,L16_INT);
  put(c,&v,sizeof(v));
  return v;
}

static long cpLong(LC *c)
{
  const unsigned char *p = take(c,L16_LONG);
  if (!p) return 0;
  long v = (long)(int32_t)leBits(p,L16_LONG);
  put(c,&v,sizeof(v));
  return v;
}

static unsigned long cpULong(LC *c)
{
  const unsigned char *p = take(c,L16_LONG);
  if (!p) return 0;
  unsigned long v = (unsigned long)(uint32_t)leBits(p,L16_LONG);
  put(c,&v,sizeof(v));
  return v;
}

static float cpFloat(LC *c)
{
  const unsigned char *p = take(c,L16_FLOAT);
  if (!p) return 0.0f;
  uint32_t bits = (uint32_t)leBits(p,L16_FLOAT);
  float v;
  memcpy(&v,&bits,sizeof(v));
  put(c,&v,sizeof(v));
  return v;
}

static double cpDouble(LC *c)
{
  const unsigned char *p = take(c,L16_DOUBLE);
  if (!p) return 0.0;
  uint64_t bits = leBits(p,L16_DOUBLE);
  double v;
  memcpy(&v,&bits,sizeof(v));
  put(c,&v,sizeof(v));
  return v;
}

//----------------------------------------------------------------------------
// char arrays pass through unchanged -- messages and masks are the only
// same-width data in the whole format.  `dst` is optional.
//----------------------------------------------------------------------------
static void cpChars(LC *c, char *dst, long n)
{
  const unsigned char *p = take(c,n);
  if (!p) return;
  if (dst) memcpy(dst,p,(size_t)n);
  put(c,p,n);
}

//----------------------------------------------------------------------------
// Bulk arrays.  `dst` collects values only where something downstream needs
// them; otherwise the data is transcoded and dropped.
//----------------------------------------------------------------------------
static void cpIntArray(LC *c, long n, int *dst = 0)
{
  for (long i = 0; i < n && !c->bad; i++)
  { int v = cpInt(c); if (dst) dst[i] = v; }
}

static void cpULongArray(LC *c, long n)
{ for (long i = 0; i < n && !c->bad; i++) cpULong(c); }

static void cpFloatArray(LC *c, long n, float *dst = 0)
{
  for (long i = 0; i < n && !c->bad; i++)
  { float v = cpFloat(c); if (dst) dst[i] = v; }
}

static void cpDoubleArray(LC *c, long n, double *dst = 0)
{
  for (long i = 0; i < n && !c->bad; i++)
  { double v = cpDouble(c); if (dst) dst[i] = v; }
}

//----------------------------------------------------------------------------
// coord is written with fwrite(&pt,sizeof(coord),...), i.e. x then y with no
// padding, so it transcodes as a pair of floats.  The static_assert above is
// what makes that equivalence safe to rely on.
//----------------------------------------------------------------------------
static void cpCoordArray(LC *c, long n, float *x = 0, float *y = 0)
{
  for (long i = 0; i < n && !c->bad; i++)
  {
    float vx = cpFloat(c), vy = cpFloat(c);
    if (x) x[i] = vx;
    if (y) y[i] = vy;
  }
}

//============================================================================
// Sanity checks.  These exist to make the NETWORK autodetect decisive: a parse
// of the wrong layout usually desynchronises inside the first mask, and always
// misses EOF.
//============================================================================

// The mask alphabet is documented above classifierList::setMasks (CLSSLIST.CPP):
// 0 and 1 force that allele, # forces don't-care, X allows any of 0/1/#, B
// allows 0/1 only.  Two fixed-position masks of exactly this shape sit in the
// middle of the static block, which makes them the most effective checkpoint in
// the file -- a walk that has lost its place almost never lands on one.
//
// NUL is rejected explicitly: strchr() finds the terminator of its own search
// string, so a run of zero bytes would otherwise pass as a valid mask.
static void checkMask(LC *c, const char *mask, int msgLen, const char *what)
{
  if (c->bad) return;
  for (int i = 0; i < msgLen; i++)
    if (mask[i] == '\0' || !strchr("01#XB",mask[i]))
    { fail(c,"%s is not a mask",what); return; }
  if (mask[msgLen] != '\0')
    fail(c,"%s is not NUL-terminated",what);
}

static void checkRange(LC *c, long v, long lo, long hi, const char *what)
{
  if (c->bad) return;
  if (v < lo || v > hi)
    fail(c,"%s is %ld, outside the plausible range %ld..%ld",what,v,lo,hi);
}

//============================================================================
// queue::save() -- one transmit or receive queue.  Always empty in every 1995
// file found (no batch ever saved mid-tick, which is the only moment a queue
// holds anything), but the layout is walked properly so a hand-made file or a
// future mid-tick snapshot is not silently truncated.
//============================================================================
static void walkQueue(LC *c, int msgLen)
{
  int count     = cpInt(c);
  int maxLength = cpInt(c);
  checkRange(c,count,0,100000,"a network queue's message count");
  checkRange(c,maxLength,0,1000000,"a network queue's maximum length");

  for (int i = 0; i < count && !c->bad; i++)
  {
    int type = cpInt(c);
    cpULong(c);                       // source agent
    cpULong(c);                       // destination agent

    if (type == LQ_ACTION)
    {
      cpChars(c,0,msgLen+1);          // action message
      cpInt(c);                       // supplier rule number
      cpULong(c);                     // supplier's agent
      cpDouble(c);                    // bid
    }
    else if (type == LQ_CLASSIFIER)
    {
      cpChars(c,0,msgLen+1);          // condition
      cpChars(c,0,msgLen+1);          // action
      cpDouble(c);                    // strength
    }
    else if (type == LQ_PAYOFF)
    {
      cpInt(c);                       // rule to pay
      cpDouble(c);                    // payoff
    }
    else
    {
      // queue::save() only ever writes the three kinds above, and queue::load()
      // treats anything it does not recognise as a payoff message -- which would
      // read 12 bytes for a record of unknown length.  Refuse instead.
      checkRange(c,type,LQ_ACTION,LQ_PAYOFF,"a queued message's type");
    }
  }
}

//============================================================================
// classifierSystem::save() for one agent -- CFS.CPP:288, and the
// classifierList::save / messageBoard::save / environmentInterface::save /
// networkInterface::save calls it makes.
//============================================================================
static void walkAgent(LC *c, borgLegacySim *s, borgLegacyAgent *a)
{
  const int M = s->msgLen;
  const long L = s->classListLen;
  const long B = s->msgBoardLen;

  //-- classifierSystem::save -------------------------------------------------
  // The flag CFS.CPP writes to record which build produced the file: 1 for
  // NETWORK, 0 for plain LCS.  This is the only self-describing field in the
  // format, and it arrives too late to steer the static block above -- hence the
  // two-attempt autodetect in borgLegacyReadSim().
  int netFlag = cpInt(c);
  if (!c->bad && netFlag != (c->net ? 1 : 0))
    fail(c,"agent record says network=%d but the layout assumed %d",
         netFlag, c->net);

  if (c->net) a->agentID = cpULong(c);
  else        a->agentID = 0;

  a->numRandCalls = cpLong(c);
  a->clockCount   = cpInt(c);
  checkRange(c,a->numRandCalls,0,0x7fffffffL,"an agent's random-call count");

  //-- classifierList::save -------------------------------------------------
  // The rule list is kept, not just transcoded: it is the learned result the
  // snapshot exists to record.  cpChars() copies M+1 bytes, the last of which is
  // the NUL message::save() writes, so the strings arrive terminated.
  a->cond     = (char **)calloc((size_t)L,sizeof(char *));
  a->action   = (char **)calloc((size_t)L,sizeof(char *));
  a->strength = (double *)calloc((size_t)L,sizeof(double));
  if (!a->cond || !a->action || !a->strength)
  { fail(c,"out of memory"); return; }

  for (long i = 0; i < L && !c->bad; i++)
  {
    a->cond[i]   = (char *)calloc((size_t)M+1,1);
    a->action[i] = (char *)calloc((size_t)M+1,1);
    if (!a->cond[i] || !a->action[i]) { fail(c,"out of memory"); return; }
    cpChars(c,a->cond[i],M+1);         // conditionMsg[i]
    cpChars(c,a->action[i],M+1);       // actionMsg[i]
  }
  cpIntArray(c,L);                    // matchFlag
  cpDoubleArray(c,L);                 // bid
  cpDoubleArray(c,L);                 // specificity
  cpDoubleArray(c,L,a->strength);     // strength
  cpDoubleArray(c,L);                 // support
  cpIntArray(c,L);                    // number
  cpIntArray(c,L);                    // selected
  cpIntArray(c,L);                    // supplierSet
  cpIntArray(c,L);                    // tempSupplierSet
  cpIntArray(c,L);                    // eliteSet
  if (c->net)
  {
    cpULong(c);                       // agentIDcopy
    cpULongArray(c,L);                // supplierID
    cpULongArray(c,L);                // tempSupplierID
  }
  a->numCrossovers  = cpInt(c);
  a->numSexual      = cpInt(c);
  a->numMutations   = cpInt(c);
  a->numCDOs        = cpInt(c);
  a->numCEOs        = cpInt(c);
  a->numTCOs        = cpInt(c);
  a->numClassifiers = cpInt(c);

  //-- messageBoard::save ---------------------------------------------------
  for (long i = 0; i < B && !c->bad; i++) cpChars(c,0,M+1);
  int actualLength = cpInt(c);
  checkRange(c,actualLength,0,B,"the message board's actual size");
  cpIntArray(c,B);                    // supplierList
  if (c->net) cpULongArray(c,B);      // supplierID
  cpDoubleArray(c,B);                 // bid

  //-- environmentInterface::save -------------------------------------------
  cpChars(c,0,M+1);                   // environMsg
  cpFloat(c);                         // crashDistance
  // EI.CPP writes environmentInterface::agentIDcopy with sizeof(long) even
  // though the member is `unsigned long`.  Same width either way, in 1995 and
  // now, so the distinction is cosmetic -- but it has to be a long-width field,
  // not an int one.
  cpLong(c);                          // agentIDcopy
  a->numCrashes = cpInt(c);
  a->crashFlag  = cpInt(c);
  a->goalFlag   = cpInt(c);

  //-- networkInterface::save -----------------------------------------------
  if (c->net)
  {
    cpULong(c);                       // agentIDcopy
    walkQueue(c,M);                   // Tx
    walkQueue(c,M);                   // Rx
  }
}

//============================================================================
// The whole file: version string, then taskEnvironment::saveStatic() (which is
// just classifierSystem::saveStatic), then taskEnvironment::save().
//
// Allocation happens mid-walk, once numAgents and the point counts are known.
// walkSim() is therefore only ever called on a zeroed struct, and every failure
// path leaves it safe for borgLegacyFreeSim().
//============================================================================
static void walkSim(LC *c, borgLegacySim *s)
{
  int i;

  //-- userInterface::save's version stamp ----------------------------------
  cpChars(c,s->version,4);
  if (c->bad) return;
  s->version[3] = '\0';
  if (strcmp(s->version,"2.1") != 0)
  { fail(c,"version string is not \"2.1\""); return; }

  //-- message::saveStatic --------------------------------------------------
  s->msgLen = cpInt(c);
  checkRange(c,s->msgLen,1,BORG_LEGACY_MAX_MSG,"the message length");
  if (c->bad) return;
  const int M = s->msgLen;

  //-- classifierSystem::saveStatic ----------------------------------------
  s->seed            = cpUnsigned(c);
  s->geneticInterval = cpInt(c);
  s->maxCount        = cpInt(c);
  s->networkEnabled  = c->net ? cpInt(c) : 0;
  checkRange(c,s->geneticInterval,1,1000000,"geneticInterval");
  checkRange(c,s->maxCount,1,1000000,"maxCount");

  //-- classifierList::saveStatic ------------------------------------------
  s->classListLen        = cpInt(c);
  s->numConditions       = cpInt(c);
  s->BBAenabled          = cpInt(c);
  s->BBAstyle            = cpInt(c);
  s->elitismEnabled      = cpInt(c);
  s->producerTaxInterval = cpInt(c);
  s->maxActionsToPost    = cpInt(c);
  s->CDOenabled          = cpInt(c);
  s->CEOenabled          = cpInt(c);
  s->TCOenabled          = cpInt(c);
  checkRange(c,s->classListLen,1,100000,"the classifier list length");

  s->bidConstant   = cpDouble(c);
  s->sexualProb    = cpDouble(c);
  s->mutationProb  = cpDouble(c);
  s->startStrength = cpDouble(c);
  s->headTax       = cpDouble(c);
  s->bidTax        = cpDouble(c);
  s->producerTax   = cpDouble(c);
  s->eliteThresh   = cpDouble(c);
  s->strengthCap   = cpDouble(c);
  s->bidCap        = cpDouble(c);

  cpChars(c,s->condMask,M+1);
  cpChars(c,s->actionMask,M+1);
  checkMask(c,s->condMask,M,"the condition mask");
  checkMask(c,s->actionMask,M,"the action mask");

  //-- messageBoard::saveStatic --------------------------------------------
  s->msgBoardLen = cpInt(c);
  checkRange(c,s->msgBoardLen,1,100000,"the message board length");

  //-- environmentInterface::saveStatic ------------------------------------
  s->goalRange    = cpFloat(c);
  s->obstRange    = cpFloat(c);
  s->sensorRange  = cpFloat(c);
  s->timeConst    = cpFloat(c);
  s->antWidth     = cpFloat(c);
  s->goalReward   = cpFloat(c);
  s->dirReward    = cpFloat(c);
  s->obstReward   = cpFloat(c);
  s->crashPenalty = cpFloat(c);

  //-- networkInterface::saveStatic ----------------------------------------
  if (c->net)
  {
    s->actionPassing    = cpInt(c);
    s->classPassing     = cpInt(c);
    s->classTxInterval  = cpInt(c);
    s->maxClassTx       = cpInt(c);
    s->maxClassRx       = cpInt(c);
    s->actionTxInterval = cpInt(c);
    s->maxActTx         = cpInt(c);
    s->maxActRx         = cpInt(c);
    s->TxStrenThresh    = cpDouble(c);
    s->RxStrenThresh    = cpDouble(c);
    s->TxBidThresh      = cpDouble(c);
    s->RxBidThresh      = cpDouble(c);
  }

  //-- taskEnvironment::save ------------------------------------------------
  s->envType     = cpInt(c);
  s->x1          = cpInt(c);
  s->y1          = cpInt(c);
  s->x2          = cpInt(c);
  s->y2          = cpInt(c);
  s->numObst     = cpInt(c);
  s->numAgents   = cpInt(c);
  s->globalClock = cpInt(c);
  checkRange(c,s->envType,0,1,"the environment type");
  checkRange(c,s->numAgents,1,1000,"the agent count");
  checkRange(c,s->numObst,0,1000000,"the obstacle count");
  if (c->bad) return;

  s->agent = (borgLegacyAgent *)calloc((size_t)s->numAgents,
                                       sizeof(borgLegacyAgent));
  if (!s->agent) { fail(c,"out of memory"); return; }

  int *finished = (int *)calloc((size_t)s->numAgents,sizeof(int));
  if (!finished) { fail(c,"out of memory"); return; }
  cpIntArray(c,s->numAgents,finished);
  for (i = 0; i < s->numAgents; i++) s->agent[i].finished = finished[i];
  free(finished);

  cpCoordArray(c,1,&s->goalX,&s->goalY);
  cpCoordArray(c,s->numObst);                      // obstPts

  int *ptCount = (int *)calloc((size_t)s->numAgents,sizeof(int));
  if (!ptCount) { fail(c,"out of memory"); return; }
  cpIntArray(c,s->numAgents,ptCount);
  for (i = 0; i < s->numAgents; i++)
  {
    // A 1995 run saved maxCount+1 positions at most: the start plus one per
    // iteration.  Anything outside that means the walk has lost its place.
    checkRange(c,ptCount[i],1,(long)s->maxCount+1,"an agent's position count");
    s->agent[i].ptCount = ptCount[i];
  }
  free(ptCount);
  if (c->bad) return;

  for (i = 0; i < s->numAgents && !c->bad; i++)
  {
    borgLegacyAgent *a = &s->agent[i];
    a->x   = (float *)calloc((size_t)a->ptCount,sizeof(float));
    a->y   = (float *)calloc((size_t)a->ptCount,sizeof(float));
    a->dir = (float *)calloc((size_t)a->ptCount,sizeof(float));
    if (!a->x || !a->y || !a->dir) { fail(c,"out of memory"); return; }

    cpCoordArray(c,a->ptCount,a->x,a->y);          // agentPts[i]
    cpFloatArray(c,a->ptCount,a->dir);             // agentDir[i]
  }

  // goalDist, oldGoalDist, goalDir, oldGoalDir, obstDist, oldObstDist,
  // obstDir, oldObstDir -- eight numAgents-long float arrays of derived sensor
  // state.  Transcoded, not modelled: borgLoadSim() restores them, and a tool
  // that wants them can ask the live taskEnvironment.
  for (i = 0; i < 8; i++) cpFloatArray(c,s->numAgents);

  // taskEnvironment keeps its own copy of the masks, which it pushes back into
  // classifierSystem::setMasks() on load.
  {
    char tCond[BORG_LEGACY_MAX_MSG+1], tAct[BORG_LEGACY_MAX_MSG+1];
    cpChars(c,tCond,M+1);
    cpChars(c,tAct,M+1);
    checkMask(c,tCond,M,"the task's condition mask");
    checkMask(c,tAct,M,"the task's action mask");
  }

  for (i = 0; i < s->numAgents && !c->bad; i++)
    walkAgent(c,s,&s->agent[i]);
}

//============================================================================
int borgLegacyReadSim(const char *path, borgLegacySim *sim)
{
  char msg[400];

  memset(sim,0,sizeof(*sim));

  FILE *fptr = fopen(path,"rb");
  if (!fptr)
  {
    snprintf(msg,sizeof(msg),"Cannot open '%s' for reading",path);
    error(BORG_ERR_LEGACY_FILE,msg);
    return 0;
  }

  fseek(fptr,0,SEEK_END);
  long len = ftell(fptr);
  fseek(fptr,0,SEEK_SET);

  unsigned char *raw = 0;
  if (len > 0) raw = (unsigned char *)malloc((size_t)len);
  if (!raw || fread(raw,1,(size_t)len,fptr) != (size_t)len)
  {
    fclose(fptr); free(raw);
    snprintf(msg,sizeof(msg),"Could not read all of '%s'",path);
    error(BORG_ERR_LEGACY_FILE,msg);
    return 0;
  }
  fclose(fptr);

  // Widening never more than doubles a field (int 2->4, long 4->8; float,
  // double and char are unchanged), so twice the input plus slack is a hard
  // upper bound on the image and put() can treat overflow as a bug, not a case.
  long cap = 2*len + 64;
  unsigned char *wide = (unsigned char *)malloc((size_t)cap);
  if (!wide)
  { free(raw); error(BORG_ERR_LEGACY_FILE,"Out of memory converting a .SIM");
    return 0; }

  //--------------------------------------------------------------------------
  // Try the DLCS layout, then the plain-LCS one.  Accept only a walk that both
  // passes every check and lands exactly on EOF -- a wrong-layout walk
  // desynchronises and will miss one or the other.
  //--------------------------------------------------------------------------
  char firstWhy[200] = "";
  for (int net = 1; net >= 0; --net)
  {
    borgLegacyFreeSim(sim);          // discard a failed previous attempt
    memset(sim,0,sizeof(*sim));

    LC c;
    memset(&c,0,sizeof(c));
    c.in = raw;  c.inLen = len;
    c.out = wide;  c.outCap = cap;
    c.net = net;

    walkSim(&c,sim);

    if (!c.bad && c.inPos != len)
      snprintf(c.why,sizeof(c.why),
               "%ld of %ld bytes accounted for -- the layout does not match",
               c.inPos,len);
    else if (!c.bad)
    {
      sim->network       = net;
      sim->fileSize      = len;
      sim->bytesConsumed = c.inPos;
      sim->modernSize    = c.outLen;
      sim->modern        = wide;     // ownership moves to the caller's struct
      free(raw);
      return 1;
    }

    if (net == 1) snprintf(firstWhy,sizeof(firstWhy),"%s",c.why);
  }

  // The mirror image of the guard in borgLoadSim(): a NATIVE .sim also opens
  // with "2.1\0", so it reaches this reader looking almost plausible.  Its
  // 4-byte message length leaves two zero bytes where this reader expects the
  // seed, which is the tell worth naming rather than leaving the caller with a
  // complaint about masks.  Computed while `raw` is still alive.
  int looksNative = 0;
  if (len > 12 && raw[0]=='2' && raw[1]=='.' && raw[2]=='1' && raw[3]==0)
  {
    long native = (long)raw[4] | ((long)raw[5]<<8) | ((long)raw[6]<<16)
                | ((long)raw[7]<<24);
    if (native >= 1 && native <= 4096 && raw[6] == 0 && raw[7] == 0)
      looksNative = 1;
  }

  borgLegacyFreeSim(sim);
  memset(sim,0,sizeof(*sim));
  free(raw);
  free(wide);

  snprintf(msg,sizeof(msg),
           "'%s' is not a readable 1995 Borg 2.1 .SIM: %s "
           "(the plain-LCS layout was tried too).%s",path,firstWhy,
           looksNative
             ? "  Its first field reads as a plausible message length at 4 bytes,"
               " so this looks like a .sim written by this build -- use \"borg"
               " info\" instead."
             : "");
  error(BORG_ERR_LEGACY_FILE,msg);
  return 0;
}

//----------------------------------------------------------------------------
void borgLegacyFreeSim(borgLegacySim *sim)
{
  if (sim->agent)
  {
    for (int i = 0; i < sim->numAgents; i++)
    {
      borgLegacyAgent *a = &sim->agent[i];
      free(a->x); free(a->y); free(a->dir);
      // The rule strings are allocated one at a time as the walk reads them, so
      // a walk that failed partway leaves a partly-filled array of null pointers
      // -- free() handles those, and the count is always classListLen because the
      // two arrays are allocated together before any string is.
      for (int j = 0; a->cond && j < sim->classListLen; j++)   free(a->cond[j]);
      for (int j = 0; a->action && j < sim->classListLen; j++) free(a->action[j]);
      free(a->cond); free(a->action); free(a->strength);
    }
    free(sim->agent);
    sim->agent = 0;
  }
  free(sim->modern);
  sim->modern = 0;
}

//----------------------------------------------------------------------------
// Did any rule-discovery operator touch the list?  See host/legacy.h.
//
// numTCOs is included even though TCOenabled is one of the documented NOT USED
// parameters: if it ever became nonzero, the assumption behind this function
// would be wrong, and reporting that is better than ignoring the field.
//----------------------------------------------------------------------------
int borgLegacyRulesUnchanged(const borgLegacySim *sim)
{
  for (int i = 0; i < sim->numAgents; i++)
  {
    const borgLegacyAgent *a = &sim->agent[i];
    if (a->numCrossovers || a->numSexual || a->numMutations ||
        a->numCDOs || a->numCEOs || a->numTCOs) return 0;
  }
  return 1;
}

//============================================================================
// taskEnvironment::saveAgentPosBinary() -- TASK.CPP.  numAgents, then the
// maximum number of positions a run could hold (maxCount+1), then per agent its
// actual count followed by that many coords and that many directions.
//============================================================================
int borgLegacyReadPos(const char *path, borgLegacyPos *pos)
{
  char msg[400];

  memset(pos,0,sizeof(*pos));

  FILE *fptr = fopen(path,"rb");
  if (!fptr)
  {
    snprintf(msg,sizeof(msg),"Cannot open '%s' for reading",path);
    error(BORG_ERR_LEGACY_FILE,msg);
    return 0;
  }

  fseek(fptr,0,SEEK_END);
  long len = ftell(fptr);
  fseek(fptr,0,SEEK_SET);

  unsigned char *raw = 0;
  if (len > 0) raw = (unsigned char *)malloc((size_t)len);
  if (!raw || fread(raw,1,(size_t)len,fptr) != (size_t)len)
  {
    fclose(fptr); free(raw);
    snprintf(msg,sizeof(msg),"Could not read all of '%s'",path);
    error(BORG_ERR_LEGACY_FILE,msg);
    return 0;
  }
  fclose(fptr);

  long cap = 2*len + 64;
  unsigned char *wide = (unsigned char *)malloc((size_t)cap);
  if (!wide)
  { free(raw); error(BORG_ERR_LEGACY_FILE,"Out of memory converting a .P");
    return 0; }

  LC c;
  memset(&c,0,sizeof(c));
  c.in = raw;  c.inLen = len;
  c.out = wide;  c.outCap = cap;

  pos->numAgents    = cpInt(&c);
  pos->maxPositions = cpInt(&c);
  checkRange(&c,pos->numAgents,1,1000,"the agent count");
  checkRange(&c,pos->maxPositions,1,1000001,"the maximum position count");

  if (!c.bad)
  {
    pos->agent = (borgLegacyPos::Agent *)
                 calloc((size_t)pos->numAgents,sizeof(borgLegacyPos::Agent));
    if (!pos->agent) fail(&c,"out of memory");
  }

  for (int i = 0; i < pos->numAgents && !c.bad; i++)
  {
    borgLegacyPos::Agent *a = &pos->agent[i];
    a->ptCount = cpInt(&c);
    checkRange(&c,a->ptCount,1,pos->maxPositions,"an agent's position count");
    if (c.bad) break;

    a->x   = (float *)calloc((size_t)a->ptCount,sizeof(float));
    a->y   = (float *)calloc((size_t)a->ptCount,sizeof(float));
    a->dir = (float *)calloc((size_t)a->ptCount,sizeof(float));
    if (!a->x || !a->y || !a->dir) { fail(&c,"out of memory"); break; }

    cpCoordArray(&c,a->ptCount,a->x,a->y);
    cpFloatArray(&c,a->ptCount,a->dir);
  }

  if (!c.bad && c.inPos != len)
    snprintf(c.why,sizeof(c.why),
             "%ld of %ld bytes accounted for -- not a 1995 .P",c.inPos,len);
  else if (!c.bad)
  {
    pos->fileSize      = len;
    pos->bytesConsumed = c.inPos;
    pos->modernSize    = c.outLen;
    pos->modern        = wide;
    free(raw);
    return 1;
  }

  borgLegacyFreePos(pos);
  memset(pos,0,sizeof(*pos));
  free(raw);
  free(wide);

  snprintf(msg,sizeof(msg),"'%s' is not a readable 1995 .P: %s",path,c.why);
  error(BORG_ERR_LEGACY_FILE,msg);
  return 0;
}

//----------------------------------------------------------------------------
void borgLegacyFreePos(borgLegacyPos *pos)
{
  if (pos->agent)
  {
    for (int i = 0; i < pos->numAgents; i++)
    { free(pos->agent[i].x); free(pos->agent[i].y); free(pos->agent[i].dir); }
    free(pos->agent);
    pos->agent = 0;
  }
  free(pos->modern);
  pos->modern = 0;
}

//============================================================================
// Writing the widened images out.
//============================================================================
static int writeImage(const unsigned char *img, long len, const char *path,
                      const char *what)
{
  char msg[400];

  if (!img)
  {
    snprintf(msg,sizeof(msg),"No converted %s to write to '%s'",what,path);
    error(BORG_ERR_LEGACY_FILE,msg);
    return 0;
  }

  remove(path);
  FILE *fptr = fopen(path,"wb");
  if (!fptr)
  {
    snprintf(msg,sizeof(msg),"Cannot open '%s' for writing",path);
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }

  size_t wrote = fwrite(img,1,(size_t)len,fptr);
  int writeFailed = ferror(fptr);
  if (fclose(fptr) != 0 || writeFailed || wrote != (size_t)len)
  {
    snprintf(msg,sizeof(msg),
             "Failed while writing '%s' -- the file is incomplete",path);
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }
  return 1;
}

//----------------------------------------------------------------------------
int borgLegacyWriteModernSim(const borgLegacySim *sim, const char *path)
{
#ifdef NETWORK
  const int buildNet = 1;
#else
  const int buildNet = 0;
#endif

  if (sim->network != buildNet)
  {
    char msg[400];
    snprintf(msg,sizeof(msg),
             "'%s' holds a %s snapshot but this borg is a %s build; "
             "CLAUDE.md: .sim files are not interchangeable between the two. "
             "Rebuild with %s #define NETWORK in CFS.H to convert it.",
             path,
             sim->network ? "DLCS (NETWORK)" : "plain-LCS (no NETWORK)",
             buildNet     ? "DLCS (NETWORK)" : "plain-LCS (no NETWORK)",
             sim->network ? "" : "out");
    error(BORG_ERR_LEGACY_FILE,msg);
    return 0;
  }

  return writeImage(sim->modern,sim->modernSize,path,".SIM");
}

//----------------------------------------------------------------------------
int borgLegacyWriteModernPos(const borgLegacyPos *pos, const char *path)
{
  return writeImage(pos->modern,pos->modernSize,path,".P");
}

//============================================================================
// .RUL inspection.  Text, so no widening -- only the counting that
// classifierList::loadRules() does not do.
//============================================================================
int borgLegacyCheckRules(const char *path, int *agents, int *rules,
                         int *msgLenSeen)
{
  char line[300];

  *agents = *rules = 0;
  *msgLenSeen = 0;

  FILE *fptr = fopen(path,"r");
  if (!fptr)
  {
    char msg[400];
    snprintf(msg,sizeof(msg),"Cannot open '%s' for reading",path);
    error(BORG_ERR_LEGACY_FILE,msg);
    return 0;
  }

  while (fgets(line,sizeof(line),fptr))
  {
    char cond[128], act[128];
    double stren;

    if (line[0] == ';') { ++*agents; continue; }

    if (sscanf(line,"%127s %127s %lf",cond,act,&stren) == 3)
    {
      int n = (int)strlen(cond);
      // Every rule in one file must be the same width, and that width is the
      // message length the file was written at.  A mismatch is what
      // message::operator=(char*) raises error 1 for, one rule at a time.
      if (!*msgLenSeen) *msgLenSeen = n;
      else if (*msgLenSeen != n) *msgLenSeen = -1;
      ++*rules;
    }
  }

  fclose(fptr);
  return 1;
}
