#ifndef MESSAGE_H
#define MESSAGE_H

//============================================================================
//  The message class is used as the basic message block for the entire
//  classifier system.  The classifierList class contains an array of
//  condition messages and an array of action messages.  The messageBoard
//  class contains an array of posted messages.  Note that all messages
//  have the same length, and therefore the 'length' parameter is static.
//  Also, we often refer to "words" instead of messages when we talk about
//  conditions and actions.  Conceptually there is a difference, but
//  syntactically, there isn't.
//============================================================================

class message
{
   static int length;  // Number of alleles (not incl. '\0').
   char *msg;          // Pointer to an allele string

 public:

   //-------------------------------------------------------------------------//
   // Initialization & reset functions and constructors
   //-------------------------------------------------------------------------//
   message();
   ~message();
   static void restoreDefaults();
   message(const char *str);
   void operator=(const char *str);
   message(message & msg2);
   void operator=(const message &msg2);

   //-------------------------------------------------------------------------//
   // Saving and loading functions
   //-------------------------------------------------------------------------//
   static void saveStatic(FILE *fptr) { fwrite(&length,sizeof(int),1,fptr); }
   static void loadStatic(FILE *fptr) { fread(&length,sizeof(int),1,fptr); }
   void save(FILE *fptr) { fwrite(msg,sizeof(char),length+1,fptr); }
   void load(FILE *fptr) { fread(msg,sizeof(char),length+1,fptr); }

   //-------------------------------------------------------------------------//
   // Message access functions and operators
   //-------------------------------------------------------------------------//
   static int size() { return length; }
   char & operator[](int i) const;
   int operator==(const message &msg2);
   int operator==(const char *str);
   int operator!=(const message &msg2);
   int operator!=(const char *str);

   void msgToStr(char *string);
};

inline void swap(message & msg1, message & msg2)
{ message temp = msg1;  msg1 = msg2;  msg2 = temp; }

#endif
