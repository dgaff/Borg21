#include"cfs.h"

// Static class memeber initialization
int classifierList::length = DEF_CLASSLIST_LEN;
int classifierList::numConditions = DEF_NUM_COND;
int classifierList::BBAenabled = DEF_BBA_ENABLED;
int classifierList::BBAstyle = DEF_BBA_STYLE;
int classifierList::elitismEnabled = DEF_ELITISM_ENABLED;
int classifierList::producerTaxInterval = DEF_PROD_TAX_INTERVAL;
int classifierList::maxActionsToPost = DEF_MAX_ACTIONS_TO_POST;
int classifierList::CDOenabled = DEF_CDO_ENABLED;
int classifierList::CEOenabled = DEF_CEO_ENABLED;
int classifierList::TCOenabled = DEF_TCO_ENABLED;

double classifierList::bidConstant = DEF_BID_CONST;
double classifierList::sexualProb = DEF_SEXUAL_PROB;
double classifierList::mutationProb = DEF_MUTATION_PROB;
double classifierList::startStrength = DEF_START_STREN;
double classifierList::headTax = DEF_HEAD_TAX;
double classifierList::bidTax = DEF_BID_TAX;
double classifierList::producerTax = DEF_PRODUCER_TAX;
double classifierList::eliteThresh = DEF_ELITE_THRESH;
double classifierList::strengthCap = DEF_STRENGTH_CAP;
double classifierList::bidCap = DEF_BID_CAP;
char * classifierList::condMask = 0;        // MUST be set to 0!
char * classifierList::actionMask = 0;      // MUST be set to 0!

//============================================================================
// FUNCTION:     deleteAll()
//
// DESCRIPTION:  Deletes all allocated arrays, except the condition and
//               action masks.  See "setMasks()" for informtion on these two
//               arrays.
//
// I/O:          none
//============================================================================

void classifierList::deleteAll()
{
  delete [] conditionMsg;
  delete [] actionMsg;
  delete [] matchFlag;
  delete [] selected;
  delete [] bid;
  delete [] specificity;
  delete [] strength;
  delete [] support;
  delete [] number;
  delete [] supplierSet;
  delete [] tempSupplierSet;
  delete [] eliteSet;
#ifdef NETWORK
  delete [] supplierID;
  delete [] tempSupplierID;
#endif
}

//============================================================================
// FUNCTION:     allocateAll()
//
// DESCRIPTION:  Allocates all arrays, except for the condition and action
//               masks.  See "setMasks()" for information on these two
//               arrays.  Note that tempSupplierSet is a temporary array
//               used to store the suppliers of classifiers which are
//               elibigle to post at a given time step.  supplierSet is
//               updated only for those rules which are selected to post.
//               Therefore, to trace chains at any given timestep, you MUST
//               use supplierSet and NOT tempSupplierSet.  Also, the
//               supplierSet array holds an index not the unique classifier
//               number stored in 'number'.
//
// I/O:          none
//============================================================================

void classifierList::allocateAll()
{
  try
  {
    conditionMsg = new message[length];
    actionMsg = new message[length];
    matchFlag = new int[length];
    selected = new int[length];
    bid = new double[length];
    specificity = new double[length];
    strength = new double[length];
    support = new double[length];
    number = new int[length];
    supplierSet = new int[length];
    tempSupplierSet = new int[length];
    eliteSet = new int[length];
#ifdef NETWORK
    supplierID = new unsigned long[length];
    tempSupplierID = new unsigned long[length];
#endif
  }
  catch(xalloc) { error(0,"classifierList::allocateAll()"); }
}

//============================================================================
// FUNCTION:     reset()
//
// DESCRIPTION:  This function is called to allocate or re-allocate all
//               arrays in the classifier list.  Array values and statistics
//               variables are also initialized here.  Note that for
//               supplierSet, -2 indicates no support, and -1 indicates
//               environment support.  Support numbers >= 0 are reserved for
//               use by classifierList for keeping tracking of rule to rule
//               support.
//
// I/O:          --> firstReset - Set to 1 only on the first time this function
//                                is called.  (The default is 0).
//============================================================================

void classifierList::reset(int firstReset)
{
  // Delete the old allocations if nec.
  if (!firstReset)
    deleteAll();

  // Allocate classifier arrays
  allocateAll();

  // Initialize classifier parameters
  for (int i=0; i<length; i++)
  {
    strength[i] = startStrength;
    bid[i] = 0.0;
    support[i] = 0.0;
    specificity[i] = 0.0;
    matchFlag[i] = 0;
    selected[i] = 0;
    number[i] = i;
    supplierSet[i] = -2;  // -2 means no support
    eliteSet[i] = 0;
#ifdef NETWORK
    supplierID[i] = 0;
    tempSupplierID[i] = 0;
#endif
  }

  // Reset masks to default of all X's
  
  int msgSize = conditionMsg->size();
  char *mask;
  try { mask = new char[msgSize+1]; }
  catch(xalloc) { error(0,"classifierList.reset()"); }

  for (i=0; i<msgSize; i++) mask[i] = 'X';  mask[i] = '\0';
  setMasks(mask,mask);

  delete [] mask;

  // Reset stat variables
  numCrossovers = numSexual = numMutations = numCDOs = 0;
  numCEOs = numTCOs = 0;
  numClassifiers = 0;  // changes as rules are added
}
//============================================================================
// FUNCTION:     softReset
//
// DESCRIPTION:  Called by the classifier system softReset function to
//               reinitialize the system counters without disturbing the
//               state of the system.  This function is used to start a
//               new iteration using the current rules.
//
// I/O:          none
//============================================================================

void classifierList::softReset()
{
  // Reset the unique classifier numbers
  for (int i=0; i<length; i++)
    number[i] = i;

  // Reset stat variables
  numCrossovers = numSexual = numMutations = numCDOs = 0;
  numCEOs = numTCOs = 0;
  numClassifiers = length;
}

//============================================================================
// FUNCTION:     restoreDefaults()
//
// DESCRIPTION:  Restores the static memebers to their default settings (those
//               used on program startup).  Reset must be called after this
//               function.
//
// I/O:          none
//============================================================================

void classifierList::restoreDefaults()
{
  BBAstyle = DEF_BBA_STYLE;
  length = DEF_CLASSLIST_LEN;
  numConditions = DEF_NUM_COND;
  BBAenabled = DEF_BBA_ENABLED;
  elitismEnabled = DEF_ELITISM_ENABLED;
  producerTaxInterval = DEF_PROD_TAX_INTERVAL;
  CDOenabled = DEF_CDO_ENABLED;
  CEOenabled = DEF_CEO_ENABLED;
  TCOenabled = DEF_TCO_ENABLED;
  maxActionsToPost = DEF_MAX_ACTIONS_TO_POST;
  bidConstant = DEF_BID_CONST;
  sexualProb = DEF_SEXUAL_PROB;
  mutationProb = DEF_MUTATION_PROB;
  startStrength = DEF_START_STREN;
  headTax = DEF_HEAD_TAX;
  bidTax = DEF_BID_TAX;
  producerTax = DEF_PRODUCER_TAX;
  eliteThresh = DEF_ELITE_THRESH;
  strengthCap = DEF_STRENGTH_CAP;
  bidCap = DEF_BID_CAP;
}
//============================================================================
// FUNCTION:     setMasks()
//
// DESCRIPTION:  Condition and action message masks are supplied to allow
//               the user to mask alleles in each message.  The user has
//               the option of setting an allele to a particular value or
//               controlling the types of alleles that can appear in a
//               particular message location.  The allowable masking
//               characters are:
//
//               0  -  The classifierList will always put a zero in this
//                     location.
//               1  -  A one will always appear in this location.
//               #  -  A don't care will always appear in this location.
//               X  -  A 0, 1, or # can appear in this location.
//               B  -  A 0 or 1 can appear in this location.
//
//               Therefore, a mask of "01XXXX" would force the first allele
//               to be 0 always, the second allele to be 1 always, and the
//               last four alleles to be 0, 1, or #.  NOTE:  The "pass-
//               through" function in the action string is NOT supported in
//               this software.  The environment interface must perform
//               pass-through, if so desired.
//
//               Also, the condition and action masks must be allocated and
//               freed separately because they are static class variables.
//               They were made static so that multiple instantiations of
//               the classifierList would have the same mask.  Therefore,
//               it is ESSENTIAL that the condMask and actionMask pointers
//               be set to zero at their point of definition.  This routine
//               checks for a zero pointer before deleting any previously
//               allocated space.
//
// I/O:          --> cMask  -  The condition message mask.
//               --> aMask  -  The action message mask.
//============================================================================

void classifierList::setMasks(char *cMask, char *aMask)
{
  // Delete only if not the first call to setMasks()
  if (condMask != 0) delete [] condMask;
  if (actionMask != 0) delete [] actionMask;

  message dummy;
  int size = dummy.size();

  if (strlen(cMask) != size || strlen(aMask) != size) error(1,"setMasks()");

  try
  { condMask = new char[size+1];
    actionMask = new char[size+1];
  }
  catch(xalloc) { error(0,"setMasks()"); }

  strcpy(condMask,cMask);
  strcpy(actionMask,aMask);
}

//============================================================================
// FUNCTION:     setClassifier()
//
// DESCRIPTION:  Sets the rule whose index is num to have the given condition,
//               action, and strength.  All other parameters are reset.  This
//               function is called from the addClassifier functions and is
//               therefore private.
//
// I/O:          --> num    - The index of the classifier to change
//               --> cond   - The new condition word
//               --> action - The new action word
//               --> stren  - The new strength
//============================================================================

void classifierList::setClassifier(int num, const message & cond,
                                   const message & action, double stren)
{
  if (num > length) error(1,"classifierList::setClassifier1()");

  conditionMsg[num] = cond;
  actionMsg[num] = action;
  strength[num] = stren;

  bid[num] = support[num] = specificity[num] = 0.0;
  matchFlag[num] = selected[num] = eliteSet[num] = 0;
  supplierSet[num] = -2;  // -2 means no support
#ifdef NETWORK
  supplierID[num] = agentIDcopy;
#endif
}

//============================================================================
// FUNCTION:    addClassifier()
//
// DESCRIPTION:
//    This function is used to add a new rule to the classifier list.  The
//    rule is placed in the 'num' slot (array index).  The new rule is given a
//    new unique number.  If the function is called with only a number, the
//    condition and action words are set to random values based on the masks.
//
// I/O:          --> num       - The index of the classifier to change
//               --> condStr   - (optional) The new condition word
//               --> actionStr - (optional) The new action word
//               --> stren     - (optional) The new strength
//============================================================================

void classifierList::addClassifier(int num, char *condStr, char *actionStr,
                                   double stren)
{
  if (num > length) error(1,"classifierList::addClassifier2()");

  message cond,action;

  if (condStr == 0)
  {
    cond = createRandomMessage(condMask),
    action = createRandomMessage(actionMask);
    stren = startStrength;
  }
  else
  {
    cond = condStr;
    action = actionStr;
  }

  setClassifier(num,cond,action,stren);

  number[num] = numClassifiers++;
}

//============================================================================
// FUNCTION:     addClassifier()
//
// DESCRIPTION:  Same as previous except that the condition and action words
//               are messages instead of strings.
//
// I/O:          --> num    - The index of the classifier to change
//               --> cond   - The new condition word
//               --> act    - The new action word
//               --> stren  - The new strength
//============================================================================

void classifierList::addClassifier(int num, const message & cond,
                                   const message & act, double stren)
{
  if (num > length) error(1,"classifierList::addClassifier1()");
  setClassifier(num,cond,act,stren);
  number[num] = numClassifiers++;
}

//============================================================================
// FUNCTION:     createRandomMessage()
//
// DESCRIPTION:  Creates a randomly generated message with the given mask.
//               See setMasks() function for info on masking.  A mask size
//               that differs from the current message size will cause an
//               error.
//
// I/O:           --> mask - The mask string.
//               <--       - Returns the randomly generated message.
//============================================================================

message classifierList::createRandomMessage(char *mask)
{
  int size = message::size();
  message msg = mask;

  for (int i=0; i<size; i++)
  {
    if (msg[i] == 'X') msg[i] = randomTernary();
    else if (msg[i] == 'B') msg[i] = randomBinary();
  }

  return msg;
}

//============================================================================
// FUNCTION:     pickWeakRule
//
// DESCRIPTION:  Probabilistically picks a weak rule from the classifier list
//               based on the eligible array.  A one in the eligible array
//               indicates that a rule is eligible for selection.  If elitism
//               is on, all elite rules are removed from eligibility.
//               The strengths of the eligible rules are reversed to give the
//               weakest rules the highest probability of being selected.
//
// I/O:           --> eligible - array indicating which rules are eligible
//                               for selection.
//               <--           - returns the INDEX of the rule selected.
//============================================================================

int classifierList::pickWeakRule(int *eligible)
{
  double maxStren = 0.0, *tempStren;

  try { tempStren = new double[length]; }
  catch(xalloc) { error(0,"classifierList::pickWeakRule()"); }

  // Determine the maximum strength of the eligible classifiers
  for (int i=0; i<length; i++)
  {
    if (elitismEnabled && eliteSet[i]) eligible[i] = 0;
    if (eligible[i] && strength[i] > maxStren) maxStren = strength[i];
  }

  // Reverse the strengths so a weak rule will be picked by roulette()
  for (i=0; i<length; i++)  tempStren[i] = maxStren - strength[i];
  int loc = roulette(tempStren,eligible,length);

  delete [] tempStren;

  return loc;
}

//============================================================================
// FUNCTION:     roulette()
//
// DESCRIPTION:  This function probabilistically picks a value from the
//               bids array.  Larger values have a higher probability of
//               being selected.  All values MUST be >= 0.  The function
//               uses a cumulative sum of the normalized bids along with the
//               uniformly distributed pseudo-random number generator to pick
//               a value.  Values are only selected from those elements tagged
//               with a one in the parallel eligible array.  The index of the
//               selected value is returned.  Note that if all of the
//               values are zero, a value is randomly picked from those
//               tagged as eligible.  If no values are tagged eligible,
//               a value is picked from all the given values.
//
// I/O:          -->  bids      -  The list of values from which a value is
//                                 to be selected.
//               -->  eligible  -  A parallel array to bids which tags values
//                                 to be used in selection.  1 == use this
//                                 value.
//               -->  maxIndex  -  The total number of elements in the
//                                 bids array.
//               <--               The INDEX of the selected value.
//============================================================================

int classifierList::roulette(double *bids, int *eligible, int maxIndex)
{
  double *eligibleBids;
  int *indexSet, choice;

  try
  { eligibleBids = new double[maxIndex];
    indexSet = new int[maxIndex];
  }
  catch(xalloc) { error(0,"classifierList::roulette()"); }

  // Copy eligible bids into new array and sum the eligible bids
  int num=0;
  double sum = 0.0;
  for (int i=0; i<maxIndex; i++)
    if (eligible[i])
    {
      eligibleBids[num] = bids[i];
      sum += bids[i];
      indexSet[num++] = i;
    }

  // Make selection
  if (sum != 0.0)     // Eligible bids with nonzero values
  {
    // Normalize all bids and form cumulative sum
    for (i=0; i<num; i++)
    {
      if (i==0) eligibleBids[i] /= sum;
      else eligibleBids[i] = eligibleBids[i] / sum + eligibleBids[i-1];
    }

    // Pick the first classifier whose normalized bid is greater than the
    // generated random number.

    double randNum = double(getRandom(100)) / 100.0;

    choice = 0;
    while(choice<num && eligibleBids[choice] <= randNum) choice++;

    choice = indexSet[choice];
  }
  else if (num > 0)   // Sum is zero, but some values are still eligible
    choice = indexSet[getRandom(num)];
  else                // No values are eligible
    choice = getRandom(maxIndex);

  delete [] eligibleBids;
  delete [] indexSet;

  return choice;
}

//============================================================================
// FUNCTION:     crossOver()
//
// DESCRIPTION:  Crossover is a genetic operator which is designed to
//               reproduce rules.  Rules can be reproduced either sexually
//               or asexually.  Crossover is performed amoung classifiers
//               with the highest strengths.  Sexual crossover occurs with
//               probability 'sexualProb', and two new rules are created.
//               One of the "children" is picked (prob. 50%) and the child
//               gets 1/3 of the strength of each of its parents.  Asexual
//               crossover occurs with probability '1-sexualProb', and a new
//               rule is not created, just copied.  The strength is split
//               between the parent and the child.  The point of crossover
//               for sexual crossover is picked randomly and can be anywhere
//               in the condition or action string of the classifiers chosen
//               to be parents.  Also, once a parent is chosen, it cannot be
//               chosen again to be the second parent or to be the child.
//               i.e. it is removed from the eligibility set before
//               roulette() is called.
//
//               Note that crossover results in the creation of a new rule.
//               As such, this new rule does not inherit the support values
//               of its parents.  Instead, supplierSet is assigned to -2,
//               indicating the creation of a new rule.
//
// I/O:          none
//============================================================================

void classifierList::crossOver()
{
  int *eligible;

  try { eligible = new int[length]; }
  catch(xalloc) { error(0,"classifierList::crossOver()"); }

  // Increment the crossover counter
  ++numCrossovers;

  // All rules are eligible for first selection
  for (int i=0; i<length; i++) eligible[i] = 1;

  // Determine type of crossover
  if ( (double(getRandom(100))/100.0) < sexualProb )  // Sexual Crossover
  {
    int parent1, parent2;
    message condition1, condition2, action1, action2;

    // Increment sexual crossover counter
    ++numSexual;

    // Pick Parents
    parent1 = roulette(strength,eligible,length);
    eligible[parent1] = 0;
    parent2 = roulette(strength,eligible,length);
    eligible[parent2] = 0;

    // Perform crossover

    condition1 = conditionMsg[parent1];
    condition2 = conditionMsg[parent2];
    action1 = actionMsg[parent1];
    action2 = actionMsg[parent2];

    int size = conditionMsg->size(),
        xOverPt = getRandom(2*size);

    if (xOverPt < size)  // crossover in condition part
      for (i=xOverPt; i<size; i++)
      {
        condition1[i] = conditionMsg[parent2][i];
        condition2[i] = conditionMsg[parent1][i];
      }
    else                   // crossover in action part
      for (i=xOverPt; i<(2*size); i++)
      {
        int k = i-size;
        action1[k] = actionMsg[parent2][k];
        action2[k] = actionMsg[parent1][k];
      }

    // Pick where the child will go
    int loc = pickWeakRule(eligible);

    // Copy the new condition and action into the selected classifier
    if (getRandom(100) > 50)
      addClassifier(loc, condition1, action1,
                    (strength[parent1]+strength[parent2]) / 3.0);
    else
      addClassifier(loc, condition2, action2,
                    (strength[parent1]+strength[parent2]) / 3.0);

    // Adjust strengths of parents
    strength[parent1] *= 2.0/3.0;
    strength[parent2] *= 2.0/3.0;
  }
  else                                                // Asexual Crossover
  {
    int parent;

    // Pick Parent
    parent = roulette(strength,eligible,length);
    eligible[parent] = 0;

    // Pick where the child will go
    int loc = pickWeakRule(eligible);

    // Add the child to the classifier list
    addClassifier(loc, conditionMsg[parent], actionMsg[parent],
                  .5 * strength[parent]);

    // Adjust strength of parent
    strength[parent] *= .5;
  }

  delete [] eligible;
}

//============================================================================
// FUNCTION:     mutation()
//
// DESCRIPTION:  This function mutates the alleles (bits) of a classifier.
//               Mutation is performed with probablilty 'mutationProb'.  Bits
//               are mutated based on their masks.  An 'X' bit is mutated to
//               one of the three alleles {0,1,#}.  A 'B' bit is mutated to
//               either 0 or 1.  Note that a bit is mutated to a different
//               value than it already is.  i.e., the mutation functions
//               don't just randomly pick from the allele set, they create a
//               different allele.  When a mutated rule is created, it keeps
//               the same support and the same strength.  Mutation is viewed as
//               the genetic alternation of a rule, not the creation of a new
//               rule.  Mutation can be viewed as rule "evolution".
//               Note that if elitism in on and a rule has elite status, the
//               rule is not mutated.
//
// I/O:          none
//============================================================================

void classifierList::mutation()
{
  int size = conditionMsg->size();

  for (int i=0; i<length; i++)
  {
    if (!elitismEnabled || (elitismEnabled && !eliteSet[i]))
    {
      for (int j=0; j<size; j++)
      {
        if (condMask[j] == 'X') mutateTernary(&conditionMsg[i][j]);
        else if (condMask[j] == 'B') mutateBinary(&conditionMsg[i][j]);
      }
      for (j=0; j<size; j++)
      {
        if (actionMask[j] == 'B') mutateBinary(&actionMsg[i][j]);
        else if (actionMask[j] == 'X') mutateTernary(&actionMsg[i][j]);
      }
    }
  }
}

//============================================================================
// FUNCTION:     CDO()
//
// DESCRIPTION:  CDO creates a new classifier with the given condition.  This
//               function is used when there is a message on the message
//               board which has no match in the classifier list.
//
//               The eligibility array is provided so that if multiple CDOs
//               are performed in one time step, one CDO will not replace
//               another CDO.  This can happen if all of the other rules have
//               high strengths and the starting strength value is low.  This
//               function modifies the eligibility list by making the newly-
//               created rule ineligible.  Also, if elitism is enabled, those
//               rules which are elite cannot be replaced by CDO.
//
//               Also, the rule is given default starting values for a new
//               classifier, since the rule created is not the genetic
//               modification of another rule but an entirely new one.  See
//               createRule() for more details.
//
// I/O:           -->  cond      -  The condition to use in the new rule.
//               <-->  eligible  -  The list of eligible locations for the
//                                  new rule to go.
//============================================================================

void classifierList::CDO(const message & cond, int *eligible)
{
  ++numCDOs;

  // Remove elite rules from eligibility
  if (elitismEnabled)
    for (int i=0; i<length; i++)
      if (eliteSet[i]) eligible[i] = 0;

  createRule(cond,eligible);
}

//============================================================================
// FUNCTION:     CEO()
//
// DESCRIPTION:  The purpose of this function is to create a new rule with the
//               current rule's condition and a random action.  This function
//               is intended for use in the environment object when a
//               situation arises where there are no environment messages and
//               one (or more) needs to be created.  Note that this function
//               would be called AFTER messages have been posted, and
//               therefore, messages on the message board should be excluded
//               from eligibility. (i.e. tagged with a 0 in the eligibile
//               array.)  The newly-created classifier will be automatically
//               removed from the eligibility array in case multiple CEO's are
//               to be performed.  Conceptually, the CEO creates an additional
//               rule that would have been posted on the message board along
//               with the others.
//
//               Note that this routine is functionally identical to CDO,
//               except that a specific rule is specified to be used as a
//               "base" for the createRule function.  Therefore, the newly-
//               created rule is given the SAME supplierSet has the rule upon
//               which it is based.
//
//               As with CDO, the eligibility array is provided if multiple
//               CEO's must be performed.  Each newly created rule is
//               removed from the eligibility list (by the CDO function).
//               Also, those rules which have already been selected to post
//               must also not be eligible
//
//
// I/O:           -->  cond      -  The condition to use in the new rule.
//               <-->  eligible  -  The list of eligible locations for the
//                                  new rule to go.
//               <--             -  The location of the newly-created rule.
//============================================================================

int classifierList::CEO(int num, int *eligible)
{
  ++numCEOs;

  // Remove elite rules from eligibility
  if (elitismEnabled)
    for (int i=0; i<length; i++)
      if (eliteSet[i]) eligible[i] = 0;

  // Store supplier since the createRule function can replace rule num.
  int oldSupplier = supplierSet[num], newRule;
#ifdef NETWORK
  unsigned long oldID = supplierID[num];
#endif
  newRule = createRule(conditionMsg[num],eligible);

  supplierSet[newRule] = oldSupplier;
#ifdef NETWORK
  supplierID[newRule] = oldID;
#endif

  return newRule;
}

//============================================================================
// FUNCTION:     createRule()
//
// DESCRIPTION:  The purpose of this function is to create a new rule in the
//               classifier list with the given condition.  This function is
//               called by the CDO and CEO functions. In order to determine
//               where this classifier will go, the roulette wheel algorithm
//               is used with all of the strengths reversed
//               (i.e. maxStrength - Strength).  This allows the weakest
//               classifier to have the highest chance of being selected.
//
//               The eligibility array is provided to allow selective
//               replacement, i.e. exclude some classifiers from roulette
//               selection.  Note that the newly created rule is automatically
//               made ineligible by setting its location in the eligible
//               array to 0.
//
//               Finally, new rules are not considered the result of
//               genetics, but rather as "learned" behavior.  That is, this
//               function creates a rule which provides an guaranteed action
//               for a particular condition.  Therefore, the new rule is given
//               the default starting strength and no support number.
//               Essentially, the rule it replaces is destroyed, not
//               genetically altered.
//
// I/O:           -->  cond      -  The condition to use in the new rule.
//               <-->  eligible  -  The list of eligible locations for the
//                                  new rule to go.
//               <--             -  The index where the new rule was placed.
//============================================================================

int classifierList::createRule(const message & cond, int *eligible)
{
  // Create a random action
  message act = createRandomMessage(actionMask);

  // Pick where the child will go
  int loc = pickWeakRule(eligible);

  // Add the new classifier to the list at loc
  addClassifier(loc,cond,act,startStrength);

  return loc;
}

//============================================================================
// FUNCTION:     readStats()
//
// DESCRIPTION:  Returns the statistics from the genetic and rule creation
//               operations.  Note that the number of asexual crossovers is
//               the total number of crossovers - the number of sexual
//               crossovers.  Also, the mutations count is a count of the
//               number of bits mutated, not the number of rules mutated.
//
// I/O:          <--  cross      -  The total number of crossovers performed.
//               <--  sexual     -  The number of sexual crossovers performed.
//               <--  mutations  -  The number of mutations performed.
//               <--  CDOs       -  The number of CDOs performed.
//               <--  CEOs       -  The number of CEOs performed.
//============================================================================

void classifierList::readStats(int *cross, int *sexual, int *mutations,
                               int *CDOs, int *CEOs)
{
 *cross = numCrossovers;
 *sexual = numSexual;
 *mutations = numMutations;
 *CDOs = numCDOs;
 *CEOs = numCEOs;
}

//============================================================================
// FUNCTION:     loadStatic()
//
// DESCRIPTION:  Loads all static parameters in the classifierList object
//               from an already-opened file.  The load() function MUST
//               be called immediately AFTER this function.  Reset does NOT
//               need to be called.  load() will handle all necessary
//               deallocation and reallocation tasks.
//
// I/O:          -->  fptr  -  File pointer to an opened, binary file.
//============================================================================

void classifierList::loadStatic(FILE *fptr)
 {
  fread(&length,sizeof(int),1,fptr);
  fread(&numConditions,sizeof(int),1,fptr);
  fread(&BBAenabled,sizeof(int),1,fptr);
  fread(&BBAstyle,sizeof(int),1,fptr);
  fread(&elitismEnabled,sizeof(int),1,fptr);
  fread(&producerTaxInterval,sizeof(int),1,fptr);
  fread(&maxActionsToPost,sizeof(int),1,fptr);
  fread(&CDOenabled,sizeof(int),1,fptr);
  fread(&CEOenabled,sizeof(int),1,fptr);
  fread(&TCOenabled,sizeof(int),1,fptr);
  fread(&bidConstant,sizeof(double),1,fptr);
  fread(&sexualProb,sizeof(double),1,fptr);
  fread(&mutationProb,sizeof(double),1,fptr);
  fread(&startStrength,sizeof(double),1,fptr);
  fread(&headTax,sizeof(double),1,fptr);
  fread(&bidTax,sizeof(double),1,fptr);
  fread(&producerTax,sizeof(double),1,fptr);
  fread(&eliteThresh,sizeof(double),1,fptr);
  fread(&strengthCap,sizeof(double),1,fptr);
  fread(&bidCap,sizeof(double),1,fptr);

  message dummy;
  int msgSize = dummy.size();
  char *cMask, *aMask;

  try
  { cMask = new char[msgSize+1];
    aMask = new char[msgSize+1];
  }
  catch(xalloc) { error(0,"classifierList::loadStatic()"); }

  fread(cMask,sizeof(char),msgSize+1,fptr);
  fread(aMask,sizeof(char),msgSize+1,fptr);
  setMasks(cMask,aMask);

  delete [] cMask;
  delete [] aMask;
}

//============================================================================
// FUNCTION:     saveStatic()
//
// DESCRIPTION:  Saves all static parameters from the classifierList object.
//               This function MUST be called BEFORE save().
//
// I/O:          -->  fptr  -  File pointer to an opened, binary file.
//============================================================================

void classifierList::saveStatic(FILE *fptr)
{
  fwrite(&length,sizeof(int),1,fptr);
  fwrite(&numConditions,sizeof(int),1,fptr);
  fwrite(&BBAenabled,sizeof(int),1,fptr);
  fwrite(&BBAstyle,sizeof(int),1,fptr);
  fwrite(&elitismEnabled,sizeof(int),1,fptr);
  fwrite(&producerTaxInterval,sizeof(int),1,fptr);
  fwrite(&maxActionsToPost,sizeof(int),1,fptr);
  fwrite(&CDOenabled,sizeof(int),1,fptr);
  fwrite(&CEOenabled,sizeof(int),1,fptr);
  fwrite(&TCOenabled,sizeof(int),1,fptr);
  fwrite(&bidConstant,sizeof(double),1,fptr);
  fwrite(&sexualProb,sizeof(double),1,fptr);
  fwrite(&mutationProb,sizeof(double),1,fptr);
  fwrite(&startStrength,sizeof(double),1,fptr);
  fwrite(&headTax,sizeof(double),1,fptr);
  fwrite(&bidTax,sizeof(double),1,fptr);
  fwrite(&producerTax,sizeof(double),1,fptr);
  fwrite(&eliteThresh,sizeof(double),1,fptr);
  fwrite(&strengthCap,sizeof(double),1,fptr);
  fwrite(&bidCap,sizeof(double),1,fptr);

  message dummy;
  int msgSize = dummy.size();
  fwrite(condMask,sizeof(char),msgSize+1,fptr);
  fwrite(actionMask,sizeof(char),msgSize+1,fptr);
}

//============================================================================
// FUNCTION:     load()
//
// DESCRIPTION:  Loads the classifier list and all supporting variables.
//               This function MUST be called AFTER loadStatic() since
//               allocations and deallocations are based on the static class
//               variables.
//
// I/O:          -->  fptr  -  File pointer to an opened, binary file.
//============================================================================
void classifierList::load(FILE *fptr)
{
  // Reallocate
  deleteAll();
  allocateAll();

  // Load classifier list

  for (int i=0; i<length; i++)
  {
    conditionMsg[i].load(fptr);
    actionMsg[i].load(fptr);
  }

  fread(matchFlag,sizeof(int),length,fptr);
  fread(bid,sizeof(double),length,fptr);
  fread(specificity,sizeof(double),length,fptr);
  fread(strength,sizeof(double),length,fptr);
  fread(support,sizeof(double),length,fptr);
  fread(number,sizeof(int),length,fptr);
  fread(selected,sizeof(int),length,fptr);
  fread(supplierSet,sizeof(int),length,fptr);
  fread(tempSupplierSet,sizeof(int),length,fptr);
  fread(eliteSet,sizeof(int),length,fptr);
#ifdef NETWORK
  fread(&agentIDcopy,sizeof(unsigned long),1,fptr);
  fread(supplierID,sizeof(unsigned long),length,fptr);
  fread(tempSupplierID,sizeof(unsigned long),length,fptr);
#endif

  // Load stats
  fread(&numCrossovers,sizeof(int),1,fptr);
  fread(&numSexual,sizeof(int),1,fptr);
  fread(&numMutations,sizeof(int),1,fptr);
  fread(&numCDOs,sizeof(int),1,fptr);
  fread(&numCEOs,sizeof(int),1,fptr);
  fread(&numTCOs,sizeof(int),1,fptr);
  fread(&numClassifiers,sizeof(int),1,fptr);
}

//============================================================================
// FUNCTION:     save()
//
// DESCRIPTION:  Saves the classifier list and all supporting variables.
//               This function MUST be called AFTER saveStatic().
//
// I/O:          -->  fptr  -  File pointer to an opened, binary file.
//============================================================================

void classifierList::save(FILE *fptr)
{
  // Save classifier list

  for (int i=0; i<length; i++)
  {
    conditionMsg[i].save(fptr);
    actionMsg[i].save(fptr);
  }

  fwrite(matchFlag,sizeof(int),length,fptr);
  fwrite(bid,sizeof(double),length,fptr);
  fwrite(specificity,sizeof(double),length,fptr);
  fwrite(strength,sizeof(double),length,fptr);
  fwrite(support,sizeof(double),length,fptr);
  fwrite(number,sizeof(int),length,fptr);
  fwrite(selected,sizeof(int),length,fptr);
  fwrite(supplierSet,sizeof(int),length,fptr);
  fwrite(tempSupplierSet,sizeof(int),length,fptr);
  fwrite(eliteSet,sizeof(int),length,fptr);
#ifdef NETWORK
  fwrite(&agentIDcopy,sizeof(unsigned long),1,fptr);
  fwrite(supplierID,sizeof(unsigned long),length,fptr);
  fwrite(tempSupplierID,sizeof(unsigned long),length,fptr);
#endif

  // Save stats
  fwrite(&numCrossovers,sizeof(int),1,fptr);
  fwrite(&numSexual,sizeof(int),1,fptr);
  fwrite(&numMutations,sizeof(int),1,fptr);
  fwrite(&numCDOs,sizeof(int),1,fptr);
  fwrite(&numCEOs,sizeof(int),1,fptr);
  fwrite(&numTCOs,sizeof(int),1,fptr);
  fwrite(&numClassifiers,sizeof(int),1,fptr);
}

//============================================================================
// FUNCTION:     getMatches()
//
// DESCRIPTION:
//
//   This function checks for matches between the posted messages on the
//   message board and the classifier list conditions.  If the message board
//   has more than one message (i.e. the BBA is in effect), support for a
//   matching classifier is taken from the first matching message on the
//   board.  Note that the "actual msgBoard size" is used instead of the
//   maximum size since there may not be a full msgBoard of information from
//   the previous time step.  The actual msgBoard size will be at least 1
//   since the environment always gets to post.
//
//   e.g.,
//          # message  posting classifier         #  classifier    Support
//          1  1101          3                    0  1101 | 1110      3
//          2  1001          10
//          3  1101          6
//
//   In this case, classifiers 3 and 6 both posted messag 1101 on the last
//   time step.  Classifier 0 matches messages 1 and 3, but the support value
//   for classifier 0 is taken from message 1.
//
//   The reason tempSupplierSet is used instead of supplierSet is because the
//   supplierSet values are only changed for those messages who are selected to
//   post, and this selection happens in another function.  The support
//   quantity used in the bid calculation is simply the bid of the supplier.
//   This quantity increases the bid of the eligible classifier.  Note that
//   support is only filled when the BBA is enabled.
//
//   CDO is performed for all msgBoard messages without a matching classifier.
//   Once a CDO is performed, the newly-created rule is excluded from future
//   CDO calls.  This is necessary because it is possible for the CDO to
//   consistently replace the same rule, provided that rule is of a much lower
//   strength than all other rules.  While such a situation is rare, it can
//   lead to an infinite loop in the worst-case scenario.  Therefore, the
//   msgBoard-classList matching loop will need to be executed at most
//   msgBoardLength+1 times.  i.e. worst-case is that each rule has to be
//   created on a separate pass of the loop.  (Note:  CDO can be disabled.)
//
//   Note that while each message board message must have at least one match,
//   a single classifier can match more than one message on the message board.
//   This means that it is possible to get a set of matching (eligible)
//   classifiers that is smaller than the length of the message board.  The
//   only requirement is that each message board message have a match.
//
// I/O:          -->  msgBoard  -  Reference to the message board.
//============================================================================

void classifierList::getMatches(messageBoard & msgBoard)
{
  int msgBoardLength = msgBoard.actualSize(),
      done = 0, numMsgMatched, loopCount = 0,
      *msgBoardMatch, *supplier, *eligibility;
  message *msg;
  double *ruleBid;
#ifdef NETWORK
  unsigned long *ID;
#endif

  try
  { msgBoardMatch = new int[msgBoardLength];
    supplier = new int[msgBoardLength];
    eligibility = new int[length];
    msg = new message[msgBoardLength];
    ruleBid = new double[msgBoardLength];
#ifdef NETWORK
    ID = new unsigned long[msgBoardLength];
#endif
  }
  catch(xalloc) { error(0,"classifierList::getMatches()"); }

  // Copy the message board
#ifdef NETWORK
  for (int j = 0; j < msgBoardLength; j++)
    msgBoard.getMessage(j,msg[j],&supplier[j],&ID[j],&ruleBid[j]);
#else
  for (int j = 0; j < msgBoardLength; j++)
    msgBoard.getMessage(j,msg[j],&supplier[j],&ruleBid[j]);
#endif

  // Make all classifiers eligible for CDO.  The CDO function will remove
  // rule eligibility as necessary.
  for (int i=0; i<length; i++) eligibility[i] = 1;

  while (!done)
  {
    ++loopCount;

    // Check loop count to see if this loop has been executed too many times,
    // if so, stop the program.
    if (loopCount > msgBoardLength+1) error(2,"classifierList::getMatches()");

    // Clear match flags for msgBoard messages
    for (j = 0; j < msgBoardLength; j++)
      msgBoardMatch[j] = 0;

    // Check for matches.  Assign support to the first match.  Each msgBoard
    // message must be checked against each classifier in case there are
    // duplicate messages on the msgBoard.  Each msgBoard message must have
    // at least one match.
    for (i = 0; i < length; i++)
    {
      tempSupplierSet[i] = -2;  // Init to no suppliers
#ifdef NETWORK
      tempSupplierID[i] = agentIDcopy;
#endif
      support[i] = 0.0;         // Init to zero support
      matchFlag[i] = 0;         // Init to no match

      for (j=0; j<msgBoardLength; j++)
      {
        if (conditionMsg[i] == msg[j])
        {
          if (!matchFlag[i])
          {
            tempSupplierSet[i] = supplier[j];          // Supplier's number
            if (BBAenabled) support[i] = ruleBid[j];   // Supplier's bid
#ifdef NETWORK
            tempSupplierID[i] = ID[j];                 // Supplier's network ID
#endif
            matchFlag[i] = 1;
          }
          msgBoardMatch[j] = 1;
        }
      }
    }

    // Determine how many message board messages were matched
    numMsgMatched = 0;
    for (j=0; j<msgBoardLength; j++)
      if (msgBoardMatch[j]) ++numMsgMatched;

    // Perform CDO for all messages not matched and check again if CDO is on
    if (numMsgMatched == msgBoardLength || !CDOenabled)
      done = 1;
    else
    {
      for (j = 0; j < msgBoardLength; j++)
        if (!msgBoardMatch[j])
          CDO(msg[j],eligibility);
    }
  }

  delete [] msgBoardMatch;
  delete [] msg;
  delete [] supplier;
  delete [] ruleBid;
  delete [] eligibility;
#ifdef NETWORK
  delete [] ID;
#endif
}

//============================================================================
// FUNCTION:     calcBids()
//
// DESCRIPTION:
//
//   Calculates the bids for each classifer.  Specificity is calculated using
//   the condition mask.  Only locations marked with an 'X' or '#' are
//   checked.  This ensures that conditions aren't improperly biased when the
//   environment setup dictates that some of the bits must be only 0 or 1.
//   NOTE:  If the condition mask has no X's or #'s in it, the specificity is
//   set to zero!
//
//   Support is determined by the getMatches function.  Each rule that posted
//   on the previous time step put its number and bid on the message board
//   along with the message.  The matching rule's Support is simply the bid
//   of the matching message board message.
//
// I/O:          none
//============================================================================

void classifierList::calcBids()
{
  int size = conditionMsg->size(), maskCount, specCount;

  for (int i=0; i<length; i++)
  {
    if (matchFlag[i])
    {
      // Calculate specificity

      specCount = maskCount = 0;

      for (int j=0; j<size; j++)
        if (condMask[j] == 'X' || condMask[j] == '#')
        {
          ++maskCount;
          if (conditionMsg[i][j] != '#' ) ++specCount;
        }

      if (maskCount > 0) specificity[i] = double(specCount) / double(maskCount);
      else specificity[i] = 0.0;

      // Calculate Bid and set to cap if necessary

      bid[i] = bidConstant * specificity[i] * strength[i] * (1 + support[i]);

      if (bid[i] > bidCap) bid[i] = bidCap;
    }
    else
      bid[i] = 0.0;
  }
}

//============================================================================
// FUNCTION:     fillMsgBoard()
//
// DESCRIPTION:
//
//  Once this function is reached, there is at least one match for each
//  message on the message board.  This does NOT mean that there are enough
//  eligible classifiers to fill the message board.  It means that there are
//  each message on the message board has a match, even if one classifier
//  matches more than one message.  This function attempts to pick
//  'maxActionsToPost' actions from the eligible classifiers.
//
//  A "select and remove" scheme is used to choose eligible messages.  This
//  means that once an action has been chosen with the roulette wheel, it is
//  removed from further roulette wheel selections.  Note that this changes
//  the probability of being selected because the cumulative sum changes with
//  each classifier removed from consideration.  However, a rule should only
//  be selected once, so this method is necessary.
//
//  Selected classifiers get their supplierSet array updated, since they have
//  new suppliers.  Unselected classifiers retain the previos supplierSet
//  vaule.  In this way, one can trace a chain from start to finish by
//  looking at the supplier for each classifier in the chain.
//
// I/O:          -->  msgBoard  -  Reference to the message board.
//============================================================================

void classifierList::fillMsgBoard(messageBoard & msgBoard)
{
  int maxCount, numClassMatches, *eligible;

  try { eligible = new int[length]; }
  catch(xalloc) { error(0,"classifierList::fillMsgBoard"); }

  // Determine the number of eligible classifiers to post.  Reset the selected
  // flag array.  The eligible array tells the roulette routine which
  // classifiers to choose from.
  numClassMatches = 0;
  for (int i = 0; i<length; i++)
  {
    if (matchFlag[i]) ++numClassMatches;
    eligible[i] = matchFlag[i];
    selected[i] = 0;
  }

  // Set the maximum allowable number of postings
  if (numClassMatches < maxActionsToPost) maxCount = numClassMatches;
  else maxCount = maxActionsToPost;

  // Select maxCount messages and post
  for (int k=0; k<maxCount; k++)
  {
    int sel = roulette(bid,eligible,length);

    selected[sel] = 1;
    supplierSet[sel] = tempSupplierSet[sel];
#ifdef NETWORK
    supplierID[sel] = tempSupplierID[sel];
    msgBoard.postMessage(k,actionMsg[sel],number[sel],agentIDcopy,bid[sel]);
#else
    msgBoard.postMessage(k,actionMsg[sel],number[sel],bid[sel]);
#endif

    eligible[sel] = 0;
  }

  // Set the actual size of the message board based on the number of messages
  // posted.
  msgBoard.actualSize(maxCount);

  delete [] eligible;
}

//============================================================================
// FUNCTION:     adjustStrengths()
//
// DESCRIPTION:
//
//  This function adjusts the strengths of the selected classifiers and the
//  support classifiers.  The selected classifiers, those given the right to
//  post, have their strength reduced by their bid.  The supplier classifiers
//  are those classifiers that posted the messages which fired the
//  selected classifiers.  Each selected classifier has one supplier
//  classifier corresponding to the message which fired the classifier.  e.g.
//
//  msgBoard  Posting ID            Selected Classifiers  Supplier
//   10010        5                  0   10010 | 10101       5
//   11011       10                  6   00101 | 10000       7
//   00101        7                 11   11011 | 11111      10
//
//  Support classifiers are paid the amount of the bid of the selected
//  classifier they support.  Note that the supplier number is obtained
//  from the number array, i.e. the supplier number is unique.
//
//  NETWORK:  If the network code has been compiled, BBA payoff messages are
//            also sent and received.  (The sendPayoff function only sends
//            messages if the network is turned on.)
//
//  This function is only performed if the BBA is enabled.
//
// I/O:          --> network  -  reference to the network interface if network
//                               code has been compiled.
//============================================================================

#ifdef NETWORK
void classifierList::adjustStrengths(networkInterface & network)
#else
void classifierList::adjustStrengths()
#endif
{
  // Pay bids and supporters only if BBA is turned on
  if (!BBAenabled) return;

  for (int i=0; i<length; i++)
  {
    // Reward only the selected classifiers
    if (selected[i])
    {
      // Pay bid for getting the right to post
      strength[i] -= bid[i];

      // Pay the supplier if action has a supplier
      if (supplierSet[i] >= 0)
      {
#ifdef NETWORK
        if (supplierID[i] != agentIDcopy)
          network.sendPayoff(agentIDcopy,supplierID[i],supplierSet[i],bid[i]);
        else
#endif
        for (int j=0; j<length; j++)
          if (supplierSet[i] == number[j]) strength[j] += bid[i];
      }
    }
  }

#ifdef NETWORK
  network.readPayoff(*this);
#endif
}

//============================================================================
// FUNCTION:     checkStrengths()
//
// DESCRIPTION:  Makes sure that all strengths are between 0.0 and
//               strengthCap.  Strengths < 0 cause problems with the
//               roulette wheel function.  Strengths are capped at a
//               saturation point to keep loops from causing overflow.
//               This function also determines which rules are elite if
//               elitism is enabled.
//
//               This function is called ONCE on each clock tick.
//
// I/O:          none
//============================================================================

void classifierList::checkStrengths()
{
  for (int i=0; i<length; i++)
  {
    if (strength[i] < 0.0) strength[i] = 0.0;
    else if (strength[i] > strengthCap) strength[i] = strengthCap;
  }

  if (elitismEnabled)
  {
    double thresh = eliteThresh * getMaxStren();
    for (i=0; i<length; i++)
    {
      if (strength[i] >= thresh) eliteSet[i] = 1;
      else eliteSet[i] = 0;
    }
  }
}

//============================================================================
// FUNCTION:     taxes()
//
// DESCRIPTION:  Applies a fixed amount of tax to the strengths of each
//               classifier.  This function performs the head tax only.
//               Bid tax and producer taxers have not been programmed into
//               this version.  If you want 'em, this is the place to put 'em.
//
// I/O:          none
//============================================================================

void classifierList::taxes()
{
  double mult = 1 - headTax;

  for (int i=0; i<length; i++)
    strength[i] *= mult;
}

//============================================================================
// FUNCTION:     loadRules()
//
// DESCRIPTION:  This is a retro-fit function used in testing LCS programming.
//               The read is not parsed in any fancy way, so you better be
//               sure you adhere to the file format.  No error checking is
//               performed.
//
//               The checkStrengths() function is called to make sure the
//               strengths are within range AND to set the elite rules
//               if elitism is enabled.
//
// I/O:          --> fptr  -  pointer to an open ASCII file set for reading.
//============================================================================

void classifierList::loadRules(FILE *fptr)
{
  message temp;
  char text[81], *cond, *action;

  try
  { cond = new char[temp.size()+1];
    action = new char[temp.size()+1];
  }
  catch(xalloc) { error(0,"classifierList::loadRules()"); }

  for (int i=0; i<length; i++)
  {
    if(fgets(text,80,fptr))
    {

      sscanf(text,"%s %s %lf",cond,action,&strength[i]);
      conditionMsg[i] = cond;
      actionMsg[i] = action;
      bid[i] = support[i] = specificity[i] = 0.0;
      matchFlag[i] = selected[i] = eliteSet[i] = 0;
      number[i] = i;
      supplierSet[i] = -2;  // -2 means no support
    }
  }
  numClassifiers = length;

  delete [] cond;
  delete [] action;

  // Makes sure loaded strengths are within range and set elitism array if nec.
  checkStrengths();
}

//============================================================================
// FUNCTION:     saveRules()
//
// DESCRIPTION:  The opposite of load rules.  Saves the rules in ASCII so you
//               can edit them.
//
//               Note that the maximum line width is 80 columns.  Make sure
//               that you don't overflow the string.
//
// I/O:          --> fptr  -  pointer to an open ASCII file set for writing.
//============================================================================

void classifierList::saveRules(FILE *fptr)
{
  char text[81], *cond, *action;

  try
  { cond = new char[message::size()+1];
    action = new char[message::size()+1];
  }
  catch(xalloc) { error(0,"classifierList::saveRules()"); }

  for (int i=0; i<length; i++)
  {
    conditionMsg[i].msgToStr(cond);
    actionMsg[i].msgToStr(action);
    sprintf(text,"%s %s %lf\n",cond,action,strength[i]);
    fputs(text,fptr);
  }

  delete [] cond;
  delete [] action;
}

//============================================================================
// FUNCTION:     getClassifier()
//
// DESCRIPTION:  Gets all the information about the classifier one could have
//               any possible use for.
//
// I/O:           --> num      - the INDEX of the classifier to get
//                <-- cond     - the condition word
//                <-- act      - the action word
//                <-- stren    - the strength
//                <-- spec     - the specificity last calculated by calcBids()
//                <-- sup      - the support last calcaulted by getMatches()
//                <-- ruleBid  - the bid last calculated by calcBids()
//                <-- match    - the match flag last determined by getMatches()
//                <-- sel      - the select flag last determined by fillMsgBoard()
//                <-- supplier - the supplier (unique classifier number) last
//                               determined by getMatches()
//                <-- ID       - (NETWORK CODE ONLY) the agent ID of the supplier
//                <-- elite    - the elite flag determined by checkStrengths()
//                <-- ruleNum  - the unique number assigned to this rule
//============================================================================

#ifdef NETWORK
void classifierList::getClassifier(int num, message & cond, message & act,
                   double *stren, double *spec, double *sup, double *ruleBid,
                   int *match, int *sel, int *supplier, unsigned long *ID,
                   int *elite, int *ruleNum)
#else
void classifierList::getClassifier(int num, message & cond, message & act,
                   double *stren, double *spec, double *sup, double *ruleBid,
                   int *match, int *sel, int *supplier, int *elite,
                   int *ruleNum)
#endif
{
  if (num > length) error(1,"classifierList::getClassifier()");
  cond = conditionMsg[num];
  act = actionMsg[num];
  *stren = strength[num];
  *spec = specificity[num];
  *sup = support[num];
  *ruleBid = bid[num];
  *match = matchFlag[num];
  *sel = selected[num];
  *supplier = supplierSet[num];
  *elite = eliteSet[num];
  *ruleNum = number[num];
#ifdef NETWORK
  *ID = supplierID[num];
#endif
}

//============================================================================
// FUNCTION:     getClassifier()
//
// DESCRIPTION:  A shorter version of the previos that just gets the condition,
//               action, and strength of a classifier.
//
// I/O:           --> num      - the INDEX of the classifier to get
//                <-- cond     - the condition word
//                <-- act      - the action word
//                <-- stren    - the strength
//============================================================================

void classifierList::getClassifier(int num, message & cond, message & act,
                                   double *stren)
{
  if (num > length) error(1,"classifierList::getClassifier() #2");
  cond = conditionMsg[num];
  act = actionMsg[num];
  *stren = strength[num];
}
//============================================================================
// FUNCTION:     payoffRule()
//
// DESCRIPTION:  This function accepts a unique classifier number and a strength
//               adjustment quantity.  It is intended for use by the environment
//               payoff function to reward a classifier.  Note that it is
//               possible that the classifier does not exist anymore in the list
//               (because of genetics or rule discovery).
//
// I/O:          --> ruleNum - The unique classifier number of the rule to be
//                             paid.
//               <-- payoff  - The payoff quantity.
//============================================================================

void classifierList::payoffRule(int ruleNum, double payoff)
{
  for (int i=0; i<length; i++)
    if (number[i] == ruleNum) strength[i] += payoff;
}

//============================================================================
// FUNCTION:     whatIsIndex()
//
// DESCRIPTION:  This function converts a unique rule number to an index into
//               the classifier array so that you can use the getClassifier
//               function to get info about the classifier.  If the classifier
//               does not exist, a -1 is returned.
//
// I/O:          --> ruleNum - the unique number of the classifier of interest.
//============================================================================

int classifierList::whatIsIndex(int ruleNum)
{
  int i=0;
  while (i < length && number[i] != ruleNum) i++;

  if (i==length) return -1;
  else return i;
}

//============================================================================
// FUNCTION:     getMaxStren()
//
// DESCRIPTION:  Gets the maximum strength of the classifier in the list.  The
//               function was written for network inteface use, but you may
//               have a use for it in the environment.
//
// I/O:          <--    The maximum strength in the classifier list.
//============================================================================

double classifierList::getMaxStren()
{
  double maxStren = 0.0;

  for (int i=0; i<length; i++)
    if (strength[i] > maxStren) maxStren = strength[i];

  return maxStren;
}

//============================================================================
// FUNCTION:     getStrength()
//
// DESCRIPTION:  Another classifier access function, except this one only
//               returns the strength of the given classifier.
//
// I/O:          --> num  -  The INDEX of the classifier of interest.
//============================================================================

double classifierList::getStrength(int num)
{
  if (num > length) error(1,"classifierList::getStrength()");
  return strength[num];
}

