#ifndef SOCKET_STUFF_INCLUDED
#define SOCKET_STUFF_INCLUDED
#ifdef _WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <unistd.h>
typedef unsigned int ULONG;
typedef int SOCKET;
typedef sockaddr_in SOCKADDR_IN;
typedef sockaddr SOCKADDR;
typedef unsigned int DWORD;
typedef short unsigned int WORD;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include <string>
#include <vector>
#include <iostream>
using namespace std;

class TSocketFrame
{
	TSocketFrame(TSocketFrame&);// ha ezt tolod, gáz van
	TSocketFrame& operator=(TSocketFrame&);//pointerekkel ilyent nem játszunk.
	void EnlargeBuffer(int mekkorara);
public:
	unsigned char* data; // csínján.
	int cursor;
	int datalen;

	// átveszi a char* kezelését, felszabadítja destruáláskor.
	TSocketFrame(unsigned char* data,int datalen):data(data),cursor(0),datalen(datalen){}
	TSocketFrame():data(0),cursor(0),datalen(0){}
	~TSocketFrame(){if (data) delete[] data;}

	void Reset(){cursor=0;}
	unsigned char ReadChar();
	unsigned int ReadWord();
	int ReadInt();
	string ReadString();
	void WriteChar(unsigned char mit);
	void WriteBytes(unsigned char* mit, int n);
	void WriteWord(unsigned int mit);
	void WriteInt(int mit);
	void WriteString(const string& mit);
};


class TBufferedSocket
{
protected:
	SOCKET sock;
	vector<char> recvbuffer;
	vector<char> sendbuffer;
	TBufferedSocket(const TBufferedSocket& mirol);
	TBufferedSocket& operator= (const TBufferedSocket& mirol);
	bool closeaftersend;
	int error;// ha nem 0, gáz volt.
public:
	
	// A függvény meghívásával elfogadja a szerzõdési feltételeket
	// miszerint teljesen lemond a socket használatáról.
	TBufferedSocket(SOCKET sock):sock(sock), closeaftersend(false),error(0){};
	TBufferedSocket(const string& hostname,int port);
	~TBufferedSocket(){ closesocket(sock); }

	int GetError(){return error;} //read only a változó.
	void Update(); //Hívjad sokat. Mert alattam az a kettõ csak a buffereket nézi
	bool RecvFrame(TSocketFrame& hova);// Légyszi újonnan generáltat, mert úgyis felülírja.
	void SendFrame(const TSocketFrame& mit,bool final=false);// Légyszi ne legyen üres.

	bool RecvBytes(vector<unsigned char>& hova,int bytes);
	void SendBytes(const unsigned char* mit,int bytes,bool final=false);// Légyszi ne legyen üres.

	bool RecvLine(string& hova); // \r\n nélkül
	const string RecvLine2(); //ha nem érdekel hogy sikrült-e, hanem mindenképp kell egy string
	void SendLine(const string& mit, bool final=false);// \r\n nélkül

	
};

// abstract class virtual callback-ekkel
template <class TContext, class TSocket=TBufferedSocket>
class TBufferedServer{
protected:
	class TMySocket: public TSocket{
	public:
		TContext context; //Default konstruktor!!!
		ULONG address; 
		TMySocket(SOCKET sock, ULONG address): 
			TSocket(sock),address(address){}
	};

	SOCKET listenersock;
	vector<TMySocket*> socketek;
	void DeleteSocket(int index);//és close
	virtual void OnConnect(TMySocket& sock)=0;
	virtual void OnUpdate(TMySocket& sock)=0;
	virtual void OnBeforeDelete(TMySocket& sock)=0;
public:
	TBufferedServer(int port);
	~TBufferedServer(){ while (!socketek.empty()) DeleteSocket(0); }
	
	void Update();
};

class TUDPSocket{
    SOCKET sock;
public:
    int error;
    TUDPSocket(int port);
    ~TUDPSocket() { closesocket(sock); };
    bool Recv(TSocketFrame& hova, DWORD& ip, WORD& port);
	//majd implementálom ha szükség lesz rá.
    //void Send(const TSocketFrame& mit, DWORD ip:DWORD, WORD port); 
};


int SelectForRead(SOCKET sock);
int SelectForWrite(SOCKET sock);
int SelectForError(SOCKET sock);


//////////////////INLINE IMPLEMENTACIO//////////////

////////// TBufferedServer ///////////

template <class TContext,class TSocket>
 TBufferedServer<TContext,TSocket>::TBufferedServer(int port)
{
	#ifdef _WIN32
	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);
	#endif

	SOCKADDR_IN sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = INADDR_ANY;
	//sockaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP to listen on
	sockaddr.sin_port = htons(port);

	listenersock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (!listenersock)
		return;

	int YES=1;
	if (setsockopt( listenersock, SOL_SOCKET, SO_REUSEADDR, (char*)&YES, sizeof(YES) ))
		return;
	
	if(bind(listenersock, (SOCKADDR *)&sockaddr, sizeof(SOCKADDR)))
		return;

	if (listen(listenersock,SOMAXCONN))
		return;
}

template <class TContext,class TSocket>
void  TBufferedServer<TContext,TSocket>::Update()
{
	// accept stuff
	while(SelectForRead(listenersock))
	{
		sockaddr_in addr;
		#ifdef _WIN32
		int addrlen=sizeof(addr);
		#else
		socklen_t addrlen=sizeof(addr);
		#endif
		SOCKET sock=accept(listenersock,(sockaddr*)&addr,&addrlen);
		if (sock==INVALID_SOCKET)
			break; // húha...
		#ifdef _WIN32
		TMySocket* ujclient=new TMySocket(sock,addr.sin_addr.S_un.S_addr);
		#else
		TMySocket* ujclient=new TMySocket(sock,addr.sin_addr.s_addr);
		#endif
		socketek.push_back(ujclient);
		OnConnect(*socketek[socketek.size()-1]);
	}

	// update és erroros cuccok kiszórása
	int i=0;
	while((unsigned int)i<socketek.size())
	{
		socketek[i]->Update();
		if (socketek[i]->GetError())
			DeleteSocket(i);
		else
			++i;
	}

	// recv stuff
	int n=socketek.size();
	for (int i=0;i<n;++i)
		OnUpdate(*socketek[i]);
}

template <class TContext,class TSocket>
void TBufferedServer<TContext,TSocket>::DeleteSocket(int i)
{
	OnBeforeDelete(*socketek[i]);
	delete socketek[i]; // ez klózol is.
	socketek.erase(socketek.begin()+i);
}


#endif
