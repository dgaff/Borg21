#include"task.h"
#include<values.h>    // for MAXFLOAT

//============================================================================
// FUNCTION:     deleteAll()
//
// DESCRIPTION:  Used to delete all memory allocations.
//
// I/O:          none
//============================================================================

void taskEnvironment::deleteAll()
{
  int i;   // 2026 PORT: hoisted from for-init (Borland scoped it to this block)
  delete [] LCS;
  delete [] obstPts;

  for (i=0; i<oldNumAgents; i++)
    delete [] agentPts[i];
  delete [] agentPts;

  for (i=0; i<oldNumAgents; i++)
    delete [] agentDir[i];
  delete [] agentDir;

  delete [] ptCount;
  delete [] agentFinished;
  delete [] goalDist;
  delete [] oldGoalDist;
  delete [] goalDir;
  delete [] oldGoalDir;
  delete [] obstDist;
  delete [] oldObstDist;
  delete [] obstDir;
  delete [] oldObstDir;
  delete [] condMask;
  delete [] actionMask;
}

//============================================================================
// FUNCTION:     allocateAll
//
// DESCRIPTION:  Used to allocate all arrays in the task environment.
//
// I/O:          none
//============================================================================

void taskEnvironment::allocateAll()
{
  int i;   // 2026 PORT: hoisted from for-init (Borland scoped it to this block)
  unsigned dummy1;
  int dummy2, maxCount;
#ifdef NETWORK
  int dummy3;
  classifierSystem::getSettings(&dummy1, &dummy2, &maxCount, &dummy3);
#else
  classifierSystem::getSettings(&dummy1, &dummy2, &maxCount);
#endif

  try
  {
    obstPts = new coord[numObst];

    agentPts = new coord huge *[numAgents];
    for (i=0; i<numAgents; i++)
      agentPts[i] = new huge coord[maxCount+1];

    agentDir = new float huge *[numAgents];
    for (i=0; i<numAgents; i++)
      agentDir[i] = new huge float[maxCount+1];

    ptCount = new int[numAgents];
    agentFinished = new int[numAgents];
    goalDist = new float[numAgents];
    oldGoalDist = new float[numAgents];
    goalDir = new float[numAgents];
    oldGoalDir = new float[numAgents];
    obstDist = new float[numAgents];
    oldObstDist = new float[numAgents];
    obstDir = new float[numAgents];
    oldObstDir = new float[numAgents];
    condMask = new char[message::size()+1];
    actionMask = new char[message::size()+1];
  }
  catch(xalloc) { error(0,"taskEnvironment::allocateAll()"); }

  // This parameter is saved so that when the user changes the number of
  // agents, the deleteAll() function will know how many agents to delete.
  oldNumAgents = numAgents;
}

//============================================================================
// FUNCTION:     reset()
//
// DESCRIPTION:
//
// This function is called to perform a reset of the task environment and all
// agents in the environment.  Items reset:  Obstacle locations, goal location,
// all LCS objects, all LCS classifier list rules.  Note that LCS objects are
// automatically reset upon instantiation, therefore, reset does not need to
// be called again for them.  Also, because the agentInfo class accesses
// the taskEnvironment class to get obstacle and goal information, the
// obstacle and goal arrays must be allocated and initialized before the LCS
// agents are instantiated.  (Hack code.)
//
// I/O:          --> firstReset  -  (optional) Called on instantiation to
//                                  indicate that allocations have not been
//                                  performed yet.
//============================================================================

void taskEnvironment::reset(int firstReset)
{
#ifdef HALLWAY
  numObst = 1002;
#else
  if (envType == STANDARD) numObst = 400;
  else { numObst = 401; numAgents = 1; }
#endif

  // Delete previous environment arrays.
  if (!firstReset) deleteAll();

  // Allocate space for obstacle locations and multiple agents
  allocateAll();

  // Reset the clockTick counter
  globalClock = 0;

  // Initialize all locations in the playing field
  initLocations();

  // Instantiate the LCS agents.  This MUST occur after the task environment
  // has been setup.
  try
  { LCS = new classifierSystem[numAgents]; }
  catch(xalloc)
  { error(0,"taskEnvironment::reset()"); }

  // Initialize the classifier list masks
#ifdef NETWORK
  if (classifierSystem::isBBAon() || (classifierSystem::isNetworkOn() &&
      networkInterface::isActionPassingOn()))
#else
  if (classifierSystem::isBBAon())
#endif
  {
    strcpy(condMask,COND_MASK_BBA);
    strcpy(actionMask,ACTION_MASK_BBA);
  }
  else
  {
    strcpy(condMask,COND_MASK_NO_BBA);
    strcpy(actionMask,ACTION_MASK_NO_BBA);
  }

  // Set the action and condition masks in the classifier list.  This MUST
  // occur after LCS instantiation if the default masks are to be overridden.
  classifierSystem::setMasks(condMask,actionMask);

  // Initialize the classifier list and give agents unique numbers.  Note that
  // the taskEnvironment assumes that these ID's will not differ from this
  // initialization.
  for (int i=0; i<numAgents; i++)
  {
#ifdef NETWORK
    LCS[i].setAgentID(i+1);
#endif

    int size = classifierList::size();
    for (int j=0; j<size; j++)
      LCS[i].initClassifier(j);
  }
}

//============================================================================
// FUNCTION:     softReset()
//
// DESCRIPTION:  Called to maintain the state of all of the agents while
//               resetting their positions.
//
// I/O:          none
//============================================================================

void taskEnvironment::softReset()
{
  globalClock = 0;
  initLocations();
  for (int i=0; i<numAgents; i++)
    LCS[i].softReset();
}

//============================================================================
// FUNCTION:     restoreDefaults()
//
// DESCRIPTION:  Called to restore the classifierSystem default parameters.
//               Reset must be called after this function.
//
// I/O:          none.
//============================================================================

void taskEnvironment::restoreDefaults()
{
  numAgents = DEFAULT_NUM_AGENTS;
  classifierSystem::restoreDefaults();
}

//============================================================================
// FUNCTION:     clockTick()
//
// DESCRIPTION:  This function is the heart of the simulation environment.
//               The function first calls the clockTick() function for each
//               DLCS agent, and then performs are network transactions.  The
//               network is SIMULATED here and is assumed to be faster than
//               the agents (a resonable assumption given the slow-moving
//               nature of the animat).
//
// I/O:          <--   Returns a 1 when all agents have finished, otherwise,
//                     returns a 0.
//============================================================================

int taskEnvironment::clockTick()
{
  int i;   // 2026 PORT: hoisted from for-init (Borland scoped it to this block)
  int numAgentsFinished = 0;

  // Increment the clock
  ++globalClock;

  // Individual agent clock ticks

  for (i=0; i<numAgents; i++)
  {
    // Clock tic only if agent is not finished.
    if (!agentFinished[i]) agentFinished[i] = LCS[i].clockTick();

    // Check to see if the current agent is done.
    if (agentFinished[i]) ++numAgentsFinished;
  }

#ifdef NETWORK
  queue tempQ;
  queueList tempObject;

  // Network Transactions
  if (classifierSystem::isNetworkOn())
  {
    // Remove messages waiting to be transmitted
    for (i=0; i<numAgents; i++)
      while (LCS[i].removeFromTxQueue(tempObject))
        tempQ.add(tempObject);

    // Send messages to designated destinations, only if agents have not
    // finished.
    while(tempQ.remove(tempObject))
      for (i=0; i<numAgents; i++)
      {
        if (!agentFinished[i])
        {
          unsigned long agentID = LCS[i].getAgentID();
          if (tempObject.dest == agentID ||
              (tempObject.dest == BROADCAST_ID && tempObject.source != agentID))
            LCS[i].addToRxQueue(tempObject);
        }
      }
  }
#endif

  if (numAgentsFinished == numAgents) return 1;
  else return 0;
}

//============================================================================
// Hook function.
//============================================================================

void taskEnvironment::saveStatic(FILE *fptr)
{
  classifierSystem::saveStatic(fptr);
}

//============================================================================
// Hook function.
//============================================================================

void taskEnvironment::loadStatic(FILE *fptr)
{
  classifierSystem::loadStatic(fptr);
}

//============================================================================
// FUNCTION:     save()
//
// DESCRIPTION:  Saves all parameters in the task environment and then calls
//               the save function for each agent.
//
// I/O:          --> fptr - A pointer to an open BINARY file set to write
//============================================================================

void taskEnvironment::save(FILE *fptr)
{
  int i;   // 2026 PORT: hoisted from for-init (Borland scoped it to this block)
  fwrite(&envType,sizeof(int),1,fptr);
  fwrite(&x1,sizeof(int),1,fptr);
  fwrite(&y1,sizeof(int),1,fptr);
  fwrite(&x2,sizeof(int),1,fptr);
  fwrite(&y2,sizeof(int),1,fptr);
  fwrite(&numObst,sizeof(int),1,fptr);
  fwrite(&numAgents,sizeof(int),1,fptr);
  fwrite(&globalClock,sizeof(int),1,fptr);

  fwrite(agentFinished,sizeof(int),numAgents,fptr);
  fwrite(&goalPt,sizeof(coord),1,fptr);
  fwrite(obstPts,sizeof(coord),numObst,fptr);
  fwrite(ptCount,sizeof(int),numAgents,fptr);
  for (i=0; i<numAgents; i++)
  {
    fwrite(agentPts[i],sizeof(coord),ptCount[i],fptr);
    fwrite(agentDir[i],sizeof(float),ptCount[i],fptr);
  }
  fwrite(goalDist,sizeof(float),numAgents,fptr);
  fwrite(oldGoalDist,sizeof(float),numAgents,fptr);
  fwrite(goalDir,sizeof(float),numAgents,fptr);
  fwrite(oldGoalDir,sizeof(float),numAgents,fptr);
  fwrite(obstDist,sizeof(float),numAgents,fptr);
  fwrite(oldObstDist,sizeof(float),numAgents,fptr);
  fwrite(obstDir,sizeof(float),numAgents,fptr);
  fwrite(oldObstDir,sizeof(float),numAgents,fptr);

  fwrite(condMask,sizeof(char),message::size()+1,fptr);
  fwrite(actionMask,sizeof(char),message::size()+1,fptr);

  for (i=0; i<numAgents; i++)
    LCS[i].save(fptr);
}

//============================================================================
// FUNCTION:     load()
//
// DESCRIPTION:  Load all parameters in the task environment and then calls
//               the load function for each agent.
//
// I/O:          --> fptr - A pointer to an open BINARY file set to read
//============================================================================

void taskEnvironment::load(FILE *fptr)
{
  int i;   // 2026 PORT: hoisted from for-init (Borland scoped it to this block)
  fread(&envType,sizeof(int),1,fptr);
  fread(&x1,sizeof(int),1,fptr);
  fread(&y1,sizeof(int),1,fptr);
  fread(&x2,sizeof(int),1,fptr);
  fread(&y2,sizeof(int),1,fptr);
  fread(&numObst,sizeof(int),1,fptr);
  fread(&numAgents,sizeof(int),1,fptr);
  fread(&globalClock,sizeof(int),1,fptr);

  deleteAll();
  allocateAll();

  fread(agentFinished,sizeof(int),numAgents,fptr);
  fread(&goalPt,sizeof(coord),1,fptr);
  fread(obstPts,sizeof(coord),numObst,fptr);
  fread(ptCount,sizeof(int),numAgents,fptr);
  for (i=0; i<numAgents; i++)
  {
    fread(agentPts[i],sizeof(coord),ptCount[i],fptr);
    fread(agentDir[i],sizeof(float),ptCount[i],fptr);
  }
  fread(goalDist,sizeof(float),numAgents,fptr);
  fread(oldGoalDist,sizeof(float),numAgents,fptr);
  fread(goalDir,sizeof(float),numAgents,fptr);
  fread(oldGoalDir,sizeof(float),numAgents,fptr);
  fread(obstDist,sizeof(float),numAgents,fptr);
  fread(oldObstDist,sizeof(float),numAgents,fptr);
  fread(obstDir,sizeof(float),numAgents,fptr);
  fread(oldObstDir,sizeof(float),numAgents,fptr);

  fread(condMask,sizeof(char),message::size()+1,fptr);
  fread(actionMask,sizeof(char),message::size()+1,fptr);

  try
  { LCS = new classifierSystem[numAgents]; }
  catch(xalloc)
  { error(0,"taskEnvironment::load()"); }

  classifierSystem::setMasks(condMask,actionMask);

  for (i=0; i<numAgents; i++)
    LCS[i].load(fptr);
}

//============================================================================
// FUNCTION:     getAgent();
//
// DESCRIPTION:  Gets agent position and direction information.  Also gets
//               obstacle and goal distance and direction information.
//
// I/O:          --> num     -  The agent index to retrieve
//               <-- aPts    -  Pointer to the agent location array
//               <-- aDir    -  Pointer to the agent direction array
//               <-- aCount  -  Pointer to the ptCount variable
//               <-- gDist   -  Pointer to the distance to goal variable
//               <-- gDir    -  Pointer to the direction to goal variable
//               <-- oDist   -  Pointer to the distance to obstacle variable
//               <-- oDir    -  Pointer to the direction to obstacle variable
//============================================================================

void taskEnvironment::getAgent(int num, coord huge **aPts,
                               float huge **aDir, int *aCount,
                               float *gDist, float *gDir,
                               float *oDist, float *oDir)
{
  if (num >= numAgents) error(1,"taskEnvironment::getAgent()");

  *aPts = agentPts[num];
  *aDir = agentDir[num];
  *aCount = ptCount[num];
  *gDist = goalDist[num];
  *gDir = goalDir[num];
  *oDist = obstDist[num];
  *oDir = obstDir[num];
}

//============================================================================
// Hook function.
//============================================================================

const message & taskEnvironment::getAgentSensorMsg(int num)
{
  if (num >= numAgents) error(1,"taskEnvironment::getAgentSensorMsg()");

  return LCS[num].getAgentSensorMsg();
}

//============================================================================
// Hook function.
//============================================================================

void taskEnvironment::readStats(int num, int *cross, int *sexual,
                                int *mutations, int *CDOs, int *CEOs,
                                int *numCrashes)
{
  if (num >= numAgents) error(1,"taskEnvironment::getAgentStats()");

  LCS[num].getAgentStats(numCrashes);
  LCS[num].readStats(cross,sexual,mutations,CDOs,CEOs);
}

//============================================================================
// Hook function.
//============================================================================

int taskEnvironment::agentDone(int num)
{
  if (num >= numAgents) error(1,"taskEnvironment::agentDone()");
  return agentFinished[num];
}

//============================================================================
// FUNCTION:     getObjects()
//
// DESCRIPTION:  Used to get the obstacle location array and the goal location.
//
// I/O:          <-- obstaclePoints - Pointer to the obstacle array
//               <-- obstCount      - number of obstacles
//               <-- goalPoint      - goal location
//============================================================================

void taskEnvironment::getObjects(coord **obstaclePoints, int *obstCount,
                                 coord *goalPoint)
{
  *obstaclePoints = obstPts;
  *obstCount = numObst;
  *goalPoint = goalPt;
}

//============================================================================
// Hook function.
//============================================================================

int taskEnvironment::getMsgBoardCurrentSize(int num)
{
  if (num >= numAgents) error(1,"taskEnvironment::getMsgBoardCurrentSize()");
  return LCS[num].getMsgBoardCurrentSize();
}

//============================================================================
// Hook function.
//============================================================================

#ifdef NETWORK
void taskEnvironment::getMsgBoardPosting(int agent, int num, message & msg,
                                         int * supplier, unsigned long *ID,
                                         double *ruleBid)
{
  if (agent >= numAgents) error(1,"taskEnvironment::getMsgBoardPosting()");
  LCS[agent].getMsgBoardPosting(num,msg,supplier,ID,ruleBid);
}
#else
void taskEnvironment::getMsgBoardPosting(int agent, int num, message & msg,
                                         int * supplier, double *ruleBid)
{
  if (agent >= numAgents) error(1,"taskEnvironment::getMsgBoardPosting()");
  LCS[agent].getMsgBoardPosting(num,msg,supplier,ruleBid);
}
#endif

//============================================================================
// Hook function.
//============================================================================

#ifdef NETWORK
void taskEnvironment::getClassifier(int agent, int num, message & cond,
                   message & act,
                   double *stren, double *spec, double *sup, double *ruleBid,
                   int *match, int *sel, int *supplier, unsigned long *ID,
                   int *elite, int *ruleNum)
{
  if (agent >= numAgents) error(1,"taskEnvironment::getClassifier()");
  LCS[agent].getClassifier(num,cond,act,stren,spec,sup,ruleBid,match,sel,
                           supplier,ID,elite,ruleNum);
}

unsigned long taskEnvironment::getAgentID(int agent)
{
  if (agent >= numAgents) error(1,"taskEnvironment::getAgentID()");
  return LCS[agent].getAgentID();
}

#else
void taskEnvironment::getClassifier(int agent, int num, message & cond,
                   message & act,
                   double *stren, double *spec, double *sup, double *ruleBid,
                   int *match, int *sel, int *supplier, int *elite, int *ruleNum)
{
  if (agent >= numAgents) error(1,"taskEnvironment::getClassifier()");
  LCS[agent].getClassifier(num,cond,act,stren,spec,sup,ruleBid,match,sel,
                           supplier,elite,ruleNum);
}
#endif

//============================================================================
// Hook function.
//============================================================================

int taskEnvironment::readClock(int agent)
{
  if (agent >= numAgents) error(1,"taskEnvironment::readClock()");
  return LCS[agent].readClock();
}

//============================================================================
// FUNCTION:     saveAgentPositions()
//
// DESCRIPTION:  This function saves the agent, obstacle, and goal positions
//               in a space-delimited ASCII file so they can be loaded into
//               a spreadsheet or matlab for display and analysis.
//
// I/O:          none
//============================================================================

void taskEnvironment::saveAgentPositions()
{
  int i;   // 2026 PORT: hoisted from for-init (Borland scoped it to this block)
  char filename[13];
  FILE *fptr;

  for (i=0; i<numAgents; i++)
  {
    int agentCount = ptCount[i];

    sprintf(filename,"agent%d.pos",i+1);
    fptr = fopen(filename,"w");
    for (int j=0; j<agentCount; j++)
      fprintf(fptr,"%10.2f %10.2f %10.2f\n",agentPts[i][j].getX(),
              agentPts[i][j].getY(),agentDir[i][j]);
    fclose(fptr);
  }

  fptr = fopen("obst.pos","w");
  for (i=0; i<numObst; i++)
    fprintf(fptr,"%10.2f %10.2f\n",obstPts[i].getX(),obstPts[i].getY());
  fclose(fptr);

  fptr = fopen("goal.pos","w");
  fprintf(fptr,"%10.2f %10.2f",goalPt.getX(),goalPt.getY());
  fclose(fptr);
}

void taskEnvironment::loadAgentPositions()
{
  char filename[13];
  FILE *fptr;

  for (int i=0; i<numAgents; i++)
  {
    sprintf(filename,"agent%d.pos",i+1);
    fptr = fopen(filename,"r");
    if (!fptr) return;

    float x,y,dir;
    int j = 0;
    fscanf(fptr,"%f %f %f ",&x,&y,&dir);
    while (!feof(fptr))
    {
      agentPts[i][j] = coord(x,y);
      agentDir[i][j++] = dir;
      fscanf(fptr,"%f %f %f ",&x,&y,&dir);
    }
    ptCount[i] = j;

    fclose(fptr);
  }
}

//============================================================================
// Hook function.
//============================================================================

void taskEnvironment::saveRules(FILE *fptr)
{
  for (int i=0; i<numAgents; i++)
  {
#ifdef NETWORK
    fprintf(fptr,"; Agent %ld\n",LCS[i].getAgentID());
#endif
    LCS[i].saveRules(fptr);
  }
}

//============================================================================
// Hook function.
//============================================================================

void taskEnvironment::loadRules(FILE *fptr)
{
  char temp[81];
  for (int i=0; i<numAgents; i++)
  {
    fgets(temp,80,fptr);
    LCS[i].loadRules(fptr);
  }
}

//============================================================================
// FUNCTION:     initLocations()
//
// DESCRIPTION:  used to initialize the locations of all the objects in the
//               playing field.
//
// I/O:          none
//============================================================================

void taskEnvironment::initLocations()
{
  int i;   // 2026 PORT: hoisted from for-init (Borland scoped it to this block)
  // Set the locations of the obstacles and goal
  if (envType == STANDARD)
  {
    // 2026 PORT: hoisted from the for-init below.  Kept inside this block, not
    // at function scope, because the else branch declares its own `double x`
    // -- hoisting further out would leave that one shadowing this one.
    int x;

    x1 = -600;  x2 = 600;  y1 = -600;  y2 = 600;

    for (i = 0, x = -300; i < 200; i++, x++)
      obstPts[i] = coord(x,-300);
    for (i = 200, x = -50; i < 400; i++, x++)
      obstPts[i] = coord(x,0);

    goalPt = coord(300,300);
  }
  else
  {
    x1 = -400;  x2 = 400;  y1 = -300;  y2 = 500;

    double x = -100.0;
    for (i=0; i<401; i++)
    {
      obstPts[i] = coord(x,sqrt(10000.0-x*x));
      x+= .5;
    }

    goalPt = coord(0,200);
  }

  // Set the agent locations and reset the done flags
  for (i=0; i<numAgents; i++)
  {
    if (envType == STANDARD)
      agentPts[i][0] = coord(-400.0 + float(i)*40.0, -450.0);
    else
      agentPts[i][0] = coord(0.0, 0.0);

    agentDir[i][0] = 1.0;
    agentFinished[i] = 0;
    ptCount[i] = 1;
  }

  // Calc goal and obstacle distances.  Must be done after ALL agent positions
  // and directions are initialized and after goal and obstacles are init.
  for (i=0; i<numAgents; i++)
  {
    calcGoalDistAndDir(agentPts[i][0], agentDir[i][0], &goalDist[i], &goalDir[i]);
    calcObstDistAndDir(i+1, agentPts[i][0], agentDir[i][0], &obstDist[i], &obstDir[i]);
  }
}

//============================================================================
// FUNCTION:     calcGoalDistAndDir()
//
// DESCRIPTION:  Determines the distance and angle to the goal from the given
//               point.
//
// I/O:          --> pt    - The reference point for the calculation
//               --> dir   - The reference direction for the calculation
//               <-- gDist - The distance to the goal from the point
//               <-- gDir  - The direction to the goal from the point
//============================================================================

void taskEnvironment::calcGoalDistAndDir(coord & pt, float dir, float *gDist,
                                         float *gDir)
{
  *gDist = distance(pt, goalPt);
  *gDir = unwrap( angle(pt, goalPt) - dir );
}

//============================================================================
// FUNCTION:     calcObstDistAndDir()
//
// DESCRIPTION:  Calculates the distance from the given point to the NEAREST
//               obstacle.  Obstacles can be the fixed obstacles in the
//               obstPts array and can be the other agents.  The agents
//               location and direction are passed into the function becasue
//               this function is called before the agent is actually moved in
//               order to check that the agent does not crash into an obstacle.
//               The agent must not consider itself as an obstacle, however, so
//               it's ID is passed into the function as well.
//
// I/O:          --> ID    - The ID of the current agent
//               --> pt    - The new location of the current agent
//               --> dir   - The new directon of the current agent
//               <-- oDist - The distance to the nearest obstacle
//               <-- oDir  - The direction to the nearest obstacle
//============================================================================

void taskEnvironment::calcObstDistAndDir(unsigned long ID, coord & pt, float dir,
                                         float *oDist, float *oDir)
{
  int i;   // 2026 PORT: hoisted from for-init (Borland scoped it to this block)
  coord closestObstPt;

  *oDist = MAXFLOAT;
  for (i=0; i<numObst; i++)
  {
    float dist = distance(pt,obstPts[i]);
    if (dist < *oDist) { *oDist = dist; closestObstPt = obstPts[i]; }
  }
  for (i=0; i<numAgents; i++)
    if (i != (int(ID)-1))  // skip the agent calling this function
    {
      float dist = distance(pt,agentPts[i][ptCount[i]-1]);
      if (dist < *oDist)
      { *oDist = dist; closestObstPt = agentPts[i][ptCount[i]-1]; }
    }
  *oDir = angle(pt, closestObstPt) - dir;
}

//============================================================================
// FUNCTION:     getDistAndDir()
//
// DESCRIPTION:  Gets the obstacle and goal distance and direction information
//               for the given agent.
//
// I/O:          --> ID        -  The agent's ID
//               <-- gDist     -  Distance to goal
//               <-- oldGDist  -  Distance to goal on previous clock tick
//               <-- gDir      -  Direction to goal
//               <-- oldGDir   -  Direction to goal on previous clock tick
//               <-- oDist     -  Distance to obstacle
//               <-- oldODist  -  Distance to obstacle on previous clock tick
//               <-- oDir      -  Direction to obstacle
//               <-- oldODir   -  Direction to obstacle on previous clock tick
//============================================================================

void taskEnvironment::getDistAndDir(unsigned long ID,
                                    float *gDist, float *oldGDist,
                                    float *gDir, float *oldGDir,
                                    float *oDist, float *oldODist,
                                    float *oDir, float *oldODir)
{
  int index = int(ID)-1;
  *gDist = goalDist[index];  *oldGDist = oldGoalDist[index];
  *gDir = goalDir[index];    *oldGDir = oldGoalDir[index];
  *oDist = obstDist[index];  *oldODist = oldObstDist[index];
  *oDir = obstDir[index];    *oldODir = oldObstDir[index];
}

//============================================================================
// FUNCTION:     saveAgentInfo()
//
// DESCRIPTION:
//
//    The environmentInterface gives this function the desired coordinate for
//    the agent identified by ID.  The taskEnvironment decides whether or not
//    this is a valid location.  The function returns a crash flag and a goal
//    flag.  The function also assumes ID is index+1 as is setup in reset().
//
// I/O:          --> ID            - The ID agent
//               --> pt            - The new point for the agent
//               --> dir           - The new direction for the agent
//               <-- crash         - Crash flag
//               --> crashDistance - The agent's crash distance
//============================================================================

int taskEnvironment::saveAgentInfo(unsigned long ID, coord & pt, float dir,
                                   int *crash, float crashDistance)
{
  int index = int(ID)-1,
      goalFlag = 0;
  float gDist, oDist, dummy;

  // Must be reset each time.
  *crash = 0;

  // Calculate the new goal distance and direction and check to see if goal
  // has been reached
  calcGoalDistAndDir(pt,dir,&gDist,&dummy);
  if (gDist < crashDistance)
    goalFlag = 1;

  // Test code.  The simulation stops when agent gets around goal.
  if (envType == CONCAVE_OBST)
  {
    float x = pt.getX(),  y = pt.getY();
    if ( ((x < -100.0 || x > 100.0) && y > 50.0) ||
         (x >= -100.0 && x <= 100.0 && y > sqrt(10000.0-x*x)) )
      goalFlag = 1;
  }

  // Calculate the new obstacle distance and direction and check to see if
  // agent has crashed into an obstacle only if the goal has not been reached
  calcObstDistAndDir(ID,pt,dir,&oDist,&dummy);
  if (!goalFlag && oDist < crashDistance)
    *crash = 1;

  // Update the agent's direction since agent is allowed to turn, even if
  // it hits an obstacle
  int count = ptCount[index];  // current point to be saved
  agentDir[index][count] = dir;

  // Use previous location if a crash has occured, otherwise update the agent
  // location to the new coordinate.
  if (*crash)
    agentPts[index][count] = agentPts[index][count-1];
  else
    agentPts[index][count] = pt;

  // Store current goal and obstacle distances and angles
  oldGoalDist[index] = goalDist[index];
  oldGoalDir[index] = goalDir[index];
  oldObstDist[index] = obstDist[index];
  oldObstDir[index] = obstDir[index];

  // Calculate the new goal distances and angles based on the new point and
  // direction.
  calcGoalDistAndDir(agentPts[index][count],agentDir[index][count],
                     &goalDist[index],&goalDir[index]);
  calcObstDistAndDir(ID,agentPts[index][count],agentDir[index][count],
                     &obstDist[index],&obstDir[index]);

  // Update the agent's clock
  ++ptCount[index];

  return goalFlag;
}

//============================================================================
// FUNCTION:     checkGoalSensors()
//
// DESCRIPTION:  Determines if the agent specified by ID can see the goal.
//
// I/O:          <-- GL       - The goal left sensor flag
//               <-- GR       - The goal right sensor flag
//               --> ID       - ID of agent to check
//               --> gRange   - The range of the goal sensors (distance)
//               --> dirRange - The angular range of the goal sensors
//============================================================================

void taskEnvironment::checkGoalSensors(int *GL, int *GR, unsigned long ID,
                                       float gRange, float dirRange)
{
  int index = int(ID)-1;
  float gDist = goalDist[index], gDir = goalDir[index];

  *GL = (gDist < gRange) && ((-10.0*PI/180.0) < gDir) && (gDir < dirRange);
  *GR = (gDist < gRange) && ((10.0*PI/180.0) > gDir) && (gDir > -dirRange);
}

//============================================================================
// FUNCTION:     checkObstSensors()
//
// DESCRIPTION:  Determines if the agent can see any obstacles.  This function
//               must check all obstacles, not just the closest one,
//               since we want the agent's sensorts to indicate any obstacles
//               that it sees.
//
// I/O:          <-- OL       - The obstacle left sensor flag
//               <-- OR       - The obstacle right sensor flag
//               --> ID       - ID of agent to check
//               --> oRange   - The range of the obstacle sensors (distance)
//               --> dirRange - The angular range of the obstacle sensors
//============================================================================

void taskEnvironment::checkObstSensors(int *OL, int *OR, unsigned long ID,
                                       float oRange, float dirRange)
{
  int i;   // 2026 PORT: hoisted from for-init (Borland scoped it to this block)
  int index = int(ID)-1;
  float curDir = agentDir[index][ptCount[index]-1];
  coord curLoc = agentPts[index][ptCount[index]-1];

  *OL = *OR = 0;
  for (i=0; i<numObst; i++)
  {
    float ang = unwrap( angle(curLoc, obstPts[i]) - curDir ),
          dist = distance(curLoc, obstPts[i]);

    if ( (dist < oRange) && ((-30.0*PI/180.0) < ang) && (ang < dirRange) )
      *OL = 1;
    if ( (dist < oRange) && ((30.0*PI/180.0) > ang) && (ang > -dirRange) )
      *OR = 1;
  }

  // Must also see if any other agents are in vicinity
  for (i=0; i<numAgents; i++)
    if (i != index)  // skip this agent
    {
      coord pt = agentPts[i][ptCount[i]-1];
      float dir = unwrap(angle(curLoc,pt) - curDir),
            dist = distance(curLoc,pt);

      if ( (dist < oRange) && ((-30.0*PI/180.0) < dir) && (dir < dirRange) )
        *OL = 1;
      if ( (dist < oRange) && ((30.0*PI/180.0) > dir) && (dir > -dirRange) )
        *OR = 1;
    }
}

//============================================================================
// FUNCTION:     saveAgentPosBinary()
//
// DESCRIPTION:  This function saves only the position of the agents in a
//               binary file.  The function was added because it takes up
//               less space that a full classifierSystem save, and it was
//               necessary to reduce disk storage constraints for extremely
//               large batch simulations.
//
// I/O:          --> fptr - Pointer to an open BINARY file set to write
//============================================================================

void taskEnvironment::saveAgentPosBinary(FILE *fptr)
{
  // Determine the maximum number of iterations allowed in a simulation.
  unsigned dummy1;
  int dummy2, maxCount;
#ifdef NETWORK
  int dummy3;
  classifierSystem::getSettings(&dummy1, &dummy2, &maxCount, &dummy3);
#else
  classifierSystem::getSettings(&dummy1, &dummy2, &maxCount);
#endif

  // The maximum number of agent positions is one greater than the max number
  // of iterations since the starting point must be saved.
  ++maxCount;

  // Save the number of agents and the maximum number of positions
  fwrite(&numAgents,sizeof(int),1,fptr);
  fwrite(&maxCount,sizeof(int),1,fptr);

  // Save the actual position count, coordinates, and directions for each agent.
  for (int i=0; i<numAgents; i++)
  {
    fwrite(&ptCount[i],sizeof(int),1,fptr);
    fwrite(agentPts[i],sizeof(coord),ptCount[i],fptr);
    fwrite(agentDir[i],sizeof(float),ptCount[i],fptr);
  }
}

//============================================================================
// FUNCTION:     loadAgentPosBinary()
//
// DESCRIPTION:  This function loads binary files save with the
//               saveAgentPosBinary() function.
//
// I/O:          --> fptr - Pointer to an open BINARY file set to read
//============================================================================

void taskEnvironment::loadAgentPosBinary(FILE *fptr)
{
  // Get the current LCS settings
#ifdef NETWORK
  unsigned seed;
  int geneticInterval, maxCount, netOn;
  classifierSystem::getSettings(&seed, &geneticInterval, &maxCount, &netOn);
#else
  unsigned seed;
  int geneticInterval, maxCount;
  classifierSystem::getSettings(&seed, &geneticInterval, &maxCount);
#endif

  // Read the number of agents and the maximum number of agent locations
  fread(&numAgents,sizeof(int),1,fptr);
  fread(&maxCount,sizeof(int),1,fptr);

  // The Maximum Count in one less than the maximum number of agent positions
  // since the staring point must also be saved.
  --maxCount;

  // Set the new settings and reset.
#ifdef NETWORK
  classifierSystem::setSettings(seed, geneticInterval, maxCount, netOn);
#else
  classifierSystem::setSettings(seed, geneticInterval, maxCount);
#endif
  reset();

  // Load each agent's point counter, position, and direction.
  for (int i=0; i<numAgents; i++)
  {
    fread(&ptCount[i],sizeof(int),1,fptr);
    fread(agentPts[i],sizeof(coord),ptCount[i],fptr);
    fread(agentDir[i],sizeof(float),ptCount[i],fptr);
  }
}
