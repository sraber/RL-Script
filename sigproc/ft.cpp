#include "fft1.h"

void rlFFT( float* data, int len, int isign )
{
//REVIEW: Put an assert here.
//l = NearestPow2(len);
//if( l > len ){ assert }

rfftsine( data, len, isign);

if( isign>0 ){
   float dn = 2.0f / (float)len;
   for( float* id = data; id<(data+len); id++ ){ *id *= dn; }
   }
}

void cpFFT( float* data, int len, int isign )
{
cfftsine( data, len, isign);

if( isign>0 ){
   // divied by 4 to account for complex len is 2*N (because complex pairs are packed into one array).
   float dn = 4.0f / (float)len;
   for( float* id = data; id<(data+len); id++ ){ *id *= dn; }
   }
}

