#ifndef SOCKET_STUFF_INCLUDED
#define SOCKET_STUFF_INCLUDED
#include <winsock.h>
#include <string>
#include <vector>
#include "standard_stuff.h"

using namespace std;

class TSocketFrame
{
	TSocketFrame(TSocketFrame&);// ha ezt tolod, gáz van
	TSocketFrame& operator=(TSocketFrame&);//pointerekkel ilyent nem játszunk.
	void EnlargeBuffer(int mekkorara);
public:
	char* data; // csínján.
	int cursor;
	int datalen;

	// átveszi a char* kezelését, felszabadítja destruáláskor.
	TSocketFrame(char* data,int datalen):data(data),datalen(datalen),cursor(0){}
	TSocketFrame():data(0),datalen(0),cursor(0){}
	~TSocketFrame(){if (data) delete[] data;}

	void Reset(){cursor=0;}
	char ReadChar();
	unsigned int ReadWord();
	int ReadInt();
	string ReadString();
	void WriteChar(char mit);
	void WriteWord(unsigned int mit);
	void WriteInt(int mit);
	void WriteString(const string& mit);
};


class TBufferedSocket
{
	SOCKET sock;
	vector<char> recvbuffer;
	vector<char> sendbuffer;
	TBufferedSocket(const TBufferedSocket& mirol);
	TBufferedSocket& operator= (const TBufferedSocket& mirol);
public:
	int error;// ha nem 0, gáz volt.
	bool closeaftersend;
	// A függvény meghívásával elfogadja a szerzõdési feltételeket
	// miszerint teljesen lemond a socket használatáról.
	TBufferedSocket(SOCKET sock):sock(sock), error(0), closeaftersend(false){};
	~TBufferedSocket(){ closesocket(sock); }

	void Update(); //Hívjad sokat. Mert alattam az a kettõ csak a buffereket nézi
	bool RecvFrame(TSocketFrame& hova);// Légyszi újonnan generáltat, mert úgyis felülírja.
	void SendFrame(TSocketFrame& mit);// Légyszi ne legyen üres.
};

// abstract class virtual callback-ekkel
template <class TContext>
class TBufferedServer{
protected:
	class TMySocket: public TBufferedSocket{
	public:
		TContext context; //Default konstruktor!!!
		ULONG address; 
		TMySocket(SOCKET sock, ULONG address): 
			TBufferedSocket(sock),address(address){}
	};

	SOCKET listenersock;
	vector<TMySocket*> socketek;
	void DeleteSocket(int index);//és close
public:
	TBufferedServer(int port);
	~TBufferedServer(){ while (!socketek.empty()) DeleteSocket(0); }
	virtual void OnConnect(TMySocket& sock)=0;
	virtual void OnUpdate(TMySocket& sock)=0;
	void Update();
};

int SelectForRead(SOCKET sock);
int SelectForWrite(SOCKET sock);
int SelectForError(SOCKET sock);


//////////////////INLINE IMPLEMENTACIO//////////////

////////// TBufferedServer ///////////

template <class TContext>
 TBufferedServer<TContext>::TBufferedServer(int port)
{
	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);


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

template <class TContext>
void  TBufferedServer<TContext>::Update()
{
	// accept stuff 
	while(SelectForRead(listenersock))
	{
		sockaddr_in addr;
		int addrlen=sizeof(addr);
		SOCKET sock=accept(listenersock,(sockaddr*)&addr,&addrlen);
		if (sock==INVALID_SOCKET)
			break; // húha...

		TMySocket* ujclient=new TMySocket(sock,addr.sin_addr.S_un.S_addr);
		socketek.push_back(ujclient);
		OnConnect(*socketek[socketek.size()-1]);
	}

	// update és erroros cuccok kiszórása
	int i=0;
	while((unsigned int)i<socketek.size())
	{
		socketek[i]->Update();
		if (socketek[i]->error)
			DeleteSocket(i);
		else
			++i;
	}

	// recv stuff
	int n=socketek.size();
	for (int i=0;i<n;++i)
		OnUpdate(*socketek[i]);
}

template <class TContext>
void TBufferedServer<TContext>::DeleteSocket(int i)
{
	delete socketek[i]; // ez klózol is.
	socketek.erase(socketek.begin()+i);
}


#endif