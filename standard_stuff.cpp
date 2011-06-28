#include "standard_stuff.h"

vector<string> explode(const string& in,const string& delim)
{
	const int delim_len = delim.length() ;
	vector<string> result ;
	string::size_type i = 0, j ;
	for (;;)
	{
		j = in.find(delim, i) ;
		result.push_back(in.substr(i, j-i)) ;
		if (j == std::string::npos)
			break ;
		i = j + delim_len ;
	}
	return result ;
}