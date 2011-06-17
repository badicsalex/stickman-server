
#include "socket_stuff.h"
#include "crypt_stuff.h"
#include <string>
#include <iostream>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
int GetTickCount()
{
	timespec tim;
	clock_gettime(CLOCK_MONOTONIC,&tim);
	return tim.tv_sec*1000+tim.tv_nsec/1000000;
}

void Sleep(int msec)
{
	usleep(msec*1000);
}
#endif


using namespace std;

struct TStickContext{
	/* adatok */
	string nev;
	int fegyver;
	int fejrevalo;
	int UID;
	unsigned long ip;
	unsigned short port;
	int kills; //mai napon
	int checksum;

	/* állapot */
	unsigned int lastrecv;
	unsigned int lastsend;
	bool loggedin;
	int x,y;
	unsigned char crypto[20];
};

struct TConfig{
	int port;
	int clientversion;
	int serverversion;
	unsigned char sharedkey[20];
} config =
{
	25252,
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
	int fejrevaló
	char[2] port
	int checksum
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
		int fejrevalo
		int killek
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
	int lastUID;
	virtual void OnConnect(TMySocket& sock)
	{
		sock.context.loggedin=false;
 		sock.context.UID=++lastUID;
		for(int i=0;i<20;++i)
			sock.context.crypto[i]=rand();
		sock.context.lastrecv=GetTickCount();
		sock.context.lastsend=0;
		sock.context.kills=0;
		sock.context.ip=sock.address;
	}
	
	void SendKick(TMySocket& sock,const string& indok="Kicked without reason",bool hard=false)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_KICK);
		frame.WriteChar(hard?1:0);
		frame.WriteString(indok);
		sock.SendFrame(frame,true);
	}

	void SendPlayerList(TMySocket& sock)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_PLAYERLIST);
		int n=socketek.size();
		frame.WriteInt(n);
		/*!TODO legközelebbi 50 kiválasztása */
		for(int i=0;i<n;++i)
		{
			TStickContext& scontext=socketek[i]->context;
			frame.WriteChar((unsigned char)(scontext.ip));
			frame.WriteChar((unsigned char)(scontext.ip>>8));
			frame.WriteChar((unsigned char)(scontext.ip>>16));
			frame.WriteChar((unsigned char)(scontext.ip>>24));

			frame.WriteChar((unsigned char)(scontext.port));
			frame.WriteChar((unsigned char)(scontext.port>>8));
		
			frame.WriteInt(scontext.UID);

			frame.WriteString(scontext.nev);

			frame.WriteInt(scontext.fegyver);
			frame.WriteInt(scontext.fejrevalo);
			frame.WriteInt(scontext.kills);
		}
		sock.SendFrame(frame);
	}

	void SendLoginOk(TMySocket& sock)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_LOGINOK);
		frame.WriteInt(sock.context.UID);
		for(int i=0;i<20;++i)
			frame.WriteChar(sock.context.crypto[i]);
		sock.SendFrame(frame);
		int ip=sock.context.ip;
		cout<<"Login OK from:"<<((ip)&0xff)<<"."<<((ip>>8)&0xff)<<"."<<((ip>>16)&0xff)<<"."<<((ip>>24)&0xff)<<endl;
	}

	void SendChat(TMySocket& sock,const string& uzenet)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_CHAT);
		frame.WriteString(uzenet);
		sock.SendFrame(frame);
	}

	void OnMsgLogin(TMySocket& sock,TSocketFrame& msg)
	{
		int verzio=msg.ReadInt();
		if (verzio<config.clientversion )
		{
			SendKick(sock,"Please update your client at http://stickman.hu to play",true);
			return;
		}
		if (sock.context.loggedin)
		{
			SendKick(sock,"Protocol error: already logged in",true);
			return;
		}
		sock.context.nev=msg.ReadString();
		if (sock.context.nev.length()==0)
		{
			SendKick(sock,"Legy szives adj meg egy nevet.",true);
			return;
		}
		string jelszo=msg.ReadString();
		/*!TODO Login verifikálása*/

		sock.context.fegyver=msg.ReadInt();
		sock.context.fejrevalo=msg.ReadInt();

		sock.context.port =msg.ReadChar();
		sock.context.port+=msg.ReadChar()<<8;
		sock.context.checksum=msg.ReadInt();
		if (msg.cursor!=msg.datalen) //nem jo a packetmeret
		{
			SendKick(sock,"Protocol error: login",true);
			return;
		}
		
		SendLoginOk(sock);

		sock.context.loggedin=true;
	}

	void OnMsgStatus(TMySocket& sock,TSocketFrame& msg)
	{
		sock.context.x=msg.ReadInt();
		sock.context.y=msg.ReadInt();

		if (msg.cursor!=msg.datalen) //nem jo a packetmeret
		{
			SendKick(sock,"Protocol error: status",true);
			return;
		}
	}

	void OnMsgChat(TMySocket& sock,TSocketFrame& msg)
	{
		string uzenet=msg.ReadString();
		if (msg.cursor!=msg.datalen) //nem jo a packetmeret
		{
			SendKick(sock,"Protocol error: chat",true);
			return;
		}
		uzenet=sock.context.nev+": "+uzenet;
		int n=socketek.size();
		for(int i=0;i<n;++i)
			SendChat(*socketek[i],uzenet);
	}

	void OnMsgKill(TMySocket& sock,TSocketFrame& msg)
	{
		int UID=msg.ReadInt();

		// Minden kill után seedet xorolunk a titkos kulccsal, és egyet
		// ráhashelünk a jelenlegi crypt értékre.
		// Ez kellõen fos verifikálása a killnek, de ennyivel kell beérni
		// Thx Kirknek a security auditingért

		unsigned char newcrypto[20];
		for(int i=0;i<20;++i)
			newcrypto[i]=sock.context.crypto[i]^config.sharedkey[i];

		SHA1_Hash(newcrypto,20,sock.context.crypto);
		
		for(int i=0;i<20;++i)
			if (sock.context.crypto[i]!=msg.ReadChar())
			{
				SendKick(sock,"Protocol error: kill verification",true);
				return;
			}
		

		if (msg.cursor!=msg.datalen) //nem jo a packetmeret
		{
			SendKick(sock,"Protocol error: kill",true);
			return;
		}

		int n=socketek.size();
		for(int i=0;i<n;++i)
			if (socketek[i]->context.UID==UID &&
				socketek[i]->context.loggedin)
				socketek[i]->context.kills+=1;
	}

	virtual void OnUpdate(TMySocket& sock)
	{
		TSocketFrame recvd;
		while(sock.RecvFrame(recvd))
		{
			char type=recvd.ReadChar();
			if (!sock.context.loggedin && type!=CLIENTMSG_LOGIN)
				SendKick(sock,"Protocol error: not logged in",true);
			else
			switch(type)
			{
				case CLIENTMSG_LOGIN:	OnMsgLogin(sock,recvd); break;
				case CLIENTMSG_STATUS:	OnMsgStatus(sock,recvd); break;
				case CLIENTMSG_CHAT:	OnMsgChat(sock,recvd); break;
				case CLIENTMSG_KILLED:	OnMsgKill(sock,recvd); break;
				default:
					SendKick(sock,"Protocol error: unknown packet type",true);
			}
			sock.context.lastrecv=GetTickCount();
		}

		/* 2000-2500 msenként küldünk playerlistát */
		if (sock.context.loggedin &&
			sock.context.lastsend<GetTickCount()-2000-(rand()&511))
		{
			SendPlayerList(sock);
			sock.context.lastsend=GetTickCount();
		}
		/* 10 másodperc tétlenség után kick van. */
		if (sock.context.lastrecv<GetTickCount()-10000)
			SendKick(sock,"Ping timeout",true);
	}
public:

	StickmanServer(int port): TBufferedServer<TStickContext>(port),lastUID(1){}

	void Update()
	{
		TBufferedServer<TStickContext>::Update();
	}
};


int main(){
	cout<<"Stickserver starting..."<<endl;
	{
		StickmanServer server(config.port);
		while(1)
		{
			server.Update();
			Sleep(10);
		}
	}
}
