#ifndef TASK_H
#define TASK_H

/*
   This class provides the front-end for all single and multiple-agent
   simulations.  The environmentInterface class and this class represent the
   functionality that would normally be realized in hardware on an actual robot.
   This class provides a "clockTick" function which iterates all LCS agents once
   and then performs network transactions.  The classifierSystem class is
   completely separate from this simulation environment except for the function
   stubs and agent parameters programmed into the environmentInterface.

   This class keeps track of all objects in the playing field and determines
   when objects interact with one another.  Therefore, the environmentInterface
   access this class when preparing to move the agent and when reading the
   agent's sensors.  Each agent treats all other agents as obstacles.  Note
   that we have encapsulated the array of classifierSystem objects in this
   class as a matter of convenience.  Technically, they should probably be
   separate, since the environment interface directly accesses this class.
   For a more in-depth explanation of the animat environment, robot model,
   and sensor and kinematic equations, see the thesis.

   Finally, several of the functions in this class are simply hook functions
   that access functions of the same name in the classifierSystem class for
   a particular agent.  These functions are for use in the user interface
   I built for debugging and evaluating simulations.  Since this user
   interface is extraneous to the research presented in the thesis and since
   it is extremely long, I have omitted it from the thesis code listing.
   The software is available from Dr. John Bay should you wish to use it.
   In any case, hook functions are labelled simply as such, and I refer the
   user to the function descriptions present in the classifierSystem class.

   DISCLAIMER:  This code is not intended to be optimal.  It is merely a
                simulation environment used to test the LCS and DLCS.
                Therefore, I apologize in advance for some of the "hack code"
                you will find here.
*/

// Default parameters (these parameters are NOT static, since there is only
// one instantiation of the taskEnvironment).
#define DEFAULT_NUM_AGENTS      2
#define COND_MASK_NO_BBA        "XXXX00000"
#define COND_MASK_BBA           "XXXXXXXXX"
#define ACTION_MASK_NO_BBA      "00BB00000"
#define ACTION_MASK_BBA         "00BBBBBBB"

#include"cfs.h"

class taskEnvironment
{
    char *condMask, *actionMask;     // Masks for classifier list

    int x1, x2, y1, y2,              // Range of playing field coordinates
        numObst,                     // Number of obstacles
        numAgents,                   // Number of agents
        globalClock,                 // Number of clockTicks
        envType,                     // Environment setup
        *ptCount;                    // Number of points in each agent location array

    coord goalPt,                    // Location of goal
          *obstPts;                  // Array of obstacle locations
    coord huge **agentPts;           // 2D array containing all locations for
                                     //   each agent
    float huge **agentDir;           // 2D array containing all directions for
                                     //   each agent
    float *goalDist, *oldGoalDist,   // Each agent's distance to goal
          *goalDir, *oldGoalDir,     // Each agent's direction to goal
          *obstDist, *oldObstDist,   // Each agent's distance to obstacles
          *obstDir, *oldObstDir;     // Each agent's direction to obstacles

    int *agentFinished;              // Flag array to indicate agent completion

    classifierSystem *LCS;           // LCS agent array

    int oldNumAgents;                // Used by allocateAll and deleteAll only
                                     // NOT saved by saving and loading functions.

    //--------------------------------------------------------------------------
    // Memory allocation functions
    //--------------------------------------------------------------------------
    void deleteAll();
    void allocateAll();

    //--------------------------------------------------------------------------
    // Object location initialization.
    //--------------------------------------------------------------------------
    void initLocations();

    //--------------------------------------------------------------------------
    // Distance and direction calculation functions.
    //--------------------------------------------------------------------------
    void calcGoalDistAndDir(coord & pt, float dir, float *gDist, float *gDir);
    void calcObstDistAndDir(unsigned long ID, coord & pt, float dir,
                            float *oDist, float *oDir);

  public:

    //--------------------------------------------------------------------------
    // General task environment functions.
    //--------------------------------------------------------------------------

    enum { STANDARD=0, CONCAVE_OBST=1 };
    int getEnvType() { return envType; }
    void setEnvType(int type) { envType = type; }

    taskEnvironment() :  envType(STANDARD), numAgents(DEFAULT_NUM_AGENTS)
    { reset(1); }
    ~taskEnvironment() { deleteAll(); }

    void reset(int firstReset = 0);
    void softReset();
    void restoreDefaults();
    int clockTick();

    //--------------------------------------------------------------------------
    // Saving and loading functions.
    //--------------------------------------------------------------------------
    static void saveStatic(FILE *fptr);
    static void loadStatic(FILE *fptr);
    void save(FILE *fptr);
    void load(FILE *fptr);

    //--------------------------------------------------------------------------
    // Agent environmentInterface access.
    //--------------------------------------------------------------------------
    int saveAgentInfo(unsigned long ID, coord & pt, float dir,
                      int *crash, float crashDistance);
    void getDistAndDir(unsigned long ID, float *gDist, float *oldGDist,
                       float *gDir, float *oldGDir, float *oDist,
                       float *oldODist, float *oDir, float *oldODir);
    const coord & getCurrentLoc(unsigned long ID)
    { int index = int(ID) - 1;
      return agentPts[index][ptCount[index]-1];
    }
    float getCurrentDir(unsigned long ID)
    { int index = int(ID) - 1;
      return agentDir[index][ptCount[index]-1];
    }
    void checkGoalSensors(int *GL, int *GR, unsigned long ID,
                          float gRange, float dirRange);
    void checkObstSensors(int *OL, int *OR, unsigned long ID,
                          float oRange, float dirRange);

    //--------------------------------------------------------------------------
    // Access functions for a user interface
    //--------------------------------------------------------------------------
    int readClock(int agent);
    void saveRules(FILE *fptr);
    void loadRules(FILE *fptr);
    int getNumAgents() { return numAgents; }
    int readGlobalClock() { return globalClock; }
    void setNumAgents(int agents) { numAgents = agents; }
    int agentDone(int num);
    void getAxesRanges(int *X1, int *X2, int *Y1, int *Y2)
    { *X1 = x1;  *X2 = x2;  *Y1 = y1;  *Y2 = y2; }
    void getAgent(int num, coord huge **antPoints, float huge **antAngles,
                  int *antCount, float *goalDist, float *goalAngle,
                  float *obstDist, float *obstAngle);
    const message & getAgentSensorMsg(int num);
    void readStats(int num, int *cross, int *sexual, int *mutations, int *CDOs,
                   int *CEOs, int *numCrashes);
    void getObjects(coord **obstaclePoints, int *obstCount, coord *goalPoint);
    int getMsgBoardCurrentSize(int num);
    void saveAgentPositions();
    void loadAgentPositions();
    void saveAgentPosBinary(FILE *fptr);
    void loadAgentPosBinary(FILE *fptr);

#ifdef NETWORK
    void getMsgBoardPosting(int agent, int num, message & msg, int * supplier,
                            unsigned long *ID, double *ruleBid);
    void getClassifier(int agent, int num, message & cond, message & act,
                       double *stren, double *spec, double *sup, double *ruleBid,
                       int *match, int *sel, int *supplier, unsigned long *ID,
                       int *elite, int *ruleNum);
    unsigned long getAgentID(int agent);
#else
    void getMsgBoardPosting(int agent, int num, message & msg, int * supplier,
                            double *ruleBid);
    void getClassifier(int agent, int num, message & cond, message & act,
                       double *stren, double *spec, double *sup, double *ruleBid,
                       int *match, int *sel, int *supplier, int *elite,
                       int *ruleNum);
#endif
};

extern taskEnvironment task;

#endif