//============================================================================
// 2026 PORT: this file held the ONLY Windows dependency below Layer 3 -- the
// global error() forwarded to UI.displayError, which dragged <windows.h> and
// the whole Win16 user interface into the simulation core.  That call now goes
// through the host hook in host/host.h, so Layers 1 and 2 link against any
// front end.  #define STRICT, <windows.h> and "ui.h" are no longer needed.
//============================================================================
#include"task.h"
#include<host.h>

//============================================================================
// FUNCTION:     error()
//
// DESCRIPTION:  This function is called by the classifier system whenever
//               there is an error in one of the functions.  The error
//               function can also be used to flag other errors in the
//               user-defined portion of the code.  However, the first ten
//               error numbers are reserved for classifier system errors.
//               They are as follows:
//
//                 0     -  Out of memory error.
//                 1     -  Array out of range error.
//                 2     -  Infinite loop error.
//                 3     -  Incompatible file type
//                 4-9   -  RESERVED for future use.
//
//               Errors 0 through 2 are fatal and the program should
//               terminate when it receives one of these errors.  Errors 1
//               and 2 represent a programming bug, most likely in the
//               environment interface.  Error 2 can also occur if there are
//               more message slots on the message board than there are
//               classifiers (see classiferList::getMatches()).  The
//               name of the function where the error occured is also
//               passed to this function to help facilitate debugging.  The
//               user must at least support these three errors, in addition
//               to any other error traps he or she wishes to add.  Also,
//               a return value is provided, although since errors 0 to 2
//               are fatal, the program must terminate and not return from
//               the error function.  Error 3 is called when the user tries to
//               load an no-network LCS from disk using network-compiled
//               code and vice-versa.  This error is non-fatal.
//
//               The user is, of course, welcome to use
//               these error traps in the environment code.
//
// I/O:           -->  num  -  The error number.  Errors 0 through 9 are
//                             RESERVED for classifier system use.
//                -->  str  -  A string passed by the routine that called the
//                             error function.  For all classifier system
//                             errors, this string contains the name of the
//                             function where the error occured.
//               <--           Return value is not defined for the classifier
//                             system.
//============================================================================

int error(int num,const char *str)
{
 return hostDisplayError(num,str);
}

//============================================================================
// Agent-based stuff
//============================================================================

// Static class memeber initialization
float environmentInterface::goalRange = DEF_GOAL_RANGE;
float environmentInterface::obstRange = DEF_OBST_RANGE;
float environmentInterface::sensorRange = DEF_SENSOR_RANGE;
float environmentInterface::timeConst = DEF_TIME_CONST;
float environmentInterface::antWidth = DEF_ANT_WIDTH;
float environmentInterface::goalReward = DEF_GOAL_REWARD;
float environmentInterface::dirReward = DEF_DIR_REWARD;
float environmentInterface::obstReward = DEF_OBST_REWARD;
float environmentInterface::crashPenalty = DEF_CRASH_PENALTY;

double unwrap(double val)
{
 if (val < 0) { while(val < -PI) val += 2.0*PI; }
 else { while (val > PI) val -= 2.0*PI; }

 return val;
}

// Reset should be called whenever any of the environment parameters are changed.
void environmentInterface::reset(int firstReset)
{
  // not used
  if (firstReset) 1;

  // Set ant parameters
  crashDistance = antWidth;

  // Reset ant location, distances, flags
  softReset();
}

// Called when you want to maintain the state of the classifier system, but
// just reset the agent position, etc.  Allows multiple iterations without
// losing the previous "brain"

void environmentInterface::softReset()
{
  // Reset ant counter and flags
  crashFlag = goalFlag = 0;
  numCrashes = 0;
}

/*
    Used in conjunction with the load functions to just allocate space based
    on the static class parameters.
*/

void environmentInterface::restoreDefaults()
{
  goalRange = DEF_GOAL_RANGE;
  obstRange = DEF_OBST_RANGE;
  sensorRange = DEF_SENSOR_RANGE;
  timeConst = DEF_TIME_CONST;
  antWidth = DEF_ANT_WIDTH;
  goalReward = DEF_GOAL_REWARD;
  dirReward = DEF_DIR_REWARD;
  obstReward = DEF_OBST_REWARD;
  crashPenalty = DEF_CRASH_PENALTY;
}

// Read sensors and put in message : "GL GR OL OR"

message environmentInterface::readSensors()
{
 int GL,GR,OL,OR;

 task.checkGoalSensors(&GL, &GR, agentIDcopy, goalRange, sensorRange);
 task.checkObstSensors(&OL, &OR, agentIDcopy, obstRange, sensorRange);

 message msg;
 int size = message::size();
 msg[0] = GL+'0';
 msg[1] = GR+'0';
 msg[2] = OL+'0';
 msg[3] = OR+'0';
 for (int i=4; i<size; i++)
//   msg[i] = '#';
   msg[i] = '0';

 return msg;
}

// moveAnt() is resposible for updating all ant, goal, and obstacle
// position and info fields in the environment class
//
// last two alleles contain actuator command:
//   00 == stop (no wheels on)
//   01 == left turn (right wheel on)
//   10 == right turn (left wheel on)
//   11 == forward (both wheels on)
// Note:  ant cannot move through an obstacle

void environmentInterface::moveAnt(message &msg)
{
  coord curLoc = task.getCurrentLoc(agentIDcopy);
  float curDir = task.getCurrentDir(agentIDcopy);

  // This wheel model allows robot to reverse its motors
/*
  int leftWheel=0, rightWheel=0,
      msb = (msg[msg.size()-2] == msg.On),
      lsb = (msg[msg.size()-1] == msg.On);

  if (msb && lsb)       { leftWheel = 1;  rightWheel = 1; }
  else if (!msb && lsb) { leftWheel = -1; rightWheel = 1; }
  else if (msb && !lsb) { leftWheel = 1;  rightWheel = -1; }
  else                  { leftWheel = 0;  rightWheel = 0; }
*/

  // This wheel model only allows robot turn motor on or off

  int leftWheel = (msg[2] == '1'),
      rightWheel = (msg[3] == '1');

  coord newPt = curLoc + timeConst / 2.0 * (rightWheel + leftWheel)
                * coord(cos(curDir), sin(curDir));

  float newDir = unwrap(curDir + timeConst/antWidth * (rightWheel - leftWheel));

  // Register new position and direction
  goalFlag = task.saveAgentInfo(agentIDcopy,newPt,newDir,&crashFlag,crashDistance);
  if (crashFlag) ++numCrashes;
}

/*
   When this function is reached, the message board contains messages from
   the previous clock tick (if the BBA is on) and messages from the network
   (if the network is on).  This function reads the sensors and copies the
   sensor string into the first four alleles of each of the messages.

   Note that the environment message contains '#' for all non-sensor alleles.
*/

#pragma argsused
void environmentInterface::readInput(messageBoard & msgBoard,
                                     classifierList & classList)
{
  int i;   // 2026 PORT: hoisted from for-init (Borland scoped it to this block)
 environMsg = readSensors();

#ifdef NETWORK
 int size = msgBoard.actualSize(), *supplier;
 message *msg;
 double *bid;
 unsigned long *ID;

 try
 { supplier = new int[size];
   msg = new message[size];
   bid = new double[size];
   ID = new unsigned long[size];
 }
 catch(xalloc) { error(0,"environmentInterface::readInput()"); }

 if (size == 0)
 {
   msgBoard.postMessage(0,environMsg,-1,agentIDcopy,0.0);
   msgBoard.actualSize(1);
 }
 else
 {
   // Copy the input message board
   for (i=0; i<size; i++)
     msgBoard.getMessage(i,msg[i],&supplier[i],&ID[i],&bid[i]);

   // Merge the first 4 alleles of the environment message into the first 4
   // alleles of the messageboard message.
   for (i=0; i<size; i++)
     for (int j=0; j<4; j++)
       msg[i][j] = environMsg[j];

   // Post the modified input message board
   for (i=0; i<size; i++)
     msgBoard.postMessage(i,msg[i],supplier[i],ID[i],bid[i]);
 }

 delete [] supplier;
 delete [] msg;
 delete [] bid;
 delete [] ID;

#else  // no NETWORK

 int size = msgBoard.actualSize(), *supplier;
 message *msg;
 double *bid;

 try
 { supplier = new int[size];
   msg = new message[size];
   bid = new double[size];
 }
 catch(xalloc) { error(0,"environmentInterface::readInput()"); }

 if (size == 0)
 {
   msgBoard.postMessage(0,environMsg,-1,0.0);
   msgBoard.actualSize(1);
 }
 else
 {
   // Copy the input message board
   for (i=0; i<size; i++)
     msgBoard.getMessage(i,msg[i],&supplier[i],&bid[i]);

   // Merge the first 4 alleles of the environment message into the first 4
   // alleles of the messageboard message.
   for (i=0; i<size; i++)
     for (int j=0; j<4; j++)
       msg[i][j] = environMsg[j];

   // Post the modified input message board
   for (i=0; i<size; i++)
     msgBoard.postMessage(i,msg[i],supplier[i],bid[i]);
 }

 delete [] supplier;
 delete [] msg;
 delete [] bid;

#endif // ifdef NETWORK

}

/*
  When the BBA is enabled, there can be more than one environment message
  posted on the message board.  This function resolves conflicting environment
  messages and moves the agent accordingly.  All non-environment messages
  are then moved to spaces 1 and above, leaving space 0 for environment
  input on the next clock tic.

  Multiple environment messages are resolved by taking the message with the
  highest bid.  If more than one message has the highest bid, the first message
  is used.

  Note that an environment message may not have been posted on this clock
  tic.  If this is the case, the envMsgPosted flag is set to 0 and the agent
  is not moved.  The environment message location will then be overwritten on
  the next clock tic.  This flag is useful for display of the final version
  of the message board.

  The previous paragraph is false.  Now, if no enviornment message is posted,
  the last environment message is used (i.e. the actuators stay in one
  state until they are changed or until the goal is reached)

  The previous two paragraphs are false.  Now, a CEO is performed when no
  environment message is posted.
  ISSUE:  Should the messages which have already been selected be excluded from
          being replaced by the newly-created rule?  Currently: yes.
*/

int environmentInterface::postOutput(messageBoard & msgBoard,
                                     classifierList & classList)
{
  int i;   // 2026 PORT: hoisted from for-init (Borland scoped it to this block)
  int msgBoardLength = msgBoard.actualSize(),
      classListSize = classList.size(),
      *supplier, *eligible;
  message *msg;
  double *bid;
#ifdef NETWORK
  unsigned long *ID;
#endif

  // This can happen if the CDO is not enabled.  We let CDO control rule
  // creation for unmatched messages on the message board.  If CDO is
  // disabled, we assume no rule creation is desired on this time step.
  // We must send a message to the agent, however.  A message of all 0's means
  // don't do anything.  Note that it is still possible to do a CEO when the
  // CDO is disabled, there just have to be mathcing messages first.
  if (msgBoardLength == 0)
  {
    message temp;
    moveAnt(temp);
    return goalFlag;
  }

  try
  {
    supplier = new int[msgBoardLength];
    eligible = new int[classListSize];
    msg = new message[msgBoardLength];
    bid = new double[msgBoardLength];
#ifdef NETWORK
    ID = new unsigned long[msgBoardLength];
#endif
  }
  catch(xalloc)
  { error(0,"agentInfo::postOutput()"); }

  // Copy the message board and tag the environment messages.
  for (i=0; i<msgBoardLength; i++)
  {
#ifdef NETWORK
    msgBoard.getMessage(i,msg[i],&supplier[i],&ID[i],&bid[i]);
#else
    msgBoard.getMessage(i,msg[i],&supplier[i],&bid[i]);
#endif
  }

  // Sort messages from largest to smallest bid
  for (i=msgBoardLength-1; i>=0; i--)
    for (int j=0; j<i; j++)
      if (bid[j] < bid[j+1])
      {
        swap(bid[j],bid[j+1]);
        swap(msg[j],msg[j+1]);
        swap(supplier[j],supplier[j+1]);
#ifdef NETWORK
        swap(ID[j],ID[j+1]);
#endif
      }

  // Preform CEO if rule stren is zero.  msg[0] is replaced with the new action.

  int index = classList.whatIsIndex(supplier[0]);

  if (classList.getStrength(index) == 0.0 && classList.isCEOon())
  {
    for (i=0; i<classListSize; i++) eligible[i] = 1;

    index = classList.CEO(index,eligible);

    message cond, action;
    double stren, spec, support, ruleBid;
    int match, sel, sup, number, elite;

#ifdef NETWORK
    unsigned long supplierID;
    classList.getClassifier(index,cond,action,&stren,&spec,&support,&ruleBid,&match,
                            &sel,&sup,&supplierID,&elite,&number);
#else
    // Doug note 2026: there was a typo &suppport here for this compile branch. I'm not
    // sure if I actually ran this branch. Obviously it would not have compiled.
    classList.getClassifier(index,cond,action,&stren,&spec,&support,&ruleBid,&match,
                            &sel,&sup,&elite,&number);
#endif
    msg[0] = action;
    bid[0] = ruleBid;
    supplier[0] = number;
#ifdef NETWORK
    ID[0] = agentIDcopy;
#endif
  }

  // Use the first message (highest strength or CEO rule) for agent action.
  moveAnt(msg[0]);

#ifdef NETWORK
  for (i=0; i<msgBoardLength; i++)
    msgBoard.postMessage(i,msg[i],supplier[i],ID[i],bid[i]);
#else
  for (i=0; i<msgBoardLength; i++)
    msgBoard.postMessage(i,msg[i],supplier[i],bid[i]);
#endif

  delete [] msg;
  delete [] supplier;
  delete [] bid;
  delete [] eligible;
#ifdef NETWORK
  delete [] ID;
#endif

  return goalFlag;
}

/*
    Obstacle direction is included, although it is not currently used.
*/

void environmentInterface::adjustStrengths(messageBoard & msgBoard,
                                  classifierList & classList)
{
 float payoff = 0.0, goalDist, oldGoalDist, goalDir, oldGoalDir, obstDist,
       oldObstDist, obstDir, oldObstDir;

 task.getDistAndDir(agentIDcopy, &goalDist, &oldGoalDist, &goalDir, &oldGoalDir,
                    &obstDist, &oldObstDist, &obstDir, &oldObstDir);

 if (goalDist < oldGoalDist)
   payoff += goalReward;
 else
   payoff -= 0.5 * goalReward;

 if (fabs(goalDir) < fabs(oldGoalDir))
   payoff += dirReward;
 else
   payoff -= 0.5 * dirReward;

 if (obstDist > oldObstDist)
   payoff += obstReward;
 else
   payoff -= 0.5 * obstReward;

 if (crashFlag)
   payoff -= crashPenalty;

 message temp1; double temp2;
 int supplier;
#ifdef NETWORK
 unsigned long ID;
 msgBoard.getMessage(0,temp1,&supplier,&ID,&temp2);
#else
 msgBoard.getMessage(0,temp1,&supplier,&temp2);
#endif

 classList.payoffRule(supplier, payoff);
}

/*
   Saves all static parameters.
*/

void environmentInterface::saveStatic(FILE *fptr)
{
  // Agent settings
  fwrite(&goalRange,sizeof(float),1,fptr);
  fwrite(&obstRange,sizeof(float),1,fptr);
  fwrite(&sensorRange,sizeof(float),1,fptr);
  fwrite(&timeConst,sizeof(float),1,fptr);
  fwrite(&antWidth,sizeof(float),1,fptr);

  // Environment settings
  fwrite(&goalReward,sizeof(float),1,fptr);
  fwrite(&dirReward,sizeof(float),1,fptr);
  fwrite(&obstReward,sizeof(float),1,fptr);
  fwrite(&crashPenalty,sizeof(float),1,fptr);
}

void environmentInterface::loadStatic(FILE *fptr)
{
  // Agent settings
  fread(&goalRange,sizeof(float),1,fptr);
  fread(&obstRange,sizeof(float),1,fptr);
  fread(&sensorRange,sizeof(float),1,fptr);
  fread(&timeConst,sizeof(float),1,fptr);
  fread(&antWidth,sizeof(float),1,fptr);

  // Environment settings
  fread(&goalReward,sizeof(float),1,fptr);
  fread(&dirReward,sizeof(float),1,fptr);
  fread(&obstReward,sizeof(float),1,fptr);
  fread(&crashPenalty,sizeof(float),1,fptr);
}

/*
   This function saves all of the parameters in the environment class.  They
   can then be recalled with the load() function.
*/

void environmentInterface::save(FILE *fptr)
{
  // Environment message
  environMsg.save(fptr);

  // Environment settings
  fwrite(&crashDistance,sizeof(float),1,fptr);

  // Parameters and stats
  fwrite(&agentIDcopy,sizeof(long),1,fptr);
  fwrite(&numCrashes,sizeof(int),1,fptr);
  fwrite(&crashFlag,sizeof(int),1,fptr);
  fwrite(&goalFlag,sizeof(int),1,fptr);
}

void environmentInterface::load(FILE *fptr)
{
  // Environment message
  environMsg.load(fptr);

  // Environment settings
  fread(&crashDistance,sizeof(float),1,fptr);

  // Parameters and stats
  fread(&agentIDcopy,sizeof(long),1,fptr);
  fread(&numCrashes,sizeof(int),1,fptr);
  fread(&crashFlag,sizeof(int),1,fptr);
  fread(&goalFlag,sizeof(int),1,fptr);
}

