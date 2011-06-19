#include "standard_stuff.h"

const string itoa(int mit)
{
	char buffer[50];
	sprintf(buffer,"%d",mit);
	return buffer;
}