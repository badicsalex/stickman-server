#include <winsock.h>
#include <iostream>
#include "socket_stuff.h"
#include "crypt_stuff.h"
#include "windows.h"
#include <string>

using namespace std;

#define APP_PORT 25252

struct TStickContext{
	/* adatok */
	string nev;
	int fegyver; 
	int UID;
	unsigned long ip;
	unsigned short port;

	/* állapot */
	unsigned int lastrecv;
	unsigned int lastsend;
	bool loggedin;
	int x,y;
	unsigned char crypto[20];
};

struct TConfig{
	int clientversion;
	int serverversion;
	unsigned char sharedkey[20];
} config =
{
	30000,
	20000,
	{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19}
};

#define CLIENTMSG_LOGIN 1
/*	Login üzenet. Erre válasz: LOGINOK, vagy KICK
	int kliens_verzió
	string név
	string jelszó
	int fegyver
*/

#define CLIENTMSG_STATUS 2
/*	Ennek az üzenetnek sok értelme nincs csak a kapcsolatot tartja fenn.
	int x
	int y
*/

#define CLIENTMSG_CHAT 3
/*	Chat, ennyi.
	string uzenet
*/

#define CLIENTMSG_KILLED 4
/*	Ha megölte a klienst valaki, ezt küldi.
	int UID
	char [20] crypto
*/

#define SERVERMSG_LOGINOK 1
/*
	int UID
	char [20] crypto
*/

#define SERVERMSG_PLAYERLIST 2
/*
	int num
	num*{
		char[4] ip
		char[2] port
		int uid
		string nev
		int fegyver
		}
*/

#define SERVERMSG_KICK 3
/*
	char hardkick (bool igazából)
	string indok
*/

#define SERVERMSG_CHAT 4
/*
	string uzenet
*/

class StickmanServer: public TBufferedServer<TStickContext>{
protected:
	virtual void OnConnect(TMySocket& sock)
	{
		sock.context.loggedin=false;
 		sock.context.UID=rand();
		sock.context.lastrecv=GetTickCount();
	}
	
	void SendKick(TMySocket& sock,const string& indok="Kicked without reason",bool hard=false)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_KICK);
		frame.WriteChar(hard?1:0);
		frame.WriteString(indok);
		sock.SendFrame(frame,true);
	}

	void SendProtocolError(TMySocket& sock)
	{
		SendKick(sock,"Protocol error",true);
	}

	void SendPlayerList(TMySocket& sock)
	{
		TSocketFrame frame;
		int n=socketek.size();
		frame.WriteInt(n);
		for(int i=0;i<n;++i)
		{
			TStickContext& scontext=socketek[i]->context;
			frame.WriteChar((unsigned char)(scontext.ip));
			frame.WriteChar((unsigned char)(scontext.ip>>8));
			frame.WriteChar((unsigned char)(scontext.ip>>16));
			frame.WriteChar((unsigned char)(scontext.ip>24));

			frame.WriteInt((unsigned char)(scontext.port));
			frame.WriteInt((unsigned char)(scontext.port>>8));
		
			frame.WriteInt(scontext.UID);

			frame.WriteString(scontext.nev);

			frame.WriteInt(scontext.fegyver);
		}
	}

	void OnMsgLogin(TMySocket& sock,TSocketFrame& msg)
	{
		int verzio=msg.ReadInt();
		if (verzio<config.clientversion )
		{
			SendKick(sock,"Please update your client at http://stickman.hu to play",true);
			return;
		}
		string nev=msg.ReadString();
		string jelszo=msg.ReadString();
		int fegyver=msg.ReadInt();

		if (msg.cursor!=msg.datalen) //nem jo a packetmeret
			SendProtocolError(sock);

	}

	void OnMsgStatus(TMySocket& sock,TSocketFrame& msg)
	{
		sock.context.x=msg.ReadInt();
		sock.context.y=msg.ReadInt();

		if (msg.cursor!=msg.datalen) //nem jo a packetmeret
			SendProtocolError(sock);
	}

	void OnMsgChat(TMySocket& sock,TSocketFrame& msg)
	{
		string uzenet=msg.ReadString();
		if (msg.cursor!=msg.datalen) //nem jo a packetmeret
			SendProtocolError(sock);

		/*!TODO Chat üzenet szétkürtölése */
	}

	void OnMsgKill(TMySocket& sock,TSocketFrame& msg)
	{
		int UID=msg.ReadInt();

		// Minden kill után egyet ráhashelünk a jelenlegi crypt értékre
		// és xoroljuk a kliens és szerver között megosztott kulccsal.
		// Ez kellõen fos verifikálása a killnek, de ennyivel kell beérni

		unsigned char newcrypto[20];
		SHA1_Hash(sock.context.crypto[i],20,newcrypto);
		for(int i=0;i<20;++i)
		{
			sock.context.crypto[i]=newcrypto[i]^config.sharedkey[i];

			// Ha már itt vagyunk, karakterenként lehet ellenõprizni is
			// Ha érdekelne, hogy átlátható-e a kód, ez külön for
			// ciklusban lenne.
			if (sock.context.crypto[i]!=msg.ReadChar())
			{
				SendKick(sock,"Kill verification error",true);
				return;
			}
		}

		if (msg.cursor!=msg.datalen) //nem jo a packetmeret
			SendProtocolError(sock);

		/*!TODO kill regisztrálása */
	}

	virtual void OnUpdate(TMySocket& sock)
	{
		TSocketFrame recvd;
		while(sock.RecvFrame(recvd))
		{
			char type=recvd.ReadChar();
			switch(type)
			{
				case CLIENTMSG_LOGIN:	OnMsgLogin(sock,recvd);break;
				case CLIENTMSG_STATUS:	OnMsgStatus(sock,recvd);break;
				case CLIENTMSG_CHAT:	OnMsgChat(sock,recvd);break;
			}
			sock.context.lastrecv=GetTickCount();
		}

		/* 2000-2500 msenként küldünk playerlistát */
		if (sock.context.lastsend<GetTickCount()+2000+(rand()&511))
			SendPlayerList(sock);

		/* 10 másodperc tétlenség után kick van. */
		if (sock.context.lastrecv<GetTickCount()+10000)
			SendKick(sock,"Ping timeout",true);
	}
public:

	StickmanServer(int port): TBufferedServer(port){}

	void Update()
	{
		TBufferedServer::Update();
	}
};


int main(){
	cout<<"Stickserver starting..."<<endl;
	{
		StickmanServer server(1234);
		while(1)
		{
			server.Update();
			Sleep(10);
		}
	}

}