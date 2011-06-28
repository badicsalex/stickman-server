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


const struct TConfig{
	int clientversion;				//kliens verzió, ez alatt kickel
	unsigned char sharedkey[20];	//a killenkénti kriptográfiai aláírás kulcsa
	string webinterface;			//a webes adatbázis hostneve
	string webinterfacedown;		//a webes adatbázis letöltõ fájljának URL-je
	string webinterfaceup;			//aa webes adatbázis killfeltöltõ fájljának URL-je
	string allowedchars;			//névben megengedett karakterek
	vector<string> csunyaszavak;	//cenzúrázandó szavak
	vector<string> viragnevek;		//amivel lecseréljük a cenzúrázandó szavakat.
	TConfig(const string& honnan)
	{
		ifstream fil(honnan.c_str());
		fil>>clientversion;
		for(int i=0;i<20;++i)
		{
			int tmp;
			fil>>hex>>tmp;
			sharedkey[i]=(unsigned char)tmp;
		}
		fil>>webinterface;
		fil>>webinterfacedown;
		fil>>webinterfaceup;
		fil>>allowedchars;

		string tmpstr;

		fil>>tmpstr; 
		csunyaszavak=explode(tmpstr,",");

		fil>>tmpstr; 
		viragnevek=explode(tmpstr,",");
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
	int glyph;
	unsigned int lastrecv;
	unsigned int lastsend;
	bool loggedin;
	int x,y;
	unsigned char crypto[20];
	unsigned int floodtime;
	string realm;
	int lastwhisp;
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
	int glyph
*/

#define SERVERMSG_WEATHER 5
/*
	byte mire
*/


class StickmanServer: public TBufferedServer<TStickContext>{
protected:

	map<string,TStickRecord> db;
	map<string,int> killdb;

	unsigned int lastUID;
	unsigned int lastUDB;
	unsigned int lastweather;
	time_t lastUDBsuccess;

	int weathermost;
	int weathercel;
	virtual void OnConnect(TMySocket& sock)
	{
		sock.context.loggedin=false;
 		sock.context.UID=++lastUID;
		for(int i=0;i<20;++i)
			sock.context.crypto[i]=(unsigned char)rand();
		sock.context.lastrecv=GetTickCount();
		sock.context.lastsend=0;
		sock.context.kills=0;
		sock.context.ip=sock.address;
		sock.context.floodtime=0;
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
		int n2=0;
		for(int i=0;i<n;++i)
			if(sock.context.realm==socketek[i]->context.realm)
				++n2;
		frame.WriteInt(n2);
		/*!TODO legközelebbi 50 kiválasztása */
		for(int i=0;i<n;++i)
		if(sock.context.realm==socketek[i]->context.realm)
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
	}

	void SendChat(TMySocket& sock,const string& uzenet,int showglyph=0)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_CHAT);
		frame.WriteString(uzenet);
		frame.WriteInt(showglyph);
		sock.SendFrame(frame);
	}

	void SendChatToAll(const string& uzenet,int showglyph=0)
	{
		int n=socketek.size();
		for(int i=0;i<n;++i)
			SendChat(*socketek[i],uzenet,showglyph);
	}

	void SendWeather(TMySocket& sock,int mire)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_WEATHER);
		frame.WriteChar((unsigned char)mire);
		sock.SendFrame(frame);
	}

	void OnMsgLogin(TMySocket& sock,TSocketFrame& msg)
	{
		int verzio=msg.ReadInt();
		if (verzio<config.clientversion )
		{
			SendKick(sock,"Kérlek update-eld a játékot a  http://stickman.hu oldalon",true);
			return;
		}
		if (sock.context.loggedin)
		{
			SendKick(sock,"Protocol error: already logged in",true);
			return;
		}
		string& nev=sock.context.nev=msg.ReadString();
		int n=nev.length();
		if (n==0)
		{
			SendKick(sock,"Légy szíves adj meg egy nevet.",true);
			return;
		}
		if (n>15)
		{
			SendKick(sock,"A név túl hosszú. Ami fura, mert a kliens ezt alapbol nem engedi...",true);
			return;
		}
		for(int i=0;i<n;++i)
			if (config.allowedchars.find(nev[i])==string::npos)
			{
				SendKick(sock,"A név meg nem engedett karaktereket tartalmaz.",true);
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
		
		if (db.count(nev))//regisztrált player
		{
			TStickRecord& record=db[nev];
			if (record.jelszo!=jelszo)
			{
				if (record.jelszo=="regi")
					SendKick(sock,"Kérlek újítsd meg a regisztrációd a http://stickman.hu/ oldalon",true);
				else
					SendKick(sock,"Hibás jelszó.",false);
				return;
			}
			sock.context.clan=record.clan;
			sock.context.registered=true;
			SendLoginOk(sock);
			string chatuzi="\x11\x01Üdvözöllek újra a játékban, \x11\x03"+nev+"\x11\x01.";
			if(record.level)
				chatuzi=chatuzi+" A weboldalon érkezett "+itoa(record.level)+" leveled.";
			SendChat(sock,chatuzi);
			SendWeather(sock,weathermost);
		}
		else
		if (jelszo.length()==0)
		{
			SendLoginOk(sock);
			string chatuzi="\x11\x01Üdvözöllek a játékban, \x11\x03"+nev+"\x11\x01. Ha tetszik, érdemes regisztrálni a stickman.hu oldalon.";
			SendChat(sock,chatuzi);
			SendWeather(sock,weathermost);
		}
		else
		{
			SendKick(sock,"Ez a név nincs regisztrálva, így jelszó nélkül lehet csak használni.",false);
			return;
		}

		sock.context.glyph=0;
		n=nev.length();
		for(int i=0;i<n;++i)
		{
			sock.context.glyph*=982451653;
			sock.context.glyph+=nev[i];
		}

		sock.context.glyph*=756065179;
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

	void ChatCommand(TMySocket& sock,const string& command,const string& parameter)
	{
		/* user commandok */

		if (command=="realm" && parameter.size()>0)
		{
			sock.context.realm=parameter;
			SendChat(sock,"Mostantól a "+parameter+" realmon játszol. A killjeid nem számítanak a toplistába");
		}else
		if (command=="norealm" || (command=="realm" && parameter.size()==0))
		{
			sock.context.realm="";
			SendChat(sock,"Visszaléptél a fõ realmba. A killjeid ismét számítanak.");
		}else
		if (command=="w" || command=="whisper")
		{
			int pos=parameter.find(' ');
			if(pos>=0)
			{
				int n=socketek.size();
				string kinek(parameter.begin(),parameter.begin()+pos);
				string uzenet(parameter.begin()+pos+1,parameter.end());
				for(int i=0;i<n;++i)
				{
					string& nev=socketek[i]->context.nev;
					if (nev==kinek)
					{
						ChatCleanup(uzenet);
						SendChat(*socketek[i],"\x11\xe3[From] "+sock.context.nev        +": "+uzenet,socketek[i]->context.glyph);
						SendChat(sock        ,"\x11\xe3[To] "  +socketek[i]->context.nev+": "+uzenet,sock.context.glyph);
						socketek[i]->context.lastwhisp=sock.context.UID;
						break;
					}
				}
			}
		}else
		if(command=="r" || command=="reply")
		{
			int n=socketek.size();
			for(int i=0;i<n;++i)
				if (socketek[i]->context.UID==sock.context.lastwhisp)
				{
					string uzenet=parameter;
					ChatCleanup(uzenet);
					SendChat(*socketek[i],"\x11\xe3[From] "+sock.context.nev        +": "+uzenet,socketek[i]->context.glyph);
					SendChat(sock        ,"\x11\xe3[To] "  +socketek[i]->context.nev+": "+uzenet,sock.context.glyph);
					socketek[i]->context.lastwhisp=sock.context.UID;
					break;
				}
		}else
		if((command=="c" || command=="clan") && sock.context.clan.size()>0)
		{
			string uzenet=parameter;
			ChatCleanup(uzenet);
			uzenet="\x11\x10"+sock.context.clan+" "+sock.context.nev+": "+uzenet;
			int n=socketek.size();
			for(int i=0;i<n;++i)
				if (socketek[i]->context.clan==sock.context.clan)
					SendChat(*socketek[i],uzenet,sock.context.glyph);
		}

		/* admin commandok */
		if (sock.context.nev!="Admin")
			return;

		if (command=="weather")
		{
			weathermost=weathercel=atoi(parameter.c_str());
			int n=socketek.size();
			for(int i=0;i<n;++i)
				SendWeather(*socketek[i],weathermost);
		}else
		if (command=="kick")
		{
			int kickuid=atoi(parameter.c_str());
			int pos=parameter.find(' ');
			string uzenet;
			if(pos>=0)
				uzenet.assign(parameter.begin()+pos,parameter.end());
			else
				uzenet="Legkozelebb ne legy balfasz.";
			int n=socketek.size();
			for(int i=0;i<n;++i)
				if (socketek[i]->context.UID==kickuid)
				{
					SendKick(*socketek[i],"Admin kickelt: "+uzenet);
					SendChatToAll("\x11\xe0" "Admin kickelte \x11\x03"+socketek[i]->context.nev+"\x11\xe0-t: "+uzenet);
					break;
				}
		}else
		if (command=="uid")
		{
			int n=socketek.size();
			for(int i=0;i<n;++i)
			{
				string& nev=socketek[i]->context.nev;
				if (nev.find(parameter)!=string::npos)
					SendChat(sock,itoa(socketek[i]->context.UID)+" : "+nev);
			}

		}
	}

	void ChatCleanup(string& uzenet)
	{
		int n=uzenet.length();
		for(int i=0;i<n;++i)
			if (uzenet[i]>=0 && uzenet[i]<0x20)
				uzenet[i]=0x20;

		int cn=config.csunyaszavak.size();
		int vn=config.viragnevek.size();
		string luzenet=uzenet;
		tolower(luzenet);
		for (int i=0;i<cn;++i)
		{
			string::size_type pos=0;
			while ( (pos = luzenet.find(config.csunyaszavak[i], pos)) != string::npos ) {
				uzenet.replace( pos, config.csunyaszavak[i].size(), config.viragnevek[rand()%vn] );
				pos++;
			}
		}

		char elobb=0;
		int cnt=0;
		unsigned int i=0;
		while(i<uzenet.size())
		{
			if (uzenet[i]==elobb)
				++cnt;
			else
			{
				elobb=uzenet[i];
				cnt=0;
			}
			if (cnt>3)
				uzenet.erase(i,1);
			else
				++i;
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

		if (uzenet.length()<=1)
			return;

		//flood ellenorzes
		if (sock.context.floodtime<GetTickCount()-12000)
			sock.context.floodtime=GetTickCount()-10000;
		else
			sock.context.floodtime+=2000;
		if (sock.context.floodtime>GetTickCount())
		{
			SendKick(sock,"Ne írj ennyi üzenetet egymás után.",true);
			return;
		}

		if (uzenet[0]=='/')
		{
			int pos=uzenet.find(' ');
			if(pos>=0)
				ChatCommand(sock, 
				            string(uzenet.begin()+1,uzenet.begin()+pos),
							string(uzenet.begin()+pos+1,uzenet.end()));
			else
				ChatCommand(sock, 
				            string(uzenet.begin()+1,uzenet.end()),
							"");
		}
		else
		{
			ChatCleanup(uzenet);
			if (sock.context.nev=="Admin")
				uzenet="\x11\xe0"+sock.context.nev+"\x11\x03: "+uzenet;
			else
				uzenet="\x11\x01"+sock.context.nev+"\x11\x03: "+uzenet;
			if (sock.context.clan.length()>0)
				uzenet="\x11\x10"+sock.context.clan+" "+uzenet;

			int n=socketek.size();
			for(int i=0;i<n;++i)
				if (socketek[i]->context.realm==sock.context.realm)
					SendChat(*socketek[i],uzenet,sock.context.glyph);
		}
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
					if (socketek[i]->context.realm.size()==0)
					{
						if (killdb.count(nev))
							killdb[nev]+=1;
						else
							killdb[nev]=1;
						db[nev].napikill+=1;
						db[nev].osszkill+=1;
					}
					SendChat(*socketek[i],"\x11\x01Megölted \x11\x03"+sock.context.nev+"\x11\x01-t.");
					SendChat(sock,"\x11\x03"+nev+" \x11\x01megölt téged.");
					break;
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
			sock.SendLine("GET "+string(request)+" HTTP/1.0");
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
			string lastname;
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
				lastname=nev;
			}
			cout<<"Last name: "<<lastname<<endl;
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
			SendKick(sock,"Ping timeout",false);
	}
public:

	StickmanServer(int port): TBufferedServer<TStickContext>(port),
		lastUID(1),lastUDB(0),lastUDBsuccess(0),lastweather(0),weathermost(8),weathercel(15){}

	void Update()
	{
		if (lastUDB<GetTickCount()-300000)//5 percenként.
		{
			UpdateDb();
			lastUDB=GetTickCount();
		}

		TBufferedServer<TStickContext>::Update();

		if (lastweather<GetTickCount()-60000)//percenként
		{
			if (weathermost==weathercel)
				weathercel=rand()%23;
			if(weathermost>weathercel)
				--weathermost;
			else
				++weathermost;

			int n=socketek.size();
			for(int i=0;i<n;++i)
				SendWeather(*socketek[i],weathermost);
			lastweather=GetTickCount();
		}
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
