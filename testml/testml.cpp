// testml.cpp : Defines the entry point for the console application.
//

#pragma once

#include "targetver.h"
#include <stdio.h>
#include <tchar.h>
#include <ml.h>


int _tmain(int argc, _TCHAR* argv[])
{
ml_start("MyLanguage Test Script", "ML>>");
return 0;
}

//------ byte codes -------
//  0x00100     con ip, port       Open socket
//  0x00101     discon socket         Close socket
//  0x00102     sclear             clear socket

// The input string has already had white space trimmed from both sides.
bool ml_parse(SyntaxTree**ppST,std::string s)
{
return false; 
}

void ml_evaulate( rlContext* context, rlTupple& rlt )
{
switch(rlt.cmd){
   //case 0x0100: my_call(context,rlt.p1,rlt.p2); break;
   }
}