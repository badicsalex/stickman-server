#include "standard_stuff.h"
#include <fstream>

const string TSimpleLang::err="TSLERR";

TSimpleLang::TSimpleLang(const string& langfile)
{
	ifstream fil(langfile.c_str());
	int langmost=0;
	while(!fil.fail() && !fil.eof())
	{
		char buffer[2048];
		fil.getline(buffer,2048);

		if (!buffer[0] || !buffer[1]) //lezáró karakter az elsõ két karakterben: size<2
			continue;
		if (buffer[0]=='[') //lang kód
		{
			langmost=atoi(buffer+1);//csodahák. itt lesz a [default]-ból 0.
		}
		else
		{
			int pos=-1;
			for (int i=0;buffer[i];++i)
				if (buffer[i]=='=')
				{
					pos=i;
					break;
				}
			if (pos<0)
				continue;

			int strmost=atoi(buffer);
			if ((int)data.size()<=langmost)
				data.resize(langmost+1);
			if ((int)data[langmost].size()<=strmost)
				data[langmost].resize(strmost+1);
			data[langmost][strmost]=buffer+pos+1;
		}

	}
}

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