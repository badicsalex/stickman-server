#include "key.h"
#include <string>
#include "sha1.h"

void setSharedKey(unsigned char * sharedkey)
{
		sharedkey[0]=0x00;
		sharedkey[1]=0x00;
		sharedkey[2]=0x00;
		sharedkey[3]=0x00;
		sharedkey[4]=0x00;
		sharedkey[5]=0x00;
		sharedkey[6]=0x00;
		sharedkey[7]=0x00;
		sharedkey[8]=0x00;
		sharedkey[9]=0x00;
		sharedkey[10]=0x00;
		sharedkey[11]=0x00;
		sharedkey[12]=0x00;
		sharedkey[13]=0x00;
		sharedkey[14]=0x00;
		sharedkey[15]=0x00;
		sharedkey[16]=0x00;
		sharedkey[17]=0x00;
		sharedkey[18]=0x00;
		sharedkey[19]=0x00;

}

std::string encodePassword(std::string pass)
{

		//sha1
	unsigned char hash[20];
	char hexstring[41];

	std::string longpass = std::string("luke, en vagyok az apad"+pass+"mert ez egy jo hosszu salt");


	sha1::calc(longpass.c_str(),longpass.size(),hash); // 10 is the length of the string
	sha1::toHexString(hash, hexstring);
	return hexstring;
}