// sigproc.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <tchar.h>
#include <SysError.h>
#include <ml.h>
#include "fft1.h"

int _tmain(int argc, _TCHAR* argv[])
{
HelpMap["ft"]="ft v \nReturn the Fourier transform of the array v.";
HelpMap["ift"]="ft v \nReturn the inverse Fourier transform of the array v.";
ml_start("MyLanguage for DSP", "MLDSP>>");
return 0;
}

//------ byte codes -------
//  0x00100     ft array       fourier trasform
//  0x00101     
//  0x00102     

void ft(rlContext* context, long p1, long p2)
{
Value s1;
RunTimeAssert( context->Stack.size()>=1 , "Run time error. Not enough parameters on the stack for \"ft\" command." )
s1 = context->Stack.front();
context->Stack.pop_front();
RunTimeAssert( s1.type!=TYPE_STRING , "Ummm.. you passed a string to the \"ft\" command.  What did you think would happen?" )

float* d;
float* ir;
float* ic;
float* id;
float* end;
unsigned long n = s1.size;

Value rslt;
if( s1.type==TYPE_NUMBER ){
   MakeComplex(rslt,n);
   d = new float[2*n];
   ir = s1.numberValue;
   id = d;
   end = ir + n;
   for(;ir<end;ir++,id+=2){ *id = *ir; *(id+1) = 0.0f; }
   }
else if( s1.type==TYPE_COMPLEX ){
   MakeComplex(rslt,n);
   d = new float[2*n];
   ir = s1.complexValue[0];
   ic = s1.complexValue[1];
   id = d;
   end = ir + n;
   for(;ir<end;ir++,ic++,id+=2){ *id = *ir; *(id+1) = *ic; }
   }

RunTimeAssert( cfftsine( d, 2*n, p1 ) , "Input data not a power of 2. \"ft\" command needs power of 2 data." )

float div = 1.0f / sqrt((float)n);
id = d;
ir = rslt.complexValue[0];
ic = rslt.complexValue[1];
end = ir + n;
for(;ir<end;ir++,ic++,id+=2){ *ir = *id * div; *ic = *(id+1) * div; }

delete[] d;


context->Stack.push_front( rslt );
}

// The input string has already had white space trimmed from both sides.
bool ml_parse(SyntaxTree**ppST,std::string s)
{
size_t pos;
pos=s.find("ft ");
if( pos==0 ){
   *ppST = new stUnaryOperator<0x0100,1>(s.substr(pos+3));
   return true;
   }
pos=s.find("ift ");
if( pos==0 ){
   *ppST = new stUnaryOperator<0x0100,-1>(s.substr(pos+3));
   return true;
   }
return false; 
}

void ml_evaulate( rlContext* context, rlTupple& rlt )
{
switch(rlt.cmd){
   case 0x0100: ft(context,rlt.p1,rlt.p2); break;
   }
}