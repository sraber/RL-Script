#include <string>
#include <vector>
#include <map>
#include <list>
#include <Debug.h>

typedef std::map<std::string,std::string> HELP_MAP;
extern HELP_MAP HelpMap;
void PopulateMLHelpMap(void);

//--------------------------------------------
//    Value representation
//
// Each value of the script language is represented by 
// a union of pointers.  The types of values represented are
// String, floating point, and complex floating point (which is pairs of floating point numbers).
// The enumeration below is used to indicate type in the Value structure.
enum ValueType
{
   TYPE_EMPTY        = 0,
   TYPE_STRING       = 1,
   TYPE_NUMBER       = 2,
   TYPE_COMPLEX      = 3
};

// Each program variable is stored as a Value structure.
// The Value structure is made up of pointers to the underlaying types.
// This was done so that when copying a structure a shallow copy is all that is needed.
// This makes pushing an instance of a Value structure onto lists and maps easier.
// The Value structure has a nested referance structure and this is used to control the
// lifetime of the dynamically allocated memory of the Value structure.
// To explain this consider the following example:
// Value v1;
// .. allocate a new floating point array and assign it to numberValue pointer.
// Value v2;  <- this might be an instance of value in a list
// v2 = v1;   <- v2 gets assigned v1
// .. the value copyconstructor copies ValueRef from v1 to v2 and increments the count.  The Ref count is now 2
// .. and the ValueRef is shared by v1 and v2.
// .. Now lets say v1 goes out of scope.
// .. v1 destructor is called and ValueRef is decremented.  The ref count goes to 1 (not zero) so
// .. the ValueRef is not deleted and the data array is not deleted.  Just the v1 structure goes away.
// .. This is correct because v2 is still using the ValueRef and the data array.
// .. If nothing else happens, when v2 goes out of scope and is destroyed, the ValueRef and the data array
// .. will be cleaned up (deleted).
struct Value
{
   struct ValueRef{
      int ref;
      ValueRef() : ref(1){ DebugLevel1( "ValueRef(" << (long)this << ")" << endl ) }
      ~ValueRef(){ DebugLevel1( "~ValueRef(" << (long)this << ")" << endl ) }
      int Decrement(){ ref--; DebugLevel1( "--ValueRef(" << (long)this << ") " << ref << endl ) return ref; }
      int Increment(){ ref++; DebugLevel1( "++ValueRef(" << (long)this << ") " << ref << endl ) return ref; }
      };
   ValueRef* pvr;
   ValueType type;
   unsigned long size;
   unsigned long id;
   union{
      std::string* stringValue;
      float* numberValue;
      float** complexValue;
      };

void CleanUp(){
   if( !pvr->Decrement()  ){
      DebugLevel1( "Value(" << (long)this << ")::CleanUp - type=" << type << endl )
      delete pvr;
      switch( type ){
         case ValueType::TYPE_STRING:
            delete stringValue;
            break;
         case ValueType::TYPE_NUMBER:
            delete numberValue;
            break;
         case ValueType::TYPE_COMPLEX:
            delete complexValue[0];
            delete complexValue[1];
            delete complexValue;
            break;
         }
      }
   type = TYPE_EMPTY;
   id = -1;
   }

void operator=(const Value& cv){
   // This must be called first.  Clean up of the existing data
   // must be done before reassigning things.
   CleanUp();

   type = cv.type;
   pvr = cv.pvr;
   size = cv.size;
   id = cv.id;
   pvr->Increment();
   stringValue   = cv.stringValue;
   numberValue   = cv.numberValue;
   complexValue  = cv.complexValue;
   }

Value(const Value& cv){
   type = cv.type;
   pvr = cv.pvr;
   size = cv.size;
   id = cv.id;
   pvr->Increment();
   stringValue   = cv.stringValue;
   numberValue   = cv.numberValue;
   complexValue  = cv.complexValue;
   }

Value() : type(TYPE_EMPTY), size(0), id(-1){
   DebugLevel1( "Value(" << (long)this << ")" << endl )
   pvr = new ValueRef();
   }

~Value(){
   DebugLevel1( "~Value(" << (long)this << ")" << endl )
   CleanUp();
   }
};

// rlTupple represents a byte code in this scripting language.
// cmd - the numeric value of the instruction
// p1  - auxilary parameter used by some commands
// p2  - auxilary parameter used by some commands
struct rlTupple
{
long cmd;
long p1;
long p2;
rlTupple() : cmd(0), p1(0), p2(0){}
};

// A vector to hold "the program", a series of byte codes.
typedef std::vector<rlTupple> RLBYTES;
// A vector that holds program string literals.  The index of the string in the vector
// is used by some commands to allow the command to reterive the actual string value
// from the vector.  A string literal appearing multiple time in a program will exist only once
// in the list.
typedef std::vector<std::string> RLLITERALS;

// Consider this simple program for explinations below:
// a=1
// b=2
// c=a + b
//
typedef std::list<Value> RLVALUELIST;        // Supports a list of Value structures.  This will be used as a program stack.
typedef std::map<std::string,Value> RLVALUEMAP;   // Maps a string to a Value.  This is how a program variable, such as a, b, and c above are associated with a Value struct.

// a rlContext contains every thing that is needed for the script virtual machine.
// The program (the vector of byte codes).
// The list of strings that represent variables (program literals)
// The program stack.  This is how the whole thing works, values are pushed on to the stack by some commands and pulled of and replaced by other commands.
// The program commands act on the Stack.
//  Locals are the instances of Value associated with each program variable.
struct rlContext
{
RLBYTES& ByteCode;
RLLITERALS& Literals;
RLVALUELIST& Stack;
RLVALUEMAP& Locals;
rlContext(RLBYTES& _val_bytes, RLLITERALS& _literals, RLVALUEMAP& _val_map, RLVALUELIST& _stack) : ByteCode(_val_bytes), Literals(_literals), Locals(_val_map), Stack(_stack){}
};

size_t FindParameterEnd(std::string rs, size_t pos = 0);

class SyntaxTree
{
public:
   virtual void Produce(rlContext* context) = 0;
};

// Use this template for nullary operators
template< const long cmd, const long p1=0, const long p2=0 > 
class stNullaryOperator : public SyntaxTree
{
public:
   stNullaryOperator(std::string rs){
      string ns = trim(rs);
      SyntaxAssert( ns.size()==0, "Nullary operator should not be followed by parameters." )
      }
   void Produce(rlContext* context){
      rlTupple tup;
      tup.cmd = cmd;
      tup.p1 = p1;
      tup.p2 = p2;
      context->ByteCode.push_back(tup);
      }
};

// Use this template for unary operators
template< const long cmd, const long p1=0, const long p2=0 > 
class stUnaryOperator : public SyntaxTree
{
public:
   SyntaxTree* st1;
   stUnaryOperator(std::string rs){
      Parse(&st1, rs);
      }
   void Produce(rlContext* context){
      st1->Produce(context);
      rlTupple tup;
      tup.cmd = cmd;
      tup.p1 = p1;
      tup.p2 = p2;
      context->ByteCode.push_back(tup);
      }
};

// Use this template for binary operators
template< const long cmd > 
class stBinaryOperator : public SyntaxTree
{
public:
   SyntaxTree* stLeft;
   SyntaxTree* stRight;
   stBinaryOperator(std::string ls, std::string rs){
      Parse( &stLeft, ls);
      Parse( &stRight, rs);
      }
   void Produce(rlContext* context){
      stLeft->Produce(context);
      stRight->Produce(context);
      rlTupple tup;
      tup.cmd = cmd;
      context->ByteCode.push_back(tup);
      }
};

template< const long cmd, const long p1=0, const long p2=0 > 
class stTwoParm : public SyntaxTree
{
public:
   SyntaxTree* st1;
   SyntaxTree* st2;
   stTwoParm(std::string rs){
      size_t pos = FindParameterEnd(rs);
      SyntaxAssert( pos!=string::npos, "Not enough parameters for byte code: " )
      Parse( &st1, rs.substr(0,pos));
      Parse( &st2, rs.substr(pos+1));
      }
   void Produce(rlContext* context){
      st2->Produce(context);
      st1->Produce(context);
      rlTupple tup;
      tup.cmd = cmd;
      tup.p1 = p1;
      tup.p2 = p2;
      context->ByteCode.push_back(tup);
      }
};

template< const long cmd, const long p1=0, const long p2=0 > 
class stThreeParm : public SyntaxTree
{
public:
   SyntaxTree* st1;
   SyntaxTree* st2;
   SyntaxTree* st3;
   stThreeParm(std::string rs){
      size_t pos1, pos2;
      pos1 = FindParameterEnd(rs);
      SyntaxAssertEx( pos1!=std::string::npos, "Not enough parameters for byte code: " << cmd )
      Parse( &st1, rs.substr(0,pos1) );
      pos1++; // step past the comma
      pos2 = FindParameterEnd(rs, pos1 );
      SyntaxAssertEx( pos2!=std::string::npos, "Not enough parameters for byte code: " << cmd )
      Parse( &st2, rs.substr(pos1,pos2-pos1) );
      pos2++;
      SyntaxAssertEx( pos2<=rs.size(), "Not enough parameters for byte code: " << cmd )
      Parse( &st3, rs.substr(pos2) );
      }
   void Produce(rlContext* context){
      st3->Produce(context);
      st2->Produce(context);
      st1->Produce(context);
      rlTupple tup;
      tup.cmd = cmd;
      tup.p1 = p1;
      tup.p2 = p2;
      context->ByteCode.push_back(tup);
      }
};


void Parse(SyntaxTree**ppST,std::string s);

void ml_start(char* your_language_name, char* your_prompt );

std::string &trim(std::string &str);
void MakeComplex( Value& s, long size );
