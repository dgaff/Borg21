#ifndef CLSSLIST_H
#define CLSSLIST_H

//============================================================================
//  The classifierList class contains the classifier list and all related
//  parameters.  The list consists of an array of condition messages and
//  an array of action messages.  In addition, there are several other
//  parallel arrays which hold specificity, strength, support, bids, etc.
//  The matchFlag array is used to flag classifiers which match the
//  message board.  Therefore a 1 in a particular element of this array
//  means that this classifier matches the message board.  Similarly, the
//  selected array contains a 1 if the corresponding classifier has been
//  selected to post.
//
//  In order to provide maximum flexibility, condition and action masking
//  is included.  Masking allows the user to specificy which alleles in
//  the condition and action messages can have which characters ('0', '1',
//  or '#').  See setMasks() for more information on masking.  Note that
//  "pass-through" is not supported.  Pass-through is used when a '#'
//  appears in the action string.  In such a case, the corresponding allele
//  in the condition message is used in place of the '#' before posting.
//  In this object, a '#' in the action message means "don't care".
//
//  The BBA can be enabled or disabled by setting the BBAenabled flag.
//  When enabled, two additional tasks are performed.  First, support
//  is calculated for each classifier that bids.  Second, winning
//  classifiers pay their supporters.  Note that the 'maxActionsToPost'
//  variable tells the classifier list how many messages it can post on the
//  message board.  See the messageBoard class for more details.
//
//  The supplierSet array is used to hold the unique number of the supplier of
//  the classifier.  For example, if supplierSet[3] == 58, then then 3rd
//  classifier if the list has a supplier whose unique number is 58.  We use
//  unique numbers because the rule discovery operators often modify the
//  rule base during mid-execution, and a supplier that was in the classifier
//  list at the beginning of the clock tick may get killed by CDO.  The
//  payoff functions search the list for the unique rule number rather than
//  an index to ensure the correct rule is paid.
//
//  NOTE:  All probabilities have a resolution of 2 decimal places!!  In other
//         words, don't set a probability lower than 0.01.
//
//  Multiple conditions, bid taxes, producer taxes, and TCOs
//  are not supported at this time, however, I have included variables for
//  to aid in implementation of them.
//============================================================================

class classifierList
{
   //-------------------------------------------------------------------------//
   // Settings
   //-------------------------------------------------------------------------//
   static int length,             // Length of classifier list
              numConditions,      // NOT USED, set to 1.
              BBAenabled,         // Turns Bucket Brigade on or off
              BBAstyle,           // NOT USED, set to 0.
              elitismEnabled,     // Turns Elitism on or off
              producerTaxInterval,// NOT USED, set to 0.
              CDOenabled,         // Turns CDO on or off
              CEOenabled,         // Turns CEO on of off
              TCOenabled,         // NOT USED, set to 0.
              maxActionsToPost;   // Maximum number of actions to post

   static double bidConstant,     // Bid constant multiplier
                 sexualProb,      // Probability of sexual reproduction
                 mutationProb,    // Probability of mutation
                 startStrength,   // Initial strength of all classifiers
                 headTax,         // Head tax for classifiers
                 bidTax,          // NOT USED
                 producerTax,     // NOT USED
                 eliteThresh,     // Threshold for elite status (% of max stren)
                 strengthCap,     // Maximum allowable strength value
                 bidCap;          // Maximum allowable bid value

   static char *condMask,         // Mask for condition word
               *actionMask;       // Mask for action word

#ifdef NETWORK
   unsigned long agentIDcopy;     // The LCS ID from classifierSystem class
#endif

   //-------------------------------------------------------------------------//
   // The classifier list
   //-------------------------------------------------------------------------//
   message *conditionMsg;         // Condition array
   message *actionMsg;            // Action array
   int *matchFlag;                // Messages matching the message board at a
                                  //   particular time step.  (1 = match)
   int *selected;                 // Classifiers selected to post. (1 = selected)
   double *bid;                   // Bid array
   double *specificity;           // Specificity array
   double *strength;              // Strength array
   double *support;               // Support array
   int *number;                   // Unique classifier number
   int *supplierSet;              // The supplier of the current classifier
   int *tempSupplierSet;          // Temporary supplier set created when eligible
                                  //   classifiers are found.
   int *eliteSet;                 // Flag array that indicates which rules have
                                  //   elite status
#ifdef NETWORK
   unsigned long *supplierID;     // Agent ID for supplier
   unsigned long *tempSupplierID; // Used when eligible classifiers are found.
#endif

   //-------------------------------------------------------------------------//
   // Stats
   //-------------------------------------------------------------------------//
   int numCrossovers, numSexual,  // Number of sexual and asexual crossovers performed
       numMutations,              // Number of mutations performed
       numCDOs,                   // Number of CDOs performed
       numClassifiers,            // Unique classifier number counter
       numCEOs,                   // NOT USED
       numTCOs;                   // NOT USED

   //-------------------------------------------------------------------------//
   // Functions to allocate and free all arrays
   //-------------------------------------------------------------------------//
   void deleteAll();
   void allocateAll();

   //-------------------------------------------------------------------------//
   // Rule selection function
   //-------------------------------------------------------------------------//
   int roulette(double *bids, int *eligible, int maxIndex);

   //-------------------------------------------------------------------------//
   // Internal genetic & rule creation functions
   //-------------------------------------------------------------------------//
   void crossOver();
   void mutation();
   void CDO(const message & cond, int *eligible);

   int mutateTernary(char *ch)
   {
    if ( (double(getRandom(100))/100.0) < mutationProb )
    {
      ++numMutations;

      if (*ch == '#')
        *ch = (getRandom(100) > 50) ? '1' : '0';
      else if (*ch == '0')
        *ch = (getRandom(100) > 50) ? '1' : '#';
      else
        *ch = (getRandom(100) > 50) ? '0' : '#';

      return 1;
    }
    else
      return 0;
   }

   int mutateBinary(char *ch)
   {
    if ( (double(getRandom(100))/100.0) < mutationProb )
    {
      ++numMutations;
      *ch = (*ch == '0') ? '1' : '0';
      return 1;
    }
    else
      return 0;
   }

   char randomTernary()
   {
     int randNum = getRandom(100);

     if (randNum > 66) return '1';
     else if (randNum > 33) return '0';
     else return '#';
   }

   char randomBinary()
   {
     if (getRandom(100) > 50) return '1';
     else return '0';
   }

   void setClassifier(int num, const message & cond, const message & act,
                      double stren);

 public:

   //-------------------------------------------------------------------------//
   // Initialization & reset functions
   //-------------------------------------------------------------------------//
   classifierList() { }
   ~classifierList()
   { deleteAll();
     delete [] condMask; delete [] actionMask;
     condMask = 0; actionMask = 0;
   }
   void reset(int firstReset = 0);
   void softReset();
   static void restoreDefaults();
   void initRule(int num, char *condStr, char *actionStr, double stren);

#ifdef NETWORK
   void setAgentID(unsigned long newID) { agentIDcopy = newID; }
#endif

   //-------------------------------------------------------------------------//
   // Public rule creation functions
   //-------------------------------------------------------------------------//
   int CEO(int num, int *eligible);
   int createRule(const message & cond, int *eligible);
   message createRandomMessage(char *mask);

   //-------------------------------------------------------------------------//
   // Rule selection function
   //-------------------------------------------------------------------------//
   int pickWeakRule(int *eligible);

   //-------------------------------------------------------------------------//
   // Classifier list access functions
   //-------------------------------------------------------------------------//
   static int isBBAon() { return BBAenabled; }
   static int isCEOon() { return CEOenabled; }
   static int size() { return length; }
   void readStats(int *cross, int *sexual, int *mutations,
                  int *CDOs, int *CEOs);
#ifdef NETWORK
   void getClassifier(int num, message & cond, message & act,
                      double *stren, double *spec, double *sup, double *ruleBid,
                      int *match, int *sel, int *supplier, unsigned long *ID,
                      int *elite, int *ruleNum);
#else
   void getClassifier(int num, message & cond, message & act,
                      double *stren, double *spec, double *sup, double *ruleBid,
                      int *match, int *sel, int *supplier, int *elite,
                      int *ruleNum);
#endif
   void getClassifier(int num, message & cond, message & act, double *stren);
   void addClassifier(int num, char *condStr=0, char *actionStr=0, double stren=0.0);
   void addClassifier(int num, const message & cond, const message & act, double stren);
   double getStrength(int num);
   void payoffRule(int ruleNum, double payoff);
   int whatIsIndex(int ruleNum);
   double getMaxStren();

   //-------------------------------------------------------------------------//
   // Saving and loading functions
   //-------------------------------------------------------------------------//
   static void loadStatic(FILE *fptr);
   static void saveStatic(FILE *fptr);
   void load(FILE *fptr);
   void save(FILE *fptr);
   void loadRules(FILE *fptr);
   void saveRules(FILE *fptr);

   //-------------------------------------------------------------------------//
   // Static parameter access functions
   //-------------------------------------------------------------------------//
   static void getSettings(int *len, int *numCond, int *BBAon, int *BBAtype,
                           int *elitismOn, int *prodTaxInt, int *CDOon,
                           int *CEOon, int *TCOon, int *maxPost,
                           double *bConst, double *crossProb,
                           double *mutateProb, double *startStren,
                           double *headTaxRate, double *bidTaxRate,
                           double *prodTaxRate, double *eliteT,
                           double *strenCap, double *bCap)
   { *len = length;  *numCond = numConditions;  *BBAon = BBAenabled;
     *BBAtype = BBAstyle;  *elitismOn = elitismEnabled;
     *prodTaxInt = producerTaxInterval;  *CDOon = CDOenabled;
     *CEOon = CEOenabled;  *TCOon = TCOenabled;  *maxPost = maxActionsToPost;
     *bConst = bidConstant;  *crossProb = sexualProb;
     *mutateProb = mutationProb;  *startStren = startStrength;
     *headTaxRate = headTax;  *bidTaxRate = bidTax;  *prodTaxRate = producerTax;
     *eliteT = eliteThresh;  *strenCap = strengthCap;  *bCap = bidCap;
   }
   static void setSettings(int len, int numCond, int BBAon, int BBAtype,
                           int elitismOn, int prodTaxInt, int CDOon,
                           int CEOon, int TCOon, int maxPost,
                           double bConst, double crossProb,
                           double mutateProb, double startStren,
                           double headTaxRate, double bidTaxRate,
                           double prodTaxRate, double eliteT,
                           double strenCap, double bCap)
   { length = len ;  numConditions = numCond;  BBAenabled = BBAon;
     BBAstyle = BBAtype;  elitismEnabled = elitismOn;
     producerTaxInterval = prodTaxInt;  CDOenabled = CDOon;
     CEOenabled = CEOon;  TCOenabled = TCOon;  maxActionsToPost = maxPost;
     bidConstant = bConst;  sexualProb = crossProb;
     mutationProb = mutateProb;  startStrength = startStren;
     headTax = headTaxRate;  bidTax = bidTaxRate;  producerTax = prodTaxRate;
     eliteThresh = eliteT;  strengthCap = strenCap;  bidCap = bCap;
   }
   static void setMasks(char *cMask, char *aMask);

   //-------------------------------------------------------------------------//
   // LCS execution cycle functions
   //-------------------------------------------------------------------------//
   void genetics() { crossOver();  mutation(); }
   void getMatches(messageBoard & msgBoard);
   void calcBids();
#ifdef NETWORK
   void adjustStrengths(networkInterface & network);
#else
   void adjustStrengths();
#endif
   void fillMsgBoard(messageBoard & msgBoard);
   void checkStrengths();
   void taxes();
};

#endif
