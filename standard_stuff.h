#ifndef T_AND_S_H_INCLUDED
#define T_AND_S_H_INCLUDED
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;

template <class Struktura>
class TSmartPointer
{
	Struktura* data;//read only
	int* refcount;
	void AddRef()
	{
		++*refcount;
	}

	void Release()
	{
		if (!--*refcount)
		{
			delete []data;
			delete refcount;
		}
	}

public:
	TSmartPointer(const Struktura* data):data(data)
	{
		refcount=new int;
		AddRef();
	}

	~TSmartPointer()
	{
		Release();
	}

	TSmartPointer(const TSmartPointer<Struktura>& mirol)
	{
		*this=mirol;
	}

	const TSmartPointer<Struktura>& operator =(const TSmartPointer<Struktura>& mirol)
	{
		Release();
		data=mirol.data;
		refcount=mirol.refcount;
		AddRef();
	}

	const Struktura& operator [](int index)const
	{
		return data[index];
	}

	const Struktura& operator ->()const
	{
		return *data;
	}

	const Struktura& operator *()const
	{
		return *data;
	}
};

inline const string itoa(int mit)
{
	char buffer[50];
	sprintf(buffer,"%d",mit);
	return buffer;
}

inline void toupperstr(string& mit)
{
	transform(mit.begin(), mit.end(), mit.begin(), toupper);
}

inline void tolowerstr(string& mit)
{
	transform(mit.begin(), mit.end(), mit.begin(), tolower);
}

vector<string> explode(const string& in,const string& delim);

inline void findandreplace(string& target,const string& mit,const string& mivel)
{
	int pos = 0;
    while ( (pos = target.find(mit, pos)) != string::npos ) {
        target.replace( pos, mit.size(), mivel );
        pos++;
    }
}
#endif