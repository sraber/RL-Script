#include <windows.h>
#include <math.h>
#define SWAP(a,b) tempr=(a);(a)=(b);(b)=tempr

#define PI 3.141592653589793

void four1(float * pdata, const int n, const int isign)
{
	int mmax,m,j,i;
	float wr,wi;
   float tempr, tempi;
   double theta;
   float wpr,wpi,wtemp;
   
	j=1;
	for (i=1;i<n;i+=2) {
	   if (j > i) {   // bit reversal. 
		   SWAP(pdata[j-1],pdata[i-1]);
		   SWAP(pdata[j],pdata[i]);
	      }
	   m=n >> 1;
	   while (m >= 2 && j > m){
		   j -= m;
		   m >>= 1;
	      }
	   j += m;
	   }

	for (mmax = 2; mmax<n; mmax <<= 1){
      theta = (double)isign * (6.28318530717959/(double)mmax);
      wtemp=(float)sin(0.5*theta);
      wpr = -2.0f*wtemp*wtemp;
      wpi=(float)sin(theta);
      wr=1.0f;
      wi=0.0f;
	   for (m=0;m<mmax;m+=2) {
	      for (i=m;i<n; i += 2*mmax) {
		      j=i+mmax;   
		      tempr = wr*pdata[j]-wi*pdata[j+1];
		      tempi = wr*pdata[j+1]+wi*pdata[j];

		      pdata[j] = pdata[i]-tempr; 
		      pdata[j+1] = pdata[i+1]-tempi;
		      pdata[i]  += tempr; 
		      pdata[i+1] += tempi; 
	         }
         wtemp=wr;
         wr=wr*wpr-wi*wpi+wr;
         wi=wi*wpr+wtemp*wpi+wi;
	      }
	   }
}
void real_four1(float * pdata, const int n, const int isign)
{
	int i,i1,i3;
	float h1r,h1i,h2r,h2i,g1,g2;
	double wr,wi;
   double theta;
   double wpr,wpi,wtemp;

   theta = PI / (double)(n>>1);
   if( isign == (-1) ){ theta = -theta; }

   // Initialize the trig recursion relation.
   wtemp = sin(0.5*theta);
   wpr = -2.0 * wtemp*wtemp;
   wpi = sin(theta);
   wr = 1.0 + wpr;
   wi = wpi;

	i1 = 0;
	i3 = n;
	for (i=1; i<n/4; i++ ) { 
		i1 += 2 ;      // i1 walks through from 2 ... 2*(n/4-1)
		i3 -= 2;       // i3 walks through from n-2 ... n-2*(n/4-1)

		h2r = isign *(pdata[i3+1] + pdata[i1+1])/2.0f;
		h2i = isign *(pdata[i3] - pdata[i1])/2.0f;
		g1 = (float)(wr*h2r-wi*h2i);
		g2 = (float)(wr*h2i+wi*h2r);

		h1r = ( pdata[i1] + pdata[i3]) * 0.5f;
		h1i = ( pdata[i1+1] - pdata[i3+1]) * 0.5f;
		pdata[i1] = h1r + g1; 
		pdata[i3] = h1r - g1;
		pdata[i1+1] = g2 + h1i;
		pdata[i3+1] = g2 - h1i ;

      // trig recursion
      wtemp=wr;
      wr=wr*wpr-wi*wpi+wr;
      wi=wi*wpr+wtemp*wpi+wi;
	   }

	h1r = pdata[0]; 
	pdata[0] = h1r + pdata[1];
	pdata[1] = h1r - pdata[1];

	if (isign == -1){
		pdata[0] /= 2.0f;
		pdata[1] /= 2.0f;
	   }
}

BOOL IsPower2(int n)
{
if (n<=0) return FALSE;

if( (n==0x00000001) ||
    (n==0x00000002) ||
    (n==0x00000004) ||
    (n==0x00000008) ||
    (n==0x00000010) ||
    (n==0x00000020) ||
    (n==0x00000040) ||
    (n==0x00000080) ||
    (n==0x00000100) ||
    (n==0x00000200) ||
    (n==0x00000400) ||
    (n==0x00000800) ||
    (n==0x00001000) ||
    (n==0x00002000) ||
    (n==0x00004000) ||
    (n==0x00008000) ||
    (n==0x00010000) ||
    (n==0x00020000) ||
    (n==0x00040000) ||
    (n==0x00080000) ||
    (n==0x00100000) ||
    (n==0x00200000) ||
    (n==0x00400000) ||
    (n==0x00800000) ||
    (n==0x01000000) ||
    (n==0x02000000) ||
    (n==0x04000000) ||
    (n==0x08000000) ||
    (n==0x10000000) ||
    (n==0x20000000) ||
    (n==0x40000000) ||
    (n==0x80000000) ){ return TRUE; }

return FALSE;
}

int rfftsine(float * pdata, const int n, const int isign)
{
if( !IsPower2(n) ){
   return 0; 
   }          
	
 if (isign == 1){
	four1(pdata,n,isign);
	real_four1(pdata,n,isign);
   }
 else{
	real_four1(pdata,n,isign);
	four1(pdata,n,isign);
   }
 return 1;
}

int cfftsine( float * pdata, const int n, const int isign)
{
	if( !IsPower2(n) )  {
    	return 0; 
	}          
		
	four1(pdata,n,isign);
    return 1;
}
