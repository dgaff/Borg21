#include"cfs.h"

#ifdef NETWORK

//============================================================================
// FUNCTION:     queueList()
//
// DESCRIPTION:  The constructor for a generic queue object.  Allocates space
//               based on the type passed to the constructor.
//
// I/O:          --> objectType  -  the type of queue object (see queueList
//                                  class)
//============================================================================

queueList::queueList(int objectType)
{
  type = objectType;
  if (type == actMsgType)
  {
    try { actMsg = new actionMessage; }
    catch(xalloc) { error(0,"queueList::queueList()"); }
  }
  else if (type == classMsgType)
  {
    try { classMsg = new classifierMessage; }
    catch(xalloc) { error(0,"queueList::queueList()"); }
  }
  else
  {
    try { payoffMsg = new payoffMessage; }
    catch(xalloc) { error(0,"queueList::queueList()"); }
  }
}

//============================================================================
// FUNCTION:     deleteAll()
//
// DESCRIPTION:  Deletes all the nodes in the queue, including the top.  Used
//               in the queue destructor and in loading a new queue.
//
// I/O:          none
//============================================================================

void queue::deleteAll()
{
  queueList *search = top->next,
            *next;

  // Delete the top node
  delete top;

  // Delete the sub nodes
  for (int i=0; i<count; i++)
  {
    next = search->next;
    delete search;
    search = next;
  }
}

//============================================================================
// FUNCTION:     allocateAll()
//
// DESCRIPTION:  Allocates a top node (of noType).  Reset the counter.  Used
//               in the constructor and in loading a new queue.
//
// I/O:          none
//============================================================================

void queue::allocateAll()
{
  try { top = current = new queueList; }
  catch(xalloc) { error(0,"queue::allocateAll()"); }

  count = 0;
}

//============================================================================
// FUNCTION:     reset()
//
// DESCRIPTION:  Clears the queue and prepares it for use.
//
// I/O:          --> firstReset  -  (optional) used by constructor to
//                                  indicating the first time the queue has
//                                  been instantiated.
//============================================================================

void queue::reset(int firstReset)
{
  if (!firstReset)
    deleteAll();

  maxLength = DEF_MAX_QUEUE_LENGTH;

  allocateAll();
}

//============================================================================
// FUNCTION:     countType()
//
// DESCRIPTION:  Counts the number of messages of a particular type in the
//               queue.
//
// I/O:          --> type  -  The type to count.
//               <--       -  The number of this type in the queue.
//============================================================================

int queue::countType(int type)
{
  queueList *search = top->next;
  int i=0, typeCount=0;

  while(i<count)
  {
    if (search->type == type) typeCount++;
    i++;
    search=search->next;
  }

  return typeCount;
}

//============================================================================
// FUNCTION:     findFirst()
//
// DESCRIPTION:  Returns a pointer to the first occurence of a particular
//               type.  Used for FIFO extraction by type.  Returns zero if
//               none of the given type are found.
//
// I/O:          --> type  -  The type to search for.
//               <--       -  A pointer to the first occurence of the type.
//============================================================================

queueList * queue::findFirst(int type)
{
  queueList *search = top->next;
  int i=0;

  while(i<count)
  {
    if (search->type == type) return search;
    i++;
    search=search->next;
  }

  return 0;
}

//============================================================================
// FUNCTION:     addNode()
//
// DESCRIPTION:  Used to add a node of the given type to the queue.  The
//               current pointer is set to this newly-added node.
//
// I/O:          --> type  -  The type of node to add.
//               <--       -  Returns one on success, zero on failure
//============================================================================

int queue::addNode(int type)
{
  if (count == maxLength) return 0;

  try { current = current->next = new queueList(type); }
  catch(xalloc) { error(0,"queue::addNode()"); }

  ++count;

  return 1;
}

//============================================================================
// FUNCTION:     removeNode()
//
// DESCRIPTION:  Private function called by the remove() functions.  This
//               function assumes that the item passed to it is a valid item.
//               This function unlinks and deletes the node.
//
// I/O:          --> item  -  pointer to the node to be removed
//============================================================================

void queue::removeNode(queueList *item)
{
  if (item == top) return;

  queueList *search = top->next,
            *prev = top;

  // Find the item to be deleted
  while(search != item) { prev = search;  search = search->next; }

  // Move the current pointer if item is the last in the list
  if (current == search) current = prev;

  // Hook previous and next objects together
  prev->next = search->next;

  // Decrement the item counter
  --count;

  // Delete the item
  delete item;
}

//============================================================================
// FUNCTION:     add()
//
// DESCRIPTION:  Accepts a reference to a queueList object, and copies that
//               object to the queue.  This function is used when adding
//               generic objects to the queue.  Usually these objects were
//               removed with the following remove() function.  Normally this
//               is only used to simulate network operation.  (See task.cpp)
//
// I/O:          --> qMsg  -  generic message object to be added.
//               <--       -  Returns one on success, zero on failure
//============================================================================

int queue::add(const queueList & qMsg)
{
  if (!addNode(qMsg.type)) return 0;

  current->source = qMsg.source;
  current->dest = qMsg.dest;

  if (qMsg.type == qMsg.actMsgType)
    current->actMsg->put(qMsg.actMsg->action, qMsg.actMsg->supplier,
                         qMsg.actMsg->source, qMsg.actMsg->bid);
  else if (qMsg.type == qMsg.classMsgType)
    current->classMsg->put(qMsg.classMsg->condition, qMsg.classMsg->action,
                           qMsg.classMsg->strength);
  else
    current->payoffMsg->put(qMsg.payoffMsg->supplier, qMsg.payoffMsg->payoff);

  return 1;
}

//============================================================================
// FUNCTION:     remove()
//
// DESCRIPTION:  Accepts a queueList reference which will be allocated to the
//               appropriate type based on the current object being removed.
//               The first object in the queue is copied into qMsg and then
//               deleted.  This function is used for network simulation.
//               (see task.cpp)
//
// I/O:          <-- qMsg  -  The queueList object in which to store the
//                            object removed from the queue.
//               <--       -  returns a one if an object was removed from the
//                            queue and a zero if not.
//============================================================================

int queue::remove(queueList & qMsg)
{
  if (current == top) return 0;

  // get the first item in the queue
  queueList *item = top->next;

  // delete the previous type allocation
  if (qMsg.type != qMsg.noType)
  {
    if (qMsg.type == qMsg.actMsgType) delete qMsg.actMsg;
    else if (qMsg.type == qMsg.classMsgType) delete qMsg.classMsg;
    else delete qMsg.payoffMsg;
  }

  // Set the new type.  Copy source and destination.
  qMsg.type = item->type;
  qMsg.source = item->source;
  qMsg.dest = item->dest;

  // Based on the new type, allocate space for the object, save the object
  // info.
  if (item->type == top->actMsgType)
  {
    try { qMsg.actMsg = new actionMessage; }
    catch(xalloc) { error(0,"queue::remove(queueList)"); }
    qMsg.actMsg->put(item->actMsg->action, item->actMsg->supplier,
                     item->actMsg->source, item->actMsg->bid);
  }
  else if (item->type == top->classMsgType)
  {
    try { qMsg.classMsg = new classifierMessage; }
    catch(xalloc) { error(0,"queue::remove(queueList)"); }
    qMsg.classMsg->put(item->classMsg->condition, item->classMsg->action,
                       item->classMsg->strength);
  }
  else
  {
    try { qMsg.payoffMsg = new payoffMessage; }
    catch(xalloc) { error(0,"queue::remove(queueList)"); }
    qMsg.payoffMsg->put(item->payoffMsg->supplier, item->payoffMsg->payoff);
  }

  // Delete the current node from the queue
  removeNode(item);

  return 1;
}

//============================================================================
// FUNCTION:     add()
//
// DESCRIPTION:  Adds an action message to the queue.
//
// I/O:          --> src   -  The sending agent
//               --> dst   -  The receiving agent (usually 0 indicating a broadcast)
//               --> qMsg  -  The action message to add.
//               <--       -  Returns one on success, zero on failure
//============================================================================

int queue::add(unsigned long src, unsigned long dst, const actionMessage & qMsg)
{
  if (!addNode(top->actMsgType)) return 0;

  current->source = src;
  current->dest = dst;
  current->actMsg->put(qMsg.action, qMsg.supplier, qMsg.source, qMsg.bid);

  return 1;
}

//============================================================================
// FUNCTION:     remove()
//
// DESCRIPTION:  Removes an action message from the queue.
//
//
// I/O:          <-- qMsg  -  The queueList object in which to store the
//                            action message removed from the queue.
//               <--       -  returns a one if an object was removed from the
//                            queue and a zero if not.
//============================================================================

int queue::remove(actionMessage & qMsg)
{
  queueList *list = findFirst(top->actMsgType);
  if (!list) return 0;

  qMsg.put(list->actMsg->action, list->actMsg->supplier, list->actMsg->source,
           list->actMsg->bid);

  removeNode(list);

  return 1;
}

//============================================================================
// FUNCTION:     add()
//
// DESCRIPTION:  Adds a classifier message to the queue.
//
// I/O:          --> src   -  The sending agent
//               --> dst   -  The receiving agent (usually 0 indicating a broadcast)
//               --> qMsg  -  The classifier message to add.
//               <--       -  Returns one on success, zero on failure
//============================================================================

int queue::add(unsigned long src, unsigned long dst, const classifierMessage & qMsg)
{
  if (!addNode(top->classMsgType)) return 0;

  current->source = src;
  current->dest = dst;
  current->classMsg->put(qMsg.condition, qMsg.action, qMsg.strength);

  return 1;
}

//============================================================================
// FUNCTION:     remove()
//
// DESCRIPTION:  Removes a classifier message from the queue.
//
// I/O:          <-- qMsg  -  The queueList object in which to store the
//                            classifier message removed from the queue.
//               <--       -  returns a one if an object was removed from the
//                            queue and a zero if not.
//============================================================================

int queue::remove(classifierMessage & qMsg)
{
  queueList *list = findFirst(top->classMsgType);
  if (!list) return 0;

  qMsg.put(list->classMsg->condition, list->classMsg->action,
           list->classMsg->strength);

  removeNode(list);

  return 1;
}

//============================================================================
// FUNCTION:     add()
//
// DESCRIPTION:  Adds a BBA payoff message to the queue.
//
// I/O:          --> src   -  The sending agent
//               --> dst   -  The receiving agent (usually 0 indicating a broadcast)
//               --> qMsg  -  The BBA payoff message to add.
//               <--       -  Returns one on success, zero on failure
//============================================================================

int queue::add(unsigned long src, unsigned long dst, const payoffMessage & qMsg)
{
  if (!addNode(top->payoffMsgType)) return 0;

  current->source = src;
  current->dest = dst;
  current->payoffMsg->put(qMsg.supplier, qMsg.payoff);

  return 1;
}

//============================================================================
// FUNCTION:     remove()
//
// DESCRIPTION:  Removes a BBA payoff message from the queue.
//
// I/O:          <-- qMsg  -  The queueList object in which to store the
//                            BBA payoff message removed from the queue.
//               <--       -  returns a one if an object was removed from the
//                            queue and a zero if not.
//============================================================================

int queue::remove(payoffMessage & qMsg)
{
  queueList *list = findFirst(top->payoffMsgType);
  if (!list) return 0;

  qMsg.put(list->payoffMsg->supplier, list->payoffMsg->payoff);

  removeNode(list);

  return 1;
}

//============================================================================
// FUNCTION:     save()
//
// DESCRIPTION:  Saves all the elements in the queue.  This function is called
//               by the classifierSystem class when a save is initiated.
//
// I/O:          --> fptr  -  A pointer to an open BINARY file set for writing.
//============================================================================

void queue::save(FILE *fptr)
{
  fwrite(&count,sizeof(int),1,fptr);
  fwrite(&maxLength,sizeof(int),1,fptr);

  queueList *search = top->next;

  for (int i=0; i<count; i++)
  {
    fwrite(&search->type,sizeof(int),1,fptr);
    fwrite(&search->source,sizeof(unsigned long),1,fptr);
    fwrite(&search->dest,sizeof(unsigned long),1,fptr);

    if (search->type == search->actMsgType)
    {
      search->actMsg->action.save(fptr);
      fwrite(&search->actMsg->supplier,sizeof(int),1,fptr);
      fwrite(&search->actMsg->source,sizeof(unsigned long),1,fptr);
      fwrite(&search->actMsg->bid,sizeof(double),1,fptr);
    }
    else if (search->type == search->classMsgType)
    {
      search->classMsg->condition.save(fptr);
      search->classMsg->action.save(fptr);
      fwrite(&search->classMsg->strength,sizeof(double),1,fptr);
    }
    else
    {
      fwrite(&search->payoffMsg->supplier,sizeof(int),1,fptr);
      fwrite(&search->payoffMsg->payoff,sizeof(double),1,fptr);
    }

    search=search->next;
  }

}

//============================================================================
// FUNCTION:     load()
//
// DESCRIPTION:  Loads elements in a queue from disk.  This function is called
//               by the classifierSystem class when a load is initiated.
//
// I/O:          --> fptr  -  A pointer to an open BINARY file set for reading.
//============================================================================

void queue::load(FILE *fptr)
{
  deleteAll();     // Deletes the current queue
  allocateAll();   // Starts a new queue in memory

  int tempCount;

  fread(&tempCount,sizeof(int),1,fptr);
  fread(&maxLength,sizeof(int),1,fptr);

  for (int i=0; i<tempCount; i++)
  {
    int type;
    unsigned long source, dest;

    fread(&type,sizeof(int),1,fptr);
    fread(&source,sizeof(unsigned long),1,fptr);
    fread(&dest,sizeof(unsigned long),1,fptr);

    if (type == top->actMsgType)
    {
      actionMessage actMsg;
      actMsg.action.load(fptr);
      fread(&actMsg.supplier,sizeof(int),1,fptr);
      fread(&actMsg.source,sizeof(unsigned long),1,fptr);
      fread(&actMsg.bid,sizeof(double),1,fptr);

      add(source,dest,actMsg);
    }
    else if (type == top->classMsgType)
    {
      classifierMessage classMsg;
      classMsg.condition.load(fptr);
      classMsg.action.load(fptr);
      fread(&classMsg.strength,sizeof(double),1,fptr);

      add(source,dest,classMsg);
    }
    else
    {
      payoffMessage payoffMsg;
      fread(&payoffMsg.supplier,sizeof(int),1,fptr);
      fread(&payoffMsg.payoff,sizeof(double),1,fptr);

      add(source,dest,payoffMsg);
    }
  }
}

#endif
