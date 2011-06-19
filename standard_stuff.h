#ifndef T_AND_S_H_INCLUDED
#define T_AND_S_H_INCLUDED
#include <stdio.h>
#include <string>
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

const string itoa(int mit);

#endif