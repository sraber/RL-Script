#ifndef __Debug__h
#define __Debug__h

#include <windows.h>
#include <strstream>
using namespace std;
#define endl "\n"


#define CATCH_LINE "Catch in file " << __FILE__ << " at line " << __LINE__ <<"." << endl

//class OutputSync{
//   CRITICAL_SECTION csOutputSync;
//public:
//   OutputSync(){
//      InitializeCriticalSection( &csOutputSync );
//      }
//
//   void EnterSection(){ 
//      EnterCriticalSection(&csOutputSync);
//      }
//
//   void LeaveSection(){ 
//      LeaveCriticalSection(&csOutputSync);
//      }
//};

// NOTE: Removed 8/29/2002.  This method of syncronization is found to have a race condition.
//       When DebugOutput class is used very near process initialization the DebugOutput class
//       can be instanced befor the OutputSync class.  Making OutputSync statically inheriated
//       by the Debug out classes fixes this problem.
//extern OutputSync OS;

class DebugOutputEx{
   friend class DebugOutput;
protected:
   DebugOutputEx(strstream* ss, CRITICAL_SECTION *cs) : ssOut(ss), pcsOutputSync(cs){}
   strstream *ssOut;
   CRITICAL_SECTION *pcsOutputSync;
public:
   ~DebugOutputEx();

   template<class T>
   DebugOutputEx& operator<<(T entry){
      *ssOut << entry;
      return *this;
      }

   DebugOutputEx& operator<<(char* entry){
      if( entry ){
         *ssOut << entry;
         }
      return *this;
      }

   DebugOutputEx& operator<<(const char* entry){
      if( entry ){
         *ssOut << entry;
         }
      return *this;
      }
   };


class DebugOutput{
   DebugOutput(DebugOutput&){};
   void operator=(DebugOutput&){}
protected:
   strstream* ssOut;
   CRITICAL_SECTION csOutputSync;
public:
   DebugOutput() : ssOut(0){
      InitializeCriticalSection( &csOutputSync );
      }
   ~DebugOutput(){}

   template<class T>
   DebugOutputEx operator<<(T entry){

// Note that we enter a critical section in this method
// but don't leave it.  We want to guard aginst another
// entry into the 
// DebugOutput >> DebugOutputEx >> DebugOutputEx ... etc..
// cycle.  Once the DubugOutputEx destructor has been
// called the cycle is complete and the code can be entered
// by another thread.  Thus we leave the critical section
// in the last line of the DebugOutputEx destructor.
//
      EnterCriticalSection(&csOutputSync);
      ssOut = new strstream;
      *ssOut << entry; 

      return DebugOutputEx(ssOut,&csOutputSync);
      }

   // REVIEW: A seperate thread can call into this method while the class
   //         is still initializing in another thread. 
   DebugOutputEx operator<<(char* entry){
      EnterCriticalSection(&csOutputSync);
      ssOut = new strstream;
      if( entry ){
         *ssOut << entry; 
         }

      return DebugOutputEx(ssOut,&csOutputSync);
      }

   DebugOutputEx operator<<(const char* entry){
      EnterCriticalSection(&csOutputSync);
      ssOut = new strstream;
      if( entry ){
         *ssOut << entry; 
         }

      return DebugOutputEx(ssOut,&csOutputSync);
      }
   };



extern DebugOutput DebugOut;

#ifdef _L1DEBUG
   #ifndef _L2DEBUG
      #define _L2DEBUG
   #endif
// NOTE: This does work.  Might want to try this in the furture.
//   #define DebugLevel1( a ) { DebugOutput db; db << a; }
   #define DebugLevel1( a ) DebugOut << a;
#else
   #define DebugLevel1( a ) 
#endif

#ifdef _L2DEBUG
   #define DebugLevel2( a ) DebugOut << a;
#else
   #define DebugLevel2( a ) 
#endif

#endif
