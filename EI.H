#ifndef EI_H
#define EI_H

//============================================================================
//  The purpose of the environment interface is to provide user-definable
//  function stubs which allow the user to taylor the classifier system
//  object for a particular use.  The user can build as many functions and
//  variables as desired into this class, but the following functions
//  must exist, and they must perform certain tasks, as outlined below.
//
//  reset()
//
//    This function is used to reset the classifier system.  This function
//    is called whenever the classifierSystem::reset() function is called.
//    Specifically, it should handle memory allocation and deallocation
//    as do all other reset functions in this software.  It should also
//    handle parameter initialization.
//
//  softReset()
//
//    This function is used to reset the classifier system for a new run
//    without resetting the state of the classifier system.  (i.e. the
//    rules from the previous iteration are not reset).  This function is
//    called whenever classifierSystem::softReset() is called.  In a typical
//    environmental situation, this function should restore the environment to
//    its initial conditions.  Note that the user can call softReset whenever
//    he or she is trying to learn a set of rules by repeated iteration.  Also
//    note that NO ALLOCATION IS PERFORMED when classifierSystem::softReset()
//    is called, so no changes to the classifier system parameters are allowed
//    between iterations where softReset is used.
//
//  restoreDefaults()
//
//    This function is used to restore the environment parameters to their
//    default values.  For example, if the user has a dynamically-allocated
//    array of varying size, the function would be called to reset that
//    array length to its default size.  The user should NOT do any memory
//    allocation in this function.  All memory allocation should be done
//    in the reset() function.  This function is provided solely for the
//    purpose of returning parameters to their default values.  This function
//    is called whenever the classifierSystem::restoreDefaults() function
//    is called.  Then, when classifierSystem::reset() is called, the
//    environmentInteface::reset() function will also be called.  See the
//    next function description for more details.
//
//  saveStatic()
//
//     This function is called by classifierList::saveStatic() to save any
//     static member variables that your environment interface may have.
//     Static member variables are used to set parameters which apply to
//     multiple instantiations of the same object.  The classifier system
//     uses static parameters in this way, so that for LCS or DLCS
//     instantiations, the crucial system parameters do not vary
//     between instantiations.  In general, these are the kinds of parameters
//     that would be restored to their default values with the
//     restoreDefaults() function.
//
//  loadStatic()
//
//     Opposite of saveStatic().
//
//  save()
//
//     This function is called by classifierList::save() to save any non-
//     static member variables that your environment interface may have.
//     For example, variables containing information about a particular
//     iteration or environment setup might be saved here.  The classifier
//     system object uses saving and loading to restore the object to the
//     exact state when it was saved.  This allows the user to stop an
//     iteration, save it, and come back to it later.
//
//  load()
//
//     Opposite of save();
//
//  readInput()
//
//    This function is called during a clockTic cycle.  The purpose of the
//    function is to post one or more messages from the environment to the
//    message board.  Depending on your setup, this function will perform
//    different tasks.  If you are using tagged messages that differentiate
//    between environment and internal messages, you must be user not to
//    post environment message over the internal messages from the previous
//    time step.  In addition, you must give the environment messages that
//    you post a supplier number of -1, so that the BBA does not attemp to
//    pay off any suppliers for rules fired by enviornment messages.  However,
//    if you are using substrings, then each message will have an environment
//    part and an internal message part.  In this case, simply put the sensor
//    information into the environment substring of each of the messages on
//    the message board.  Of course you are free to set up any other type of
//    message format you can devise, just be sure that the input and output
//    interfaces work together to get internal messages on the message board
//    by the time you exit this function.  For DLCS operation, messages from
//    the network will also be on the message board at this time step (assuming
//    action passing is enabled), and you are free to treat them just like you
//    treat internal messages.  Remeber, messages must be posted on the
//    message board consecutively, starting at zero, and you must set the
//    actual message size of the message board when you are finished posting.
//
//  postOutput()
//
//    This function is called during a clockTic cycle.  The purpose of this
//    function is to post one or more messages to the environment.  The
//    classifier system calls this function after it has selected messages
//    to post from those eligible classifiers in the classifier list.  It is
//    possible to have more messages for the environment than allowed on the
//    message board at this time, since the classifier system does not
//    distinguish between types of messages when choosing messages to post.
//    Therefore, any extra environment messages should be resolved in this
//    function.  When action passing is enabled, any messages left on the
//    message board after exitting this function will be transmitted over the
//    network according to the transmission rules.  Therefore, if you have
//    different tags for internal, environment, and network messages, you
//    must remove those message you do not wish to have sent over the
//    environment.  Those internal messages would then be put back on the
//    message board by the readInput() function.  Again, if you are using
//    substrings, this is not an issue.  Also, the postOutput function
//    should return a flag indicating whether or not the classifier system
//    iteration should stop.  This flag is returned through the
//    classifierSystem::clockTic() function.
//
//  adjustStengths()
//
//    This is the environment payoff function.  This function is called
//    after postOutput() and is designed to provide environment feedback
//    for the environment messages posted.  What environment feedback
//    entails is adjusting the strength of the classifiers which posted
//    the environment messages.  The strength can be adjusted any way
//    the user sees fit, with the notion of increasing strength for
//    effective rules and decreasing strength for non-effective rules.  The
//    messageBoard::supplier() function can be used to identify which
//    classifier posted the corresponding message board message.  Note that
//    any strengths which end up less than zero are set to zero, and any
//    strengths greater than classifierList::strengthCap are set to
//    strengthCap by the classifier system.
//
//  Finally, a global error catching function called error() must be
//  supplied by the user.  This function is outlined in ei.cpp.
//============================================================================


#include"coord.h"

// Animat-specific static variable initalization
#define DEF_GOAL_RANGE  2000.0
#define DEF_OBST_RANGE  50.0
#define DEF_SENSOR_RANGE 1.57    // approximately PI/2.0
#define DEF_TIME_CONST  15.0
#define DEF_ANT_WIDTH   15.0
#define DEF_GOAL_REWARD  2.0
#define DEF_DIR_REWARD   5.0
#define DEF_OBST_REWARD  0.0
#define DEF_CRASH_PENALTY 5.0

#define PI acos(-1)

double unwrap(double val);
inline double degrees(double ang) { return ang * 180.0 / PI; }

class environmentInterface
{
   // Environment message
   message environMsg;

   // Agent ID
#ifdef NETWORK
   unsigned long agentIDcopy;
#endif

   // Agent settings
   static float goalRange, obstRange,   // Distance robot can "see"
                sensorRange,            // Angular range of sensors
                timeConst,              // Time constant for pos/dir equations
                antWidth;               // Actual width in axis coord's of ant

   // Environment settings
   static float goalReward,dirReward,obstReward,  // Rewards
                crashPenalty;                     // Penalties
   float        crashDistance;                    // Distance to crash

   // Parameters and stats
   int numCrashes,                          // Cumulative number of crashes
       crashFlag,                           // Crash has ocurred.
       goalFlag;                            // Goal reached flag

   //---------------------------------------------------------------------------
   // Internal functions
   //---------------------------------------------------------------------------
   message readSensors();
   void moveAnt(message &msg);

 public:
   //---------------------------------------------------------------------------
   // Reset and initialization functions
   //---------------------------------------------------------------------------
   environmentInterface() {}
   ~environmentInterface() {}

   void reset(int firstReset=0);
   void softReset();
   static void restoreDefaults();

   //---------------------------------------------------------------------------
   // Agent access functions
   //---------------------------------------------------------------------------
#ifdef NETWORK
   void setAgentID(unsigned long newID) { agentIDcopy = newID; }
#endif
   const message & getSensorMsg() { return environMsg; }
   void getAgentStats(int *crashes) { *crashes = numCrashes; }

   //---------------------------------------------------------------------------
   // Static parameter access functions
   //---------------------------------------------------------------------------
   static void getAgentSettings(float *gRange, float *oRange, float *sRange,
                                float *tConst, float *aWidth)
   { *gRange = goalRange; *oRange = obstRange; *sRange = sensorRange;
     *tConst = timeConst; *aWidth = antWidth; }
   static void setAgentSettings(float gRange, float oRange, float sRange,
                                float tConst, float aWidth)
   { goalRange = gRange; obstRange = oRange; sensorRange = sRange;
     timeConst = tConst; antWidth = aWidth; }
   static void getEnvSettings(float *gReward, float *dReward, float *oReward,
                              float *cPenalty)
   { *gReward = goalReward; *dReward = dirReward; *oReward = obstReward;
     *cPenalty = crashPenalty; }
   static void setEnvSettings(float gReward, float dReward, float oReward,
                              float cPenalty)
   { goalReward = gReward; dirReward = dReward; obstReward = oReward;
     crashPenalty = cPenalty; }

   //---------------------------------------------------------------------------
   // Saving and loading functions
   //---------------------------------------------------------------------------
   static void saveStatic(FILE *fptr);
   static void loadStatic(FILE *fptr);
   void save(FILE *fptr);
   void load(FILE *fptr);

   //---------------------------------------------------------------------------
   // functions called on a clock tick
   //---------------------------------------------------------------------------
   void readInput(messageBoard & msgBoard, classifierList & classList);
   void adjustStrengths(messageBoard & msgBoard, classifierList & classList);
   int postOutput(messageBoard & msgBoard, classifierList & classList);
};

// Global error messsage function prototype 
int error(int num, char *str = "");

#endif
