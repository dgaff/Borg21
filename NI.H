#ifdef NETWORK

//==============================================================================
// The networkInterface class provides network message queueing for use in
// communication between mutliple DLCS agents  An LCS can pass three kinds
// of messages:
//
//   1.  Classifiers Messages
//   2.  Action Messages
//   3.  Payoff Messages
//
// Classifier message passing allows multiple agents to share "good" rules.
// High-strength rules are broadcast over the network to all LCS agents.
// Action message passing allows agents to "communicate" with one another.
// In other words, one agent can send an action message over the network which
// causes a rule in another agent to get selected.  In this way, chaining
// can occur between multiple agents.  Payoff messages are primarily used for
// BBA payoff across the network.  i.e., when a chain has formed across the
// network, and the end of the chain gets selected to post, the selected
// rule pays its supplier by sending a payoff message over the network.
//
// The network interface transmits and receives messages by posting to a
// transmit queue and reading from a receive queue.  In this way, the LCS
// does not have to stop and wait for messages (which it couldn't do anyway
// because it wouldn't know exactly how many messages it was waiting for).
// The LCS simply writes to and reads from the queues at specific points in
// the execution cycle.  The actual network is responsible for removing the
// messages from the transmit queue and transmitting them, and for putting all
// incoming messages into the receive queue.
//==============================================================================

class networkInterface
{
    static int actionPassingEnabled,      // 1 == action passing enabled
               classifierPassingEnabled,  // 1 == classifier passing enabled
               classifierTxInterval,      // Frequency of classifier transmission
               maxClassifiersToTx,        // Maximum no. of classifiers to transmit
               maxClassifiersToRx,        // Maximum no. of classifiers to receive
               actionTxInterval,          // Frequency of action transmission
               maxActionsToTx,            // Maximum no. of actions to transmit
               maxActionsToRx;            // Maximum no. of actions to receive
    static double TxStrenThresh,          // Transmit strength threshold
                  RxStrenThresh,          // Receive strength threshold
                  TxBidThresh,            // Transmit bid threshold
                  RxBidThresh;            // Receive bid threshold

    queue Tx, Rx;                         // Transmit and Receive queues
    unsigned long agentIDcopy;            // ID copied from classifierSystem class

    void sortClassMsgs(classifierMessage *classMsg, int num);
    void sortActMsgs(actionMessage *actMsg, int num);

  public:

    //------------------------------------------------------------------------//
    // Initialization and reset functions.
    //------------------------------------------------------------------------//
    networkInterface() { }
    ~networkInterface() { }
    void reset();
    void softReset();
    static void restoreDefaults();

    //------------------------------------------------------------------------//
    // Network interface access functions.
    //------------------------------------------------------------------------//
    void setAgentID(unsigned long ID) { agentIDcopy = ID; }

    void postNetwork(int clockCount, messageBoard & msgBoard,
                     classifierList & classList);
    void readNetwork(messageBoard & msgBoard, classifierList & classList);
    void sendPayoff(unsigned long source, unsigned long dest, int supplier,
                    double payoff);
    void readPayoff(classifierList & classList);

    int removeFromTxQueue(queueList & qMsg)
    { return Tx.remove(qMsg); }
    void addToRxQueue(const queueList & qMsg)
    { Rx.add(qMsg); }
    static int isActionPassingOn() { return actionPassingEnabled; }
    static int isClassifierPassingOn() { return classifierPassingEnabled; }

    //------------------------------------------------------------------------//
    // Static parameter access functions.
    //------------------------------------------------------------------------//
    static void setSettings(int actPass, int classPass, int classTxInt,
                            int maxClassTx, int maxClassRx, int actTxInt,
                            int maxActTx, int maxActRx,
                            double TxStren, double RxStren,
                            double TxBid, double RxBid)
    {
      actionPassingEnabled = actPass; classifierPassingEnabled = classPass;
      classifierTxInterval = classTxInt; maxClassifiersToTx = maxClassTx;
      maxClassifiersToRx = maxClassRx; actionTxInterval = actTxInt;
      maxActionsToTx = maxActTx; maxActionsToRx = maxActRx;
      TxStrenThresh = TxStren; RxStrenThresh = RxStren;
      TxBidThresh = TxBid; RxBidThresh = RxBid;
    }

    static void getSettings(int *actPass, int *classPass, int *classTxInt,
                            int *maxClassTx, int *maxClassRx, int *actTxInt,
                            int *maxActTx, int *maxActRx,
                            double *TxStren, double *RxStren,
                            double *TxBid, double *RxBid)
    {
      *actPass = actionPassingEnabled; *classPass = classifierPassingEnabled;
      *classTxInt = classifierTxInterval; *maxClassTx = maxClassifiersToTx;
      *maxClassRx = maxClassifiersToRx; *actTxInt = actionTxInterval;
      *maxActTx = maxActionsToTx; *maxActRx = maxActionsToRx;
      *TxStren = TxStrenThresh; *RxStren = RxStrenThresh;
      *TxBid = TxBidThresh; *RxBid = RxBidThresh;
    }

    //------------------------------------------------------------------------//
    // Saving and loading functions.
    //------------------------------------------------------------------------//
    static void saveStatic(FILE *fptr);
    static void loadStatic(FILE *fptr);
    void save(FILE *fptr);
    void load(FILE *fptr);
};

#endif
