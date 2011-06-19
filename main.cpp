#define _CRT_SECURE_NO_WARNINGS
#include "socket_stuff.h"
#include "crypt_stuff.h"
#include "standard_stuff.h"
#include <string>
#include <set>
#include <map>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <time.h>
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
typedef unsigned short WORD;
#endif


using namespace std;


struct TConfig{
	int clientversion;				//kliens verzió, ez alatt kickel
	unsigned char sharedkey[20];	//a killenkénti kriptográfiai aláírás kulcsa
	string webinterface;			//a webes adatbázis hostneve
	string webinterfacedown;		//a webes adatbázis letöltõ fájljának URL-je
	string webinterfaceup;			//aa webes adatbázis killfeltöltõ fájljának URL-je
	TConfig(const string& honnan)
	{
		ifstream fil(honnan.c_str());
		fil>>clientversion;
		for(int i=0;i<20;++i)
		{
			int tmp;
			fil>>hex>>tmp;
			sharedkey[i]=tmp;
		}
		fil>>webinterface;
		fil>>webinterfacedown;
		fil>>webinterfaceup;
	}

} config("config.cfg");

struct TStickContext{
	/* adatok */
	string nev;
	string clan;
	bool registered;
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

struct TStickRecord{
	int id;
	string jelszo;
	int osszkill;
	int napikill;
	string clan;
	set<WORD> medal;
	int level;
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

	map<string,TStickRecord> db;
	map<string,int> killdb;

	unsigned int lastUID;
	unsigned int lastUDB;
	time_t lastUDBsuccess;

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
		cout<<"Kicked "<<sock.context.nev<<": "<<indok<<endl;
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
		int ip=sock.context.ip;
		cout<<"Login "<<sock.context.nev<<" from "<<((ip)&0xff)<<"."<<((ip>>8)&0xff)<<"."<<((ip>>16)&0xff)<<"."<<((ip>>24)&0xff)<<endl;
		
		if (db.count(sock.context.nev))//regisztrált player
		{
			TStickRecord& record=db[sock.context.nev];
			if (record.jelszo!=jelszo)
			{
				SendKick(sock,"Hibas jelszo.",false);
				return;
			}
			sock.context.clan=record.clan;
			sock.context.registered=true;
			SendLoginOk(sock);
			string chatuzi="Udvozollek ujra a jatekban, "+sock.context.nev+".";
			if(record.level)
				chatuzi=chatuzi+" A weboldalon erkezett "+itoa(record.level)+" leveled.";
			SendChat(sock,chatuzi);
		}
		else
		if (jelszo.length()==0)
		{
			SendLoginOk(sock);
			string chatuzi="Udvozollek a jatekban, "+sock.context.nev+". Ha tetszik a jatek, erdemes regisztralni a stickman.hu oldalon.";
			SendChat(sock,chatuzi);
		}
		else
		{
			SendKick(sock,"Ez a nev nincs regisztralva, igy jelszo nelkul lehet csak hasznalni.",false);
			return;
		}

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
			{
				socketek[i]->context.kills+=1;
				if(socketek[i]->context.registered)
				{
					const string& nev=socketek[i]->context.nev;
					if (killdb.count(nev))
						killdb[nev]+=1;
					else
						killdb[nev]=1;
					db[nev].napikill+=1;
					db[nev].osszkill+=1;
				}
			}
	}


	void UpdateDb()
	{
		cout<<"Updating database..."<<endl;
		//post kills
		if(killdb.size()>0)
		{
			cout<<"Uploading kills "<<killdb.size()<<" kills."<<endl;
			TBufferedSocket sock("stickman.hu",80);

			string postmsg;
			postmsg.reserve(64*1024);
			for(map<string,int>::iterator i=killdb.begin();i!=killdb.end();++i)
			{
				const string& nev=i->first;
				postmsg+=nev+"\r\n";
				postmsg+=itoa(i->second)+"\r\n";
				for (set<WORD>::iterator j=db[nev].medal.begin();j!=db[nev].medal.end();++j)
				{
					postmsg+=((char)*j);
					postmsg+=((char)(*j>>8));
				}
				postmsg+="\r\n";
			}

			sock.SendLine("POST "+config.webinterfaceup+" HTTP/1.1");
			sock.SendLine("Host: "+config.webinterface);
			sock.SendLine("Connection: close");
			sock.SendLine("Content-Type: application/x-www-form-urlencoded");
			sock.SendLine("Content-Length: "+itoa(postmsg.length()+2));
			sock.SendLine("");
			sock.SendLine(postmsg);
			
			string lin;
			while(!sock.error)
			{
					sock.Update();
					Sleep(1);
			}
			//!TODO
			while(sock.RecvLine(lin))
				cout<<lin<<endl;
			/* while(sock.RecvLine(lin))
				delete killdb[lin] */
			killdb.clear();
		}

		//get db
		{
			TBufferedSocket sock(config.webinterface,80);

			char request[1024];
			sprintf(request,config.webinterfacedown.c_str(),lastUDBsuccess);			
			cout<<"Download req: "<<config.webinterface<<string(request)<<endl;
			sock.SendLine("GET "+string(request)+" HTTP/1.1");
			sock.SendLine("Host: "+config.webinterface);
			sock.SendLine("Connection: close");
			sock.SendLine("");

			//Küldjönk el és recv-eljünk mindent (kapcsolatzárásig)
			while(!sock.error)
			{
					sock.Update();
					Sleep(1);
			}

			//HTTP header átugrása
			string headerline;
			do
			{
				if(!sock.RecvLine(headerline)) //vége szakad a cuccnak, ez nem üres sor!
				{
					cout<<"Problem: nincs http body."<<endl;
					headerline="shit.";
					break;
				}
			}while(headerline.length()>0);

			//Feldolgozás
			while(1)
			{
				string nev;
				if(!sock.RecvLine(nev)) //vége szakad a cuccnak, ez nem üres sor!
				{
					cout<<"Problem: nem ures sorral er veget az adas."<<endl;				
					break;
				}

				if (nev.length()==0) //üres sor, ez jó, sikerült ápdételni
				{
					lastUDBsuccess=time(0);
					break;
				}
				string medal=sock.RecvLine2();

				TStickRecord ujrec;
				ujrec.id=atoi(sock.RecvLine2().c_str());
				ujrec.jelszo=sock.RecvLine2();
				ujrec.osszkill=atoi(sock.RecvLine2().c_str());
				ujrec.napikill=atoi(sock.RecvLine2().c_str());
				ujrec.clan=sock.RecvLine2();
				ujrec.level=atoi(sock.RecvLine2().c_str());
				int n=medal.length();
				for(int i=0;i<n;i+=2)
					ujrec.medal.insert(medal[i]|(medal[i+1]<<8));
				db[nev]=ujrec;
			}
		}
		cout<<"Finished. "<<db.size()<<" player records."<<endl;
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

	StickmanServer(int port): TBufferedServer<TStickContext>(port),lastUID(1),lastUDB(0),lastUDBsuccess(0){}

	void Update()
	{
		if (lastUDB<GetTickCount()-300000)//5 percenként.
		{
			UpdateDb();
			lastUDB=GetTickCount();
		}
		TBufferedServer<TStickContext>::Update();
	}
};


int main(){
	cout<<"Stickserver starting..."<<endl;
	{
		StickmanServer server(25252);
		while(1)
		{
			server.Update();
			Sleep(10);
		}
	}
}
