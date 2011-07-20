#ifndef CRYPT_STUFF_INCLUDED
#define CRYPT_STUFF_INCLUDED
#include <string>
void SHA1_Hash(const unsigned char* data, int len,unsigned char hash[20]);
void MD5_Hash(const unsigned char* data, int len,unsigned char hash[16]);
std::string Base64Encode(const unsigned char* bytes_to_encode, unsigned int in_len);
#endif