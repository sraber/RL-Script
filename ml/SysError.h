#ifndef __SysError_h
#define __SysError_h

#include <windows.h>
#include <fstream>
#include <strstream>
                             
class SafeStrStream : public std::strstream
{
public:
   SafeStrStream(){}
   ~SafeStrStream(){
      freeze(false);
      }
};

using namespace std;

class SysError{
   enum{ MAX_MSG = 1024, MAX_FILE = 256 };
public:
   long ErrorCode;
   char strMsg[MAX_MSG];
   char strFile[MAX_FILE];
   long dwLine;

   void SysStrCpy(char* dest, const char* src, int len);

   void FormatError( bool fFormatCode,
                     long code,
                     char* file,
                     long line );

   SysError( const char* msg = 0,
             long code = -1,
             char* file = 0,
             long line = 0 );

   SysError( long code,
             char* file = 0,
             long line = 0 );
};

ostream& operator<<(ostream& s, SysError& err);

#ifndef SysErr1
   #define SysErr1(a,b,c,d) SysError(a,b,c,d)
#endif

#ifndef SysErr2
   #define SysErr2(a,b,c) SysError(a,b,c)
#endif

#define SysError1(a,b) SysErr1(a,b,__FILE__,__LINE__)
#define SysError2(a) SysErr2(a,__FILE__,__LINE__)

#define ThrowSysError1(msg) throw SysError1( msg, -1);
#define ThrowSysError2(msg, code) throw SysError1( msg, code);

#define SysAssert1(test) if(!(test)){ throw SysError1( "Assert Failed: " #test,-1); }
#define SysAssert2(test,msg) if(!(test)){ throw SysError1(msg,-1); }

#define SyntaxAssert(test,msg) if(!(test)){ throw SysError1(msg,1); }
#define ThrowSyntaxAssert( msg ) throw SysError1(msg,1);
#define SyntaxAssertEx(test,msg) if(!(test)){ \
   SafeStrStream ss;\
   ss << msg;\
   throw SysError1(ss.str(),1); \
   }

#define RunTimeAssert(test,msg) if(!(test)){ throw SysError1(msg,2); }
#define ThrowRunTimeAssert( msg ) throw SysError1(msg,2);
#define ThrowRunTimeAssertEx(msg) {\
   SafeStrStream ss;\
   ss << msg << ends;\
   throw SysError1(ss.str(),2); \
   }
#ifdef _L1ASSERT

   #ifndef _L2ASSERT
      #define _L2ASSERT
   #endif

   #define Level1Assert1(test)          SysAssert1(test)
   #define Level1Assert2(test,msg)      SysAssert2(test,msg)
   #define Level1Assert3(test,code,msg) SysAssert3(test,code,msg)
   #define Level1ComAssert(hr)          SysComAssert(hr)
   #define Level1WinAssert(b)           SysWinAssert(b)
#else
   #define Level1Assert1(test) 
   #define Level1Assert2(test,msg) 
   #define Level1Assert3(test,code,msg) 
   #define Level1ComAssert(hr) 
   #define Level1WinAssert(b) 
#endif

#ifdef _L2ASSERT
   #define Level2Assert1(test)          SysAssert1(test)
   #define Level2Assert2(test,msg)      SysAssert2(test,msg)
   #define Level2Assert3(test,code,msg) SysAssert3(test,code,msg)
   #define Level2ComAssert(hr)          SysComAssert(hr)
   #define Level2WinAssert(b)           SysWinAssert(b)
#else
   #define Level2Assert1(test) 
   #define Level2Assert2(test,msg) 
   #define Level2Assert3(test,code,msg) 
   #define Level2ComAssert(hr) 
   #define Level2WinAssert(b) 
#endif

#endif



