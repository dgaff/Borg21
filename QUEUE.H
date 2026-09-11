#ifdef NETWORK
#ifndef QUEUE_H
#define QUEUE_H

//==============================================================================
// The queueList structure is used to build a linked list of various data types.
// The embedded union uses pointers to the objects, since some of the structures
// that can be queued are several bytes long and require special contruction.
// By using pointers, the compiler allocates enough memory for a pointer,
// instead of enough memory for the largest object inside the union.  Since
// this queue is used for network message queuing, the source and destination
// ID's are included for every queueList object.  The queue class is
// responsible for linking and unlinking queueList objects as well as filling
// in object information.  Queue objects are removed in a FIFO fashion,
// either in data type order or in generic queueList object order.
//
// Each message type contains a put() function which allows its elements to
// be set quickly.
//
// NOTE:  queue overflow is indicated by a return value of zero from the add()
//        functions in the queue class.
//==============================================================================

#define DEF_MAX_QUEUE_LENGTH    100

/*
   The action message type is used to send action messages over the network.
   Supplier and bid information are also sent, since these quantities must
   be posted on the message board.
*/

struct actionMessage
{
  message action;
  int supplier;
  unsigned long source;
  double bid;

  void put(const message & act, int sup, unsigned long src, double b)
  { action = act;  supplier = sup;  source = src;  bid = b; }
};

/*
   The classifier message type is used to send classifiers over the network.
   The condition, action, and strength are the only information sent.
*/

struct classifierMessage
{
  message condition;
  message action;
  double strength;

  void put(const message & cond, const message & act, double stren)
  { condition = cond; action = act; strength = stren; }
};

/*
   The payoff message type is used to send BBA payoff messages over the
   network.  The rule to be paid (supplier) is given, as well as the payoff
   amount.  The queueList object holds the agent's destination ID.
*/

struct payoffMessage
{
  int supplier;
  double payoff;

  void put(int sup, double pay)
  { supplier = sup; payoff = pay; }
};

/*
   The generic node type used in the linked list.  The node automatically
   deletes the data stored in it when the node itself is deleted.
*/

struct queueList
{
  enum { noType, actMsgType, classMsgType, payoffMsgType };

  int type;              // one of the types in the enum above

  unsigned long source;  // Sending agent ID
  unsigned long dest;    // Receiving agent ID

  union
  {
    actionMessage     *actMsg;
    classifierMessage *classMsg;
    payoffMessage     *payoffMsg;
  };

  queueList *next;

  queueList() { type = noType; }
  queueList(int objectType);
  ~queueList()
  { if (type == actMsgType) delete actMsg;
    else if (type == classMsgType) delete classMsg;
    else if (type == payoffMsgType) delete payoffMsg;
  }
};

/*
    Manages the linked list.
*/

class queue
{
     int count,              // The total number of items in the queue
         maxLength;          // The maximum number of items allowed
     queueList *top,         // Top of the linked list
               *current;     // Most recent entry into the list

     int addNode(int type);
     void removeNode(queueList *item);
     queueList * findFirst(int type);
     void allocateAll();
     void deleteAll();

   public:

     queue() { reset(1); }
     ~queue() { deleteAll(); }
     void reset(int firstReset = 0);
     int countType(int type);
     void save(FILE *fptr);
     void load(FILE *fptr);
     int add(const queueList & qMsg);
     int add(unsigned long src, unsigned long dst, const actionMessage & qMsg);
     int add(unsigned long src, unsigned long dst, const classifierMessage & qMsg);
     int add(unsigned long src, unsigned long dst, const payoffMessage & qMsg);
     int remove(queueList & qMsg);
     int remove(actionMessage & qMsg);
     int remove(classifierMessage & qMsg);
     int remove(payoffMessage & qMsg);
};

#endif // ifndef QUEUE_H
#endif // ifdef NETWORK
