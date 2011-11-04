#ifndef T_AND_S_H_INCLUDED
#define T_AND_S_H_INCLUDED
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <fstream>
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

class TSimpleLang {
	vector< vector< string > > data; //ne legyen túl magas ID-je a langnak :P
	TSimpleLang(TSimpleLang&);
	TSimpleLang& operator=(TSimpleLang&);
	static const string err;
public:
	TSimpleLang(const string& langfile);
	bool HasLang(int id) const
	{
		return id>=0 && (int)data.size()>id && data[id].size()>0;
	}
	const string& operator()(int langid,int stringid) const
	{
		if (langid<0 || (int)data.size()<=langid)
			return err;
		if (stringid<0 || (int)data[langid].size()<=stringid)
			return err;
		return data[langid][stringid];
	}
};

//TKey-nek kell < operátor, Save és Load fv.
//TValue-nak Save és Load fv.
// throwol runtime_error-t, ha gáz van
template <class TKey, class TValue>
class TAutoPersistentMap{
	map<TKey,TValue> data;
	const string filenev;
	bool dirty;
	TAutoPersistentMap(const TAutoPersistentMap&);
	TAutoPersistentMap& operator=(const TAutoPersistentMap&);

	void SaveToFile(const string& fnev)
	{
		ofstream fil(fnev.c_str());
		unsigned int tmp=0;
		fil.write((char*)&tmp,sizeof(tmp));
		tmp=data.size();
		fil.write((char*)&tmp,sizeof(tmp));
		for(map<TKey,TValue>::iterator i=data.begin();i!=data.end();++i)
		{
			(*i).first.Save(fil);
			(*i).second.Save(fil);
		}
		fil.flush();
		fil.seekp(0);
		tmp=0xD011FACE;
		fil.write((char*)&tmp,sizeof(tmp));
		fil.flush();
		fil.close();
	};
public:
	
	TAutoPersistentMap(const string& filenev):filenev(filenev),dirty(false)
	{
		ifstream fil((filenev+".pm1").c_str());
		unsigned int num=0;
		unsigned int validfile;
		fil.read((char*)&validfile,sizeof(validfile));

		if (fil.fail() || validfile!=0xD011FACE)
		{
			fil.close();
			fil.open((filenev+".pm2").c_str());
			fil.read((char*)&validfile,sizeof(validfile));
			if (fil.fail() || validfile!=0xD011FACE)
				num=0;
			else
				fil.read((char*)&num,sizeof(num));
		}
		else
			fil.read((char*)&num,sizeof(num));


		for(unsigned int i=0;i<num;++i)
		{
			TKey key;
			TValue val;
			key.Load(fil);
			val.Load(fil);
			data[key]=val;
		}

		fil.close();
	}

	void Flush()
	{
		SaveToFile(filenev+".pm1");
		SaveToFile(filenev+".pm2");
	}

	void Set(const TKey& k, const TValue& v)
	{
		dirty=true;
		data[k]=v;
	}

	const TValue& Get(const TKey& k)
	{
		return data[k];
	}

	~TAutoPersistentMap()
	{
		if (dirty)
			Flush();
	}
};

struct TPersistentInt{
	int value;
	operator int() { return value; }
	TPersistentInt(int a=0):value(a){}

	bool operator < (const TPersistentInt& a) const
	{
		return value<a.value;
	}

	bool operator < (const int a) const
	{
		return value<a;
	}

	void Save(ostream& strm) const
	{
		strm.write((const char*)&value,sizeof(value));
	}
	
	void Load(istream& strm)
	{
		strm.read((char*)&value,sizeof(value));
	}

};

inline const string itoa(int mit)
{
	char buffer[50];
	sprintf(buffer,"%d",mit);
	return buffer;
}


inline void toupper(string& mit)
{
	int (*fn)(int)=toupper;
	transform(mit.begin(), mit.end(), mit.begin(),fn);
}

inline void tolower(string& mit)
{
	int (*fn)(int)=tolower;
	transform(mit.begin(), mit.end(), mit.begin(), fn);
}

vector<string> explode(const string& in,const string& delim);

inline void findandreplace(string& target,const string& mit,const string& mivel)
{
	string::size_type pos = 0;
    while ( (pos = target.find(mit, pos)) != string::npos ) {
        target.replace( pos, mit.size(), mivel );
        pos++;
    }
}

inline bool startswith(const string& mi, const string& mivel)
{
	return mi.compare(0,mivel.size(),mivel)==0;
}
#endif
