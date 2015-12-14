#include "SysError.h"

SysError::SysError( const char* msg,
                    long code,
                    char* file,
                    long line )
{
ErrorCode = code;
SysStrCpy(strMsg,msg,MAX_MSG);
SysStrCpy(strFile,file,MAX_FILE);
dwLine = line;
}

SysError::SysError( long code,
                    char* file,
                    long line )
{
ErrorCode = code;

FormatError( TRUE,
             code,
             file,
             line 
            );
}

void SysError::FormatError( bool fFormatCode,
                            long code,
                            char* file,
                            long line )
{
BOOL bRet = !fFormatCode;

if( !bRet ){
   bRet = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
            0, code,
            0,
            strMsg,
            sizeof(strMsg),
            0);
   } 

if (!bRet)
   SysStrCpy(strMsg,"Unknown Error",MAX_MSG);

SysStrCpy(strFile,file,MAX_FILE);
dwLine = line;
}


void SysError::SysStrCpy(char* dest, const char* src, int len)
{
if( !src ){
   *dest = '\0';
   return;
   }

len--;
do{
   *dest++ = *src++;
   }while( --len && *src );

*dest = '\0'; 

return;
}

ostream& operator<<(ostream& s, SysError& err)
{
s << endl 
  << "---- SysError ----" << endl
  << (err.ErrorCode==1 ? "Syntax Error" : "Run time error") << endl
  << "      Message: " << err.strMsg << endl
  << "         File: " << err.strFile << endl
  << "         Line: " << err.dwLine << endl
  << "------------------" << endl;

return s;
}

