#ifndef CFS_H
#define CFS_H

//============================================================================
// The classifier system class contains all the necessary classes and
// functions to implement a learning classifier system.  Every instantiation
// of the classifierSystem object is a separate LCS.  The classifier system
// has several common parameters which are shared over multiple
// instantiations.  These are the static member variables, whose defaults
// are listed below.  To use the classifierSystem object, a user must
// program the function stubs contained in the ei module (environment
// interface) as well as build a front-end to repeatedly call the
// classifierSystem::clockTick() function.  See ei.h for more details.
// Note that all code in the modules for this software contains code for both
// the standard LCS and the distributed LCS.  Any portion of code surrounded
// by the
//    #ifdef NETWORK
//      ...
//    #endif
// statements is code specifically for DLCS operation.  To completely remove
// this code from a compile, comment out the #define NETWORK line.
// You can also compile the code for network use and just disable the network
// by setting the 'networkEnabled' parameter to 0, as well.  I have provided
// the define in order to explicitly differentiate between DLCS and LCS code
// and to provide a way to reduce code size if you don't play on distributing
// the LCS.
//
// NOTE:  The disk-saved version for code compiled with the NETWORK define
//        is DIFFERENT than code compiled without it.  You can't load one
//        from the other.  (This isn't too hard to change if you really have
//        a problem with it.  I just didn't see a reason to do it.)
//============================================================================

#define NETWORK

//----------------------------------------------------------------------------
// Standard C libary includes.
//----------------------------------------------------------------------------
#include<math.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>

//----------------------------------------------------------------------------
// Since the new and delete operators are used for all memory allocations,
// it is necessary to use try-catch exception handling to check for
// allocation problems.  The default operation of the new operator in
// Borland C++ 4.0 is to throw the 'xalloc' exception when a new operation
// fails.  Therefore, a catch(xalloc){} block is included after all new
// operations.
//----------------------------------------------------------------------------
#include<except.h>

//----------------------------------------------------------------------------
// 2026 PORT: restores Borland's rand() and its RAND_MAX of 32767.  getRandom()
// below divides by RAND_MAX+1, which overflows int on a modern toolchain and
// makes every draw 0 or negative -- silently, with no diagnostic.  This include
// MUST stay ahead of getRandom().  See compat/borland_rand.h for the details.
//----------------------------------------------------------------------------
#include<borland_rand.h>

//----------------------------------------------------------------------------
// Interface to the pseudo-random number generator.  This inlined function
// must be defined BEFORE including the classifier system components.
//----------------------------------------------------------------------------
extern long numRandCalls;
inline int getRandom(int __num)
{ ++numRandCalls;  return (int)(((long)rand()*__num)/(RAND_MAX+1)); }

//----------------------------------------------------------------------------
// Swap functions are used in sorting.
//----------------------------------------------------------------------------
inline void swap(int & val1, int & val2)
{ int temp = val1;  val1 = val2;  val2 = temp; }
inline void swap(double & val1, double & val2)
{ double temp = val1;  val1 = val2;  val2 = temp; }
inline void swap(unsigned long & val1, unsigned long & val2)
{ unsigned long temp = val1;  val1 = val2;  val2 = temp; }

//----------------------------------------------------------------------------
// Classifier system components.
//----------------------------------------------------------------------------
#ifdef NETWORK
class networkInterface;
#endif

#include"message.h"
#include"msgboard.h"
#include"clsslist.h"
#include"ei.h"

#ifdef NETWORK
  #include"queue.h"
  #include"ni.h"
#endif

//----------------------------------------------------------------------------
// Default classifier system settings.  These settings are used upon
// initialization and when restoreDefaults() is called in the
// classifierSystem class.  Some of the parameters are not supported yet
// but should be initialized to the values shown to provide future
// compatibility.  If you have some favorite settings, put them here.
//----------------------------------------------------------------------------

#define DEF_MSG_LEN          9
#define DEF_SEED             100
#define DEF_MSGBOARD_LEN     1
#define DEF_CLASSLIST_LEN    32
#define DEF_NUM_COND         1            // NOT USED, set to 1
#define DEF_BBA_ENABLED      0
#define DEF_BBA_STYLE        0            // NOT USED, set to 0
#define DEF_ELITISM_ENABLED  0            // NOT USED, set to 0
#define DEF_MAX_ACTIONS_TO_POST 1
#define DEF_CDO_ENABLED      1
#define DEF_CEO_ENABLED      1
#define DEF_TCO_ENABLED      0            // NOT USED, set to 0
#define DEF_BID_CONST        0.125
#define DEF_SEXUAL_PROB      0.7
#define DEF_MUTATION_PROB    0.1
#define DEF_START_STREN      0.0
#define DEF_HEAD_TAX         0.0
#define DEF_BID_TAX          0.0          // NOT USED, set to 0.0
#define DEF_PRODUCER_TAX     0.0          // NOT USED, set to 0.0
#define DEF_PROD_TAX_INTERVAL 1           // NOT USED, set to 1
#define DEF_ELITE_THRESH     0.0          // NOT USED, set to 0.0
#define DEF_STRENGTH_CAP     100.0
#define DEF_BID_CAP          100.0
#define DEF_GENETIC_INTERVAL 4000
#define DEF_MAX_ITERATIONS   4000

#ifdef NETWORK
  #define BROADCAST_ID                0
  #define DEF_NET_STATE               1            // 1 == on, 0 == off

  #define DEF_CLASS_PASSING_STATE     1
  #define DEF_CLASS_TX_INTERVAL       1
  #define DEF_MAX_CLASS_TO_TX         1
  #define DEF_MAX_CLASS_TO_RX         1
  #define DEF_TX_STREN_THRESH         0.8
  #define DEF_RX_STREN_THRESH         0.8

  #define DEF_ACTION_PASSING_STATE    0
  #define DEF_ACTION_TX_INTERVAL      1
  #define DEF_MAX_ACTIONS_TO_TX       1
  #define DEF_MAX_ACTIONS_TO_RX       1
  #define DEF_TX_BID_THRESH           0.8
  #define DEF_RX_BID_THRESH           0.8
#endif

class classifierSystem
{
   //-------------------------------------------------------------------------
   // Settings
   //-------------------------------------------------------------------------
   static int geneticInterval;    // The number of time steps between genetics
   static int maxCount;           // The maximum number of iteration allowed
   static unsigned seed;          // The pseudo-random number generator seed
#ifdef NETWORK
   static int networkEnabled;     // Network on flag
#endif

   //-------------------------------------------------------------------------
   // Classifier system components
   //-------------------------------------------------------------------------
   messageBoard msgBoard;         // Message board
   classifierList classList;      // Classifier List
   environmentInterface environ;  // Environment
#ifdef NETWORK
   networkInterface network;      // Network
#endif

   //-------------------------------------------------------------------------
   // Parameters
   //-------------------------------------------------------------------------
   int clockCount;                // Iteration counter
#ifdef NETWORK
   unsigned long agentID;         // Network identification
#endif

 public:

   //-------------------------------------------------------------------------//
   // Initialization & reset functions
   //-------------------------------------------------------------------------//
#ifdef NETWORK
   classifierSystem() : agentID(0) { reset(1); }
#else
   classifierSystem() { reset(1); }
#endif
   static void restoreDefaults();
   void reset(int firstReset = 0);
   void softReset();
   void initClassifier(int num, char *condStr=0, char *msgStr=0, double stren=0.0)
   { classList.addClassifier(num,condStr,msgStr,stren); }

   //-------------------------------------------------------------------------//
   // Classifier system access functions
   //-------------------------------------------------------------------------//
   int clockTick();
   int readClock() { return clockCount; }
   static int isBBAon()
   { return classifierList::isBBAon(); }
   static int getSeed() { return seed; }
   int getMsgBoardCurrentSize() { return msgBoard.actualSize(); }
   void readStats(int *cross, int *sexual, int *mutations,
                  int *CDOs, int *CEOs)
   { classList.readStats(cross,sexual,mutations,CDOs,CEOs); }
#ifdef NETWORK
   void getMsgBoardPosting(int num, message & msg, int * supplier,
                           unsigned long *ID, double *ruleBid)
   { msgBoard.getMessage(num,msg,supplier,ID,ruleBid); }
   void getClassifier(int num, message & cond, message & act,
                      double *stren, double *spec, double *sup, double *ruleBid,
                      int *match, int *sel, int *supplier, unsigned long *ID,
                      int *elite, int *ruleNum)
   {
    classList.getClassifier(num,cond,act,stren,spec,sup,ruleBid,match,sel,
                            supplier,ID,elite,ruleNum);
   }
   static int isNetworkOn() { return networkEnabled; }
   void setAgentID(unsigned long newID)
   { agentID = newID;
     classList.setAgentID(newID);
     network.setAgentID(newID);
     environ.setAgentID(newID);
   }
   unsigned long getAgentID() { return agentID; }
   int removeFromTxQueue(queueList & qMsg)
   { return network.removeFromTxQueue(qMsg); }
   void addToRxQueue(const queueList & qMsg)
   { network.addToRxQueue(qMsg); }
#else
   void getMsgBoardPosting(int num, message & msg, int * supplier,
                          double *ruleBid)
   { msgBoard.getMessage(num,msg,supplier,ruleBid); }
   void getClassifier(int num, message & cond, message & act,
                      double *stren, double *spec, double *sup, double *ruleBid,
                      int *match, int *sel, int *supplier, int *elite,
                      int *ruleNum)
   {
    classList.getClassifier(num,cond,act,stren,spec,sup,ruleBid,match,sel,
                            supplier,elite,ruleNum);
   }
#endif

   //-------------------------------------------------------------------------//
   // Saving and loading functions
   //-------------------------------------------------------------------------//
   static void saveStatic(FILE *fptr);
   static void loadStatic(FILE *fptr);
   void save(FILE *fptr);
   void load(FILE *fptr);
   void saveRules(FILE *fptr) { classList.saveRules(fptr); }
   void loadRules(FILE *fptr) { classList.loadRules(fptr); }

   //-------------------------------------------------------------------------//
   // Static parameter access functions
   //-------------------------------------------------------------------------//
   static void setMasks(char *cMask, char *aMask)
   { classifierList::setMasks(cMask,aMask); }
#ifdef NETWORK
   static void getSettings(unsigned *sd, int *genInterval, int *max, int *netOn)
   { *sd = seed; *genInterval = geneticInterval; *max = maxCount;
     *netOn = networkEnabled; }
   static void setSettings(unsigned sd, int genInterval, int max, int netOn)
   { seed = sd; geneticInterval = genInterval; maxCount = max;
     networkEnabled = netOn; }
   static void getSettings(unsigned *sd, int *genInterval, int *max, int *netOn,
                           int *msgBoardLength,
                           int *classListLen, int *numCond, int *BBAon,
                           int *BBAtype, int *elitismOn, int *prodTaxInt,
                           int *CDOon, int *CEOon, int *TCOon, int *maxPost,
                           double *bConst, double *crossProb,
                           double *mutateProb, double *startStren,
                           double *headTaxRate, double *bidTaxRate,
                           double *prodTaxRate, double *eliteT,
                           double *strenCap, double *bCap,
                           int *actPass, int *classPass, int *classTxInt,
                           int *maxClassTx, int *maxClassRx, int *actTxInt,
                           int *maxActTx, int *maxActRx, double *TxStren,
                           double *RxStren, double *TxBid, double *RxBid)
   { *sd = seed; *genInterval = geneticInterval; *max = maxCount;
     *netOn = networkEnabled;
     messageBoard::getSettings(msgBoardLength);
     classifierList::getSettings(classListLen,numCond,BBAon,BBAtype,elitismOn,
                                 prodTaxInt,CDOon,CEOon,TCOon,maxPost,bConst,
                                 crossProb,mutateProb,startStren,headTaxRate,
                                 bidTaxRate,prodTaxRate,eliteT,strenCap,bCap);
     networkInterface::getSettings(actPass,classPass,classTxInt,maxClassTx,
                          maxClassRx, actTxInt,maxActTx,maxActRx,TxStren,
                          RxStren, TxBid, RxBid);
   }

   static void setSettings(unsigned sd, int genInterval, int max, int netOn,
                           int msgBoardLength,
                           int classListLen, int numCond, int BBAon,
                           int BBAtype, int elitismOn, int prodTaxInt,
                           int CDOon, int CEOon, int TCOon, int maxPost,
                           double bConst, double crossProb,
                           double mutateProb, double startStren,
                           double headTaxRate, double bidTaxRate,
                           double prodTaxRate, double eliteT,
                           double strenCap, double bCap,
                           int actPass, int classPass, int classTxInt,
                           int maxClassTx, int maxClassRx, int actTxInt,
                           int maxActTx, int maxActRx, double TxStren,
                           double RxStren, double TxBid, double RxBid)
   { seed = sd; geneticInterval = genInterval;  maxCount = max;
     networkEnabled = netOn;
     messageBoard::setSettings(msgBoardLength);
     classifierList::setSettings(classListLen,numCond,BBAon,BBAtype,elitismOn,
                                 prodTaxInt,CDOon,CEOon,TCOon,maxPost,bConst,
                                 crossProb,mutateProb,startStren,headTaxRate,
                                 bidTaxRate,prodTaxRate,eliteT,strenCap,bCap);
     networkInterface::setSettings(actPass,classPass,classTxInt,maxClassTx,
                          maxClassRx, actTxInt,maxActTx,maxActRx,TxStren,
                          RxStren, TxBid,RxBid);
   }
#else
   static void getSettings(unsigned *sd, int *genInterval, int *max)
   { *sd = seed; *genInterval = geneticInterval; *max = maxCount; }
   static void setSettings(unsigned sd, int genInterval, int max)
   { seed = sd; geneticInterval = genInterval; maxCount = max; }
   static void getSettings(unsigned *sd, int *genInterval, int *max,
                           int *msgBoardLength,
                           int *classListLen, int *numCond, int *BBAon,
                           int *BBAtype, int *elitismOn, int *prodTaxInt,
                           int *CDOon, int *CEOon, int *TCOon, int *maxPost,
                           double *bConst, double *crossProb,
                           double *mutateProb, double *startStren,
                           double *headTaxRate, double *bidTaxRate,
                           double *prodTaxRate, double *eliteT,
                           double *strenCap, double *bCap)
   { *sd = seed; *genInterval = geneticInterval; *max = maxCount;
     messageBoard::getSettings(msgBoardLength);
     classifierList::getSettings(classListLen,numCond,BBAon,BBAtype,elitismOn,
                                 prodTaxInt,CDOon,CEOon,TCOon,maxPost,bConst,
                                 crossProb,mutateProb,startStren,headTaxRate,
                                 bidTaxRate,prodTaxRate,eliteT,strenCap,bCap);
   }

   static void setSettings(unsigned sd, int genInterval, int max,
                           int msgBoardLength,
                           int classListLen, int numCond, int BBAon,
                           int BBAtype, int elitismOn, int prodTaxInt,
                           int CDOon, int CEOon, int TCOon, int maxPost,
                           double bConst, double crossProb,
                           double mutateProb, double startStren,
                           double headTaxRate, double bidTaxRate,
                           double prodTaxRate, double eliteT,
                           double strenCap, double bCap)
   { seed = sd; geneticInterval = genInterval;  maxCount = max;
     messageBoard::setSettings(msgBoardLength);
     classifierList::setSettings(classListLen,numCond,BBAon,BBAtype,elitismOn,
                                 prodTaxInt,CDOon,CEOon,TCOon,maxPost,bConst,
                                 crossProb,mutateProb,startStren,headTaxRate,
                                 bidTaxRate,prodTaxRate,eliteT,strenCap,bCap);
   }
#endif

   // Animat-specific functions
   const message & getAgentSensorMsg()
   { return environ.getSensorMsg(); }
   void getAgentStats(int *numCrashes)
   { environ.getAgentStats(numCrashes); }
};

#endif
