#ifndef T_AND_S_H_INCLUDED
#define T_AND_S_H_INCLUDED

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
		ref_count=new int;
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

class TThread{ 
	void* threadhandle;
	virtual int VirtThreadFunc()=0;//absztrakt, figyeljen oda a "running"-ra.
	static unsigned long __stdcall ThreadFunc(void* pointer);
	TThread(TThread&);
	TThread& operator=(TThread&);// ne másolgass köcsög, mert szétbasz a destruktorod.
protected:
	bool running;
public:
	bool IsRunning();
	TThread();
	~TThread();
};

class TTimer: public TThread{
	
	int interval;
	virtual void OnTimer()=0;//absztrakt!
	virtual int VirtThreadFunc();
	TTimer(TTimer&);
	TTimer& operator = (TTimer&);//ne másolgass.
public:
	TTimer(int interval):interval(interval){}
};


#endif