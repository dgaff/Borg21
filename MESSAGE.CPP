#include"cfs.h"

// Static class memeber initialization
int message::length = DEF_MSG_LEN;

//============================================================================
// FUNCTION:     message()
//
// DESCRIPTION:  Message constructor.  Allocates space for the message and
//               initializes all alleles to '0';
//
// I/O:          none
//============================================================================

message::message()
{
  try { msg = new char[length+1]; }
  catch(xalloc) { error(0,"message::message()"); }
  for (int i=0; i<length; i++)
    msg[i] = '0';
  msg[length] = '\0';
}

//============================================================================
// FUNCTION:     ~message()
//
// DESCRIPTION:  Message destructor.  Frees message memory.
//
// I/O:          none
//============================================================================

message::~message()
{
  delete [] msg;
}

//============================================================================
// FUNCTION:     restoreDefaults()
//
// DESCRIPTION:  Restores the static parameters to their default values.
//
// I/O:          none
//============================================================================

void message::restoreDefaults()
{
  length = DEF_MSG_LEN;
}

//============================================================================
// FUNCTION:     operator[]()
//
// DESCRIPTION:  Overloaded array dereference operator.  Gives direct access
//               to the alleles of a message.  The function is const so that
//               it can be used to access both constant and non-constant
//               messages.
//
// I/O:           -->  i  -  The element to return.
//               <--      -  A reference to the char array storing the
//                           alleles.
//============================================================================

char & message::operator[](int i) const
{
 if (i >= length) error(1,"message::operator[]");
 return msg[i];
}

//============================================================================
// FUNCTION:     operator==()
//
// DESCRIPTION:  Overloaded equivalency operator.  Checks to see if two
//               messages are equivalent.  Equivalent is defined as
//                 0 == 0
//                 1 == 1
//                 # == 0, 1, or #
//
// I/O:           -->  msg2  -  Reference to second message to be compared.
//               <--         -  1 for equivalent, 0 for not equivalent.
//============================================================================

int message::operator==(const message &msg2)
{
 int equivalent = 1;

 for (int i=0; i<length; i++)
   if (msg[i] != '#' && msg2.msg[i] != '#' &&
       msg[i] != msg2.msg[i]) equivalent = 0;

 return equivalent;
}

//============================================================================
// FUNCTION:     operator!=()
//
// DESCRIPTION:  Overloaded non-equivalency operator.  Checks to see if two
//               messages are different.  See operator==().
//
// I/O:           -->  msg2  -  Reference to second message to be compared.
//               <--         -  1 for not equivalent, 0 for equivalent.
//============================================================================

int message::operator!=(const message &msg2)
{
 if (*this == msg2) return 0;
 else return 1;
}

//============================================================================
// FUNCTION:     operator==()
//
// DESCRIPTION:  Same as previous, except compares message to a string.
//
// I/O:           -->  str  -  Pointer to string to be compared.
//               <--        -  1 for equivalent, 0 for not equivalent.
//============================================================================

int message::operator==(const char *str)
{
 int equivalent = 1;

 if (strlen(str) != length) error(1,"message::operator==(char*)");

 for (int i=0; i<length; i++)
   if (msg[i] != '#' && str[i] != '#' &&
       msg[i] != str[i]) equivalent = 0;

 return equivalent;
}

//============================================================================
// FUNCTION:     operator!=()
//
// DESCRIPTION:  Same as previous, except compares message to a string.
//
// I/O:           -->  str  -  Pointer to string to be compared.
//               <--        -  1 for not equivalent, 0 for equivalent.
//============================================================================

int message::operator!=(const char *str)
{
 if (*this == str) return 0;
 else return 1;
}

//============================================================================
// FUNCTION:     message() copy constructor with string assignment
//
// DESCRIPTION:  Copy constructor used to create and initialize messages in
//               the following manner:  message msg = "0##11";
//
// I/O:           -->  str  -  The string to be assigned to the message.
//============================================================================

message::message(const char *str)
{
 if (strlen(str) != length) error(1,"message::message(char*)");

 try { msg = new char[length+1]; }
 catch(xalloc) { error(0,"message::message(char*)"); }
 msg[length] = '\0';  // Ensure that the string will be null-terminated.
 strcpy(msg,str);
}

//============================================================================
// FUNCTION:     operator=()
//
// DESCRIPTION:  Assignment operator overload.  This function is used to
//               assign strings to a message which has already been defined:
//                 message msg;
//                 msg = "0##11";
//
// I/O:           -->  str  -  The string to be assigned to the message.
//============================================================================

void message::operator=(const char *str)
{
 if (strlen(str) != length) error(1,"message::operator=(char*)");

 strcpy(msg,str);
}

//============================================================================
// FUNCTION:     message() copy constructor with message assignment
//
// DESCRIPTION:  Copy constructor used to create and initalize a message
//               with another message:
//                   message msg1 = msg2;
//               with msg2 defined elsewhere.
//
// I/O:           -->  msg2  -  The message to be assigned to newly-created
//                              message.
//============================================================================

message::message(message & msg2)
{
 try { msg = new char[length+1]; }
 catch(xalloc) { error(0,"message::message(&)"); }
 msg[length] = '\0';
 strcpy(msg,msg2.msg);
}

//============================================================================
// FUNCTION:     operatore=()
//
// DESCRIPTION:  Overloaded assignment operator with message initialzation.
//               This fucntion assigns a message to another message which
//               as already been created:
//                   message msg1;
//                   msg2 = msg1;
//               with msg2 defined elsewhere.
//
// I/O:           -->  msg2  -  The message to be assigned to given message.
//============================================================================

void message::operator=(const message &msg2)
{
 strcpy(msg,msg2.msg);
}

//============================================================================
// FUNCTION:     msgToStr()
//
// DESCRIPTION:  Converts the given message to a string.
//
// I/O:          <-- string  -  the string version of the message.
//============================================================================

void message::msgToStr(char *string)
{
 for (int i=0; i<length; i++)
   string[i] = msg[i];

 string[i] = '\0';
}


