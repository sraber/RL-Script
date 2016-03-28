// rl.cpp : Defines the entry point for the console application.
//

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
#include "SysError.h"

#include "ml.h"

using namespace std;

#define R_PI 3.14159265f

//----------------------------------------
// Users of the MyLanguage library must implement this function,
// even if it is empty.
bool ml_parse(SyntaxTree**ppST,string s);
//Example:
//bool ml_parse(SyntaxTree**ppST,string s){ return false; }

void ml_evaulate( rlContext* context, rlTupple& rlt );

// Users of the MyLanguage library must also implement this function.
// It may be empty.
//Example:
//void ml_evaulate( rlContext* context, rlTupple& rlt )
//{
//switch(rlt.cmd){
//   case 0x0100: your_function1(context,rlt.p1,rlt.p2); break;
//   case 0x0200: another_function(context,rlt); break;
//   }
//}
//--------------------------------------------

HELP_MAP HelpMap;

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

inline bool FindString( std::string cmd, std::list<string>& lcomp, int* pret )
{
int i = 1;
*pret = 0;
std::list<string>::iterator istr;
for( istr =  lcomp.begin();
     istr != lcomp.end();
     istr++, i++ ){
   if( cmd.find(*istr) ==  0){
      *pret = i;
      return true;
      }
   }
return false;
}

void MakeComplex( Value& s, long size )
{
s.type = TYPE_COMPLEX;
s.size = size;
s.complexValue = new float*[2];
s.complexValue[0] = new float[size]; 
s.complexValue[1] = new float[size]; 
}

//-------------------------------------------
// This function assumes that the string does not have even paired braces.
// It assumes that the first open brace has been found and it is given a string 
// that points one character past that.  There may be nested pairs of braces so 
// count up for each  open and count down for each close.  When the count reaches
// zero the character position.
//
size_t FindClosingBrace( string &str )
{
int open = 0;
for(unsigned int i=0;i<str.length();i++){
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
ThrowSyntaxAssert( "No closing brace" )
return 0; // will never hit this line.
}

// Could have used a template.
size_t FindClosingSquareBrace( string &str )
{
int open = 0;
for(unsigned int i=0;i<str.length();i++){
   if( str[i]=='[' ){
      open++;
      }
   else if( str[i]==']' ){
      if( !open ){
         return (size_t)i;
         }
      else{
         open--;
         }
      }
   }
ThrowSyntaxAssert( "No closing square brace" )
return 0; // will never hit this line.
}

// Looking for a comma but also want to skip over text inside of braces ().
// Example text:  myfunction a,((somefunction a,b,c)/2),7
// After the first comma we don't want to hit the comma's in a,b,c .
//
size_t FindParameterEnd(std::string rs, size_t p1 )
{
size_t pos = rs.find_first_of(",(",p1);
if(pos==string::npos){ return pos; }
while(rs[pos]=='('){
   pos = FindClosingBrace( rs.substr(pos+1,string::npos) );
   pos = rs.find_first_of(",(",pos);
   if( pos==string::npos){ break; }
   }
return pos;
}
//--------------------------------------------

// This structutre is used to support functions.  The byte code of a function
// block is held seperatly and is accessed by associating this structure with a function name.
// The string literals are literals at the function level.  They are local variables.
struct rlFunction
{
list<string> ParamList;
RLBYTES ByteCode;
RLLITERALS Literals;
};

typedef std::map<string,long> RLFUNCTIONMAP; // Maps a function name to an index.
typedef std::vector<rlFunction> RLFUNCTIONS; // A vector to hold Function structures.

RLFUNCTIONMAP FunctionMap;
RLFUNCTIONS Functions;

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
      nv.size = v.size;
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
   static bool ShowPrompt;
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
         if( ShowPrompt ){
            cout << prompt;
            }
         getline(*pis,cmd);
         }
      }
   stBlock(std::istream* pis,string prompt,std::list<string>& lcomp, int* pret, string& rcmd ){
      string cmd;
      goto START;
      while( !FindString(cmd, lcomp, pret) ){
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
         if( ShowPrompt ){
            cout << prompt;
            }
         getline(*pis,cmd);
		 if (pis->flags() & ios::eofbit) {
			break;
			}
         }
      rcmd = cmd;
      }
   void Produce(rlContext* context){
      list<SyntaxTree*>::iterator ist;
      for( ist = st_list.begin(); ist != st_list.end(); ist++ ){
         (*ist)->Produce(context);
         }
      }
};

// Using a sleezy side affect to control when loop and function prompt is shown.
// When we load script we don't want to see this output.
bool stBlock::ShowPrompt = true;

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

      st_block = new stBlock(pis,"Function:>>","end"); // Doesn't exit until user enters "end"

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

// Supports if / elseif / else.  Elseif is supported by a recursive call to stIf(..)
class stIf : public SyntaxTree
{
public:
   SyntaxTree* st_conditional;
   SyntaxTree* st_if_block;
   SyntaxTree* st_else_block;

   stIf(std::istream* pis, string s){
      int ret;
      string rcmd;
      st_else_block = 0;
      std::list<std::string> elist;
      elist.push_back( "endif" );     // 1
      elist.push_back( "elseif " );  // 2
      elist.push_back( "else" );   // 3
      Parse( &st_conditional, s);
      st_if_block = new stBlock(pis,"if>>",elist, &ret, rcmd);
      if( ret!=1 ){ // Something other than end was used to break out of the loop.
         if( ret==3 ){ 
            elist.pop_back(); elist.pop_back(); // else has been used.  Once it is used else and elseif are no longer valid.
            st_else_block = new stBlock(pis,"else>>",elist, &ret, rcmd);
            }
         else if( ret==2 ){
            st_else_block = new stIf(pis, rcmd.substr(7,string::npos) );
            }
         }
      }
   void Produce(rlContext* context){
      //ex: if i<10
      st_conditional->Produce(context); 
      
      rlTupple tup; 
      tup.p1=0; 
      tup.p2=0;

      tup.cmd = 0x39; // ! - flip the result of the conditional above.
      context->ByteCode.push_back(tup); 

      // We flip the sign of the conditional because we only have itj (if true jump),
      // in this case we want to jump if flase and execute the block if true.
      // We could add an ifj (if false jump byte code) but why not try to make due for now.
      tup.cmd = 0x32;      // itj - If true jump
      //tup.p1 = 0;        // We'll figure out the jump later
      context->ByteCode.push_back( tup );

      int size_before_block = context->ByteCode.size();

      st_if_block->Produce(context);           // Write block commands
      if( st_else_block ){
         rlTupple tup1; 
         tup1.p1=0; 
         tup1.p2=0;
         tup1.cmd = 0x31; // jump
         context->ByteCode.push_back(tup1); 
         }

      context->ByteCode[size_before_block-1].p1 = context->ByteCode.size() - size_before_block; // Jump over the byte code

      if( st_else_block ){
         size_before_block = context->ByteCode.size();  
         st_else_block->Produce(context);           // Write block commands
         context->ByteCode[size_before_block-1].p1 = context->ByteCode.size() - size_before_block; // Jump over the byte code
         }
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
      SyntaxAssert( pos_eq!=string::npos, "No equals sign in loop command" )
      pos_to=s.find(" to ");
      SyntaxAssert( pos_to!=string::npos, "No \"to\" key word in loop command" )
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
      st_block = new stBlock(pis,"Loop>>","next"); // Doesn't exit until user enters "next"
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

     
      tup.cmd = 0x35; // >=
      context->ByteCode.push_back(tup); 

      tup.cmd = 0x32;               // itf - If true jump
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


template< const long cmd, const long p1=0, const long p2=0 > 
class stThreeParmWithOptions : public SyntaxTree
{
public:
   SyntaxTree* st1;
   SyntaxTree* st2;
   SyntaxTree* st3;
   stThreeParmWithOptions(string rs){
      size_t pos1, pos2;

      pos1 = FindParameterEnd(rs);
      SyntaxAssertEx( pos1!=string::npos, "Not enough parameters for byte code: " << cmd )
      Parse( &st1, rs.substr(0,pos1) );
      pos1++; // step past the comma
      pos2 = FindParameterEnd(rs, pos1 );
      if( pos2==string::npos ){
         SyntaxAssertEx( pos1<=rs.size(), "Not enough parameters for byte code: " << cmd )
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

class stIndex : public SyntaxTree
{
public:
   SyntaxTree* st1;
   SyntaxTree* st2;
   stIndex(string strlit, string bracket){
      Parse( &st1, strlit );
      Parse( &st2, bracket );
      }
   void Produce(rlContext* context){
      st2->Produce(context);
      st1->Produce(context);
      rlTupple tup;
      tup.cmd = 0x0C; //SEG code
      tup.p1 = 1;     //Cause SEG to use two parameter method
      tup.p2 = 0;
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
      pos2 = FindParameterEnd(rs);
      SyntaxAssert( pos2!=string::npos, "not enough parameters for crop command. Syntax: crop [array], [start], [length]" )
      Parse( &st1, rs.substr(pos1,pos2-pos1) );
      pos1=pos2+1;

      pos2 = FindParameterEnd(rs, pos1);
      SyntaxAssert( pos2!=string::npos ,"not enough parameters for crop command. Syntax: crop [array], [start], [length]" )
      Parse( &st2, rs.substr(pos1,pos2-pos1) );
      pos1=pos2+1;
     
 
      SyntaxAssert( pos1<rs.size(), "not enough parameters for crop command. Syntax: crop [array], [start], [length]" )
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

class stRemove : public SyntaxTree
{
public:
   string value;
   stRemove(string rs){
      value = trim(rs);
      }
   void Produce(rlContext* context){
      rlTupple tup;
      RLLITERALS::iterator lit = std::find(context->Literals.begin(),context->Literals.end(),value);
      if( lit==context->Literals.end() ){
         context->Literals.push_back(value);
         tup.p1 = context->Literals.size() - 1;
         }
      else{
         tup.p1 = lit - context->Literals.begin();
         }
      tup.cmd = 0x1E; // remove
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
      tup.cmd = 0x30;
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

   pos=s1.find("cd ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x0D>(s.substr(pos+3));
      return;
      }
   pos=s1.find("ls");
                                 // Making sure the fill string length is only 2
   if( pos==0 && s.size()==2 ){  // allows other things such as variable names to contain
                                 // the characters ls.
      *ppST = new stNullaryOperator<0x0E>(s.substr(pos+2));
      return;
      }
   pos=s1.find("sum ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x1B>(s.substr(pos+3));
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
   pos = s1.find("abs ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x40>(s.substr(pos + 4));
	   return;
   }
   pos=s1.find("cos ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x20>(s.substr(pos+4));
      return;
      }
   pos=s1.find("sin ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x21>(s.substr(pos+4));
      return;
      }
   pos=s1.find("tan ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x22>(s.substr(pos+4));
      return;
      }

   pos = s1.find("acos ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x41>(s.substr(pos + 5));
	   return;
   }
   pos = s1.find("asin ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x43>(s.substr(pos + 5));
	   return;
   }
   pos = s1.find("atan ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x45>(s.substr(pos + 5));
	   return;
   }
   pos = s1.find("atan2 ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x47>(s.substr(pos + 6));
	   return;
   }

   pos = s1.find("acosh ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x42>(s.substr(pos + 6));
	   return;
   }
   pos = s1.find("asinh ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x44>(s.substr(pos + 6));
	   return;
   }
   pos = s1.find("atanh ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x46>(s.substr(pos + 6));
	   return;
   }
   pos = s1.find("cosh ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x49>(s.substr(pos + 5));
	   return;
   }
   pos = s1.find("sinh ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x4D>(s.substr(pos + 5));
	   return;
   }
   pos = s1.find("tanh ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x4E>(s.substr(pos + 5));
	   return;
   }
   pos = s1.find("ceil ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x48>(s.substr(pos + 5));
	   return;
   }
   pos = s1.find("floor ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x4B>(s.substr(pos + 6));
	   return;
   }
   pos = s1.find("log2 ");
   if (pos == 0) {
	   *ppST = new stUnaryOperator<0x4C>(s.substr(pos + 5));
	   return;
   }
   pos = s1.find("pow ");
   if (pos == 0) {
	   *ppST = new stTwoParm<0x53>(s.substr(pos + 4));
	   return;
   }
   pos=s1.find("seg ");
   if( pos==0 ){
      *ppST = new stThreeParmWithOptions<0x0C>(s.substr(pos+4));
      return;
      }

   pos = s1.find("max ");
   if (pos == 0) {
	   *ppST = new stTwoParm<0x50>(s.substr(pos + 4));
	   return;
   }
   pos = s1.find("min ");
   if (pos == 0) {
	   *ppST = new stTwoParm<0x51>(s.substr(pos + 4));
	   return;
   }
   pos = s1.find("mod ");
   if (pos == 0) {
	   *ppST = new stTwoParm<0x52>(s.substr(pos + 4));
	   return;
   }

   pos=s1.find("init ");
   if( pos==0 ){
      *ppST = new stThreeParmWithOptions<0x23>(s.substr(pos+5));
      return;
      }
   pos=s1.find("help");
   if( pos==0 && s.size()==4 ){
      *ppST = new stNullaryOperator<0x19>(s.substr(pos+4));
      return;
      }
   pos=s1.find("help ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x19,1>(s.substr(pos+4));
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
   pos=s1.find("remove ");
   if( pos==0 ){
      *ppST = new stRemove(s.substr(pos+7));
      return;
      }
   pos=s1.find("string ");
   if( pos==0 ){
      *ppST = new stUnaryOperator<0x09>(s.substr(pos+7));
      return;
      }
   pos=s1.find("complex ");
   if( pos==0 ){
      *ppST = new stTwoParm<0x04>(s.substr(pos+8));
      return;
      }
   pos=s1.find("values");
   if( pos==0 ){
      *ppST = new stNullaryOperator<0x1D>(s.substr(pos+6));
      return;
      }


   //---------------------------------------------
   // This is where the parser is specialized for
   // additional script elements.
   //
   if( ml_parse(ppST, s) ){
      return;
      }
   //
   //---------------------------------------------

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
   else if( s[n-1]=='/' ){
      *ppST = new stBinaryOperator<0x14>(s.substr(0,n-1), s.substr(n,string::npos) );
      return;
      }
   else if( s[n-1]=='>' ){
      if( n>1 && s[n]=='=' ){
         *ppST = new stBinaryOperator<0x35>(s.substr(0,n-1), s.substr(n+1,string::npos) );
         }
      else{
         *ppST = new stBinaryOperator<0x33>(s.substr(0,n-1), s.substr(n,string::npos) );
         }
      return;
      }
   else if( s[n-1]=='<' ){
      if( n>1 && s[n]=='=' ){
         *ppST = new stBinaryOperator<0x36>(s.substr(0,n-1), s.substr(n+1,string::npos) );
         }
      else{
         *ppST = new stBinaryOperator<0x34>(s.substr(0,n-1), s.substr(n,string::npos) );
         }
      return;
      }
   else if( s[n-1]=='=' && n>1 && s[n]=='=' ){
      *ppST = new stBinaryOperator<0x38>(s.substr(0,n-1), s.substr(n+1,string::npos) );
      return;
      }
   else if( s[n-1]=='!' ){
      if( n>1 && s[n]=='=' ){
         *ppST = new stBinaryOperator<0x37>(s.substr(0,n-1), s.substr(n+1,string::npos) );
         }
      else{
         *ppST = new stUnaryOperator<0x39>(s.substr(n,string::npos) );
         }
      return;
      }
   else if( s[n-1]=='(' ){
      int pos = FindClosingBrace( s.substr(n,string::npos) );
      SyntaxAssert( pos, "empty braces." )
      // Adding 2 accounts for the open and close brace.
      if( (pos+2)==s.length() ){
         Parse( ppST, s.substr(n,s.length()-2) );
         return;
         }
      else{
         n=pos+2;
         }
      }
   else if( s[n-1]=='[' ){
      int pos = FindClosingSquareBrace( s.substr(n,string::npos) );
      SyntaxAssert( pos, "empty braces." )
      // Adding 2 accounts for the open and close brace.
      //*ppST = new stIndex( s.substr(0,n-1),s.substr(n,pos) );
      //return;

      // Adding 2 accounts for the open and close brace.
      if( (n+pos+1)==s.length() ){
         *ppST = new stIndex( s.substr(0,n-1),s.substr(n,pos) );
         return;
         }
      else{
         n=pos+2;
         }

      }
   }

*ppST = new stLiteral(s);
}

void ml_start(char* your_language_name, char* your_prompt )
{
string cmd;
PopulateMLHelpMap();
HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE); 
if (hStdout != INVALID_HANDLE_VALUE) { SetConsoleTextAttribute(hStdout, FOREGROUND_RED | BACKGROUND_BLUE | FOREGROUND_INTENSITY); }
cout << "Welcome to " << your_language_name << "." << endl << "A scripting language based on MyLanguage." << endl << endl; 
if (hStdout != INVALID_HANDLE_VALUE) { SetConsoleTextAttribute(hStdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);}
cout << "Enter a command" << endl;
goto START;

while( cmd!="exit" ){
   if( cmd.find("run ") == 0 ){
      char buf[255];
      string str = trim( cmd.substr(4) );
      fstream f;
      f.open(str.c_str());
      if( f.is_open() ){
         string line;
         cout << "currenly running file: " << str << endl;
         stBlock::ShowPrompt = false;
         while( !f.eof() ){
            f.getline(buf,255);
            string line(buf);
            stRoot root(&f, line);
            root.Produce(&GlobalContext);
            Evaulate(&GlobalContext);
            }
         stBlock::ShowPrompt = true;
         }
      else{
         cout << "Well... I couldn't find that file.  Are you in the correct directory?" << endl;
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
         stBlock::ShowPrompt = true;
         stRoot root(&cin, cmd);
         root.Produce(&GlobalContext);
         Evaulate(&GlobalContext);
         }
      catch(SysError ser){
         cout << "Ummm.. that didn't work!" << endl
               << ser << endl;
         }
      catch(...){
         cout << "Ummm.. that didn't work!" << endl;
         }
      GlobalContext.Literals.clear();
      GlobalContext.Stack.clear();
      GlobalContext.ByteCode.clear();
      }
   START:
   cout << your_prompt;
   getline(cin,cmd);
   }
}

/////////////////////////////////////////////////////////////
//          Byte Code Command Implementation
/////////////////////////////////////////////////////////////

void strliteral(rlContext* context, long p1, long p2)
{
Value rlv;
rlv.stringValue = new string;
*rlv.stringValue = context->Literals[p1]; 
rlv.size = rlv.stringValue->size();
rlv.type = TYPE_STRING;
context->Stack.push_front(rlv);
}

void show(rlContext* context, long p1, long p2)
{
Value rlv;
RunTimeAssert( !context->Stack.empty(), "Nothing on the stack." )
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

void sv(rlContext* context, long p1, long p2)
{
RLVALUEMAP::iterator iter = context->Locals.begin();
for( ;iter!=context->Locals.end();iter++ ){
   cout << iter->first << endl;
   }
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

void remove(rlContext* context, long p1, long p2)
{
RunTimeAssert( context->Literals.size()>=p1, "Run time error. String literal does not exist." )
string ls = context->Literals[p1];
context->Locals.erase(ls);
}

void tocomplex(rlContext* context, long p1, long p2)
{
Value rv;
Value iv;
RunTimeAssert( context->Stack.size()>=2, "Run time error. Not enough parameters on the stack for \"complex\" command." )

rv = context->Stack.front();
context->Stack.pop_front();
iv = context->Stack.front();
context->Stack.pop_front();

RunTimeAssert( rv.type==TYPE_NUMBER, "Run time error. \"complex\" command parameters must be a real ." )
RunTimeAssert( iv.type==TYPE_NUMBER, "Run time error. \"complex\" command parameters must be a real ." )

Value nv;
int n = rv.size > iv.size ? iv.size : rv.size;
MakeComplex(nv,n);
for(int i=0;i<n;i++){
   nv.complexValue[0][i] = rv.numberValue[i];
   nv.complexValue[1][i] = iv.numberValue[i];
   }

context->Stack.push_front( nv );
}

void tostring(rlContext* context, long p1, long p2)
{
Value rv;
Value iv;
RunTimeAssert( context->Stack.size()>=1, "Run time error. Not enough parameters on the stack for \"string\" command." )

rv = context->Stack.front();
if( rv.type==TYPE_STRING ){
	return;
	}

context->Stack.pop_front();

RunTimeAssert( rv.type==TYPE_NUMBER, "Run time error. \"string\" command parameters must be a real ." )

Value nv;
nv.type = TYPE_STRING;
std::strstream ss;
ss << rv.numberValue[0] << std::ends;
nv.stringValue = new std::string(ss.str());
ss.freeze(false);
nv.size = nv.stringValue->size();

context->Stack.push_front( nv );
}

void real(rlContext* context, long p1, long p2)
{
Value rlv;
RunTimeAssert( context->Stack.size()>=1, "Run time error. Not enough parameters on the stack for \"real\" command." )

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
RunTimeAssert( context->Stack.size()>=1, "Run time error. Not enough parameters on the stack for \"imag\" command." )

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
   ThrowRunTimeAssert( "Cannot take the imaginary part of a real." )
   }
}

void seg(rlContext* context, long p1, long p2)
{
Value rlv;
Value sv;
Value ev;
Value nv;
int s,e,n;

if( p1 ){
   RunTimeAssert( context->Stack.size()>=2, "Run time error. Not enough parameters on the stack for \"seg\" command." )

   rlv = context->Stack.front();
   context->Stack.pop_front();

   sv = context->Stack.front();
   context->Stack.pop_front();

   n = rlv.size;
   s = sv.numberValue[0];
   RunTimeAssert( s < n, "Run time error. Start value greater than array size." )
   e = s+1;
   }
else{
   RunTimeAssert( context->Stack.size()>=3, "Run time error. Not enough parameters on the stack for \"seg\" command." )

   rlv = context->Stack.front();
   context->Stack.pop_front();

   sv = context->Stack.front();
   context->Stack.pop_front();

   ev = context->Stack.front();
   context->Stack.pop_front();

   n = rlv.size;
   s = sv.numberValue[0];
   RunTimeAssert( s < n, "Run time error. Start value greater than array size." )
   e = ev.numberValue[0];
   if( !e ){ e = n; }
   if( e > n ){ e = n; }
   RunTimeAssert( s <= e, "Run time error. Start value greater than end value." )
   }

if( rlv.type==TYPE_STRING ){
   nv.type=TYPE_STRING;
   nv.stringValue = new string;
   (*nv.stringValue) = rlv.stringValue->substr(s,e-s);
   nv.size = nv.stringValue->size();
   }
else if( rlv.type==TYPE_NUMBER ){
   nv.CleanUp();
   nv.pvr = rlv.pvr;
   // NOTE: If the number Value is released first then this temp
   //       variable will be deleted and will probably not end well.
   nv.pvr->Increment();
   nv.size = (int)(e-s);
   // The type must be set after Cleanup is called.
   nv.type = TYPE_NUMBER;
   nv.numberValue = rlv.numberValue + s;
   }
else if( rlv.type==TYPE_COMPLEX ){
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
   nv.complexValue[0] = rlv.complexValue[0] + s;
   nv.complexValue[1] = rlv.complexValue[1] + s;
   }
else{
   ThrowRunTimeAssert( "Run time error." )
   }

context->Stack.push_front( nv );
}

void conj(rlContext* context, long p1, long p2)
{
Value rlv;
RunTimeAssert( context->Stack.size()>=1 , "Run time error. Not enough parameters on the stack for \"conj\" command." )

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
RunTimeAssert( context->Stack.size()>=2, "Equals: Not enough parameters on the stack." )
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
            lv.size = rv.size;
            }
         else{
            ThrowRunTimeAssert( "Wrong type for lvalue." )
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
         RunTimeAssert( lv.type==TYPE_NUMBER , "Wrong type for lvalue." )
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
         RunTimeAssert( lv.type==TYPE_COMPLEX , "Wrong type for lvalue." )
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
RunTimeAssert( context->Stack.size()>=1 ,"Run time error. Not enough parameters on the stack for read command." )

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
RunTimeAssert( context->Stack.size()>=1 ,"Run time error. Not enough parameters on the stack for cd command." )

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

RunTimeAssert( context->Stack.size()>=3, "Run time error. Not enough parameters on the stack for crop command." )

av = context->Stack.front();
context->Stack.pop_front();

sv = context->Stack.front();
context->Stack.pop_front();

ev = context->Stack.front();
context->Stack.pop_front();

int n = (long)ev.numberValue[0];
int s = (long)sv.numberValue[0];
if( s+n > av.size ){ n = av.size - s; }
int m = s + n;
Value nv;

if( av.type==TYPE_STRING ){
   nv.type = TYPE_STRING;
   nv.size = n;
   nv.stringValue = new string();
   *nv.stringValue = av.stringValue->substr(s,n);
   context->Stack.push_front( nv );
   return;
   }
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

// Pull value off of stack.  (It should be an array.)
void size(rlContext* context, long p1, long p2)
{
Value lv;
RunTimeAssert( context->Stack.size() >= 1, "size cmd.  Not enough parameters on the stack." )
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
//                 MATH FUNCTORs
//
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
struct folog2 : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return log2(Left);
	}
};


template<class Ty>
struct foExp : public unary_function<Ty, Ty>{	// functor for unary operator-
	Ty operator()(const Ty& Left) const{	// apply operator- to operand
		return exp(Left);
		}
};

template<class Ty>
struct foexp2 : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return exp2(Left);
	}
};

template<class Ty>
struct foSqrt : public unary_function<Ty, Ty>{	// functor for unary operator-
	Ty operator()(const Ty& Left) const{	// apply operator- to operand
		return sqrt(Left);
		}
};

template<class Ty>
struct foTan : public unary_function<Ty, Ty>{	// functor for unary operator-
	Ty operator()(const Ty& Left) const{	// apply operator- to operand
		return tan(Left);
		}
};

template<class Ty>
struct foSin : public unary_function<Ty, Ty>{	// functor for unary operator-
	Ty operator()(const Ty& Left) const{	// apply operator- to operand
		return sin(Left);
		}
};

template<class Ty>
struct foCos : public unary_function<Ty, Ty>{	// functor for unary operator-
	Ty operator()(const Ty& Left) const{	// apply operator- to operand
		return cos(Left);
		}
};

// code / function
// 0x40 abs(_In_ float _Xx)
// 0x41 acos(_In_ float _Xx)
// 0x42 acosh(_In_ float _Xx)
// 0x43 asin(_In_ float _Xx)
// 0x44 asinh(_In_ float _Xx)
// 0x45 atan(_In_ float _Xx)
// 0x46 atanh(_In_ float _Xx)
// 0x47 atan2(_In_ float _Xx)
// 0x48 ceil(_In_ float _Xx)
// 0x49 cosh(_In_ float _Xx)
// 0x4A exp2(_In_ float _Xx)
// 0x4B floor(_In_ float _Xx)
// 0x4C log2(_In_ float _Xx)
// 0x4D sinh(_In_ float _Xx)
// 0x4E tanh(_In_ float _Xx)

// 0x50 fmax(_In_ float _Xx, _In_ float _Yx)
// 0x51 fmin(_In_ float _Xx, _In_ float _Yx)
// 0x52 fmod(_In_ float _Xx, _In_ float _Yx)

// 0x53 powf(_In_ float _Xx, _In_ float _Yx)


template<class Ty>
struct foabs : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return abs(Left);
	}
};

template<class Ty>
struct foacos : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return acos(Left);
	}
};

template<class Ty>
struct foacosh : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return acosh(Left);
	}
};

template<class Ty>
struct foasin : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return asin(Left);
	}
};

template<class Ty>
struct foasinh : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return asin(Left);
	}
};

template<class Ty>
struct foatan : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return atan(Left);
	}
};

template<class Ty>
struct foatanh : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return atanh(Left);
	}
};

template<class Ty>
struct foceil : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return ceil(Left);
	}
};

template<class Ty>
struct focosh : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return cosh(Left);
	}
};

template<class Ty>
struct fofloor : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return floor(Left);
	}
};

template<class Ty>
struct fosinh : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return sinh(Left);
	}
};

template<class Ty>
struct fotanh : public unary_function<Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& Left) const {	// apply operator to operand
		return tanh(Left);
	}
};

//-----------------------
//        Binary Functor
//
template<class Ty>
struct fofmax : public binary_function<Ty, Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& P1, const Ty& P2) const {	// apply operator to operand
		return fmax(P1,P2);
	}
};

template<class Ty>
struct fofmin : public binary_function<Ty, Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& P1, const Ty& P2) const {	// apply operator to operand
		return fmin(P1, P2);
	}
};

template<class Ty>
struct fofmod : public binary_function<Ty, Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& P1, const Ty& P2) const {	// apply operator to operand
		return fmod(P1, P2);
	}
};

template<class Ty>
struct foatan2 : public binary_function<Ty, Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& P1, const Ty& P2) const {	// apply operator to operand
		return atan2(P1, P2);
	}
};

template<class Ty>
struct fopowf : public binary_function<Ty, Ty, Ty> {	// functor for unary operator
	Ty operator()(const Ty& P1, const Ty& P2) const {	// apply operator to operand
		return powf(P1, P2);
	}
};

//
//----------------------------------------------------------------------------------------

template< class T >
void unary_real_op(rlContext* context, RLBYTES::iterator& iter)
{
	Value s1;
	T op;

	RunTimeAssert(context->Stack.size() >= 1, "Run time error. Not enough parameters on the stack for command.")
	s1 = context->Stack.front();
	context->Stack.pop_front();

	Value rslt;
	if (s1.type == TYPE_NUMBER) {
		rslt.type = TYPE_NUMBER;
		unsigned long n = s1.size;
		rslt.numberValue = new float[n];
		rslt.size = n;
		float* is1 = s1.numberValue;
		float* it = rslt.numberValue;
		float* iend = is1 + n;
		for (; is1<iend; is1++, it++) { *it = op(*is1); }
	}
	else if (s1.type == TYPE_COMPLEX) {
		ThrowRunTimeAssertEx("The operator " << typeid(T).name() << " doesn't work on complex numbers.")
	}
	else {
		ThrowRunTimeAssertEx("Yur asking me to " << typeid(T).name() << " things that can't be done!")
	}

	context->Stack.push_front(rslt);
}

template< class T, class U >
void unary_op(rlContext* context, RLBYTES::iterator& iter)
{
Value s1;
T op;
U cop;

RunTimeAssert( context->Stack.size()>=1, "Run time error. Not enough parameters on the stack for command." )
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
   ThrowRunTimeAssertEx( "Yur asking me to " << typeid(T).name() << " things that can't be done!" )
   }

context->Stack.push_front( rslt );
}

template< class T >
void binary_real_op(rlContext* context, RLBYTES::iterator& iter)
{
	Value s1;
	Value s2;
	T op;

	RunTimeAssert(context->Stack.size() >= 2, "Run time error. Not enough parameters on the stack for command.")
	s1 = context->Stack.front();
	context->Stack.pop_front();
	s2 = context->Stack.front();
	context->Stack.pop_front();

	Value rslt;
	if (s1.type == TYPE_NUMBER) {
		rslt.type = TYPE_NUMBER;
		unsigned long n = s1.size;
		rslt.numberValue = new float[n];
		rslt.size = n;
		float* is1 = s1.numberValue;
		float* it = rslt.numberValue;
		float* iend = is1 + n;
		float* is2 = s2.numberValue;
		for (; is1<iend; is1++, it++) { *it = op(*is1, *is2); }
	}
	else if (s1.type == TYPE_COMPLEX) {
		ThrowRunTimeAssertEx("Yur asking me to " << typeid(T).name() << " things that can't be done!")
	}
	else {
		ThrowRunTimeAssertEx("Yur asking me to " << typeid(T).name() << " things that can't be done!")
	}

	context->Stack.push_front(rslt);
}

template< class T, class U >
void binary_op(rlContext* context, RLBYTES::iterator& iter)
{
	Value s1;
	Value s2;
	T op;
	U cop;

	RunTimeAssert(context->Stack.size() >= 2, "Run time error. Not enough parameters on the stack for command.")
	s1 = context->Stack.front();
	context->Stack.pop_front();
	s2 = context->Stack.front();
	context->Stack.pop_front();

	Value rslt;
	if (s1.type == TYPE_NUMBER) {
		rslt.type = TYPE_NUMBER;
		unsigned long n = s1.size;
		rslt.numberValue = new float[n];
		rslt.size = n;
		float* is1 = s1.numberValue;
		float* it = rslt.numberValue;
		float* iend = is1 + n;
		float* is2 = s2.numberValue;
		for (; is1<iend; is1++, it++) { *it = op(*is1,*is2); }
	}
	else if (s1.type == TYPE_COMPLEX) {
		rslt.type = TYPE_COMPLEX;
		unsigned long n = s1.size;
		MakeComplex(rslt, n);
		for (int i = 0; i<n; i++) {
			complex<float> crslt = cop(	complex<float>(s1.complexValue[0][i], s1.complexValue[1][i]),
										complex<float>(s2.complexValue[0][i], s2.complexValue[1][i])
										);
			rslt.complexValue[0][i] = crslt.real();
			rslt.complexValue[1][i] = crslt.imag();
		}
	}
	else {
		ThrowRunTimeAssertEx("Yur asking me to " << typeid(T).name() << " things that can't be done!")
	}

	context->Stack.push_front(rslt);
}

void line(rlContext* context, long p1, long p2)
{
Value lv;
Value sv;
Value ov;

RunTimeAssert( context->Stack.size()>=3,"Run time error. Not enough parameters on the stack for line command." )

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

RunTimeAssert( context->Stack.size()>=2 ,"Run time error. Not enough parameters on the stack for noise command." )

lv = context->Stack.front();
context->Stack.pop_front();

av = context->Stack.front();
context->Stack.pop_front();

RunTimeAssert( lv.type==TYPE_NUMBER , "Run time error.  argument mismatch in call to noise." )
RunTimeAssert( av.type==TYPE_NUMBER , "Run time error.  argument mismatch in call to noise." )

Value nv;

int n = *lv.numberValue;
nv.type = TYPE_NUMBER;
nv.numberValue = new float[n];
nv.size = n;

std::tr1::ranlux64_base_01 eng;
std::tr1::normal_distribution<float> generator(*(av.numberValue));

for( int i=0;i<n;i++){ (nv.numberValue)[i] = generator(eng); }
context->Stack.push_front( nv );
}

// Pull value 2 off of stack.
// Pull value 1 off of stack.
template< class T, class U, class S >
void mathop_for_add(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;
Value s1;
Value s2;
T op;
U cop;
S sop;

RunTimeAssert( context->Stack.size()>=2 , "Run time error. Not enough parameters on the stack for rlplus command." )

s2 = context->Stack.front();

if( s2.type==TYPE_NUMBER ){
	mathop<T,U>(context, iter);
   }
else if( s2.type==TYPE_STRING ){
	Value rslt;

	context->Stack.pop_front();
	s1 = context->Stack.front();
	context->Stack.pop_front();
	RunTimeAssert( s1.type==s2.type,"Different types passed to binary math operator.  No can do!")

	rslt.type = TYPE_STRING;
	rslt.stringValue = new string;
	*rslt.stringValue = *s1.stringValue + *s2.stringValue;
	rslt.size = rslt.stringValue->size();
	context->Stack.push_front( rslt );
   }
else{
   ThrowRunTimeAssertEx( "Yur asking me to " << typeid(T).name() << " things that can't be added!")
   }
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

RunTimeAssert( context->Stack.size()>=2 , "Run time error. Not enough parameters on the stack for rlplus command." )

s2 = context->Stack.front();
context->Stack.pop_front();

s1 = context->Stack.front();
context->Stack.pop_front();

RunTimeAssert( s1.type==s2.type,"Different types passed to binary math operator.  No can do!")

Value rslt;
if( s1.type==TYPE_NUMBER ){
   rslt.type = TYPE_NUMBER;
   if( s1.size==1 ){
      unsigned long n = s2.size;
      (rslt.numberValue) = new float[n]; 
      rslt.size = n;
      float* is1 = s1.numberValue;
      float* is2 = s2.numberValue;
      float* it = rslt.numberValue;
      float* iend = is2 + n;
      for( ;is2<iend;is2++,it++ ){ *it = op(*is1, *is2); }
      }
   else if( s2.size==1 ){
      unsigned long n = s1.size;
      (rslt.numberValue) = new float[n]; 
      rslt.size = n;
      float* is1 = s1.numberValue;
      float* is2 = s2.numberValue;
      float* it = rslt.numberValue;
      float* iend = is1 + n;
      for( ;is1<iend;is1++,it++ ){ *it = op(*is1, *is2); }
      }
   else{
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
   }
else if( s1.type==TYPE_COMPLEX ){
   rslt.type = TYPE_COMPLEX;
   if( s1.size==1 ){
      unsigned long n = s2.size;
      rslt.complexValue = new float*[2];
      rslt.complexValue[0] = new float[n]; 
      rslt.complexValue[1] = new float[n]; 
      rslt.size = n;
      for( int i=0;i<n;i++ ){ 
         complex<float> crslt = cop( complex<float>( s1.complexValue[0][0],s1.complexValue[1][0] ) , complex<float>(s2.complexValue[0][i],s2.complexValue[1][i]) );
         rslt.complexValue[0][i] = crslt.real(); 
         rslt.complexValue[1][i] = crslt.imag(); 
         }
      }
   else if( s2.size==1 ){
      unsigned long n = s1.size;
      rslt.complexValue = new float*[2];
      rslt.complexValue[0] = new float[n]; 
      rslt.complexValue[1] = new float[n]; 
      rslt.size = n;
      for( int i=0;i<n;i++ ){ 
         complex<float> crslt = cop( complex<float>( s1.complexValue[0][i],s1.complexValue[1][i] ) , complex<float>(s2.complexValue[0][0],s2.complexValue[1][0]) );
         rslt.complexValue[0][i] = crslt.real(); 
         rslt.complexValue[1][i] = crslt.imag(); 
         }
      }
   else{
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
   }
else{
   ThrowRunTimeAssertEx( "Yur asking me to " << typeid(T).name() << " things that can't be added!")
   }

context->Stack.push_front( rslt );
}

void sum(rlContext* context, long p1, long p2)
{
Value s1;
RunTimeAssert( context->Stack.size()>=1 , "Run time error. Not enough parameters on the stack for \"conj\" command." )

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
RunTimeAssert( context->Stack.size()>=1 , "Run time error. Not enough parameters on the stack for \"conj\" command." )

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
RunTimeAssert( context->Stack.size()>=2 , "Run time error. Not enough parameters on the stack for inc command." )

Value stp = context->Stack.front();
context->Stack.pop_front();
Value s1 = context->Stack.front();
context->Stack.pop_front();
RunTimeAssert( stp.type==TYPE_NUMBER , "Run time error.  Loop step not a number." )
(*s1.numberValue) += (long)(*stp.numberValue);
}

// Pull value 1 off of stack.
// Pull value 2 off of stack.
void iltj(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;

RunTimeAssert( context->Stack.size()>=3 , "Run time error. Not enough parameters on the stack for ilt command." )

Value stp = context->Stack.front();
context->Stack.pop_front();

Value s2 = context->Stack.front();
context->Stack.pop_front();

Value s1 = context->Stack.front();
context->Stack.pop_front();

RunTimeAssert( s1.type==TYPE_NUMBER && s2.type==TYPE_NUMBER && stp.type==TYPE_NUMBER, "Run time error. Parameters incorrect for iltj command." )

(*s1.numberValue) += (long)(*stp.numberValue);
if( *s1.numberValue <= *s2.numberValue ){
   iter += p1;
   }
}

// Pull value 1 off of stack.
void itj(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;

RunTimeAssert( context->Stack.size()>=1 , "Run time error. Not enough parameters on the stack for ilt command." )

Value s1 = context->Stack.front();
context->Stack.pop_front();

RunTimeAssert( s1.type==TYPE_NUMBER,  "Run time error. Parameters incorrect for itj command." )

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

RunTimeAssert( fun.ParamList.size()<=context->Stack.size(), "Too few parameters in function call." )

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

RunTimeAssert( context->Stack.size()>=2 , "Run time error. Not enough parameters on the stack for rlplus command." )

s2 = context->Stack.front();
context->Stack.pop_front();

s1 = context->Stack.front();
context->Stack.pop_front();

RunTimeAssert( s1.type==s2.type,"Different types passed to binary math operator.  No can do!" )
Value rslt;
(rslt.numberValue) = new float; 
rslt.size = 1;
rslt.type = TYPE_NUMBER;

if( s1.type==TYPE_NUMBER ){
   rslt.numberValue[0] = (float)op(s1.numberValue[0], s2.numberValue[0]);
   }
else if( s1.type==TYPE_COMPLEX ){
   float v1 = sqrt( s1.complexValue[0][0]*s1.complexValue[0][0] + s1.complexValue[1][0] * s1.complexValue[1][0] );
   float v2 = sqrt( s2.complexValue[0][0]*s2.complexValue[0][0] + s2.complexValue[1][0] * s2.complexValue[1][0] );
   rslt.numberValue[0] = (float)op(v1, v2);
   }
else{
   ThrowRunTimeAssertEx( "Yur asking me to " << typeid(T).name() << " things that can't be compared!" )
   }

context->Stack.push_front( rslt );
}

template< class T >
void unary_logic_op(rlContext* context, RLBYTES::iterator& iter)
{
long p1 = iter->p1;
Value s1;
T op;

RunTimeAssert( context->Stack.size()>=1 , "Run time error. Not enough parameters on the stack for unary logic command." )

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
   ThrowRunTimeAssertEx( "Yur asking me to " << typeid(T).name() << " things that can't be compared!" )
   }

context->Stack.push_front( rslt );
}

//////////////////////////////////////////////////////////////
//     Help
//////////////////////////////////////////////////////////////

void help(rlContext* context, long p1, long p2)
{
if(p1){
   Value s1;
   RunTimeAssert( context->Stack.size()>=1 , "Run time error. Not enough parameters on the stack for \"conj\" command." )
   s1 = context->Stack.front();
   context->Stack.pop_front();
   RunTimeAssert( s1.type==TYPE_STRING, "Run time error. Help parameter must be a string." )
   HELP_MAP::iterator hit = HelpMap.find(*s1.stringValue);
   if( hit!=HelpMap.end() ){
      cout << hit->second << endl;
      }
   else{
      cout << "Could not find help for " << *s1.stringValue << "." << endl;
      }
   }
else{
   HELP_MAP::iterator hit;
   for( hit = HelpMap.begin();
        hit!= HelpMap.end();
        hit++ ){
      cout << hit->first << endl;
      }
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
      case 0x01: read(context,iter->p1,iter->p2); break;
      case 0x02: write(context,iter->p1,iter->p2); break;
      case 0x03: strliteral(context,iter->p1,iter->p2); break;
      case 0x04: tocomplex(context,iter->p1,iter->p2); break;
      case 0x05: real(context,iter->p1,iter->p2); break;
      case 0x06: imag(context,iter->p1,iter->p2); break;
      case 0x07: equals(context,iter->p1,iter->p2); break;
      case 0x08: discover(context,iter->p1,iter->p2); break;
      case 0x09: tostring(context,iter->p1,iter->p2); break;
      case 0x0A: break;//Reserved:Number cast
      case 0x0B: show(context,iter->p1,iter->p2); break;
      case 0x0C: seg(context,iter->p1,iter->p2);break;
      case 0x0D: chdir(context,iter->p1,iter->p2); break;
      case 0x0E: dir(context,iter->p1,iter->p2); break;
      case 0x0F: crop(context,iter->p1,iter->p2); break;
      case 0x11: mathop_for_add<plus<float>,plus<complex<float>>,plus<string>>(context,iter); break;
      case 0x12: mathop<minus<float>,minus<complex<float>>>(context,iter); break;
      case 0x13: mathop<multiplies<float>,multiplies<complex<float>>>(context,iter); break;
      case 0x14: mathop<divides<float>,divides<complex<float>>>(context,iter); break;
      case 0x15: inc(context,iter); break;
      case 0x18: conj(context,iter->p1,iter->p2); break;

      case 0x19: help(context,iter->p1,iter->p2); break;

      case 0x1A: size(context,iter->p1,iter->p2); break;
      case 0x1B: sum(context,iter->p1,iter->p2); break;
      case 0x1C: neg(context,iter->p1,iter->p2); break;
      case 0x1D: sv(context,iter->p1,iter->p2); break;
      case 0x1E: remove(context,iter->p1,iter->p2); break;

      //case 0x20: trig(context,iter->p1,iter->p2); break;
      case 0x20: unary_op< foCos<float>, foSin< complex<float>>>(context,iter); break;
      case 0x21: unary_op< foSin<float>, foSin< complex<float>>>(context,iter); break;
      case 0x22: unary_op< foTan<float>, foSin< complex<float>>>(context,iter); break;
      case 0x23: line(context,iter->p1,iter->p2); break;
      case 0x24: noise(context,iter->p1,iter->p2); break;

      case 0x26: unary_op< foSqrt<float>, foSqrt< complex<float>>>(context,iter); break;
      case 0x27: unary_op< foLn<float>, foLn< complex<float>>>(context,iter); break;
      case 0x28: unary_op< foLog<float>, foLog< complex<float>>>(context,iter); break;
      case 0x29: unary_op< foExp<float>, foExp< complex<float>>>(context,iter); break;

      case 0x30: call(context,iter); break;
      case 0x31: jump(context,iter); break;
      //case 0x32: iltj(context,iter); break;
      case 0x32: itj(context,iter); break;
      case 0x33: logic_op< greater<float> >(context,iter); break;
      case 0x34: logic_op< less<float> >(context,iter); break;
      case 0x35: logic_op< greater_equal<float> >(context,iter); break;
      case 0x36: logic_op< less_equal<float> >(context,iter); break;
      case 0x37: logic_op< not_equal_to<float> >(context,iter); break;
      case 0x38: logic_op< equal_to<float> >(context,iter); break;
      case 0x39: unary_logic_op< logical_not<float> >(context,iter); break;

	  case 0x40: unary_real_op< foabs<float>>(context, iter); break;
	  case 0x41: unary_op< foacos<float>, foacos< complex<float>>>(context, iter); break;
	  case 0x42: unary_op< foacosh<float>, foacosh< complex<float>>>(context, iter); break;
	  case 0x43: unary_real_op< foasin<float>>(context, iter); break;
	  case 0x44: unary_op< foasinh<float>, foasinh< complex<float>>>(context, iter); break;
	  case 0x45: unary_real_op< foatan<float>>(context, iter); break;
	  case 0x46: unary_op< foatanh<float>, foatanh< complex<float>>>(context, iter); break;
	  case 0x47: binary_real_op< foatan2<float>>(context, iter); break;
	  case 0x48: unary_real_op< foceil<float>>(context, iter); break;
	  case 0x49: unary_op< focosh<float>, focosh< complex<float>>>(context, iter); break;
	  case 0x4A: unary_real_op< foexp2<float>>(context, iter); break;
	  case 0x4B: unary_real_op< fofloor<float>>(context, iter); break;
	  case 0x4C: unary_real_op< folog2<float>>(context, iter); break;
	  case 0x4D: unary_op< fosinh<float>, fosinh< complex<float>>>(context, iter); break;
	  case 0x4E: unary_op< fotanh<float>, fotanh< complex<float>>>(context, iter); break;

	  case 0x50: binary_real_op< fofmax<float>>(context, iter); break;
	  case 0x51: binary_real_op< fofmin<float>>(context, iter); break;
	  case 0x52: binary_real_op< fofmod<float>>(context, iter); break;
	  case 0x53: binary_real_op< fopowf<float>>(context, iter); break;

	  default: ml_evaulate( context, *iter );
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

      case 0x30: *ois << "call " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x31: *ois << "jump " << iter->p1 <<" , " << iter->p2 << endl; break;
      case 0x32: *ois << "ilt " << iter->p1 <<" , " << iter->p2 << endl; break;
      };
   }
}