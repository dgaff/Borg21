#ifndef MSGBOARD_H
#define MSGBOARD_H

//============================================================================
//  The message board class provides the message board onto which messages
//  are posted and from which messages are read.  Two sizes are used in the
//  message board.  One is the maximum size, and the other is the actual
//  size.  The classifier system may be unable to completely fill the
//  message board on any time step, so the actual size is adjusted by
//  classifierList after it posts.  In addition to storing messages, the
//  message board also stores the number of the classifier which posted the
//  message (the supplier).  A negative supplier number should be used for
//  those messages posted strictly by the environment.  The bid is also
//  placed on the message board for use in the support calculation.  If
//  the network code is compiled, the agent ID is also posted.
//============================================================================

class messageBoard
{
   static int length;    // Maximum number of messages on the message board.
   int actualLength;     // Actual number of messages on the message board.

   message *msgList;     // Array of messages.
   int *supplierList;    // Number of the classifier who posted the
                         //   corresponding message.
#ifdef NETWORK
   unsigned long *supplierID;     // ID Agent who posted the message
#endif
   double *bid;          // The bid of the supplier

   //-------------------------------------------------------------------------//
   // Functions to allocate and free all arrays
   //-------------------------------------------------------------------------//
   void allocateAll();
   void deleteAll();

 public:

   //-------------------------------------------------------------------------//
   // Initialization & reset functions
   //-------------------------------------------------------------------------//
   messageBoard() {}
   ~messageBoard() { deleteAll(); }
   void reset(int firstReset=0);
   void softReset();
   static void restoreDefaults();

   //-------------------------------------------------------------------------//
   // Message board access functions
   //-------------------------------------------------------------------------//
   int actualSize(int newSize = -1);
   static int size() { return length; }
#ifdef NETWORK
   void postMessage(int num, message & msg, int supplier, unsigned long ID,
                    double ruleBid);
   void getMessage(int num, message & msg, int * supplier, unsigned long *ID,
                   double *ruleBid);
#else
   void postMessage(int num, message & msg, int supplier, double ruleBid);
   void getMessage(int num, message & msg, int * supplier, double *ruleBid);
#endif
   double getMaxBid();

   //-------------------------------------------------------------------------//
   // Saving and loading functions
   //-------------------------------------------------------------------------//
   static void saveStatic(FILE *fptr) { fwrite(&length,sizeof(int),1,fptr); }
   static void loadStatic(FILE *fptr) { fread(&length,sizeof(int),1,fptr); }
   void save(FILE *fptr);
   void load(FILE *fptr);

   //-------------------------------------------------------------------------//
   // Static parameter access functions
   //-------------------------------------------------------------------------//
   static void getSettings(int *len) { *len = length; }
   static void setSettings(int len)  { length = len; }
};

#endif
