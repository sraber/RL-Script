// rl.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <iostream>
#include <direct.h>
#include <iostream>
#include <fstream>
#include <string>
#include <strstream>
#include <list>
#include <vector>
#include <map>
#include <algorithm>
#include <random>
#include <math.h>
#include <complex>
#include <functional>
#include "Debug.h"
#include "dsp.h"


using namespace std;

#define R_PI 3.14159265f

//--------------------------------------------
// Some string manipulation code
static inline string &ltrim(string &str)
{
size_t startpos = str.find_first_not_of(" ");
if( string::npos != startpos ){
   str = str.substr( startpos );
   }
return str;
}

static inline string &rtrim(string &str)
{
// trim trailing spaces
size_t endpos = str.find_last_not_of(" ");
if( string::npos != endpos ){
    str = str.substr( 0, endpos+1 );
   }
return str;
}

static inline string &trim(string &str)
{
ltrim(str);
rtrim(str);
return str;
}
//-------------------------------------------
// This function assumes that the string does not have even paired braces.
// It assumes that the first open brace has been found an it is given a string 
// that points one character past that.  There may be nested pairs of braces so 
// count up for each  open and count down for each close.  When the count reaches
// zero the character position.
//
size_t FindClosingBrace( string &str )
{
int open = 0;
for(int i=0;i<str.length();i++){
   if( str[i]=='(' ){
      open++;
      }
   else if( str[i]==')' ){
      if( !open ){
         return (size_t)i;
         }
      else{
         open--;
         }
      }
   }
throw "ERROR: no closing brace";
return 0; // will never hit this line.
}
//--------------------------------------------

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
      ValueRef() : ref(1){ DebugOut << "ValueRef(" << (long)this << ")" << endl; }
      ~ValueRef(){ DebugOut << "~ValueRef(" << (long)this << ")" << endl; }
      int Decrement(){ ref--; DebugOut << "--ValueRef(" << (long)this << ") " << ref << endl; return ref; }
      int Increment(){ ref++; DebugOut << "++ValueRef(" << (long)this << ") " << ref << endl; return ref; }
      };
   ValueRef* pvr;
   ValueType type;
   unsigned long size;
   unsigned long id;
   union{
      string* stringValue;
      float* numberValue;
      float** complexValue;
      };

void CleanUp(){
   if( !pvr->Decrement()  ){
      DebugOut << "Value(" << (long)this << ")::CleanUp - type=" << type << endl;
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
   DebugOut << "Value(" << (long)this << ")" << endl;
   pvr = new ValueRef();
   }

~Value(){
   DebugOut << "~Value(" << (long)this << ")" << endl;
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
typedef std::vector<string> RLLITERALS;

// This structutre is used to support functions.  The byte code of a function
// block is held seperatly and is accessed by associating this structure with a function name.
// The string literals are literals at the function level.  They are local variables.
struct rlFunction
{
list<string> ParamList;
RLBYTES ByteCode;
RLLITERALS Literals;
};

// Consider this simple program for explinations below:
// a=1
// b=2
// c=a + b
//
typedef std::list<Value> RLVALUELIST;        // Supports a list of Value structures.  This will be used as a program stack.
typedef std::map<string,Value> RLVALUEMAP;   // Maps a string to a Value.  This is how a program variable, such as a, b, and c above are associated with a Value struct.
typedef std::map<string,long> RLFUNCTIONMAP; // Maps a function name to an index.
typedef std::vector<rlFunction> RLFUNCTIONS; // A vector to hold Function structures.

RLFUNCTIONMAP FunctionMap;
RLFUNCTIONS Functions;

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

RLBYTES  MainBytes;
RLLITERALS MainLiterals;
RLVALUEMAP Globals;
RLVALUELIST Stack;

rlContext GlobalContext(MainBytes,MainLiterals,Globals,Stack);

void Evaulate( rlContext* context );
void PrintByteCode( rlContext* context, std::ostream* ois );

//---------------------------------------------------
//
Value DeepCopy( Value& v )
{
Value nv;
switch(v.type ){
   case TYPE_STRING:
      {
      nv.type = TYPE_STRING;
      nv.stringValue = new string(*v.stringValue);
      }
      break;
   case TYPE_NUMBER:
      {
      nv.type=TYPE_NUMBER;
      nv.numberValue = new float[v.size];
      nv.size = v.size;
      for(unsigned long i=0;i<nv.size;i++){ (nv.numberValue)[i] = (v.numberValue)[i]; }
      }
      break;
   case TYPE_COMPLEX:
      {
      nv.type=TYPE_COMPLEX;
      nv.complexValue = new float*[2];
      nv.complexValue[0] = new float[v.size];
      nv.complexValue[1] = new float[v.size];
      nv.size = v.size;
      for(unsigned long i=0;i<nv.size;i++){ nv.complexValue[0][i] = v.complexValue[0][i]; }
      for(unsigned long i=0;i<nv.size;i++){ nv.complexValue[1][i] = v.complexValue[1][i]; }
      }
      break;
   }
return nv;
}
//---------------------------------------------------

//---------------------------------------------------
//    Abstract Syntax tree structures

class SyntaxTree
{
public:
   virtual void Produce(rlContext* context) = 0;
};

// Function prototype of the recursive decent parser.  This is where most of the work gets done.
void Parse(SyntaxTree**ppST,string s);

// Each one of the structures below represents a script language instruction.  When a program statement is parsed, it is parsed to 
// form a hierarchy of instaces of the structures below.  Together they make up the Abstract Syntax Tree that represents the
// parsed program code.  In more sophisticated languages and compilers there would be many more stages of processing on the AST.
// Optomization and such.  But in this code the AST is used to gererate the program byte code.
// It goes: string parser -> AST -> Byte Code -> Execuation
//
class stRoot : public SyntaxTree
{
public:
   SyntaxTree* stRight;
   stRoot(std::istream* pis, string s);
   void Produce(rlContext* context){
      stRight->Produce(context);
      }
};

class stEquals : public SyntaxTree
{
public:
   SyntaxTree* stLeft;
   SyntaxTree* stRight;
   stEquals(string ls, string rs){
      stLeft = 0;
      stRight = 0;
      if( rs.size() ){
         Parse( &stLeft, ls);
         Parse( &stRight, rs);
         }
      else{
         stRight = 0;
         Parse( &stLeft, ls);
         }
      }
   void Produce(rlContext* context){
      // Equal operation calling plan:
      //   pop right from stack, pop left from stack
      if( stRight ){
         stLeft->Produce(context);
         stRight->Produce(context);
         rlTupple tup;
         tup.cmd = 0x07;
         context->ByteCode.push_back(tup);
         }
      else{
         stLeft->Produce(context);
         // Push the show byte code onto the program
         rlTupple tup;
         tup.cmd = 0x0B;
         context->ByteCode.push_back(tup);
         }
      }
};

class stBlock : public SyntaxTree
{
public:
   list<SyntaxTree*> st_list;
   SyntaxTree* stRight;
   stBlock(std::istream* pis,string prompt,string complete){
      string cmd;
      goto START;
      while( cmd!=complete ){
         if( cmd.find("exit") == 0 ){
            exit(0);
            }
         else if( cmd.find("function") == 0 ){
            cout << "Key word \"function\" not allowed in here, but it's cool, keep on typing." << endl;
            }
         else{
            st_list.push_back( new stRoot(pis, cmd) );
            }
         START:
         cout << prompt;
         getline(*pis,cmd);
         }
      }
   void Produce(rlContext* context){
      list<SyntaxTree*>::iterator ist;
      for( ist = st_list.begin(); ist != st_list.end(); ist++ ){
         (*ist)->Produce(context);
         }
      }
};

class stFunction : public SyntaxTree
{
public:
   SyntaxTree* st_block;
   long fid;
   stFunction(std::istream* pis, string s){
      size_t pos,pos_name,pos1,pos2;
      string name;

      // We are given the characters following the word Function.
      // Find the name of the function.
      pos_name = s.find_first_of(" ");
      name = s.substr(0,pos_name);

      long id = Functions.size() + 1;
      Functions.resize(id);
      rlFunction& fun = Functions.back();

      list<size_t> commas;
      pos = s.find_first_of(",",pos_name+1);
      while( pos!=string::npos ){
         commas.push_back(pos);
         pos = s.find_first_of(",",pos+1);
         }

      if( commas.empty() ){
         string param;
         if( pos_name!=string::npos ){
            param = s.substr(pos_name);
            trim(param);
            if( param.size()>0 ){
               fun.ParamList.push_back(param);
               }
            }
         }
      else{
         pos1 = pos_name;
         for( list<size_t>::iterator iter = commas.begin();
               iter != commas.end();
               iter++ ){
            pos2 = *iter;
            fun.ParamList.push_back( trim( s.substr(pos1,pos2-pos1) ) );
            pos1 = pos2+1;
            }
         fun.ParamList.push_back( trim( s.substr(pos1,string::npos) ) );
         }

      st_block = new stBlock(pis,"RL:Function:>>","end"); // Doesn't exit until user enters "next"

      fid = id - 1;
      FunctionMap[name + " "]=fid;
      }
   void Produce(rlContext* context){
      rlFunction& rf = Functions[fid];
      RLVALUEMAP locals;
      RLVALUELIST temp_stack;

      rlContext function_context(rf.ByteCode,rf.Literals,locals,temp_stack);
      st_block->Produce(&function_context);   // Write block commands
      }
};

class stIf : public SyntaxTree
{
public:
   SyntaxTree* st_conditional;
   SyntaxTree* st_block;

   stIf(std::istream* pis, string s){
      Parse( &st_conditional, s);
      st_block = new stBlock(pis,"RL:if>>","eif"); // Doesn't exit until user enters "eif"
      }
   void Produce(rlContext* context){
      //ex: if i<10
      st_conditional->Produce(context); 
      
      rlTupple tup; 
      tup.p1=0; 
      tup.p2=0;

      tup.cmd = 0xF9; // ! - flip the result of the conditional above.
      context->ByteCode.push_back(tup); 

      // We flip the sign of the conditional because we only have itj (if true jump),
      // in this case we want to jump if flase and execute the block if true.
      // We could add an ifj (if false jump byte code) but why not try to make due for now.
      tup.cmd = 0xF2;      // itj - If true jump
      //tup.p1 = 0;        // We'll figure out the jump later
      context->ByteCode.push_back( tup );

      int size_before_block = context->ByteCode.size();

      st_block->Produce(context);           // Write block commands

      context->ByteCode[size_before_block-1].p1 = context->ByteCode.size() - size_before_block; // Jump over the byte code
      }
};

class stLoop : public SyntaxTree
{
public:
   SyntaxTree* st_eq;
   SyntaxTree* st_block;
   SyntaxTree* stLeft;
   SyntaxTree* stRight;
   SyntaxTree* st_Step;

   stLoop(std::istream* pis, string s){
      size_t pos_eq;
      size_t pos_to;
      size_t pos_step;
      pos_eq=s.find("=");
      if( pos_eq==string::npos ){ throw "No equals sign in loop command"; }
      pos_to=s.find(" to ");
      if( pos_to==string::npos ){ throw "No \"to\" key word in loop command"; }
      pos_step=s.find(" step ");
      st_eq = new stEquals( s.substr(0,pos_eq), s.substr(pos_eq+1,pos_to-pos_eq-1) );
      Parse( &stLeft, s.substr(0,pos_eq));
      Parse( &stRight, s.substr(pos_to+4,pos_step-pos_to-4));
      if( pos_step!=string::npos ){
         Parse( &st_Step, s.substr(pos_step+6,string::npos));
         }
      else{
         Parse( &st_Step, "1");
         }
      st_block = new stBlock(pis,"RL:Loop>>","next"); // Doesn't exit until user enters "next"
      }
   void Produce(rlContext* context){
      long top;
      st_eq->Produce(context);      // Set the value of the counter.
      top = context->ByteCode.size();

      //ex: loop i=1 to 6 step 2

      st_block->Produce(context);   // Write block commands
      stRight->Produce(context);    // Right side of inequality.
      stLeft->Produce(context);     // write counter inequality.
      st_Step->Produce(context);    // Step size.
      stLeft->Produce(context);     // write counter inequality.

      rlTupple tup; 
      tup.p1=0; 
      tup.p2=0;

      tup.cmd = 0x11; // +
      context->ByteCode.push_back(tup); 

      tup.cmd = 0x07; // =
      context->ByteCode.push_back(tup); 
      
      stLeft->Produce(context);     // write counter inequality.

     
      tup.cmd = 0xF5; // >=
      context->ByteCode.push_back(tup); 

      tup.cmd = 0xF2;               // itf - If true jump
      tup.p1 = top - context->ByteCode.size() - 1;
      context->ByteCode.push_back(tup); 
     }
};

// Class stRoot constructor
//  
// It has to be here in the code becuase its implementation is dependant
// on the defination of some other classes above.
//
stRoot::stRoot(std::istream* pis, string s)
{
size_t pos;
trim(s);

pos=s.find("if ");
if( pos==0 ){
   stRight = new stIf(pis, s.substr(pos+3,string::npos) );
   return;
   }
pos=s.find("loop ");
if( pos==0 ){
   stRight = new stLoop(pis, s.substr(pos+5,string::npos) );
   return;
   }
pos=s.find("function ");
if( pos==0 ){
   stRight = new stFunction(pis, trim(s.substr(pos+8)));
   return;
   }

size_t n = s.size();
pos=s.find_first_of("=");
while( pos!=string::npos ){
   if( pos>0 && s[pos-1]!='<' && s[pos-1]!='>' && s[pos-1]!='!' ){
      if( pos==n-1 ){ break; }
      if( s[pos+1]!='=' ){ break; }
      }
   pos=s.find_first_of("=",pos+1);
   }
if( pos==string::npos ){
   Parse( &stRight, s);
   }
else{
   stRight = new stEquals(s.substr(0,pos), s.substr(pos+1,string::npos) );
   }
}

//----------------------

// Use this template for unary operators
template< const long cmd, const long p1=0, const long p2=0 > 
class stUnaryOperator : public SyntaxTree
{
public:
   SyntaxTree* st1;
   stUnaryOperator(string rs){
      Parse( &st1, rs );
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
   stBinaryOperator(string ls, string rs){
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
class stThreeParmWithOptions : public SyntaxTree
{
public:
   SyntaxTree* st1;
   SyntaxTree* st2;
   SyntaxTree* st3;
   stThreeParmWithOptions(string rs){
      size_t pos1, pos2;

      pos1 = rs.find_first_of( "," );
      if( pos1==string::npos ){ cout << "not enough parameters for command." << endl; throw "error"; }
      Parse( &st1, rs.substr(0,pos1) );
      pos1++; // step past the comma
      pos2 = rs.find_first_of( ",", pos1 );
      if( pos2==string::npos ){
         if( pos1>rs.size() ){ cout << "not enough parameters for command." << endl; throw "error"; }
         Parse( &st2, rs.substr(pos1) );
         Parse( &st3, string("0.0") );
         }
      else{ 
         Parse( &st2, rs.substr(pos1,pos2-pos1) );
         pos2++;
         Parse( &st3, rs.substr(pos2) );
         }
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

template< const long cmd, const long p1=0, const long p2=0 > 
class stTwoParm : public SyntaxTree
{
public:
   SyntaxTree* st1;
   SyntaxTree* st2;
   stTwoParm(string rs){
      size_t pos = rs.find_first_of(",");
      if( pos==string::npos ){ cout << "Call to write missing parameters.  Syntax: write [value], [path and file name] " << endl; throw "error"; }
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

class stLiteral : public SyntaxTree
{
public:
   string value;
   stLiteral(string rs){
      value = trim(rs);
      }
   void Produce(rlContext* context){
      rlTupple tup;
      // Literal
      RLLITERALS::iterator lit = std::find(context->Literals.begin(),context->Literals.end(),value);
      if( lit==context->Literals.end() ){
         context->Literals.push_back(value);
         tup.p1 = context->Literals.size() - 1;
         }
      else{
         tup.p1 = lit - context->Literals.begin();
         }
      tup.cmd = 0x03;
      context->ByteCode.push_back(tup);
      // Discover
      tup.cmd = 0x08;
      tup.p1 = 0;
      context->ByteCode.push_back(tup);
      }
};

class stCrop : public SyntaxTree
{
public:
   SyntaxTree* st1;
   SyntaxTree* st2;
   SyntaxTree* st3;
   stCrop(string rs){
      size_t pos1, pos2;

      pos1 = 0;
      pos2 = rs.find_first_of( "," );
      if( pos2==string::npos ){ cout << "not enough parameters for crop command. Syntax: crop [array], [start], [length]" << endl; throw "error"; }
      Parse( &st1, rs.substr(pos1,pos2-pos1) );
      pos1=pos2+1;

      pos2 = rs.find_first_of( ",", pos1 );
      if( pos2==string::npos ){ cout << "not enough parameters for crop command. Syntax: crop [array], [start], [length]" << endl; throw "error"; }
      Parse( &st2, rs.substr(pos1,pos2-pos1) );
      pos1=pos2+1;
     
 
      if( pos1==rs.size() ){ cout << "not enough parameters for crop command. Syntax: crop [array], [start], [length]" << endl; throw "error"; }
      Parse( &st3, rs.substr(pos1) );

      }
   void Produce(rlContext* context){
      st3->Produce(context);
      st2->Produce(context);
      st1->Produce(context);
      // Write
      rlTupple tup;
      tup.cmd = 0x0F;
      context->ByteCode.push_back(tup);
      }
};

class stCall : public SyntaxTree
{
public:
   list<SyntaxTree*> stList;
   long fid;
   stCall(string s, long id){
      SyntaxTree* st;
      size_t pos1,pos2;
      fid = id;
      rlFunction& fun = Functions[fid];

      list<size_t> commas;
      pos1 = s.find_first_of(",");
      while( pos1!=string::npos ){
         commas.push_back(pos1);
         pos1 = s.find_first_of(",",pos1+1);
         }

      if( commas.empty() ){
         trim(s);
         if( s.size()>0 ){
            Parse( &st, s);
            stList.push_back( st );
            }
         }
      else{
         pos1 = 0;
         for( list<size_t>::iterator iter = commas.begin();
               iter != commas.end();
               iter++ ){
            pos2 = *iter;
            Parse( &st, trim( s.substr(pos1,pos2-pos1) ) );
            stList.push_back(st);
            pos1 = pos2+1;
            }
         Parse( &st, trim( s.substr(pos1,string::npos) ) );
         stList.push_back(st);
         }
      }
   void Produce(rlContext* context){
      list<SyntaxTree*>::iterator ist;
      for( ist = stList.begin(); ist != stList.end(); ist++ ){
         (*ist)->Produce(context);
         }
      rlTupple tup;
      tup.cmd = 0xF0;
      tup.p1 = fid;
      context->ByteCode.push_back(tup);
      }
};
//-----------------------------------------------------------------

////////////////////////////////////////////////////////////////////
//         The Recursive Decent Parser
//
// Program syntax is ultimatly controled by this function.
//
void Parse(SyntaxTree**ppST,string s)
{
trim(s);

for(int n=1; n<=s.size(); n++ ){
   size_t pos;
   string s1 = s.substr(0,n);

// Order the statements by number of characters, small to large.

   pos=s1.find("cd ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x0D>(s.substr(pos+3));
      return;
      }
   pos=s1.find("ls");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x0E>(s.substr(pos+2));
      return;
      }
/*
   pos=s1.find("lp ");
   if( pos==0 ){
      // check that string is bound properly.  Like first line, then space after.
      *ppST = new stLP(s.substr(pos+3));
      return;
      }
   pos=s1.find("hp ");
   if( pos==0 ){
      *ppST = new stHP(s.substr(pos+3));
      return;
      }
*/
   pos=s1.find("ft ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x30>(s.substr(pos+3));
      return;
      }
   pos=s1.find("sum ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x1B>(s.substr(pos+3));
      return;
      }
   pos=s1.find("irft ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x31,0>(s.substr(pos+4));
      return;
      }
   pos=s1.find("icft ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x31,1>(s.substr(pos+4));
      return;
      }
   pos=s1.find("sqrt ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x26>(s.substr(pos+5));
      return;
      }
   pos=s1.find("log ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x27>(s.substr(pos+4));
      return;
      }
   pos=s1.find("ln ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x28>(s.substr(pos+3));
      return;
      }
   pos=s1.find("exp ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x29>(s.substr(pos+4));
      return;
      }
   pos=s1.find("dir");
   if( pos==0 && s.size()==3){
      *ppST = new stUnaryOperator<0x0E>(s.substr(pos+3));
      return;
      }
   pos=s1.find("cos ");
   if( pos==0 ){
      // byte code 0x20 is trig function, p1 indicates which function.  See the trig function for details.
      *ppST = new stThreeParmWithOptions<0x20,0>(s.substr(pos+4));
      return;
      }
   pos=s1.find("seg ");
   if( pos==0 ){
      *ppST = new stThreeParmWithOptions<0x0C>(s.substr(pos+4));
      return;
      }
   pos=s1.find("sin ");
   if( pos==0 ){
      *ppST = new stThreeParmWithOptions<0x20,1>(s.substr(pos+4));
      return;
      }
   pos=s1.find("tan ");
   if( pos==0 ){
      *ppST = new stThreeParmWithOptions<0x20,2>(s.substr(pos+4));
      return;
      }
   pos=s1.find("init ");
   if( pos==0 ){
      *ppST = new stThreeParmWithOptions<0x23>(s.substr(pos+5));
      return;
      }
   // Yes, this is a second command based on the same byte code.
   pos=s1.find("line ");
   if( pos==0 ){
      *ppST = new stThreeParmWithOptions<0x23>(s.substr(pos+5));
      return;
      }
   pos=s1.find("noise ");
   if( pos==0 ){
      *ppST = new stTwoParm<0x24>(s.substr(pos+6));
      return;
      }
   pos=s1.find("crop ");
   if( pos==0 ){
      *ppST = new stCrop(s.substr(pos+5));
      return;
      }
   pos=s1.find("read ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x01>(s.substr(pos+5));
      return;
      }
   pos=s1.find("conj ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x18>(s.substr(pos+5));
      return;
      }
   pos=s1.find("real ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x05>(s.substr(pos+5));
      return;
      }
   pos=s1.find("size ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x1A>(s.substr(pos+5));
      return;
      }
   pos=s1.find("imag ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x06>(s.substr(pos+5));
      return;
      }
   pos=s1.find("write ");
   if( pos==0 ){
      *ppST = new stTwoParm<0x02>(s.substr(pos+6));
      return;
      }
   pos=s1.find("complex ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x04>(s.substr(pos+8));
      return;
      }

   RLFUNCTIONMAP::iterator fit;
   if( s1.size()==s.size() ){
      fit = FunctionMap.find( s1 + " " );
      }
   else{
      fit = FunctionMap.find( s1 );
      }

   if( fit!=FunctionMap.end() ){
      *ppST = new stCall(s.substr(s1.size()), fit->second);
      return;
      }

   if(      s[n-1]=='+' ){
      *ppST = new stBinaryOperator<0x11>(s.substr(0,n-1), s.substr(n,string::npos) );
      return;
      }
   else if( s[n-1]=='-' ){
      if( n > 1 ){
         *ppST = new stBinaryOperator<0x12>(s.substr(0,n-1), s.substr(n,string::npos) );
         }
      else{
         *ppST = new stUnaryOperator<0x1C>(s.substr(n,string::npos) );
         }
      return;
      }
   else if( s[n-1]=='*' ){
      *ppST = new stBinaryOperator<0x13>(s.substr(0,n-1), s.substr(n,string::npos) );
      return;
      }
   else if( s[n-1]=='>' ){
      if( n>1 && s[n]=='=' ){
         *ppST = new stBinaryOperator<0xF5>(s.substr(0,n-1), s.substr(n+1,string::npos) );
         }
      else{
         *ppST = new stBinaryOperator<0xF3>(s.substr(0,n-1), s.substr(n,string::npos) );
         }
      return;
      }
   else if( s[n-1]=='<' ){
      if( n>1 && s[n]=='=' ){
         *ppST = new stBinaryOperator<0xF6>(s.substr(0,n-1), s.substr(n+1,string::npos) );
         }
      else{
         *ppST = new stBinaryOperator<0xF4>(s.substr(0,n-1), s.substr(n,string::npos) );
         }
      return;
      }
   else if( s[n-1]=='=' && n>1 && s[n]=='=' ){
      *ppST = new stBinaryOperator<0xF8>(s.substr(0,n-1), s.substr(n+1,string::npos) );
      return;
      }
   else if( s[n-1]=='!' ){
      if( n>1 && s[n]=='=' ){
         *ppST = new stBinaryOperator<0xF7>(s.substr(0,n-1), s.substr(n+1,string::npos) );
         }
      else{
         *ppST = new stUnaryOperator<0xF9>(s.substr(n,string::npos) );
         }
      return;
      }
   else if( s[n-1]=='(' ){
      int pos = FindClosingBrace( s.substr(n,string::npos) );
      if( !pos ){ throw "ERROR: empty braces."; }
      // Adding 2 accounts for the open and close brace.
      if( (pos+2)==s.length() ){
         Parse( ppST, s.substr(n,s.length()-2) );
         return;
         }
      else{
         n=pos+2;
         }
      }
   }

*ppST = new stLiteral(s);
}

int _tmain(int argc, _TCHAR* argv[])
{
string cmd;

cout << "Enter a command" << endl;
goto START;

while( cmd!="exit" ){
   if( cmd.find("run") == 0 ){
      char buf[255];
      string str = trim( cmd.substr(3) );
      fstream f;
      f.open(str.c_str());
      if( f.is_open() ){
         string line;
         cout << "currenly running file: " << str << endl;
         while( !f.eof() ){
            f.getline(buf,255);
            string line(buf);
            cout << ">> " << line << endl;
            stRoot root(&f, line);
            root.Produce(&GlobalContext);
            Evaulate(&GlobalContext);
            }
         }
      }
   else if( cmd.find("bytecode") == 0 ){
      //typedef std::map<string,long> RLFUNCTIONMAP;
      //typedef std::vector<rlFunction> RLFUNCTIONS;
      RLVALUEMAP local_values;
      RLVALUELIST local_stack;
      fstream file( "bytecode.txt", ios::out );
      RLFUNCTIONMAP::iterator iter;
      for( iter = FunctionMap.begin(); iter!=FunctionMap.end(); iter++ ){
         string name = iter->first;
         file << "function: " << name << endl << endl;
         rlFunction& rfun = Functions[iter->second];
         rlContext local_context(rfun.ByteCode,rfun.Literals,local_values,local_stack);
         PrintByteCode(&local_context,&file);
         }

      file.close();
      }
   else{
      try{

         stRoot root(&cin, cmd);
         root.Produce(&GlobalContext);
         Evaulate(&GlobalContext);

/*
         fstream file( "bytecode.txt", ios::out );

         file << endl << "Byte Code" << endl;

         PrintByteCode(&GlobalContext,&file);
*/
         }
      catch(...){
         cout << "Ummm.. that didn't work!" << endl;
         }
      GlobalContext.Literals.clear();
      GlobalContext.Stack.clear();
      GlobalContext.ByteCode.clear();
      }
   START:
   cout << "R>>";
   getline(cin,cmd);
   }

	return 0;
}

/////////////////////////////////////////////////////////////
//          Byte Code Command Implementation
/////////////////////////////////////////////////////////////

void strliteral(rlContext* context, long p1, long p2)
{
Value rlv;
rlv.stringValue = new string;
*rlv.stringValue = context->Literals[p1]; 
rlv.type = TYPE_STRING;
context->Stack.push_front(rlv);
}

void show(rlContext* context, long p1, long p2)
{
Value rlv;
if( context->Stack.empty() ){ cout << "Nothing on the stack." << endl; throw "error"; }
rlv = context->Stack.front();
context->Stack.pop_front();
switch( rlv.type ){
   case ValueType::TYPE_EMPTY:
      cout << "empty" << endl;
      break;
   case ValueType::TYPE_STRING:
      cout << *rlv.stringValue << endl;
      break;
   case ValueType::TYPE_NUMBER:
      for( unsigned long i=0;i<rlv.size;i++){ cout << (rlv.numberValue)[i] << endl; } 
      break;
   case ValueType::TYPE_COMPLEX:
      for( unsigned long i=0;i<rlv.size;i++){ 
         cout << rlv.complexValue[0][i] << " , " << rlv.complexValue[1][i] << endl; 
         } 
      break;
   };
}

bool isnumeric(string st) 
{
    int len = st.length(), ascii_code, decimal_count = -1, negative_count = -1;
    for (int i = 0; i < len; i++) {
        ascii_code = int(st[i]);
        switch (ascii_code) {
            case 44: // Allow commas.
                // This will allow them anywhere, so ",,," would return true.
                // Write code here to require commas be every 3 decimal places.
                break;
            case 45: // Allow a negative sign.
                negative_count++;
                if (negative_count || i != 0) {
                    return false;
                }
                break;
            case 46: // Allow a decimal point.
                decimal_count++;
                if (decimal_count) {
                    return false;
                }
                break;
            default:
                if (ascii_code < 48 || ascii_code > 57) {
                    return false;
                }
                break;
        }
    }
return true;
}

void discover(rlContext* context, long p1, long p2)
{
Value rlv;

rlv = context->Stack.front();
if( rlv.type!=TYPE_STRING ){
   return;
   }

RLVALUEMAP::iterator it = context->Locals.find( *rlv.stringValue );

if( it!=context->Locals.end() ){
   context->Stack.pop_front();
   context->Stack.push_front( it->second );
   }
else if( (it=GlobalContext.Locals.find( *rlv.stringValue ))
         !=GlobalContext.Locals.end() ){
   context->Stack.pop_front();
   context->Stack.push_front( it->second );
   }
else if( rlv.type==TYPE_STRING ){
   if( isnumeric(*rlv.stringValue) ){
      context->Stack.pop_front();
      Value nv;
      strstream ss;
      ss << rlv.stringValue->c_str() << ends;
      nv.type=TYPE_NUMBER;
      nv.size=1;
      nv.numberValue = new float;
      ss >> (*nv.numberValue);
      context->Stack.push_front( nv );
      }
   }
}

void tocomplex(rlContext* context, long p1, long p2)
{
Value rlv;
if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for \"complex\" command." << endl; throw "error"; }

rlv = context->Stack.front();
if( rlv.type==TYPE_COMPLEX ){
   return;
   }
else if( rlv.type==TYPE_NUMBER ){
   Value nv;
   int n = rlv.size;
   nv.type = TYPE_COMPLEX;
   nv.size = n;
   nv.complexValue = new float*[2];
   nv.complexValue[0] = new float[n];
   nv.complexValue[1] = new float[n];
   for(int i=0;i<n;i++){
      nv.complexValue[0][i] = rlv.numberValue[i];
      nv.complexValue[1][i] = 0.0f;
      }

   context->Stack.pop_front();
   context->Stack.push_front( nv );
   return;
   }

cout << "Run time error. Whatever it was that you wanted to turn complex it can't be done." << endl; throw "error";
}

void real(rlContext* context, long p1, long p2)
{
Value rlv;
if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for \"real\" command." << endl; throw "error"; }

rlv = context->Stack.front();
if( rlv.type==TYPE_NUMBER || rlv.type==TYPE_STRING ){
   return;
   }
context->Stack.pop_front();

if( rlv.type==TYPE_COMPLEX ){
   Value nv;
   nv.CleanUp();
   nv.pvr = rlv.pvr;
   // NOTE: If the complex number Value is released first then this temp
   //       variable will leak the complex side of the number because it does
   //       not know that it is really part of a complex number.
   nv.pvr->Increment();
   int n = rlv.size;
   nv.size = n;
   nv.type = TYPE_NUMBER;
   nv.numberValue = rlv.complexValue[0];
   context->Stack.push_front( nv );
   return;
   }
}

void imag(rlContext* context, long p1, long p2)
{
Value rlv;
if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for \"imag\" command." << endl; throw "error"; }

rlv = context->Stack.front();
if( rlv.type==TYPE_STRING ){
   return;
   }
context->Stack.pop_front();

if( rlv.type==TYPE_COMPLEX ){
   Value nv;
   nv.CleanUp();
   nv.pvr = rlv.pvr;
   // NOTE: If the complex number Value is released first then this temp
   //       variable will leak the complex side of the number because it does
   //       not know that it is really part of a complex number.
   nv.pvr->Increment();
   int n = rlv.size;
   nv.size = n;
   nv.type = TYPE_NUMBER;
   nv.numberValue = rlv.complexValue[1];
   context->Stack.push_front( nv );
   return;
   }
else{
   cout << "Run time error. Cannot take the imaginary part of a real." << endl; throw "error";
   }
}

void seg(rlContext* context, long p1, long p2)
{
Value rlv;
Value sv;
Value ev;
int s,e,n;

if( context->Stack.size()<3 ){ cout << "Run time error. Not enough parameters on the stack for \"seg\" command." << endl; throw "error"; }

rlv = context->Stack.front();
context->Stack.pop_front();

sv = context->Stack.front();
context->Stack.pop_front();

ev = context->Stack.front();
context->Stack.pop_front();

Value nv;
if( rlv.type==TYPE_STRING ){
   nv.type=TYPE_STRING;
   nv.stringValue = new string;
   (*nv.stringValue) = rlv.stringValue->substr((long)(sv.numberValue[0]),(long)(ev.numberValue[0]-sv.numberValue[0]));
   }
else if( rlv.type==TYPE_NUMBER ){
   n = rlv.size;
   s = sv.numberValue[0];
   e = ev.numberValue[0];
   if( !e ){ e = n; }
   if( e > n ){ e = n; }
   if( s > e ){ cout << "Run time error. Start value greater than end value." << endl; throw "error"; }
   nv.CleanUp();
   nv.pvr = rlv.pvr;
   // NOTE: If the number Value is released first then this temp
   //       variable will be deleted and will probably not end well.
   nv.pvr->Increment();
   nv.size = (int)(e-s);
   // The type must be set after Cleanup is called.
   nv.type = TYPE_NUMBER;
   nv.numberValue = rlv.numberValue + (long)(sv.numberValue[0]);
   context->Stack.push_front( nv );
   return;
   }
else if( rlv.type==TYPE_COMPLEX ){
   n = rlv.size;
   s = sv.numberValue[0];
   e = ev.numberValue[0];
   if( !e ){ e = n; }
   if( e > n ){ e = n; }
   if( s > e ){ cout << "Run time error. Start value greater than end value." << endl; throw "error"; }
   nv.CleanUp();
   nv.pvr = rlv.pvr;
   // NOTE: If the complex number Value is released first then this temp
   //       variable will leak the complex side of the number because it does
   //       know that it is really part of a complex number.
   nv.pvr->Increment();
   n = rlv.size;
   nv.size = (int)(e-s);
   nv.type = TYPE_COMPLEX;

   //REVIEW: The 2 element float* array is going to leak!

   nv.complexValue = new float*[2];
   nv.complexValue[0] = rlv.complexValue[0] + (long)(sv.numberValue[0]);
   nv.complexValue[1] = rlv.complexValue[1] + (long)(sv.numberValue[0]);
   context->Stack.push_front( nv );
   return;
   }
else{
   cout << "Run time error." << endl; throw "error";
   }
}

void conj(rlContext* context, long p1, long p2)
{
Value rlv;
if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for \"conj\" command." << endl; throw "error"; }

rlv = context->Stack.front();
if( rlv.type!=TYPE_COMPLEX ){
   return;
   }

context->Stack.pop_front();

Value nv = DeepCopy(rlv);
int n = rlv.size;

for(int i=0;i<n;i++ ){ nv.complexValue[1][i] = -nv.complexValue[1][i]; }
context->Stack.push_front( nv );
}


void equals(rlContext* context, long p1, long p2)
{
Value rv;
Value lv;
if( context->Stack.size()<2 ){
   cout << "Syntax incorrect. Not enough parameters." << endl;
   throw "error";
   }
rv = context->Stack.front();
context->Stack.pop_front();

lv = context->Stack.front();
context->Stack.pop_front();

switch( rv.type ){
   case ValueType::TYPE_STRING:
      {
      if( lv.pvr->ref==1 ){
         if( rv.pvr->ref==1 ){
            context->Locals[ *lv.stringValue ] = rv;
            }
         else{
            context->Locals[ *lv.stringValue ] = DeepCopy(rv);
            }
         }
      else{
         if( lv.type==TYPE_STRING ){
            (*lv.stringValue) = (*rv.stringValue);
            }
         else{
            cout << "Wrong type for lvalue." << endl; throw "error";
            }
         }
      break;
      }
   case ValueType::TYPE_NUMBER:
      {
      if( lv.pvr->ref==1 ){
         if( rv.pvr->ref==1 ){
            context->Locals[ *lv.stringValue ] = rv;
            }
         else{
            context->Locals[ *lv.stringValue ] = DeepCopy(rv);
            }
         }
      else{
         if( lv.type!=TYPE_NUMBER ){ cout << "Wrong type for lvalue." << endl; throw "error"; }
         unsigned long n = rv.size;
         if( lv.size < n ){ n = lv.size; }
         for(unsigned long i=0;i<n;i++){ (lv.numberValue)[i] = (rv.numberValue)[i]; }
         }
      break;
      }
   case ValueType::TYPE_COMPLEX:
      {
      if( lv.pvr->ref==1 ){
         if( rv.pvr->ref==1 ){
            context->Locals[ *lv.stringValue ] = rv;
            }
         else{
            context->Locals[ *lv.stringValue ] = DeepCopy(rv);
            }
         }
      else{
         if( lv.type!=TYPE_COMPLEX ){ cout << "Wrong type for lvalue." << endl; throw "error"; }
         unsigned long n = rv.size;
         if( lv.size < n ){ n = lv.size; }
         for(unsigned long i=0;i<n;i++){ lv.complexValue[0][i] = rv.complexValue[0][i]; }
         for(unsigned long i=0;i<n;i++){ lv.complexValue[1][i] = rv.complexValue[1][i]; }
         }
      break;
      }
   }
}

// Pull value off of stack.
// Pull file name off of stack.
void write(rlContext* context, long p1, long p2)
{
Value rv;
Value path;
char buf[100];
float val;

rv = context->Stack.front();
context->Stack.pop_front();

path = context->Stack.front();
context->Stack.pop_front();

if( path.type!=TYPE_STRING ){
   cout << "path not string in function read" << endl;
   return;
   }

fstream file( path.stringValue->c_str(), ios::out );
if( !file.good() ){
   cout << "There was a problem opening the output file" << endl;
   return;
   }

switch( rv.type ){
   case ValueType::TYPE_STRING:
      {
      file << *rv.stringValue;
      break;
      }
   case ValueType::TYPE_NUMBER:
      {
      for(int i=0;i<rv.size;i++){
         file << rv.numberValue[i] << endl;
         }
      break;
      }
   case ValueType::TYPE_COMPLEX:
      {
      for(int i=0;i<rv.size;i++){
         file << rv.complexValue[0][i] << "," << rv.complexValue[1][i] << endl;
         }
      break;
      }
   };
}

void read(rlContext* context, long p1, long p2)
{
Value nv;
Value path;
char buf[100];
float val;

path = context->Stack.front();
context->Stack.pop_front();

if( path.type!=TYPE_STRING ){
   cout << "path not string in function read" << endl;
   return;
   }

fstream file( path.stringValue->c_str(), ios::in );
if( !file.good() ){
   cout << "There was a problem opening the input file" << endl;
   return;
   }

int fcomplex = 0;
if( file.eof() ){ return; }

file.getline(buf,100);
for( int i=0;i<100;i++ ){
   if( buf[i] == '/n' ){ break; }
   if( buf[i] == ',' ){ fcomplex = true; break; }
   }

if( fcomplex ){
   vector<float> t1;
   vector<float> t2;
   do{
      char* b1 = 0;
      for(int i=0;i<100;i++){
         if( buf[i] == ',' ){ buf[i] = 0; b1 = buf + i + 1; break; }
         }
      strstream ss1;
      ss1 << buf << ends;
      ss1 >> val;
      t1.push_back( val );
      strstream ss2;
      if( b1 ){
         ss2 << b1 << ends;
         ss2 >> val;
         t2.push_back( val );
         }
      else{
         t2.push_back( 0.0f );
         }

      file.getline(buf,100);
      }while( !file.eof() );

   int n = t1.size();
   nv.complexValue = new float*[2];
   nv.complexValue[0] = new float[n];
   nv.complexValue[1] = new float[n];
   nv.type = TYPE_COMPLEX;
   nv.size = n;

   for(int i=0;i<n;i++){
      nv.complexValue[0][i] = t1[i];
      nv.complexValue[1][i] = t2[i];
      }
   }
else{
   vector<float> t1;
   do{
      strstream ss1;
      ss1 << buf << ends;
      ss1 >> val;
      t1.push_back( val );
      file.getline(buf,100);
      }while( !file.eof() );

   int n = t1.size();
   nv.numberValue = new float[n];
   nv.type = TYPE_NUMBER;
   nv.size = n;

   for(int i=0;i<n;i++){
      nv.numberValue[i] = t1[i];
      }
   }
   
context->Stack.push_front(nv);
}

void chdir(rlContext* context, long p1, long p2)
{
Value path;
if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for cd command." << endl; throw "error"; }

path = context->Stack.front();
context->Stack.pop_front();

if( path.type!=TYPE_STRING ){
   cout << "path not string in function read" << endl;
   return;
   }
if( _chdir(path.stringValue->c_str()) ){
   cout << "Couldn't change directory." << endl;
   }
}

void dir(rlContext* context, long p1, long p2)
{
WIN32_FIND_DATA FindFileData;
HANDLE hFind;
hFind = FindFirstFile("*", &FindFileData);
if( hFind != INVALID_HANDLE_VALUE ){
   while(FindNextFile(hFind, &FindFileData)){
      if( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ){
         cout << "[" << FindFileData.cFileName << "]" << endl;
         }
      else{
         cout << FindFileData.cFileName << endl;
         }
      }
   }
FindClose(hFind);
}

// Pull value off of stack.
// Pull file name off of stack.
void crop(rlContext* context, long p1, long p2)
{
Value av;
Value sv;
Value ev;

if( context->Stack.size()<3 ){ cout << "Run time error. Not enough parameters on the stack for crop command." << endl; throw "error"; }

av = context->Stack.front();
context->Stack.pop_front();

sv = context->Stack.front();
context->Stack.pop_front();

ev = context->Stack.front();
context->Stack.pop_front();

if( av.type==TYPE_STRING ){
   cout << "nothing to crop.  Parameter is not an array." << endl;
   return;
   }

int n = (long)ev.numberValue[0];
int s = (long)sv.numberValue[0];
if( s+n > av.size ){ n = av.size = s; }
int m = s + n;
Value nv;
if( av.type==TYPE_NUMBER ){
   nv.type = TYPE_NUMBER;
   nv.size = n;
   nv.numberValue = new float[n];
   int j = 0;
   for(int i=s;i<m;i++,j++){
      nv.numberValue[j] = av.numberValue[i];
      }
   context->Stack.push_front( nv );
   return;
   }
if( av.type==TYPE_COMPLEX ){
   nv.type = TYPE_COMPLEX;
   nv.size = n;
   nv.complexValue = new float*[2];
   nv.complexValue[0] = new float[n];
   nv.complexValue[1] = new float[n];
   int j = 0;
   for(int i=s;i<m;i++,j++){
      nv.complexValue[0][j] = av.complexValue[0][i];
      nv.complexValue[1][j] = av.complexValue[1][i];
      }
   context->Stack.push_front( nv );
   return;
   }

}

// Pull value name off of stack.  (It should be an array.
void ft(rlContext* context, long p1, long p2)
{
Value lv;
long n;
if( context->Stack.size() < 1 ){ cout << "FT cmd.  Not enough parameters on the stack." << endl; throw "error"; }
lv = context->Stack.front();
context->Stack.pop_front();
Value nv;

if( lv.type==TYPE_NUMBER ){
   n = lv.size;
   float* pt = new float[n];
   float* ipt = pt;
   for(int i=0;i<n;i++,ipt++){ *ipt = lv.numberValue[i]; }
   rlFFT( pt, n, 1 );
   n >>= 1;
   nv.type = TYPE_COMPLEX;
   nv.size = n;
   nv.complexValue = new float*[2];
   nv.complexValue[0] = new float[n];
   nv.complexValue[1] = new float[n];
   nv.complexValue[0][0] = *pt;
   nv.complexValue[1][0] = 0.0f;
   nv.complexValue[0][n-1] = *(pt+1);
   nv.complexValue[1][n-1] = 0.0f;
   ipt = pt + 2;
   for(int i=1;i<(n-1);i++,ipt+=2){ 
      nv.complexValue[0][i] = *ipt; 
      nv.complexValue[1][i] = *(ipt+1); 
      }
   delete[] pt;  
   }
else if( lv.type==TYPE_COMPLEX ){
   n = lv.size;
   float* pt = new float[2*n];
   float* ipt = pt;
   for(int i=0;i<n;i++,ipt+=2){ *ipt = lv.complexValue[0][i];  *(ipt+1) = lv.complexValue[1][i]; }
   cpFFT( pt, 2*n, 1 );
   nv.type = TYPE_COMPLEX;
   nv.size = n;
   nv.complexValue = new float*[2];
   nv.complexValue[0] = new float[n];
   nv.complexValue[1] = new float[n];
 
   ipt = pt;
   for(int i=0;i<n;i++,ipt+=2){ 
      nv.complexValue[0][i] = *ipt;
      nv.complexValue[1][i] = *(ipt+1);
      }

   delete[] pt;  
   }
else{
   cout << "Invalid argument in call to FT." << endl; throw "error";
   }

context->Stack.push_front( nv );
}

// Pull value off of stack.  (It should be an array.)
void ift(rlContext* context, long p1, long p2)
{
Value lv;
long n;
if( context->Stack.size() < 1 ){ cout << "FT cmd.  Not enough parameters on the stack." << endl; throw "error"; }
lv = context->Stack.front();
context->Stack.pop_front();
Value nv;

if( lv.type==TYPE_NUMBER ){
    cout << "Invalid argument in call to FT. The array must be complex." << endl; throw "error";
   }
else if( lv.type==TYPE_COMPLEX ){
   if( p1 ){
      n = lv.size;
      float* pt = new float[2*n];
      float* ipt = pt;
      for(int i=0;i<n;i++,ipt+=2){ *ipt = lv.complexValue[0][i];  *(ipt+1) = lv.complexValue[1][i]; }
      cpFFT( pt, 2*n, -1 );
      nv.type = TYPE_COMPLEX;
      nv.size = n;
      nv.complexValue = new float*[2];
      nv.complexValue[0] = new float[n];
      nv.complexValue[1] = new float[n];
      ipt = pt;
      for(int i=0;i<n;i++,ipt+=2){ nv.complexValue[0][i] = *ipt;  nv.complexValue[1][i] = *(ipt+1); }
      delete[] pt;      
      }
   else{
      n = lv.size;
      float* pt = new float[2*n];
      float* ipt = pt;
      nv.type = TYPE_NUMBER;
      nv.numberValue = pt; 
      nv.size = 2*n;
      *ipt = lv.complexValue[0][1];  *(ipt+1) = lv.complexValue[0][n-1];
      ipt+=2;
      for(int i=2;i<n;i++,ipt+=2){ *ipt = lv.complexValue[0][i];  *(ipt+1) = lv.complexValue[1][i]; }
      rlFFT( pt, 2*n, -1 );
      }
   }
else{
   cout << "Invalid argument in call to FT." << endl; throw "error";
   }

context->Stack.push_front( nv );
}

// Pull value off of stack.  (It should be an array.)
void size(rlContext* context, long p1, long p2)
{
Value lv;
if( context->Stack.size() < 1 ){ cout << "size cmd.  Not enough parameters on the stack." << endl; throw "error"; }
lv = context->Stack.front();
context->Stack.pop_front();

Value nv;
nv.type = TYPE_NUMBER;
nv.numberValue = new float;
nv.size = 1;

if( lv.type==TYPE_STRING ){
   *nv.numberValue = lv.stringValue->size();
   }
else{
   *nv.numberValue = lv.size;
   }

context->Stack.push_front( nv );
}

//------------------------------------------------------------------------
template<class Ty>
struct foLn : public unary_function<Ty, Ty>{	// functor for unary operator-
	Ty operator()(const Ty& Left) const{	// apply operator- to operand
		return log(Left);
		}
};

template<class Ty>
struct foLog : public unary_function<Ty, Ty>{	// functor for unary operator-
	Ty operator()(const Ty& Left) const{	// apply operator- to operand
		return log10(Left);
		}
};

template<class Ty>
struct foExp : public unary_function<Ty, Ty>{	// functor for unary operator-
	Ty operator()(const Ty& Left) const{	// apply operator- to operand
		return exp(Left);
		}
};

template<class Ty>
struct foSqrt : public unary_function<Ty, Ty>{	// functor for unary operator-
	Ty operator()(const Ty& Left) const{	// apply operator- to operand
		return sqrt(Left);
		}
};

template< class T, class U >
void trans_op(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;
Value s1;
T op;
U cop;

if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for command." << endl; throw "error"; }

s1 = context->Stack.front();
context->Stack.pop_front();

Value rslt;
if( s1.type==TYPE_NUMBER ){
   rslt.type = TYPE_NUMBER;
   unsigned long n = s1.size;
   rslt.numberValue = new float[n]; 
   rslt.size = n;
   float* is1 = s1.numberValue;
   float* it = rslt.numberValue;
   float* iend = is1 + n;
   for( ;is1<iend;is1++,it++ ){ *it = op(*is1); }
   }
else if( s1.type==TYPE_COMPLEX ){
   rslt.type = TYPE_COMPLEX;
   unsigned long n = s1.size;
   rslt.complexValue = new float*[2];
   rslt.complexValue[0] = new float[n]; 
   rslt.complexValue[1] = new float[n]; 
   rslt.size = n;
   for( int i=0;i<n;i++ ){ 
      complex<float> crslt = cop( complex<float>( s1.complexValue[0][i],s1.complexValue[1][i] ) );
      rslt.complexValue[0][i] = crslt.real(); 
      rslt.complexValue[1][i] = crslt.imag(); 
      }
   }
else{
   cout << "Yur asking me to " << typeid(T).name() << " things that can't be done!" << endl;
   return;
   }

context->Stack.push_front( rslt );
}

// Pull value off of stack.
// Pull file name off of stack.
void trig(rlContext* context, long p1, long p2)
{
Value lv;
Value fv;
Value ov;

if( context->Stack.size()<3 ){ cout << "Run time error. Not enough parameters on the stack for trig command." << endl; throw "error"; }

lv = context->Stack.front();
context->Stack.pop_front();

fv = context->Stack.front();
context->Stack.pop_front();

ov = context->Stack.front();
context->Stack.pop_front();

Value nv;

int n = *lv.numberValue;
nv.type = TYPE_NUMBER;
nv.numberValue = new float[n];
nv.size = n;

float f = 2.0f * R_PI * *fv.numberValue;
float p = *ov.numberValue;

switch(p1){
   case 2: for( int i=0;i<n;i++){ (nv.numberValue)[i] = tan((f*(float)i/(float)(n-1))+p); } break;
   case 1: for( int i=0;i<n;i++){ (nv.numberValue)[i] = sin((f*(float)i/(float)(n-1))+p); } break;
   case 0: for( int i=0;i<n;i++){ (nv.numberValue)[i] = cos((f*(float)i/(float)(n-1))+p); } break;
   default: cout << "byte code invalid, p1 not in valid range." << endl; throw "error";
   }
context->Stack.push_front( nv );
}

void line(rlContext* context, long p1, long p2)
{
Value lv;
Value sv;
Value ov;

if( context->Stack.size()<3 ){ cout << "Run time error. Not enough parameters on the stack for line command." << endl; throw "error"; }

lv = context->Stack.front();
context->Stack.pop_front();

ov = context->Stack.front();
context->Stack.pop_front();

sv = context->Stack.front();
context->Stack.pop_front();

Value nv;

int n = *lv.numberValue;
nv.type = TYPE_NUMBER;
nv.numberValue = new float[n];
nv.size = n;

for( int i=0;i<n;i++){ (nv.numberValue)[i] = (float)i*(*sv.numberValue)+(*ov.numberValue); }
context->Stack.push_front( nv );
}

void noise(rlContext* context, long p1, long p2)
{
Value lv;
Value av;

if( context->Stack.size()<2 ){ cout << "Run time error. Not enough parameters on the stack for noise command." << endl; throw "error"; }

lv = context->Stack.front();
context->Stack.pop_front();

av = context->Stack.front();
context->Stack.pop_front();

if( lv.type!=TYPE_NUMBER ){ cout << "Run time error.  argument mismatch in call to noise." << endl; throw "error"; }
if( av.type!=TYPE_NUMBER ){ cout << "Run time error.  argument mismatch in call to noise." << endl; throw "error"; }

Value nv;

int n = *lv.numberValue;
nv.type = TYPE_NUMBER;
nv.numberValue = new float[n];
nv.size = n;

std::tr1::ranlux64_base_01 eng;
std::tr1::normal_distribution<float> generator(*(av.numberValue));

for( int i=0;i<n;i++){ (nv.numberValue)[i] = generator(eng) - generator.mean(); }
context->Stack.push_front( nv );
}

// Pull value 2 off of stack.
// Pull value 1 off of stack.
template< class T, class U >
void mathop(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;
Value s1;
Value s2;
T op;
U cop;

if( context->Stack.size()<2 ){ cout << "Run time error. Not enough parameters on the stack for rlplus command." << endl; throw "error"; }

s2 = context->Stack.front();
context->Stack.pop_front();

s1 = context->Stack.front();
context->Stack.pop_front();

if( s1.type!=s2.type ){
   cout << "Different types passed to binary math operator.  No can do!" << endl;
   return;
   }

Value rslt;
if( s1.type==TYPE_NUMBER ){
   rslt.type = TYPE_NUMBER;
   unsigned long n = s1.size;
   if( s2.size < n ){ n = s2.size; }
   (rslt.numberValue) = new float[n]; 
   rslt.size = n;
   float* is1 = s1.numberValue;
   float* is2 = s2.numberValue;
   float* it = rslt.numberValue;
   float* iend = is1 + n;
   for( ;is1<iend;is1++,is2++,it++ ){ *it = op(*is1, *is2); }
   }
else if( s1.type==TYPE_COMPLEX ){
   rslt.type = TYPE_COMPLEX;
   unsigned long n = s1.size;
   if( s2.size < n ){ n = s2.size; }
   rslt.complexValue = new float*[2];
   rslt.complexValue[0] = new float[n]; 
   rslt.complexValue[1] = new float[n]; 
   rslt.size = n;
   for( int i=0;i<n;i++ ){ 
      complex<float> crslt = cop( complex<float>( s1.complexValue[0][i],s1.complexValue[1][i] ) , complex<float>(s2.complexValue[0][i],s2.complexValue[1][i]) );
      rslt.complexValue[0][i] = crslt.real(); 
      rslt.complexValue[1][i] = crslt.imag(); 
      }
   }
else{
   cout << "Yur asking me to " << typeid(T).name() << " things that can't be added!" << endl;
   return;
   }

context->Stack.push_front( rslt );
}

void sum(rlContext* context, long p1, long p2)
{
Value s1;
if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for \"conj\" command." << endl; throw "error"; }

s1 = context->Stack.front();
if( s1.type==TYPE_STRING ){
   return;
   }

context->Stack.pop_front();

Value rslt;
if( s1.type==TYPE_NUMBER ){
   rslt.type = TYPE_NUMBER;
   unsigned long n = s1.size;
   (rslt.numberValue) = new float; 
   rslt.size = 1;
   float* is1 = s1.numberValue;
   float* iend = is1 + n;
   float sum = 0.0f;
   for( ;is1<iend;is1++ ){ sum += *is1; }
   rslt.numberValue[0] = sum;
   }
else if( s1.type==TYPE_COMPLEX ){
   rslt.type = TYPE_COMPLEX;
   unsigned long n = s1.size;
   rslt.complexValue = new float*[2];
   rslt.complexValue[0] = new float; 
   rslt.complexValue[1] = new float; 
   rslt.size = 1;
   float rl = 0.0f;
   float im = 0.0f;
   for( int i=0;i<n;i++ ){ 
      rl += s1.complexValue[0][i];
      im += s1.complexValue[1][i];
      }
   rslt.complexValue[0][0] = rl; 
   rslt.complexValue[1][0] = im; 
   }

context->Stack.push_front( rslt );
}

void neg(rlContext* context, long p1, long p2)
{
Value s1;
if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for \"conj\" command." << endl; throw "error"; }

s1 = context->Stack.front();
if( s1.type==TYPE_STRING ){
   return;
   }
context->Stack.pop_front();

Value rslt;
if( s1.type==TYPE_NUMBER ){
   rslt.type = TYPE_NUMBER;
   unsigned long n = s1.size;
   (rslt.numberValue) = new float[n]; 
   rslt.size = n;
   float* is1 = s1.numberValue;
   float* it = rslt.numberValue;
   float* iend = is1 + n;
   for( ;is1<iend;is1++,it++ ){ *it = -(*is1); }
   }
else if( s1.type==TYPE_COMPLEX ){
   rslt.type = TYPE_COMPLEX;
   unsigned long n = s1.size;
   rslt.complexValue = new float*[2];
   rslt.complexValue[0] = new float[n]; 
   rslt.complexValue[1] = new float[n]; 
   rslt.size = n;
   for( int i=0;i<n;i++ ){ 
      rslt.complexValue[0][i] = -s1.complexValue[0][i]; 
      rslt.complexValue[1][i] = -s1.complexValue[1][i]; 
      }
   }
context->Stack.push_front( rslt );
}

// Pull value 1 off of stack.
void inc(rlContext* context, RLBYTES::iterator& iter)
{
if( context->Stack.size()<2 ){ cout << "Run time error. Not enough parameters on the stack for inc command." << endl; throw "error"; }

Value stp = context->Stack.front();
context->Stack.pop_front();
Value s1 = context->Stack.front();
context->Stack.pop_front();
if( stp.type!=TYPE_NUMBER ){ cout << "Run time error.  Loop step not a number." << endl; throw "error"; }
(*s1.numberValue) += (long)(*stp.numberValue);
}

// Pull value 1 off of stack.
// Pull value 2 off of stack.
void iltj(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;

if( context->Stack.size()<3 ){ cout << "Run time error. Not enough parameters on the stack for ilt command." << endl; throw "error"; }

Value stp = context->Stack.front();
context->Stack.pop_front();

Value s2 = context->Stack.front();
context->Stack.pop_front();

Value s1 = context->Stack.front();
context->Stack.pop_front();

if( s1.type!=TYPE_NUMBER || s2.type!=TYPE_NUMBER || stp.type!=TYPE_NUMBER ){  cout << "Run time error. Parameters incorrect for iltj command." << endl; throw "error"; }

(*s1.numberValue) += (long)(*stp.numberValue);
if( *s1.numberValue <= *s2.numberValue ){
   iter += p1;
   }
}

// Pull value 1 off of stack.
void itj(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;

if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for ilt command." << endl; throw "error"; }

Value s1 = context->Stack.front();
context->Stack.pop_front();

if( s1.type!=TYPE_NUMBER ){  cout << "Run time error. Parameters incorrect for itj command." << endl; throw "error"; }

if( *s1.numberValue ){
   iter += p1;
   }
}

void jump(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;
iter += p1;
}

// Pull value 2 off of stack.
void call(rlContext* context, RLBYTES::iterator& iter)
{
RLVALUEMAP local_values;
RLVALUELIST local_stack;

long fid = iter->p1;
rlFunction& fun = Functions[fid];
rlContext local_context(fun.ByteCode,fun.Literals,local_values,local_stack);

if( fun.ParamList.size()>context->Stack.size() ){
   cout << "Parameter missing in function call." << endl;
   throw "Parameter missing";
   }

for( list<string>::reverse_iterator pit = fun.ParamList.rbegin();
      pit != fun.ParamList.rend();
      pit++ ){
   local_values[*pit] = context->Stack.front();
   context->Stack.pop_front();
   }

Evaulate( &local_context );

// Review, if parameter return supported, transfer off local_stack to global.
if( local_stack.size() > 0 ){
   context->Stack.push_front( local_stack.front() );
   }
}

/////////////////////////////////////////////////////////////
//          Logical Operators
/////////////////////////////////////////////////////////////

// Pull value 2 off of stack.
// Pull value 1 off of stack.
template< class T >
void logic_op(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;
Value s1;
Value s2;
T op;

if( context->Stack.size()<2 ){ cout << "Run time error. Not enough parameters on the stack for rlplus command." << endl; throw "error"; }

s2 = context->Stack.front();
context->Stack.pop_front();

s1 = context->Stack.front();
context->Stack.pop_front();

if( s1.type!=s2.type ){
   cout << "Different types passed to binary math operator.  No can do!" << endl;
   throw "error";
   }

Value rslt;
(rslt.numberValue) = new float; 
rslt.size = 1;
rslt.type = TYPE_NUMBER;

if( s1.type==TYPE_NUMBER ){
   rslt.numberValue[0] = (float)op(s1.numberValue[0], s2.numberValue[0]);
   }
else if( s1.type==TYPE_COMPLEX ){
   cout << "Can't compare complex values!" << endl;
   throw "error";
   }
else{
   cout << "Yur asking me to " << typeid(T).name() << " things that can't be compared!" << endl;
   return;
   }

context->Stack.push_front( rslt );
}

template< class T >
void unary_logic_op(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;
Value s1;
T op;

if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for unary logic command." << endl; throw "error"; }

s1 = context->Stack.front();
context->Stack.pop_front();

Value rslt;
(rslt.numberValue) = new float; 
rslt.size = 1;
rslt.type = TYPE_NUMBER;

if( s1.type==TYPE_NUMBER ){
   rslt.numberValue[0] = (float)op(s1.numberValue[0]);
   }
else{
   cout << "Yur asking me to " << typeid(T).name() << " things that can't be compared!" << endl;
   throw "error";
   }

context->Stack.push_front( rslt );
}

/////////////////////////////////////////////////////////////
//          Byte Code Evaulator
/////////////////////////////////////////////////////////////

void Evaulate( rlContext* context )
{
RLBYTES::iterator iter;
for( iter=context->ByteCode.begin();
     iter!=context->ByteCode.end();
     iter++ ){
   switch(iter->cmd){
      case 0x01: read(context,iter->p1,iter->p2); break;
      case 0x02: write(context,iter->p1,iter->p2); break;
      case 0x03: strliteral(context,iter->p1,iter->p2); break;
      case 0x04: tocomplex(context,iter->p1,iter->p2); break;
      case 0x05: real(context,iter->p1,iter->p2); break;
      case 0x06: imag(context,iter->p1,iter->p2); break;
      case 0x07: equals(context,iter->p1,iter->p2); break;
      case 0x08: discover(context,iter->p1,iter->p2); break;
      case 0x09: break;
      case 0x0A: break;// 
      case 0x0B: show(context,iter->p1,iter->p2); break;
      case 0x0C: seg(context,iter->p1,iter->p2);break;
      case 0x0D: chdir(context,iter->p1,iter->p2); break;
      case 0x0E: dir(context,iter->p1,iter->p2); break;
      case 0x0F: crop(context,iter->p1,iter->p2); break;
      case 0x11: mathop<plus<float>,plus<complex<float>>>(context,iter); break;
      case 0x12: mathop<minus<float>,minus<complex<float>>>(context,iter); break;
      case 0x13: mathop<multiplies<float>,multiplies<complex<float>>>(context,iter); break;
      case 0x14: mathop<divides<float>,divides<complex<float>>>(context,iter); break;
      case 0x15: inc(context,iter); break;
      case 0x18: conj(context,iter->p1,iter->p2); break;

      case 0x1A: size(context,iter->p1,iter->p2); break;
      case 0x1B: sum(context,iter->p1,iter->p2); break;
      case 0x1C: neg(context,iter->p1,iter->p2); break;

      case 0x20: trig(context,iter->p1,iter->p2); break;
      case 0x23: line(context,iter->p1,iter->p2); break;
      case 0x24: noise(context,iter->p1,iter->p2); break;

      case 0x26: trans_op< foSqrt<float>, foSqrt< complex<float>>>(context,iter); break;
      case 0x27: trans_op< foLn<float>, foSqrt< complex<float>>>(context,iter); break;
      case 0x28: trans_op< foLog<float>, foSqrt< complex<float>>>(context,iter); break;
      case 0x29: trans_op< foExp<float>, foSqrt< complex<float>>>(context,iter); break;
      case 0x30: ft(context,iter->p1,iter->p2); break;
      case 0x31: ift(context,iter->p1,iter->p2); break;

      case 0xF0: call(context,iter); break;
      case 0xF1: jump(context,iter); break;
      //case 0xF2: iltj(context,iter); break;
      case 0xF2: itj(context,iter); break;
      case 0xF3: logic_op< greater<float> >(context,iter); break;
      case 0xF4: logic_op< less<float> >(context,iter); break;
      case 0xF5: logic_op< greater_equal<float> >(context,iter); break;
      case 0xF6: logic_op< less_equal<float> >(context,iter); break;
      case 0xF7: logic_op< not_equal_to<float> >(context,iter); break;
      case 0xF8: logic_op< equal_to<float> >(context,iter); break;
      case 0xF9: unary_logic_op< logical_not<float> >(context,iter); break;
     };
   }
}

void PrintByteCode( rlContext* context, std::ostream* ois )
{
*ois << "string literals" << endl;
for(int i=0;i<context->Literals.size(); i++){
   *ois << i << " : " << context->Literals[i] << endl;
   }
*ois << endl << "byte code" << endl;
RLBYTES::iterator iter;
for( iter=context->ByteCode.begin();
     iter!=context->ByteCode.end();
     iter++ ){
   switch(iter->cmd){
      case 0x01: *ois << "read " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x02: *ois << "write " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x03: *ois << "strliteral " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x04: *ois << "tocomplex " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x05: *ois << "real " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x06: *ois << "imag " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x07: *ois << "equals " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x08: *ois << "discover " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x09: break;
      case 0x0A: break;// 
      case 0x0B: *ois << "show " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x0C: *ois << "seg " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x0D: *ois << "chdir " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x0E: *ois << "dir " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x0F: *ois << "crop " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x11: *ois << "rlplus " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x12: *ois << "subtract " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x13: *ois << "mult " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x14: *ois << "div " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x15: *ois << "inc " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x18: *ois << "conj " << iter->p1 <<" , " << iter->p2 << endl; break;

      case 0x1A: *ois << "size " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x1B: *ois << "sum " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x1C: *ois << "neg " << iter->p1 <<" , " << iter->p2 << endl; break;

      case 0x20: *ois << "trig " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x23: *ois << "line " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x24: *ois << "noise " << iter->p1 <<" , " << iter->p2 << endl; break;

      case 0x26: *ois << "rlsqrt " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x27: *ois << "rllog " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x28: *ois << "rlln " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x29: *ois << "rlexp " << iter->p1 <<" , " << iter->p2 << endl; break;

      case 0x30: *ois << "ft " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x31: *ois << "ift " << iter->p1 <<" , " << iter->p2 << endl; break;

      case 0xF0: *ois << "call " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0xF1: *ois << "jump " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0xF2: *ois << "ilt " << iter->p1 <<" , " << iter->p2 << endl; break;
      };
   }
}