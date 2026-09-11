#include"cfs.h"

// Random number generator count.  Used to keep track of the number of
// getRandom() calls so that the pseudo-random number generator can be
// restored to its previous state upon loading.  See
// classifierSystem::load().
long numRandCalls;

// Static class memeber initialization
unsigned classifierSystem::seed = DEF_SEED;
int classifierSystem::geneticInterval = DEF_GENETIC_INTERVAL;
int classifierSystem::maxCount = DEF_MAX_ITERATIONS;
#ifdef NETWORK
int classifierSystem::networkEnabled = DEF_NET_STATE;
#endif

//============================================================================
// FUNCTION:     reset()
//
// DESCRIPTION:  This function is called to reset the classifier system
//               object.  A reset is done whenever a classifierSystem object
//               is instantiated.  Subsequent resets can be done to start a
//               simulation over, or to set up a new classifier system object
//               when system parameters are changed.
//
//               NOTE:  This reset is the ONLY reset that needs to be called
//                      to reset the classifier system object.  This function
//                      handles all calls to member objects.
//
// I/O:           -->  firstReset  -  (optional) This flag should be set only
//                                    on the first reset of the classifier
//                                    system.
//============================================================================

void classifierSystem::reset(int firstReset)
{
 srand(seed);
 numRandCalls = 0;
 clockCount = 0;
 msgBoard.reset(firstReset);
 classList.reset(firstReset);
 environ.reset(firstReset);
#ifdef NETWORK
 network.reset();
#endif
}

//============================================================================
// FUNCTION:     softReset()
//
// DESCRIPTION:  This function is called to reinitialize the system counters
//               without disturbing the state of the system.  This function is
//               used to start a new iteration using the current rules.  All
//               statistics, rule numbers, iteration counters, etc. are reset.
//               Note that the random number generator is reset, too, since
//               we assume that the state of the classifier system will be
//               sufficiently different that the same string of random numbers
//               will not affect the results.
//
// I/O:          none
//============================================================================

void classifierSystem::softReset()
{
 srand(seed);
 numRandCalls = 0;
 clockCount = 0;

 msgBoard.softReset();
 classList.softReset();
 environ.softReset();
#ifdef NETWORK
 network.softReset();
#endif
}

//============================================================================
// FUNCTION:     restoreDefaults()
//
// DESCRIPTION:  This function is called to restore all defaults in the
//               classifier system and its member objects.  Defaults are
//               defined in the include file for this module.  You MUST
//               call reset after calling this function.
//
//               NOTE:  This is the only restoreDefaults() function that
//                      needs to be called to restore the defaults of the
//                      classifier system objects.  All member object
//                      default functions are called from this function.
//
// I/O:          none
//============================================================================

void classifierSystem::restoreDefaults()
{
  seed = DEF_SEED;
  geneticInterval = DEF_GENETIC_INTERVAL;
  maxCount = DEF_MAX_ITERATIONS;
#ifdef NETWORK
  networkEnabled = DEF_NET_STATE;
#endif

  message::restoreDefaults();
  messageBoard::restoreDefaults();
  classifierList::restoreDefaults();
  environmentInterface::restoreDefaults();
#ifdef NETWORK
  networkInterface::restoreDefaults();
#endif
}

//============================================================================
// FUNCTION:     clockTick()
//
// DESCRIPTION:  This function is called to perform one iteration of the
//               classifier system.  This function should be called only
//               after the classifier system has been correctly setup.  The
//               basic execution cycle is:
//
//               1. Read messages from the input interface and the environment
//                  interface and post them on the message board.  Messages
//                  are read from the network first so that the user can
//                  overwrite parts of the message if substrings have been
//                  defined.
//               2. Check matches with message board
//               3. Calculate bids of matching classifiers
//               4. Select and post message(s)
//               5. Send messages to environment via the output interface and
//                  to network interface
//               6. Perform BBA payoff, environment payoff, and taxes.
//               7. Perform genetics
//
//               Step 7 is actually done at the beginning of the execution
//               cycle because the clock counter is also incremented there.
//               This function is called until the iteration is "finished."
//               An iteration is finished when the environment says so or when
//               'maxCount' is exceeded.
//
// I/O:          <--           1 == done, 0 == not done.
//============================================================================

int classifierSystem::clockTick()
{
  int done;

  // Increment the clock
  ++clockCount;

  // Perform genetic operators on classifier list
  if ( !(clockCount % geneticInterval) )
    classList.genetics();

  // Clear message board if the BBA is off (no internal messages when BBA off)
  if (!isBBAon()) msgBoard.actualSize(0);

  // Read network
#ifdef NETWORK
  network.readNetwork(msgBoard,classList);
#endif

  // Read environment.
  environ.readInput(msgBoard,classList);

  // Get matches with message board.
  classList.getMatches(msgBoard);

  // Calculate bids of matching classifiers
  classList.calcBids();

  // Select and post message(s)
  classList.fillMsgBoard(msgBoard);

  // Send messages to environment.  Message board can be altered here.
  done = environ.postOutput(msgBoard,classList);

  // Send messages to the network
#ifdef NETWORK
  network.postNetwork(clockCount,msgBoard,classList);
#endif

  // Call BBA payoff function (skipped if BBA is not on)
#ifdef NETWORK
  classList.adjustStrengths(network);
#else
  classList.adjustStrengths();
#endif

  // Call environment payoff function
  environ.adjustStrengths(msgBoard,classList);

  // Apply taxes to rules
  classList.taxes();

  // Make sure strengths are between 0 and classifierList::strengthCap
  classList.checkStrengths();

  // Check for maximum interations
  if (clockCount >= maxCount) done = 1;

  return done;
}

//============================================================================
// FUNCTION:     saveStatic()
//
// DESCRIPTION:  This function is called to save the static members of the
//               classifier system.  This function MUST be called before
//               save().  Note that all saveStatic() functions of classifier
//               system members are called here.  In other words, to save
//               a classifier system object, just call saveStatic() and save()
//               in this order and these functions will ensure that all
//               embedded class information is saved.
//
// I/O:           -->  fptr  -  Pointer to an open, binary file.
//============================================================================

void classifierSystem::saveStatic(FILE *fptr)
{
  message::saveStatic(fptr);

  fwrite(&seed,sizeof(unsigned),1,fptr);
  fwrite(&geneticInterval,sizeof(int),1,fptr);
  fwrite(&maxCount,sizeof(int),1,fptr);
#ifdef NETWORK
  fwrite(&networkEnabled,sizeof(int),1,fptr);
#endif

  classifierList::saveStatic(fptr);
  messageBoard::saveStatic(fptr);
  environmentInterface::saveStatic(fptr);
#ifdef NETWORK
  networkInterface::saveStatic(fptr);
#endif
}

//============================================================================
// FUNCTION:     loadStatic()
//
// DESCRIPTION:  This function is called to load the static members of the
//               classifier system.  This function MUST be called before
//               load().  Note that all loadStatic() functions are called
//               for memeber objects.  In other words, just call loadStatic()
//               and load() in this order to load a classifier system object.
//               You do NOT have to call reset().  If you do, the system will
//               be reset to the parameters of the object you loaded.  Normally
//               one would use object saving and loaded to save a simulation
//               in the middle and come back to it later.  You especially do
//               not want to call reset if you intend to continue the
//               simulation.
//
// I/O:           -->  fptr  -  Pointer to an open, binary file.
//============================================================================

void classifierSystem::loadStatic(FILE *fptr)
{
  message::loadStatic(fptr);

  fread(&seed,sizeof(unsigned),1,fptr);
  fread(&geneticInterval,sizeof(int),1,fptr);
  fread(&maxCount,sizeof(int),1,fptr);
#ifdef NETWORK
  fread(&networkEnabled,sizeof(int),1,fptr);
#endif

  classifierList::loadStatic(fptr);
  messageBoard::loadStatic(fptr);
  environmentInterface::loadStatic(fptr);
#ifdef NETWORK
  networkInterface::loadStatic(fptr);
#endif
}

//============================================================================
// FUNCTION:     save()
//
// DESCRIPTION:  This function saves the classifier system parameters.  Note
//               that numRandCalls is generated by the global function
//               getRandom().  This quantity must be saved so that the
//               pseudo-random number generator can be "brought up to speed"
//               when the classifier system data is loaded.  This allows
//               the user to save a simulation and return to it later where
//               he or she left off.  This function must be called AFTER
//               saveStatic().  Note that all save() functions are called
//               for member objects.
//
// I/O:           -->  fptr  -  Pointer to an open, binary file.
//============================================================================

void classifierSystem::save(FILE *fptr)
{
#ifdef NETWORK
  int temp = 1;
  fwrite(&temp,sizeof(int),1,fptr);
  fwrite(&agentID,sizeof(unsigned long),1,fptr);
#else
  int temp = 0;
  fwrite(&temp,sizeof(int),1,fptr);
#endif

  fwrite(&numRandCalls,sizeof(long),1,fptr);
  fwrite(&clockCount,sizeof(int),1,fptr);

  classList.save(fptr);
  msgBoard.save(fptr);
  environ.save(fptr);
#ifdef NETWORK
  network.save(fptr);
#endif
}

//============================================================================
// FUNCTION:     load()
//
// DESCRIPTION:  This function loads the classifier system parameters.  The
//               pseudo-random number generator is "brought up to speed"
//               by calling rand() (standard C library function)
//               numRandCalls times.  This allows the user to resume a
//               simulation where he or she left off.  Note that all load
//               functions are called for memeber objects.
//
// I/O:           -->  fptr  -  Pointer to an open, binary file.
//============================================================================

void classifierSystem::load(FILE *fptr)
{
  int temp;

#ifdef NETWORK
  fread(&temp,sizeof(int),1,fptr);
  if (temp != 1) { error(3,""); return; }
  fread(&agentID,sizeof(unsigned long),1,fptr);
#else
  fread(&temp,sizeof(int),1,fptr);
  if (temp != 0) { error(3,""); return; }
  fwrite(&temp,sizeof(int),1,fptr);
#endif

  fread(&numRandCalls,sizeof(long),1,fptr);
  fread(&clockCount,sizeof(int),1,fptr);

  classList.load(fptr);
  msgBoard.load(fptr);
  environ.load(fptr);
#ifdef NETWORK
  network.load(fptr);
#endif

  // Bring random number generator up-to-date on random number calls
  srand(seed);
  for (long i=0; i<numRandCalls; i++)
    rand();
}

