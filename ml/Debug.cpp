#include "Debug.h"

DebugOutputEx::~DebugOutputEx()
{
*ssOut << std::ends;

// NOTE: Added 8/09 by SR.  Changed OutputDebugString to OutputDebugStringA.  This allows
//         the program to be compiled with Unicode strings turned on.
//         I think this will be compatible with previous code.
OutputDebugStringA(ssOut->str());
ssOut->rdbuf()->freeze(FALSE);
delete ssOut;
}
