#include <ml.h>

void PopulateMLHelpMap(void)
{
HelpMap["sum"]="sum a - Where a is an array.";
HelpMap["sqrt"]="sqrt a - Where a is an array compute the square root of each element in the array.";
HelpMap["log"]="log a - Compute the log of each element in array a.";
HelpMap["ln"]="ln a - Compute the natural log of each element in array a.";
HelpMap["exp"]="exp a - Compute the exponent of each element in array a.";
HelpMap["cos"]="cos length, frequency, [phase] \n Create an array of length specified with specified frequency and optional initial phase.";
HelpMap["sin"]="sin length, frequency, [phase] \n Create an array of length specified with specified frequency and optional initial phase.";
HelpMap["tan"]="tan length, frequency, [phase] \n Create an array of length specified with specified frequency and optional initial phase.";
HelpMap["init"]="init length, constant \n Initialize an array of length and constant starting value.";
HelpMap["line"]="line length, constant, [slope]\n Initialize an array to size of length with constant starting value and optional value for step increase.";
HelpMap["noise"]="noise length, amplitude\n Initialize an array to size of length with and average amplitude.";
/*HelpMap[""]=
HelpMap[""]=
HelpMap[""]=
HelpMap[""]=
HelpMap[""]=
HelpMap[""]=*/
}