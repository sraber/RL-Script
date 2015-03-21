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
#include <math.h>
#include <complex>
#include "Debug.h"

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
//--------------------------------------------

enum ValueType
{
   TYPE_TEMP         = 0x10000000,
   TYPE_EMPTY        = 0,
   TYPE_STRING       = 1,
   TYPE_NUMBER       = 2,
   TYPE_COMPLEX      = 3,
   TYPE_ARRAY        = 4,
   TYPE_COMPLEX_ARRAY= 5
};

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
   union{
      string* stringValue;
      float* numberValue;
      complex<float>* complexValue;
      float* numberArray;
   // This is a union, can't have the real side and imaginary side in the union
      float* complexArrayReal;
      //float* complexArrayImag;
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
            delete complexValue;
            break;
         case ValueType::TYPE_ARRAY:
            delete[] numberArray;
            break;
         case ValueType::TYPE_COMPLEX_ARRAY:
            delete[] complexArrayReal;
            //delete[] complexArrayImag;
            break;
         }
      }
   }

void operator=(const Value& cv){
   // This must be called first.  Clean up of the existing data
   // must be done before reassigning things.
   CleanUp();

   type = cv.type;
   pvr = cv.pvr;
   size = cv.size;
   pvr->Increment();
   stringValue   = cv.stringValue;
   numberValue   = cv.numberValue;
   complexValue  = cv.complexValue;
   numberArray   = cv.numberArray;
   complexArrayReal  = cv.complexArrayReal;
   //complexArrayImag  = cv.complexArrayImag;
   }

Value(const Value& cv){
   type = cv.type;
   pvr = cv.pvr;
   size = cv.size;
   pvr->Increment();
   stringValue   = cv.stringValue;
   numberValue   = cv.numberValue;
   complexValue  = cv.complexValue;
   numberArray   = cv.numberArray;
   complexArrayReal  = cv.complexArrayReal;
   //complexArrayImag  = cv.complexArrayImag;
   }

Value() : type(TYPE_EMPTY), size(0){
   DebugOut << "Value(" << (long)this << ")" << endl;
   pvr = new ValueRef();
   }

~Value(){
   DebugOut << "~Value(" << (long)this << ")" << endl;
   CleanUp();
   }
};

struct rlTupple
{
long cmd;
long p1;
long p2;
rlTupple() : cmd(0), p1(0), p2(0){}
};

typedef std::vector<rlTupple> RLBYTES;
typedef std::vector<string> RLLITERALS;

struct rlFunction
{
list<string> ParamList;
RLBYTES ByteCode;
RLLITERALS Literals;
};

typedef std::list<Value> RLVALUELIST;
typedef std::map<string,Value> RLVALUEMAP;
typedef std::map<string,long> RLFUNCTIONMAP;
typedef std::vector<rlFunction> RLFUNCTIONS;

RLFUNCTIONMAP FunctionMap;
RLFUNCTIONS Functions;

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
//    Abstract Syntax tree structures

class SyntaxTree
{
public:
   virtual void Produce(rlContext* context) = 0;
};

void Parse(SyntaxTree**ppST,string s);
void LeftParse(SyntaxTree**ppST,string s);

class stRoot : public SyntaxTree
{
public:
   SyntaxTree* stRight;
   stRoot(std::istream* pis, string s);
   void Produce(rlContext* context);
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
         LeftParse( &stLeft, ls);
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

//REVIEW:  Ugggggllly
//
// class root
stRoot::stRoot(std::istream* pis, string s)
{
size_t pos;
trim(s);
/*
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
*/
pos=s.find("=");
if( pos==string::npos ){
   Parse( &stRight, s);
   }
else{
   stRight = new stEquals(s.substr(0,pos), s.substr(pos+1,string::npos) );
   }
}

void stRoot::Produce(rlContext* context)
{
stRight->Produce(context);
}
//----------------------

// Use this template for unary operators
template< const long cmd > 
class stOneParam : public SyntaxTree
{
public:
   SyntaxTree* st1;
   stOneParam(string rs){
      Parse( &st1, rs );
      }
   void Produce(rlContext* context){
      st1->Produce(context);
      rlTupple tup;
      tup.cmd = cmd;
      context->ByteCode.push_back(tup);
      }
};

// Use this template for binary operators
template< const long cmd > 
class stTwoParam : public SyntaxTree
{
public:
   SyntaxTree* stLeft;
   SyntaxTree* stRight;
   stTwoParam(string ls, string rs){
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

class stTrig : public SyntaxTree
{
   long Op;
public:
   SyntaxTree* st1;
   SyntaxTree* st2;
   SyntaxTree* st3;
   stTrig(string rs, long op){
      size_t pos1, pos2;
      Op = op;

      pos1 = 0;
      pos2 = rs.find_first_of( "," );
      if( pos2==string::npos ){ cout << "not enough parameters for cos command. cos Len, Hz, phase (optional)" << endl; throw "error"; }
      Parse( &st1, rs.substr(pos1,pos2-pos1) );
      pos1=pos2+1;

      pos2 = rs.find_first_of( ",", pos1 );
      if( pos2==string::npos ){
         if( pos1>rs.size() ){ cout << "not enough parameters for cos command. cos Len, Hz, phase (optional)" << endl; throw "error"; }
         Parse( &st2, rs.substr(pos1) );
         Parse( &st3, string("0.0") );
         }
      else{ 
         Parse( &st2, rs.substr(pos1) );
         pos1=pos2+1;
         Parse( &st3, rs.substr(pos1) );
         }
      }
   void Produce(rlContext* context){
      st3->Produce(context);
      st2->Produce(context);
      st1->Produce(context);
      // Cos Wave
      rlTupple tup;
      tup.cmd = 0x20;
      tup.p1 = Op;
      context->ByteCode.push_back(tup);
      }
};

class stCos : public stTrig
{
public:
   stCos(string s) : stTrig(s,0){}
};

class stSin : public stTrig
{
public:
   stSin(string s) : stTrig(s,1){}
};

class stTan : public stTrig
{
public:
   stTan(string s) : stTrig(s,2){}
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

class stLeftLiteral : public SyntaxTree
{
public:
   string value;
   stLeftLiteral(string rs){
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
      }
};

void Parse(SyntaxTree**ppST,string s)
{
trim(s);

for(int n=1; n<=s.size(); n++ ){
   size_t pos;
   string s1 = s.substr(0,n);

// Order the statements by number of characters, small to large.
/*
   pos=s1.find("cd ");
   if( pos==0 ){
      *ppST = new stChDir(s.substr(pos+3));
      return;
      }
   pos=s1.find("ls");
   if( pos==0 ){
      *ppST = new stDir(s.substr(pos+2));
      return;
      }
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
   pos=s1.find("ft ");
   if( pos==0 ){
      *ppST = new stOneParam<0x30>(s.substr(pos+3));
      return;
      }
   pos=s1.find("ift ");
   if( pos==0 ){
      *ppST = new stOneParam<0x31>(s.substr(pos+4));
      return;
      }
   pos=s1.find("sqrt ");
   if( pos==0 ){
      *ppST = new stOneParam<0x26>(s.substr(pos+5));
      return;
      }
   pos=s1.find("log ");
   if( pos==0 ){
      *ppST = new stOneParam<0x27>(s.substr(pos+4));
      return;
      }
   pos=s1.find("ln ");
   if( pos==0 ){
      *ppST = new stOneParam<0x28>(s.substr(pos+3));
      return;
      }
   pos=s1.find("exp ");
   if( pos==0 ){
      *ppST = new stOneParam<0x29>(s.substr(pos+4));
      return;
      }
   pos=s1.find("dir");
   if( pos==0 && s.size()==3){
      *ppST = new stDir(s.substr(pos+3));
      return;
      }
*/
   pos=s1.find("cos ");
   if( pos==0 ){
      *ppST = new stCos(s.substr(pos+4));
      return;
      }
   pos=s1.find("sin ");
   if( pos==0 ){
      *ppST = new stSin(s.substr(pos+4));
      return;
      }
/*
   pos=s1.find("tan ");
   if( pos==0 ){
      *ppST = new stTan(s.substr(pos+4));
      return;
      }
   pos=s1.find("crop ");
   if( pos==0 ){
      *ppST = new stCrop(s.substr(pos+5));
      return;
      }
   pos=s1.find("read ");
   if( pos==0 ){
      *ppST = new stRead(s.substr(pos+5));
      return;
      }
   pos=s1.find("conj ");
   if( pos==0 ){
      *ppST = new stConjugate(s.substr(pos+5));
      return;
      }
   pos=s1.find("real ");
   if( pos==0 ){
      *ppST = new stReal(s.substr(pos+5));
      return;
      }
   pos=s1.find("size ");
   if( pos==0 ){
      *ppST = new stOneParam<0x1A>(s.substr(pos+5));
      return;
      }
   pos=s1.find("imag ");
   if( pos==0 ){
      *ppST = new stImag(s.substr(pos+5));
      return;
      }
   pos=s1.find("write ");
   if( pos==0 ){
      *ppST = new stWrite(s.substr(pos+6));
      return;
      }
   pos=s1.find("complex ");
   if( pos==0 ){
      *ppST = new stComplex(s.substr(pos+8));
      return;
      }
*/
   RLFUNCTIONMAP::iterator fit;
   if( s1.size()==s.size() ){
      fit = FunctionMap.find( s1 + " " );
      }
   else{
      fit = FunctionMap.find( s1 );
      }
/*
   if( fit!=FunctionMap.end() ){
      *ppST = new stCall(s.substr(s1.size()), fit->second);
      return;
      }
*/
   if(      s[n-1]=='+' ){
      *ppST = new stTwoParam<0x11>(s.substr(0,n-1), s.substr(n,string::npos) );
      return;
      }
   else if( s[n-1]=='-' ){
      *ppST = new stTwoParam<0x12>(s.substr(0,n-1), s.substr(n+1,string::npos) );
      return;
      }
   else if( s[n-1]=='*' ){
      *ppST = new stTwoParam<0x13>(s.substr(0,n-1), s.substr(n+1,string::npos) );
      return;
      }
   else if( s[n-1]=='/' ){
      *ppST = new stTwoParam<0x14>(s.substr(0,n-1), s.substr(n+1,string::npos) );
      return;
      }
   }

*ppST = new stLiteral(s);
}

void LeftParse(SyntaxTree**ppST,string s)
{
size_t pos;
/*
pos=s.find("read ");
if( pos!=string::npos ){
   return;
   }
pos=s.find("write ");
if( pos!=string::npos ){
   return;
   }
pos=s.find("lp ");
if( pos!=string::npos ){
   return;
   }
pos=s.find("hp ");
if( pos!=string::npos ){
   return;
   }
pos=s.find("complex ");
if( pos!=string::npos ){
   *ppST = new stComplex(s.substr(pos+8));
   return;
   }
*/
*ppST = new stLeftLiteral(s);
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
/*
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
*/
   else{
      try{
         stRoot root(&cin, cmd);
         root.Produce(&GlobalContext);
         Evaulate(&GlobalContext);
/*
         fstream file( "bytecode.txt", ios::out );

         RLLITERALS::iterator it;
         int n = 0;
         for( it = GlobalContext.Literals.begin();
              it!= GlobalContext.Literals.end();
              it++, n++ ){
            file << n << " , " << *it << endl;
            }

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
      cout << *rlv.numberValue << endl;
      break;
   case ValueType::TYPE_COMPLEX:
      cout << *rlv.complexValue << endl;
      break;
   case ValueType::TYPE_ARRAY:
      cout << *rlv.numberArray << endl;
      break;
   case ValueType::TYPE_COMPLEX_ARRAY:
      //cout << *rlv.complexArray << endl;
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
      strstream ss;
      ss << rlv.stringValue->c_str() << ends;
      Value nv;
      nv.type=TYPE_NUMBER;
      nv.size=1;
      nv.numberValue = new float;
      ss >> (*nv.numberValue);
      context->Stack.push_front( nv );
      }
   return;
   }

}

/*
void tocomplex(rlContext* context, long p1, long p2)
{
Value rlv;
if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for \"complex\" command." << endl; throw "error"; }

rlv = context->Stack.front();
if( rlv.type==TYPE_COMPLEX_ARRAY || rlv.type==TYPE_COMPLEX ){
   return;
   }

Value nv;
if( rlv.type==TYPE_ARRAY ){
   int n = rlv.numberArray->size();
   nv.type = TYPE_COMPLEX_ARRAY;
   nv.complexArray = new VectorXcf(n);
   for(int i=0;i<n;i++){
      (*nv.complexArray)(i) = (*rlv.numberArray)(i);
      }

   context->Stack.pop_front();
   context->Stack.push_front( nv );
   return;
   }

if( rlv.type==TYPE_NUMBER ){
   nv.type = TYPE_COMPLEX;
   nv.complexValue = new complex<float>(*rlv.numberValue,0.0f);
   context->Stack.pop_front();
   context->Stack.push_front( nv );
   return;
   }
}

void real(rlContext* context, long p1, long p2)
{
Value rlv;
if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for \"real\" command." << endl; throw "error"; }

rlv = context->Stack.front();
if( rlv.type==TYPE_ARRAY || rlv.type==TYPE_NUMBER || rlv.type==TYPE_STRING ){
   return;
   }
context->Stack.pop_front();

Value nv;
if( rlv.type==TYPE_COMPLEX_ARRAY ){
   int n = rlv.complexArray->size();
   nv.type = TYPE_ARRAY;
   nv.numberArray = new VectorXf(n);
   (*nv.numberArray) = rlv.complexArray->real();
   context->Stack.push_front( nv );
   return;
   }

if( rlv.type==TYPE_COMPLEX ){
   nv.type = TYPE_NUMBER;
   nv.numberValue = new float;
   (*nv.numberValue) = rlv.complexValue->real();
   context->Stack.push_front( nv );
   return;
   }
}

void imag(rlContext* context, long p1, long p2)
{
Value rv;
if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for \"imag\" command." << endl; throw "error"; }

rv = context->Stack.front();
if( rv.type==TYPE_STRING ){
   return;
   }
context->Stack.pop_front();

Value nv;
switch( rv.type ){
   case ValueType::TYPE_NUMBER:
      {
      nv.type = TYPE_NUMBER;
      nv.numberValue = new float(0.0f);
      break;
      }
   case ValueType::TYPE_COMPLEX:
      {
      nv.type = TYPE_NUMBER;
      nv.numberValue = new float(rv.complexValue->imag());
      break;
      }
   case ValueType::TYPE_ARRAY:
      {
      int n = rv.numberArray->size();
      nv.type = TYPE_ARRAY;
      nv.numberArray = new VectorXf(n);
      for(int i=0;i<n;i++){ (*nv.numberArray)(i) = 0.0f; }
      break;
      }
   case ValueType::TYPE_COMPLEX_ARRAY:
      {
      int n = rv.complexArray->size();
      nv.type = TYPE_ARRAY;
      nv.numberArray = new VectorXf(n);
      (*nv.numberArray)=rv.complexArray->imag();
      break;
      }
   }

context->Stack.push_front( nv );
}

void conj(rlContext* context, long p1, long p2)
{
Value rlv;
if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for \"conj\" command." << endl; throw "error"; }

rlv = context->Stack.front();
if( rlv.type!=TYPE_COMPLEX_ARRAY && rlv.type!=TYPE_COMPLEX ){
   return;
   }
context->Stack.pop_front();

Value nv;
if( rlv.type==TYPE_COMPLEX_ARRAY ){
   int n = rlv.numberArray->size();
   nv.type = TYPE_COMPLEX_ARRAY;
   nv.complexArray = new VectorXcf(n);
   (*nv.complexArray) = rlv.complexArray->conjugate();
   context->Stack.push_front( nv );
   return;
   }

if( rlv.type==TYPE_COMPLEX ){
   nv.type = TYPE_COMPLEX;
   nv.complexValue = new complex<float>(rlv.complexValue->real(),-rlv.complexValue->imag());
   context->Stack.push_front( nv );
   return;
   }
}
*/

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

// REVIEW: Left side, as things stand, should always be string.
if( lv.type!=TYPE_STRING ){    cout << "Wrong type for lvalue." << endl; throw "error"; }
// Why bother to find it in the globals, just make what we need and put it there.

Value nv;
switch( rv.type ){
   case ValueType::TYPE_STRING:
      {
      nv.type = TYPE_STRING;
      nv.stringValue = new string( *rv.stringValue );
      context->Locals[ *lv.stringValue ] = nv;
      break;
      }
   case ValueType::TYPE_NUMBER:
      {
      nv.type = TYPE_NUMBER;
      nv.numberValue = new float(*rv.numberValue);
      context->Locals[ *lv.stringValue ] = nv;
      break;
      }
   case ValueType::TYPE_COMPLEX:
      {
      nv.type = TYPE_COMPLEX;
      nv.complexValue = new complex<float>(*rv.complexValue);
      context->Locals[ *lv.stringValue ] = nv;
      break;
      }
   case ValueType::TYPE_ARRAY:
      {
      int n = rv.size;
      nv.type = TYPE_ARRAY;
      (nv.numberArray) = new float[n];
      float* it = nv.numberArray;
      float* is = rv.numberArray;
      float* iend = is + n;
      for( ;is<iend;is++,it++ ){ *it = *is; }
      context->Locals[ *lv.stringValue ] = nv;
      break;
      }
   case ValueType::TYPE_COMPLEX_ARRAY:
      {
/*
      int n = rv.complexArray->size();
      nv.type = TYPE_COMPLEX_ARRAY;
      nv.complexArray = new VectorXcf(*rv.complexArray);
      context->Locals[ *lv.stringValue ] = nv;
      break;
*/
      }
   }
}

/*
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
      file << *rv.numberValue;
      break;
      }
   case ValueType::TYPE_COMPLEX:
      {
      file << *rv.complexValue;
      break;
      }
   case ValueType::TYPE_ARRAY:
      {
      file << *rv.numberArray;
      break;
      }
   case ValueType::TYPE_COMPLEX_ARRAY:
      {
      for( int i=0;
           i < rv.complexArray->size();
           i++ ){
         file << (*rv.complexArray)(i).real() << "," << (*rv.complexArray)(i).imag() << endl;
         }
      break;
      }
   };
}

void read(rlContext* context, long p1, long p2)
{
Value rlv;
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

vector<float> temp;
while(!file.eof() ){
   file.getline(buf,100);
   strstream ss;
   ss << buf << ends;
   ss >> val;
   temp.push_back(val);
   }

int n = temp.size();
rlv.numberArray = new VectorXf(n);
rlv.type = TYPE_ARRAY;

for(int i=0;i<n;i++){
   (*rlv.numberArray)[i] = temp[i];
   }

context->Stack.push_front(rlv);
}

void chdir(rlContext* context, long p1, long p2)
{
Value path;

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

if( av.type!=::TYPE_ARRAY && av.type!=::TYPE_COMPLEX_ARRAY ){
   cout << "nothing to crop.  Parameter is not an array." << endl;
   return;
   }

Value nv;
if( av.type==TYPE_ARRAY ){
   int n = av.numberArray->size();
   nv.type = TYPE_ARRAY;
   nv.numberArray = new VectorXf();
   *nv.numberArray = av.numberArray->segment((long)(*sv.numberValue),(long)(*ev.numberValue));

   context->Stack.push_front( nv );
   return;
   }
if( av.type==TYPE_COMPLEX_ARRAY ){
   int n = av.complexArray->size();
   nv.type = TYPE_COMPLEX_ARRAY;
   nv.complexArray = new VectorXcf();
   *nv.complexArray = av.complexArray->segment((long)(*sv.numberValue),(long)(*ev.numberValue));

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

if( lv.type==TYPE_ARRAY ){
   n = lv.numberArray->size();
   float* pt = new float[n];
   float* ipt = pt;
   for(int i=0;i<n;i++,ipt++){ *ipt = (*lv.numberArray)(i); }
   rlFFT( pt, n );
   n >>= 1;
   nv.type = TYPE_COMPLEX_ARRAY;
   nv.complexArray = new VectorXcf(n); 
   (*nv.complexArray)[0] = *pt;
   (*nv.complexArray)[n-1] = *(pt+1);
   ipt = pt + 2;
   for(int i=1;i<(n-1);i++,ipt+=2){ (*nv.complexArray)[i] = complex<float>(*ipt,*(ipt+1)); }

   delete[] pt;  
   }
else if( lv.type==TYPE_COMPLEX_ARRAY ){
   n = lv.complexArray->size();
   float* pt = new float[2*n];
   float* ipt = pt;
   for(int i=0;i<n;i++,ipt+=2){ *ipt = (*lv.complexArray)(i).real();  *(ipt+1) = (*lv.complexArray)(i).imag(); }
   cpFFT( pt, 2*n );
   nv.type = TYPE_COMPLEX_ARRAY;
   nv.complexArray = new VectorXcf(n); 
   ipt = pt;
   for(int i=0;i<n;i++,ipt+=2){ (*nv.complexArray)[i] = complex<float>(*ipt,*(ipt+1)); }

   delete[] pt;  
   }
else{
   cout << "Invalid argument in call to FT." << endl; throw "error";
   }

context->Stack.push_front( nv );
}

//ift doesn't work!!!!

// Pull value off of stack.  (It should be an array.)
void ift(rlContext* context, long p1, long p2)
{
Value lv;
long n;
if( context->Stack.size() < 1 ){ cout << "FT cmd.  Not enough parameters on the stack." << endl; throw "error"; }
lv = context->Stack.front();
context->Stack.pop_front();
Value nv;

if( lv.type==TYPE_ARRAY ){
    cout << "Invalid argument in call to FT. The array must be complex." << endl; throw "error";
   }
else if( lv.type==TYPE_COMPLEX_ARRAY ){
   n = lv.complexArray->size();
   float* pt = new float[2*n];
   float* ipt = pt;
   for(int i=0;i<n;i++,ipt+=2){ *ipt = (*lv.complexArray)(i).real();  *(ipt+1) = (*lv.complexArray)(i).imag(); }
   icpFFT( pt, 2*n );
   nv.type = TYPE_COMPLEX_ARRAY;
   nv.complexArray = new VectorXcf(n); 
   ipt = pt;
   for(int i=0;i<n;i++,ipt+=2){ (*nv.complexArray)[i] = complex<float>(*ipt,*(ipt+1)); }

   delete[] pt;  
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
long n;
if( context->Stack.size() < 1 ){ cout << "size cmd.  Not enough parameters on the stack." << endl; throw "error"; }
lv = context->Stack.front();
context->Stack.pop_front();

Value nv;
nv.type = TYPE_NUMBER;
nv.numberValue = new float;

if( lv.type==TYPE_ARRAY ){
   *nv.numberValue = lv.numberArray->size();
   }
else if( lv.type==TYPE_COMPLEX_ARRAY ){
   *nv.numberValue = lv.complexArray->size();
   }
else if( lv.type==TYPE_STRING ){
   *nv.numberValue = lv.stringValue->size();
   }
else{
   *nv.numberValue = 1;
   }

context->Stack.push_front( nv );
}

void rlsqrt(rlContext* context, long p1, long p2)
{
Value lv;
long n;
if( context->Stack.size() < 1 ){ cout << "FT cmd.  Not enough parameters on the stack." << endl; throw "error"; }
lv = context->Stack.front();
context->Stack.pop_front();
Value rslt;
if( lv.type==TYPE_ARRAY ){
   rslt.type = TYPE_ARRAY;
   int n = lv.numberArray->size();
   rslt.numberArray = new VectorXf( n ); 
   for(int i=0;i<n;i++){ (*rslt.numberArray)(i) = sqrt( (*lv.numberArray)(i) ); }
   }
//else if( s1.type==TYPE_COMPLEX_ARRAY ){
//   rslt.type = TYPE_COMPLEX_ARRAY;
//   rslt.complexArray = new VectorXcf(); 
//   *rslt.complexArray = *s1.complexArray - *s2.complexArray;
//   }
else if( lv.type==TYPE_NUMBER ){
   rslt.type = TYPE_NUMBER;
   rslt.numberValue = new float; 
   *rslt.numberValue = sqrt( *lv.numberValue );
   }
else{
   cout << "Yur asking me to sqrt things that can't be sqrt'ed!" << endl;
   return;
   }
context->Stack.push_front( rslt );
}

void rllog(rlContext* context, long p1, long p2)
{
Value lv;
long n;
if( context->Stack.size() < 1 ){ cout << "log cmd.  Not enough parameters on the stack." << endl; throw "error"; }
lv = context->Stack.front();
context->Stack.pop_front();
Value rslt;
if( lv.type==TYPE_ARRAY ){
   rslt.type = TYPE_ARRAY;
   int n = lv.numberArray->size();
   rslt.numberArray = new VectorXf( n ); 
   for(int i=0;i<n;i++){ (*rslt.numberArray)(i) = log10( (*lv.numberArray)(i) ); }
   }
else if( lv.type==TYPE_NUMBER ){
   rslt.type = TYPE_NUMBER;
   rslt.numberValue = new float; 
   *rslt.numberValue = log10( *lv.numberValue );
   }
else{
   cout << "Yur asking me to log things that can't be log'ed!" << endl;
   return;
   }

context->Stack.push_front( rslt );
}

void rlln(rlContext* context, long p1, long p2)
{
Value lv;
long n;
if( context->Stack.size() < 1 ){ cout << "log cmd.  Not enough parameters on the stack." << endl; throw "error"; }
lv = context->Stack.front();
context->Stack.pop_front();
Value rslt;
if( lv.type==TYPE_ARRAY ){
   rslt.type = TYPE_ARRAY;
   int n = lv.numberArray->size();
   rslt.numberArray = new VectorXf( n ); 
   for(int i=0;i<n;i++){ (*rslt.numberArray)(i) = log( (*lv.numberArray)(i) ); }
   }
else if( lv.type==TYPE_NUMBER ){
   rslt.type = TYPE_NUMBER;
   rslt.numberValue = new float; 
   *rslt.numberValue = log( *lv.numberValue );
   }
else{
   cout << "Yur asking me to ln things that can't be ln'ed!" << endl;
   return;
   }

context->Stack.push_front( rslt );
}

void rlexp(rlContext* context, long p1, long p2)
{
Value lv;
long n;
if( context->Stack.size() < 1 ){ cout << "exp cmd.  Not enough parameters on the stack." << endl; throw "error"; }
lv = context->Stack.front();
context->Stack.pop_front();
Value rslt;
if( lv.type==TYPE_ARRAY ){
   rslt.type = TYPE_ARRAY;
   int n = lv.numberArray->size();
   rslt.numberArray = new VectorXf( n ); 
   for(int i=0;i<n;i++){ (*rslt.numberArray)(i) = exp( (*lv.numberArray)(i) ); }
   }
else if( lv.type==TYPE_NUMBER ){
   rslt.type = TYPE_NUMBER;
   rslt.numberValue = new float; 
   *rslt.numberValue = exp( *lv.numberValue );
   }
else{
   cout << "Yur asking me to exp things that can't be exp'ed!" << endl;
   return;
   }

context->Stack.push_front( rslt );
}
*/

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
nv.type = TYPE_ARRAY;
nv.numberArray = new float[n];
nv.size = n;
float f = 2.0f * R_PI * *fv.numberValue;
float p = *ov.numberValue;

switch(p1){
   case 2: for( int i=0;i<n;i++){ (nv.numberArray)[i] = tan((f*(float)i/(float)(n-1))+p); } break;
   case 1: for( int i=0;i<n;i++){ (nv.numberArray)[i] = sin((f*(float)i/(float)(n-1))+p); } break;
   case 0: for( int i=0;i<n;i++){ (nv.numberArray)[i] = cos((f*(float)i/(float)(n-1))+p); } break;
   default: cout << "byte code invalid, p1 not in valid range." << endl; throw "error";
   }
context->Stack.push_front( nv );
}

// Pull value 1 off of stack.
void inc(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;

if( context->Stack.size()<1 ){ cout << "Run time error. Not enough parameters on the stack for inc command." << endl; throw "error"; }

Value s1 = context->Stack.front();
context->Stack.pop_front();
(*s1.numberValue) += p1;
}


// Pull value 2 off of stack.
// Pull value 1 off of stack.
void rlplus(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;
Value s1;
Value s2;

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
if( s1.type==TYPE_ARRAY ){
   rslt.type = TYPE_ARRAY;
   unsigned long n = s1.size;
   if( s2.size < n ){ n = s2.size; }
   (rslt.numberArray) = new float[n]; 
   rslt.size = n;
   float* is1 = s1.numberArray;
   float* is2 = s2.numberArray;
   float* it = rslt.numberArray;
   float* iend = is1 + n;
   for( ;is1<iend;is1++,is2++,it++ ){ *it = *is1 + *is2; }
   }
else if( s1.type==TYPE_COMPLEX_ARRAY ){
/*
   rslt.type = TYPE_COMPLEX_ARRAY;
   if(s1.complexArray->size()!=s2.complexArray->size()){ cout << "Arrays are different lengths. They must be the same.  Try using  crop." << endl;  throw "error"; }
   rslt.complexArray = new VectorXcf(); 
   *rslt.complexArray = *s1.complexArray + *s2.complexArray;
*/
   }
else if( s1.type==TYPE_NUMBER ){
   rslt.type = TYPE_NUMBER;
   rslt.size = 1;
   rslt.numberValue = new float; 
   *rslt.numberValue = *s1.numberValue + *s2.numberValue;
   }
else{
   cout << "Yur asking me to add things that can't be added!" << endl;
   return;
   }

context->Stack.push_front( rslt );
}

// Pull value 2 off of stack.
// Pull value 1 off of stack.
void subtract(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;
Value s1;
Value s2;
if( context->Stack.size()<2 ){ cout << "Run time error. Not enough parameters on the stack for subtract command." << endl; throw "error"; }

s2 = context->Stack.front();
context->Stack.pop_front();

s1 = context->Stack.front();
context->Stack.pop_front();

if( s1.type!=s2.type ){
   cout << "Different types passed to binary math operator.  No can do!" << endl;
   return;
   }

Value rslt;
if( s1.type==TYPE_ARRAY ){
   rslt.type = TYPE_ARRAY;
   unsigned long n = s1.size;
   if( s2.size < n ){ n = s2.size; }
   (rslt.numberArray) = new float[n]; 
   rslt.size = n;
   float* is1 = s1.numberArray;
   float* is2 = s2.numberArray;
   float* it = rslt.numberArray;
   float* iend = is1 + n;
   for( ;is1<iend;is1++,is2++,it++ ){ *it = *is1 - *is2; }
   }
else if( s1.type==TYPE_COMPLEX_ARRAY ){
/*
   rslt.type = TYPE_COMPLEX_ARRAY;
   if(s1.complexArray->size()!=s2.complexArray->size()){ cout << "Arrays are different lengths. They must be the same.  Try using  crop." << endl;  throw "error"; }
   rslt.complexArray = new VectorXcf(); 
   *rslt.complexArray = *s1.complexArray - *s2.complexArray;
*/
   }
else if( s1.type==TYPE_NUMBER ){
   rslt.type = TYPE_NUMBER;
   rslt.size = 1;
   rslt.numberValue = new float; 
   *rslt.numberValue = *s1.numberValue - *s2.numberValue;
   }
else{
   cout << "Yur asking me to subtract things that can't be added!" << endl;
   return;
   }

context->Stack.push_front( rslt );
}

// Pull value 2 off of stack.
// Pull value 1 off of stack.
void mult(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;
Value s1;
Value s2;
if( context->Stack.size()<2 ){ cout << "Run time error. Not enough parameters on the stack for mult command." << endl; throw "error"; }

s2 = context->Stack.front();
context->Stack.pop_front();

s1 = context->Stack.front();
context->Stack.pop_front();

if( s1.type!=s2.type ){
   cout << "Different types passed to binary math operator.  No can do!" << endl;
   return;
   }

Value rslt;
if( s1.type==TYPE_ARRAY ){
   rslt.type = TYPE_ARRAY;
   unsigned long n = s1.size;
   if( s2.size < n ){ n = s2.size; }
   (rslt.numberArray) = new float[n]; 
   rslt.size = n;
   float* is1 = s1.numberArray;
   float* is2 = s2.numberArray;
   float* it = rslt.numberArray;
   float* iend = is1 + n;
   for( ;is1<iend;is1++,is2++,it++ ){ *it = *is1 * *is2; }
   }
else if( s1.type==TYPE_COMPLEX_ARRAY ){
/*
   int n = s1.complexArray->size() < s2.complexArray->size() ? s1.complexArray->size() : s2.complexArray->size();
   rslt.type = TYPE_COMPLEX_ARRAY;
   rslt.complexArray = new VectorXcf(n); 
   for( int i=0; i < n; i++ ){
      (*rslt.complexArray)(i) = (*s1.complexArray)(i) * (*s2.complexArray)(i);
      }
*/
   }
else if( s1.type==TYPE_NUMBER ){
   rslt.type = TYPE_NUMBER;
   rslt.numberValue = new float; 
   rslt.size = 1;
   *rslt.numberValue = *s1.numberValue * *s2.numberValue;
   }
else if( s1.type==TYPE_NUMBER ){
   rslt.type = TYPE_COMPLEX;
   rslt.complexValue = new complex<float>();
   rslt.size = 1;
   *rslt.complexValue = *s1.complexValue * *s2.complexValue;
   }
else{
   cout << "Yur asking me to multiply things that can't be multiplied!" << endl;
   return;
   }

context->Stack.push_front( rslt );
}

void div(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;
Value s1;
Value s2;
if( context->Stack.size()<2 ){ cout << "Run time error. Not enough parameters on the stack for sub command." << endl; throw "error"; }

s2 = context->Stack.front();
context->Stack.pop_front();

s1 = context->Stack.front();
context->Stack.pop_front();

if( s1.type!=s2.type ){
   cout << "Different types passed to binary math operator.  No can do!" << endl;
   return;
   }

Value rslt;
if( s1.type==TYPE_ARRAY ){
   rslt.type = TYPE_ARRAY;
   unsigned long n = s1.size;
   if( s2.size < n ){ n = s2.size; }
   (rslt.numberArray) = new float[n]; 
   rslt.size = n;
   float* is1 = s1.numberArray;
   float* is2 = s2.numberArray;
   float* it = rslt.numberArray;
   float* iend = is1 + n;
   for( ;is1<iend;is1++,is2++,it++ ){ *it = *is1 / *is2; }
   }
else if( s1.type==TYPE_COMPLEX_ARRAY ){
/*
   int n = s1.complexArray->size() < s2.complexArray->size() ? s1.complexArray->size() : s2.complexArray->size();
   rslt.type = TYPE_COMPLEX_ARRAY;
   rslt.complexArray = new VectorXcf(n); 
   for( int i=0; i < n; i++ ){
      (*rslt.complexArray)(i) = (*s1.complexArray)(i) / (*s2.complexArray)(i);
      }
*/
   }
else if( s1.type==TYPE_NUMBER ){
   rslt.type = TYPE_NUMBER;
   rslt.numberValue = new float; 
   *rslt.numberValue = *s1.numberValue / *s2.numberValue;
   }
else if( s1.type==TYPE_NUMBER ){
   rslt.type = TYPE_COMPLEX;
   rslt.complexValue = new complex<float>();
   *rslt.complexValue = *s1.complexValue / *s2.complexValue;
   }
else{
   cout << "Yur asking me to multiply things that can't be multiplied!" << endl;
   return;
   }

context->Stack.push_front( rslt );
}

// Pull value 1 off of stack.
// Pull value 2 off of stack.
void ilt(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;
Value s1;
Value s2;
if( context->Stack.size()<2 ){ cout << "Run time error. Not enough parameters on the stack for ilt command." << endl; throw "error"; }

s2 = context->Stack.front();
context->Stack.pop_front();

s1 = context->Stack.front();
context->Stack.pop_front();

if( !(*s1.numberValue < *s2.numberValue) ){
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
//          Byte Code Evaulator
/////////////////////////////////////////////////////////////

void Evaulate( rlContext* context )
{
RLBYTES::iterator iter;
for( iter=context->ByteCode.begin();
     iter!=context->ByteCode.end();
     iter++ ){
   switch(iter->cmd){
//      case 0x01: read(context,iter->p1,iter->p2); break;
//      case 0x02: write(context,iter->p1,iter->p2); break;
      case 0x03: strliteral(context,iter->p1,iter->p2); break;
//      case 0x04: tocomplex(context,iter->p1,iter->p2); break;
//      case 0x05: real(context,iter->p1,iter->p2); break;
//      case 0x06: imag(context,iter->p1,iter->p2); break;
      case 0x07: equals(context,iter->p1,iter->p2); break;
      case 0x08: discover(context,iter->p1,iter->p2); break;
      case 0x09: break;
      case 0x0A: break;// 
      case 0x0B: show(context,iter->p1,iter->p2); break;
      case 0x0C: break;// 
//      case 0x0D: chdir(context,iter->p1,iter->p2); break;
//      case 0x0E: dir(context,iter->p1,iter->p2); break;
//      case 0x0F: crop(context,iter->p1,iter->p2); break;

      case 0x11: rlplus(context,iter); break;
      case 0x12: subtract(context,iter); break;
      case 0x13: mult(context,iter); break;
      case 0x14: div(context,iter); break;
/*
      case 0x15: inc(context,iter); break;

      case 0x18: conj(context,iter->p1,iter->p2); break;

      case 0x1A: size(context,iter->p1,iter->p2); break;
*/
      case 0x20: trig(context,iter->p1,iter->p2); break;
/*
      case 0x26: rlsqrt(context,iter->p1,iter->p2); break;
      case 0x27: rllog(context,iter->p1,iter->p2); break;
      case 0x28: rlln(context,iter->p1,iter->p2); break;
      case 0x29: rlexp(context,iter->p1,iter->p2); break;

      case 0x30: ft(context,iter->p1,iter->p2); break;
      case 0x31: ift(context,iter->p1,iter->p2); break;
*/
      case 0xF0: call(context,iter); break;
      case 0xF1: jump(context,iter); break;
      case 0xF2: ilt(context,iter); break;
     };
   }
}
