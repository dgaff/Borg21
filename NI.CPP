#include"cfs.h"

#ifdef NETWORK

// Static class memeber initialization
int networkInterface::actionPassingEnabled = DEF_ACTION_PASSING_STATE;
int networkInterface::classifierPassingEnabled = DEF_CLASS_PASSING_STATE;
int networkInterface::classifierTxInterval = DEF_CLASS_TX_INTERVAL;
int networkInterface::maxClassifiersToTx = DEF_MAX_CLASS_TO_TX;
int networkInterface::maxClassifiersToRx = DEF_MAX_CLASS_TO_RX;
int networkInterface::actionTxInterval = DEF_ACTION_TX_INTERVAL;
int networkInterface::maxActionsToTx = DEF_MAX_ACTIONS_TO_TX;
int networkInterface::maxActionsToRx = DEF_MAX_ACTIONS_TO_RX;
double networkInterface::TxStrenThresh = DEF_TX_STREN_THRESH;
double networkInterface::RxStrenThresh = DEF_RX_STREN_THRESH;
double networkInterface::TxBidThresh = DEF_TX_BID_THRESH;
double networkInterface::RxBidThresh = DEF_RX_BID_THRESH;

//============================================================================
// FUNCTION:     reset()
//
// DESCRIPTION:  This function is called when the DLCS is reset.  It simply
//               clears the queues.  Note that the 'firstReset' parameter
//               is not needed here because the queues get a reset(1) upon
//               instantiation.
//
// I/O:          none
//============================================================================

void networkInterface::reset()
{
  Tx.reset();
  Rx.reset();
}

//============================================================================
// FUNCTION:     softReset()
//
// DESCRIPTION:  Not used.  Provided for future compatibility.
//
// I/O:          none.
//============================================================================

void networkInterface::softReset()
{
}

//============================================================================
// FUNCTION:     restoreDefaults()
//
// DESCRIPTION:  Restores the DLCS network settings to their defaults.  Reset
//               must be called after this function.
//
// I/O:          none
//============================================================================

void networkInterface::restoreDefaults()
{
  actionPassingEnabled = DEF_ACTION_PASSING_STATE;
  classifierPassingEnabled = DEF_CLASS_PASSING_STATE;
  classifierTxInterval = DEF_CLASS_TX_INTERVAL;
  maxClassifiersToTx = DEF_MAX_CLASS_TO_TX;
  maxClassifiersToRx = DEF_MAX_CLASS_TO_RX;
  actionTxInterval = DEF_ACTION_TX_INTERVAL;
  maxActionsToTx = DEF_MAX_ACTIONS_TO_TX;
  maxActionsToRx = DEF_MAX_ACTIONS_TO_RX;
  TxStrenThresh = DEF_TX_STREN_THRESH;
  RxStrenThresh = DEF_RX_STREN_THRESH;
  TxBidThresh = DEF_TX_BID_THRESH;
  RxBidThresh = DEF_RX_BID_THRESH;
}

//============================================================================
// FUNCTION:     sortClassMsgs()
//
// DESCRIPTION:  This function sorts classifier message objects by from
//               highest to lowest strength, since the there will often be more
//               classifiers available than are allowed to be transmitted
//               or received.
//
// I/O:          --> classMsg  -  The array of classifier messages to be sorted.
//               --> num       -  The number of messages in the array.
//============================================================================

void networkInterface::sortClassMsgs(classifierMessage * classMsg, int num)
{
  for (int i=num-1; i>=0; i--)
    for (int j=0; j<i; j++)
      if (classMsg[j].strength < classMsg[j+1].strength)
      {
        swap(classMsg[j].condition,classMsg[j+1].condition);
        swap(classMsg[j].action,classMsg[j+1].action);
        swap(classMsg[j].strength,classMsg[j+1].strength);
      }
}

//============================================================================
// FUNCTION:     sortActMsgs()
//
// DESCRIPTION:  This function is called to sort actions messages from largest
//               to smallest strength, since there are often more eligible
//               action messages to be transmitted or received than are allowed.
//
// I/O:          --> actMsg  -  The array of action messages to be sorted.
//               --> num     -  The number of messages in the array.
//============================================================================

void networkInterface::sortActMsgs(actionMessage * actMsg, int num)
{
  for (int i=num-1; i>=0; i--)
    for (int j=0; j<i; j++)
      if (actMsg[j].bid < actMsg[j+1].bid)
      {
        swap(actMsg[j].action,actMsg[j+1].action);
        swap(actMsg[j].supplier,actMsg[j+1].supplier);
        swap(actMsg[j].bid,actMsg[j+1].bid);
        swap(actMsg[j].source,actMsg[j+1].source);
      }
}

//============================================================================
// FUNCTION:     postNetwork
//
// DESCRIPTION:
//
//   This function is called after messages are posted to the environment.
//   It performs either classifier passing, action passing, or both depending
//   on the flags set.  Note that classifier and action messages are only
//   passed on the interval defined by 'classifierTxInterval' and
//   'actionTxInterval', respectfully.
//
//   Classifier passing involves the broadcasting of rules over the network.
//   The rule's condition, action, and strength are sent.  In order to decide
//   what rules will get passed, this function determines a minimum strength
//   value required for classifier transmission.  Then, an array of eligible
//   classifier messages is created.  The array is sorted from highest strength
//   to lowest strength, and at most 'maxClassifiersToTx' of the highest-strength
//   rules are put into the transmit queue.  Note that the strength threshold
//   is based on the largest strength in the classifer list when this function
//   is called.  The 'TxStrenThresh' parameter specifies the fraction of
//   maximum strength that the threshold be, e.g. if the maximum strength is
//   50 and TxStrenThresh = .80, the threshold will be 50 * .80 = 40.  Then,
//   all rules with strength higher than 40 will be eligible for transmission.
//
//   Action passing involves the broadcasting of posted actions over the
//   network.  In other words, the posted actions from the current clock tick
//   can be used to communicate with other LCS agents.  This function is called
//   after the postOutput function for the environment.  Instead of a strength
//   threshold, action passing uses a bid threshold to decide which rules are
//   eligible for transmission.  The threshold is calculated in the same manner
//   as above.  The steps taken are as follows.  First, the function uses the
//   bid threshold to decide what rules on the message board are eligible
//   for transmission.  These rules are copied into an array of eligible
//   action messages.  An action message consists of a message, supplier,
//   agent ID, and a bid.  The agent ID is simply the unique long integer
//   assigned to the current classifier.  (An IP address, for example).  The
//   eligible action messages are then sorted from highest to lowest bid,
//   and at most 'maxActionsToTx' are put into the transmit queue.  Note that
//   each message on the message board has a ID associated with it.  This is
//   the ID of the agent who posted the message.  However, for the messages
//   on the message board at this point in the execution cycle, all the ID's
//   will be the same as the current 'agentID'.  The 'agentID' parameter is
//   still passed to this function for use in classifier passing, in which the
//   message board is not accessed.  It is important to note however, that all
//   posted messages on the message board at this time are from the current
//   LCS.
//
// I/O:          --> clockCount  -  The current clock tick.
//               --> msgBoard    -  Reference to the messsage board.
//               --> classList   -  Reference to the classifier list.
//============================================================================

void networkInterface::postNetwork(int clockCount,
                                   messageBoard & msgBoard,
                                   classifierList & classList)
{
  if (!classifierSystem::isNetworkOn()) return;

  // Classifier passing

  if (classifierPassingEnabled && (clockCount % classifierTxInterval) == 0)
  {
    int numClass = classList.size();
    double strenThresh = classList.getMaxStren() * TxStrenThresh,
           stren;
    message cond, act;
    classifierMessage *classToSend;

    try { classToSend = new classifierMessage[numClass]; }
    catch(xalloc) { error(0,"networkInterface::postNetwork()"); }

    // Copy classifiers that are eligible to be broadcast
    int j=0;
    for (int i=0; i<numClass; i++)
    {
      classList.getClassifier(i,cond,act,&stren);
      if (stren >= strenThresh)
        classToSend[j++].put(cond,act,stren);
    }

    // Sort the eligible classifiers from highest to lowest strength
    sortClassMsgs(classToSend,j);

    // Post min(maxClassifiersToTx, j) classifiers
    for (i=0; i<maxClassifiersToTx && i<j; i++)
      Tx.add(agentIDcopy,BROADCAST_ID,classToSend[i]);

    delete [] classToSend;
  }

  // Action passing

  if (actionPassingEnabled && (clockCount % actionTxInterval) == 0)
  {
    int msgBoardSize = msgBoard.actualSize(),
        supplier;
    message msg;
    double ruleBid,
           bidThresh = msgBoard.getMaxBid() * TxBidThresh;
    unsigned long ID; // ID of message on action board, will be same as agentID
    actionMessage *actToSend;

    try { actToSend = new actionMessage[msgBoardSize]; }
    catch(xalloc) { error(0,"networkInterface::postNetwork()"); }

    // Copy the action messages that are eligible to be broadcast
    int j=0;
    for (int i=0; i<msgBoardSize; i++)
    {
      msgBoard.getMessage(i,msg,&supplier,&ID,&ruleBid);
      if (ruleBid >= bidThresh)
        actToSend[j++].put(msg,supplier,ID,ruleBid);
    }

    // Sort the action messages from highest to lowest bid
    sortActMsgs(actToSend,j);

    // Post min(maxActionsToTx, j) action messages
    for (i=0; i<maxActionsToTx && i<j; i++)
      Tx.add(agentIDcopy,BROADCAST_ID,actToSend[i]);

    delete [] actToSend;
  }
}

//============================================================================
// FUNCTION:     readNetwork()
//
// DESCRIPTION:
//
//   This function is called before the readInput function from the environment.
//   The purpose of this function is to retrieve classifier messages, action
//   messages, or both from the receive queue.  The time interval in which
//   such messages will arrive is dictated by the transmission intervals used
//   in the postNetwork() function.  Note that in action passing, rules are
//   added to the END of the current message board so that they do not
//   replace internal rules already on the message board.  The environment is,
//   of course, free to manipulate the received actions as necessary, since
//   the read from the input interface occurs immediately after this function
//   is called.
//
//   Classifier passing works as follows.  All of the currently-enqueued
//   classifier messages are removed from the receive queue.  Those rules are
//   then sorted from highest to lowest strength.  As with rule transmission,
//   a strength threshold is calculated using the maximum strength in the
//   present classifier list (NOT the maximum strength of the received
//   classifiers).  This threshold keeps rules with strengths lower than the
//   majority of the rules in the classifier list from being added to the
//   list.  Up to 'maxClassifiersToRx' eligible rules are put into the classifier
//   list by probabilistically selecting weak rules in the list.  Note that
//   a "select and remove" scheme is used when selecting mulitple weak rules.
//   Therefore, if more than one classifier is to be added to the classifier
//   list, the "select and remove" scheme guarantees that each classifeir to
//   be added gets a unique spot in the message board.
//
//   Action passing works as follows.  First, all of the currently-enqueued
//   action messages are removed from the receive queue.  Those messages are
//   sorted from highest to lowest bid.  Again, a bid threshold is calculated
//   based on the maximum bid of the current message board (i.e. the
//   environment-posted messages).  Up to 'maxActionsToRx' messages with
//   bids higher than this threshold are posted to the message board.  Note
//   that if the message board is filled to capacity before 'maxActionsToRx'
//   is reached, no more messages will be posted.
//
// I/O:          --> msgBoard    -  Reference to the messsage board.
//               --> classList   -  Reference to the classifier list.
//============================================================================

void networkInterface::readNetwork(messageBoard & msgBoard,
                                   classifierList & classList)
{
  if (!classifierSystem::isNetworkOn()) return;

  // Classifier passing

  if (classifierPassingEnabled)
  {
    queueList temp;
    int numMsg = Rx.countType(temp.classMsgType),
        numClass = classList.size(),
        *eligible;
    double strenThresh = classList.getMaxStren() * RxStrenThresh;
    classifierMessage *classRecvd;

    if (numMsg > 0)
    {
      try
      { classRecvd = new classifierMessage[numMsg];
        eligible = new int[numClass];
      }
      catch(xalloc) { error(0,"networkInterface::readNetwork()"); }

      // The eligble array will be used to replace weak classifiers with those
      // from the receive queue.
      for (int i=0; i<numClass; i++) eligible[i] = 1;

      // Remove the classifiers from the queue
      for (i=0; i<numMsg; i++)
        Rx.remove(classRecvd[i]);

      // Sort the classifiers from largest to smallest strength
      sortClassMsgs(classRecvd,numMsg);

      // Put min(# of eligible classifiers, maxClassifiersToRx) in classifier list
      int j=0;
      for (i=0; i<numMsg && j<maxClassifiersToRx; i++)
        if (classRecvd[i].strength >= strenThresh)
        {
          j++;
          int loc = classList.pickWeakRule(eligible);
          classList.addClassifier(loc, classRecvd[i].condition,
                                  classRecvd[i].action, classRecvd[i].strength);
        }

      delete [] classRecvd;
      delete [] eligible;
    }
  }

  // Action passing

  if (actionPassingEnabled)
  {
    queueList temp;
    int numMsg = Rx.countType(temp.actMsgType),
        msgBoardIndex = msgBoard.actualSize(),
        maxMsgBoardSize = msgBoard.size();
    double bidThresh = msgBoard.getMaxBid() * RxBidThresh;
    actionMessage *actRecvd;

    if (numMsg > 0)
    {
      try { actRecvd = new actionMessage[numMsg]; }
      catch(xalloc) { error(0,"networkInterface::readNetwork()"); }

      // Remove the action messages from the queue
      for (int i=0; i<numMsg; i++)
        Rx.remove(actRecvd[i]);

      // Sort the action messages from highest to lowest bid
      sortActMsgs(actRecvd,numMsg);

      // Post min( # of message slots left on the message board, 'maxActionsToRx',
      // # of eligible actions) actions on the message board
      int j=0;
      for (i=0; i<numMsg && j<maxActionsToRx && msgBoardIndex < maxMsgBoardSize;
           i++)
        if (actRecvd[i].bid >= bidThresh)
        {
          msgBoard.postMessage(msgBoardIndex, actRecvd[i].action,
                               actRecvd[i].supplier, actRecvd[i].source,
                               actRecvd[i].bid);
          j++;  msgBoardIndex++;
        }

      // Set the new message board size
      msgBoard.actualSize(msgBoardIndex);

      delete [] actRecvd;
    }
  }
}

//============================================================================
// FUNCTION:     sendPayoff()
//
// DESCRIPTION:
//
//   This function is used to send a BBA payoff message to another LCS agent.
//   For example, if a rule from another agent is sent across the network
//   and triggers a rule in this LCS, this LCS can pay the "supplier"
//   across the network.  Payoff messages are given a specific destination,
//   rather than a broadcast, although this is not absolutely necessary.  I've
//   just done it for convenience.  The payoff message could instead by
//   broadcast, and each agent would check to see if the message belongs to
//   it.  I wanted to reduce network traffic.
//
// I/O:          --> source   - The sending agent ID.
//               --> dest     - The receiving agent ID.
//               --> supplier - The rule in the receiving agent to get paid.
//               --> payoff   - The amount to pay the rule.
//============================================================================

void networkInterface::sendPayoff(unsigned long source, unsigned long dest,
                                  int supplier, double payoff)
{
  if (!classifierSystem::isNetworkOn()) return;

  payoffMessage qMsg;

  qMsg.put(supplier,payoff);
  Tx.add(source,dest,qMsg);
}

//============================================================================
// FUNCTION:     readPayoff()
//
// DESCRIPTION:
//
//   This function checks for rule payoff messages in the receive queue and
//   adjusts the strengths of the affected rules accordingly.  Note that this
//   function assumes that the network has only delivered payoff messages to
//   this LCS if they are explicity for this LCS.  (Payoff messages are not
//   broadcast, they are sent to a specific destination.)  In other words,
//   this function doesn't check the destination ID.
//
// I/O:          --> classList  -  reference to the classifier list.
//============================================================================

void networkInterface::readPayoff(classifierList & classList)
{
  if (!classifierSystem::isNetworkOn()) return;

  payoffMessage qMsg;

  while (Rx.remove(qMsg))
    classList.payoffRule(qMsg.supplier,qMsg.payoff);
}

//============================================================================
// FUNCTION:     saveStatic()
//
// DESCRIPTION:  Used to save the static parameters for the network interface.
//               This function must be called before save().
//
// I/O:          --> fptr  -  Pointer to an open BINARY file set to write.
//============================================================================

void networkInterface::saveStatic(FILE *fptr)
{
  fwrite(&actionPassingEnabled,sizeof(int),1,fptr);
  fwrite(&classifierPassingEnabled,sizeof(int),1,fptr);
  fwrite(&classifierTxInterval,sizeof(int),1,fptr);
  fwrite(&maxClassifiersToTx,sizeof(int),1,fptr);
  fwrite(&maxClassifiersToRx,sizeof(int),1,fptr);
  fwrite(&actionTxInterval,sizeof(int),1,fptr);
  fwrite(&maxActionsToTx,sizeof(int),1,fptr);
  fwrite(&maxActionsToRx,sizeof(int),1,fptr);
  fwrite(&TxStrenThresh,sizeof(double),1,fptr);
  fwrite(&RxStrenThresh,sizeof(double),1,fptr);
  fwrite(&TxBidThresh,sizeof(double),1,fptr);
  fwrite(&RxBidThresh,sizeof(double),1,fptr);
}

//============================================================================
// FUNCTION:     loadStatic()
//
// DESCRIPTION:  Used to load the static parameters for the network interface.
//               This function must be called before load().
//
// I/O:          --> fptr  -  Pointer to an open BINARY file set to read.
//============================================================================

void networkInterface::loadStatic(FILE *fptr)
{
  fread(&actionPassingEnabled,sizeof(int),1,fptr);
  fread(&classifierPassingEnabled,sizeof(int),1,fptr);
  fread(&classifierTxInterval,sizeof(int),1,fptr);
  fread(&maxClassifiersToTx,sizeof(int),1,fptr);
  fread(&maxClassifiersToRx,sizeof(int),1,fptr);
  fread(&actionTxInterval,sizeof(int),1,fptr);
  fread(&maxActionsToTx,sizeof(int),1,fptr);
  fread(&maxActionsToRx,sizeof(int),1,fptr);
  fread(&TxStrenThresh,sizeof(double),1,fptr);
  fread(&RxStrenThresh,sizeof(double),1,fptr);
  fread(&TxBidThresh,sizeof(double),1,fptr);
  fread(&RxBidThresh,sizeof(double),1,fptr);
}

//============================================================================
// FUNCTION:     save()
//
// DESCRIPTION:  Used to save the network interface.  This function must be
//               called after saveStatic();
//
// I/O:          --> fptr  -  Pointer to an open BINARY file set to write.
//============================================================================

void networkInterface::save(FILE *fptr)
{
  fwrite(&agentIDcopy,sizeof(unsigned long),1,fptr);
  Tx.save(fptr);
  Rx.save(fptr);
}

//============================================================================
// FUNCTION:     load()
//
// DESCRIPTION:  Used to load the network interface.  This function must be
//               called after loadStatic();
//
// I/O:          --> fptr  -  Pointer to an open BINARY file set to read.
//============================================================================

void networkInterface::load(FILE *fptr)
{
  fread(&agentIDcopy,sizeof(unsigned long),1,fptr);
  Tx.load(fptr);
  Rx.load(fptr);
}

#endif
