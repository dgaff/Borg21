#include"cfs.h"

// Static class memeber initialization
int messageBoard::length = DEF_MSGBOARD_LEN;

//============================================================================
// FUNCTION:     allocateAll()
//
// DESCRIPTION:  Allocates the message board arrays.
//
// I/O:          none
//============================================================================

void messageBoard::allocateAll()
{
  try
  {
  msgList = new message[length];
  supplierList = new int[length];
  bid = new double[length];
#ifdef NETWORK
  supplierID = new unsigned long[length];
#endif
  }
  catch(xalloc) { error(0,"messageBoard::allocateAll()"); }
}

//============================================================================
// FUNCTION:     deleteAll()
//
// DESCRIPTION:  Frees the message board arrays.
//
// I/O:          none
//============================================================================

void messageBoard::deleteAll()
{
  delete [] msgList;
  delete [] supplierList;
  delete [] bid;
#ifdef NETWORK
  delete [] supplierID;
#endif
}

//============================================================================
// FUNCTION:     reset()
//
// DESCRIPTION:  Frees and re-allocates arrays as necesssary.  Initializes
//               the suppliers to -2, indicating that message was not
//               posted by anyone, although this is not really necessary
//               since the actual length is set to 0.  I just like to
//               initialize things so there are no surprises.
//
// I/O:          none
//============================================================================

void messageBoard::reset(int firstReset)
{
  if (!firstReset)
    deleteAll();

  allocateAll();

  for (int i=0; i<length; i++)
  {
    supplierList[i] = -2;
#ifdef NETWORK
    supplierID[i] = 0;
#endif
    bid[i] = 0.0;
  }

  actualLength = 0;
}

//============================================================================
// FUNCTION:     softReset()
//
// DESCRIPTION:  Called by the classifier system softReset function to
//               reinitialize the system counters without disturbing the
//               state of the system.  This function is used to start a
//               new iteration using the current rules.  The actual length is
//               reset to zero.
//
// I/O:          none
//============================================================================

void messageBoard::softReset()
{
  actualLength = 0;
}

//============================================================================
// FUNCTION:     restoreDefaults()
//
// DESCRIPTION:  Restores static parameters to their default settings.  Reset
//               should be called after this function.
//
// I/O:          none
//============================================================================

void messageBoard::restoreDefaults()
{
  length = DEF_MSGBOARD_LEN;
}

//============================================================================
// FUNCTION:     postMessage()
//
// DESCRIPTION:  Used to post a message to the message board.  The location
//               to post and the index of the posting classifier must also
//               be supplied.  Note that a supplier of -2 means that no
//               classifier supported the message, and a supplier of -1 means
//               that the environment posted the message.  In these two cases,
//               bid should be zero.
//
// I/O:           -->  num       -  The location to which the message will
//                                  be posted.
//                -->  msg       -  The message to post.
//                -->  supplier  -  The classifier that posted the message.
//                -->  ID        -  (NETWORK CODE ONLY) the agent ID of the
//                                  agent that posted the message.
//                -->  ruleBid   -  The bid of the classifier that posted
//                                  the message.
//============================================================================
#ifdef NETWORK
void messageBoard::postMessage(int num, message & msg, int supplier,
                               unsigned long ID, double ruleBid)
{
  if (num >= length) error(1,"messageBoard::postMessage()");
  msgList[num] = msg;
  supplierList[num] = supplier;
  supplierID[num] = ID;
  bid[num] = ruleBid;
}
#else
void messageBoard::postMessage(int num, message & msg, int supplier,
                               double ruleBid)
{
  if (num >= length) error(1,"messageBoard::postMessage()");
  msgList[num] = msg;
  supplierList[num] = supplier;
  bid[num] = ruleBid;
}
#endif

//============================================================================
// FUNCTION:     getMessage()
//
// DESCRIPTION:  The exact opposite of postMessge().  Used to read messages
//               one the message board.  Note:  messages are NOT deleted when
//               they are read.  This is just so the environment can read
//               the messages and pick out the ones it wants.  If the
//               environment doesn't use some of the messages, it should
//               simply repost those messages and set the new actual message
//               board size.
//
// I/O:           -->  num       -  The location of the message to be read
//                <--  msg       -  Reference to the message
//                <--  supplier  -  The supplier
//                <--  ID        -  (NETWORK CODE ONLY) the agent ID
//                <--  ruleBid   -  The bid
//============================================================================

#ifdef NETWORK
void messageBoard::getMessage(int num, message & msg, int * supplier,
                              unsigned long *ID, double *ruleBid)
{
  if (num >= length) error(1,"messageBoard::getMessage()");
  msg = msgList[num];
  *supplier = supplierList[num];
  *ID = supplierID[num];
  *ruleBid = bid[num];
}
#else
void messageBoard::getMessage(int num, message & msg, int * supplier,
                              double *ruleBid)
{
  if (num >= length) error(1,"messageBoard::getMessage()");
  msg = msgList[num];
  *supplier = supplierList[num];
  *ruleBid = bid[num];
}
#endif
//============================================================================
// FUNCTION:     actualSize()
//
// DESCRIPTION:  Returns the actual size of the message board.  i.e. the
//               number of messages posted.  With an argument, this function
//               will also set the size of the message board.
//
// I/O:           -->  newSize  -  (optional) The new number of messages on
//                                 the message board.
//               <--               The size of the message board (or new size
//                                 if one is set).
//============================================================================

int messageBoard::actualSize(int newSize)
{
  if (newSize != -1)
    if (newSize <= length) actualLength = newSize;
    else error(1,"messageBoard::actualSize()");

  return actualLength;
}

//============================================================================
// FUNCTION:     save()
//
// DESCRIPTION:  Used to save the message board.  This function should be
//               called AFTER saveStatic().
//
// I/O:           -->  fptr  -  A pointer to an open, binary file.
//============================================================================

void messageBoard::save(FILE *fptr)
{
  for (int i=0; i<length; i++)
    msgList[i].save(fptr);

  fwrite(&actualLength,sizeof(int),1,fptr);
  fwrite(supplierList,sizeof(int),length,fptr);
#ifdef NETWORK
  fwrite(supplierID,sizeof(unsigned long),length,fptr);
#endif
  fwrite(bid,sizeof(double),length,fptr);
}

//============================================================================
// FUNCTION:     load()
//
// DESCRIPTION:  Used to load the message board.  This function should be
//               called AFTER saveStatic().  It is not necessary to call
//               reset after loading the message board.  This function
//               handles all deallocation and reallocation.
//
// I/O:           -->  fptr  -  A pointer to an open, binary file.
//============================================================================

void messageBoard::load(FILE *fptr)
{
  deleteAll();
  allocateAll();

  for (int i=0; i<length; i++)
    msgList[i].load(fptr);

  fread(&actualLength,sizeof(int),1,fptr);
  fread(supplierList,sizeof(int),length,fptr);
#ifdef NETWORK
  fread(supplierID,sizeof(unsigned long),length,fptr);
#endif
  fread(bid,sizeof(double),length,fptr);
}

//============================================================================
// FUNCTION:     getMaxBid()
//
// DESCRIPTION:  Gets the maximum bid of the messages posted on the message
//               board.  This function was written for network use, but can
//               be used by the environment interface if so desired.
//
// I/O:          none
//============================================================================

double messageBoard::getMaxBid()
{
  double maxBid = 0.0;
  for (int i=0; i<actualLength; i++)
    if (bid[i] > maxBid) maxBid = bid[i];

  return maxBid;
}

