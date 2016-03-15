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
HelpMap["crop"]="crop v, s, n \n Return an array that is a portion of the origional array v starting from s with n elements. Works on strings as well.";
HelpMap["read"]="read [path and file name] \nRead a file with line feed seperating array values.";
HelpMap["write"]="write v, [path and file name] \nWrite array v to file.";
HelpMap["conj"]="conj v \nReturn the complex conjugate of the array v";
HelpMap["real"]="real v \nReturn the real part of the array v.";
HelpMap["imag"]="imag v \nReturn the imaginary part of the array v.";
HelpMap["size"]="size v \nReturn the length of array v or string v.";
HelpMap["remove"]="remove var \nRemove a variable from the values list.";
HelpMap["values"]="Show all the dynamically established program variables.";
HelpMap["size"]="size v \n";
HelpMap["loop"]="loop syntax\nloop i=[s] to [e] step [s]\ncode...\nnext\nwhere s=start value\ne=end value (inclusive)\nstep (optional) is step size";
HelpMap["function"]="Define a function with the keyword function.\nfunction MyFunction [a],[b],[...]\ncode\nend\nUse function to define a function and add as many comma delimited parameters as you like.\nThose variables will be accessible by the function code.\nJust enter a variable name on it's own line to return it.";
HelpMap["if"]="Normal if elseif else supported.";
HelpMap["complex"]="complex r, i\nReturn the complex value r + ji or r,i in this code.";
}