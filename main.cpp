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
#include <ctime>


//FLAGEK
//#define WHITELIST

#ifdef _WIN32
#include <windows.h>


#else
//mi a fasznak van egyatalan SIGPIPE?! Az eszem megall.
#include <signal.h>

#include <sys/time.h>
#include <unistd.h>
unsigned long long GetTickCount64()
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

 struct Finished1v1{
	 string player1,player2;
	 int score1,score2;
 };
 


 struct TConfig{
	int clientversion;				//kliens verzió, ez alatt kickel
	unsigned int datachecksum;
	unsigned char sharedkey[20];	//a killenkénti kriptográfiai aláírás kulcsa
	string webinterface;			//a webes adatbázis hostneve
	string webinterfacedown;		//a webes adatbázis letöltõ fájljának URL-je
	string webinterfaceup;			//aa webes adatbázis killfeltöltõ fájljának URL-je
	string allowedchars;			//névben megengedett karakterek
	string lowercasechars;			//névben megengedett karakterek
	vector<string> csunyaszavak;	//cenzúrázandó szavak
	vector<string> viragnevek;		//amivel lecseréljük a cenzúrázandó szavakat.
	vector<string> adminok;			//adminok listája
	string cfgsource;				//a config fájl helye

	TConfig(const string& honnan)
	{
		cfgsource=honnan;
		reload();
	}

	void reload()
	{
		ifstream fil(cfgsource.c_str());
		fil>>clientversion;
		fil>>hex>>datachecksum;


	
		for(int i=0;i<20;++i)
		{
			int tmp;
			fil>>hex>>tmp;
			sharedkey[i]=(unsigned char)tmp;
		}
			
		fil>>webinterface;
		fil>>webinterfacedown;
		fil>>webinterfaceup;

		string tmpstr;

		fil>>tmpstr; 
		adminok=explode(tmpstr,",");
		
		fil>>allowedchars;
		fil>>lowercasechars;

		

		fil>>tmpstr; 
		csunyaszavak=explode(tmpstr,",");

		fil>>tmpstr; 
		viragnevek=explode(tmpstr,",");
	}

	const string ToLowercase(const string& mit) const
	{
		string result;
		result=mit;
		for (unsigned int i=0;i<mit.length();++i)
			for (unsigned int j=0;j<allowedchars.length();++j)
				if (result[i]==allowedchars[j])
					result[i]=lowercasechars[j];
		return result;
	}
} config("config.cfg");



struct autoMessages{

	vector<std::string> msglist;
	string file;

	autoMessages(string f)
	{
		file = f;
		load();
	}

	void load()
	{
		try	
		{
			ifstream fil(file.c_str());
			msglist.clear();
			char tmp[1024];
			while (!fil.eof())
				{				
				fil.getline(tmp,1023);
				msglist.push_back(tmp);
				}
			fil.close();
			cout << "loaded "<<msglist.size()<<" messages\n";
		}
		catch (exception e)
		{
			cout<<"error reading "<<file<<": "<<e.what()<<"\n";
		}
			
		
	}

	string randomMessage()
	{
		if (msglist.size()==0) return "";
		int k = rand()%msglist.size();
		return msglist[k];
	}


} autoMsg("msglist.txt");


struct TKill{
	TKill(){
		for(int i=0;i<8;++i)
			data[i]=0;
	}
	TKill(const TKill& honnan){
		for(int i=0;i<8;++i)
			data[i]=honnan.data[i];
	}
	TKill& operator=(const TKill& honnan){
		for(int i=0;i<8;++i)
			data[i]=honnan[i];
		return *this;
	}
	int& operator[](int index){return data[index];}
	const int& operator[](int index) const {return data[index];}
private:
	int data[8];
};

struct T1v1Game{
	string kihivoNev,ellenfelNev;
	int kihivoPont,ellenfelPont;
	char allapot;
	int limit;
};

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
	unsigned checksum;
	int nyelv;
	bool admin;
	bool kihivo;
	bool is1v1;
	bool verified;
	bool realmAdmin;

	//1v1games
	bool readyFor1v1Games;


	/* állapot */
	int glyph;
	unsigned long long lastrecv; //utolsó fogadott csomag
	unsigned long long lastsend; //utolsó küldött playerlista
	unsigned long long lastudpquery; //utolsó kérés hogy kliens küldjön UDP-t
	bool gotudp; //kaptunk UDP csomagot a port javításhoz.
	int udpauth;
	bool loggedin;
	int x,y;
	unsigned char crypto[20];
	unsigned long long floodtime;
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
	int fegyv;
	int level;
	int kills;
};



#define VARAKOZAS 0
#define FOLYAMATBAN 1
#define VEGE 2


#define	FEGYV_M4A1 0
#define	FEGYV_M82A1 1
#define	FEGYV_LAW 2
#define	FEGYV_MP5A3 3

#define FEGYV_MPG 4
#define	FEGYV_QUAD 5
#define	FEGYV_NOOB 6
#define	FEGYV_X72 7

int fegyvtoint(int i)
{
	int t = i;
	if (i>127) t= i-128+4;
	return t%8;
};

#define CLIENTMSG_LOGIN 1
/*	Login üzenet. Erre válasz: LOGINOK, vagy KICK
	int kliens_verzió
	int nyelv
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

#define CLIENTMSG_MEDAL 5
/*	A kliens medált kér
	int medal id
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

#define SERVERMSG_SENDUDP 6
/*
  int auth
*/

#define SERVERMSG_EVENT 7
/*
  string nev
  int phase
*/

#define SERVERMSG_1V1 8
/*
  string kihivo neve
  int limit
*/

#define SERVERMSG_MEDAL 9
/*
  string medalid
*/

class StickmanServer: public TBufferedServer<TStickContext>{
protected:
	const TSimpleLang lang;
	TUDPSocket udp;

	map<string,TStickRecord> db;
	map<string,TKill> killdb;

	map<string,string> bans;

	map<unsigned short,string> medalnevek;


	vector<T1v1Game> challenges;

	unsigned long long lastUID;
	unsigned long long lastUDB;
	unsigned long long nextevent;

	time_t lastUDBsuccess;
	
	unsigned long long lastweather;
	int weathermost;
	int weathercel;
	unsigned long long laststatusfile;
	bool disablekill;

	struct tm timer;
	bool timer_active;


	TMySocket* getSocketByName(const string nev)
	{
		for (unsigned i=0;i<socketek.size();i++)
			if (socketek[i]->context.nev==nev) return socketek[i];
		return 0;
	}

	virtual void OnConnect(TMySocket& sock)
	{
		
		sock.context.loggedin=false;
		sock.context.registered=false;
		sock.context.verified=false;
 		sock.context.UID=++lastUID;
		for(int i=0;i<20;++i)
			sock.context.crypto[i]=(unsigned char)rand();
		sock.context.lastrecv=GetTickCount64();
		sock.context.lastsend=0;
		sock.context.lastudpquery=0; //utolsó kérés hogy kliens küldjön UDP-t
		sock.context.gotudp=false; //ennyit várunk a következõ kérésig
		sock.context.kills=0;
		sock.context.ip=sock.address;
		sock.context.floodtime=0;
		sock.context.udpauth=rand();
		sock.context.nyelv=0;
		sock.context.readyFor1v1Games = false;
	}

	virtual void OnBeforeDelete(TMySocket& sock)
	{
		cout << sock.context.nev << " lecsatlakozott \n";
		AddToChatlog(sock.context.nev + " lecsatlakozott \n");

		TMySocket *so;
		for (unsigned i=0;i<challenges.size();i++)
		{
			if ((getSocketByName(challenges[i].ellenfelNev)==&sock) && challenges[i].allapot==FOLYAMATBAN)
			{
				so = getSocketByName(challenges[i].kihivoNev);
				if (so) 
				{
					so->context.realm="";
					so->context.is1v1=false;
					so->context.realmAdmin = false;
					SendChat(*so,lang(so->context.nyelv,45));
					SendChallenge(*so,"",2);
				}
				if (challenges[i].kihivoPont>=challenges[i].ellenfelPont*1.5 && challenges[i].kihivoPont>=5)
					challenges[i].allapot=VEGE;
				else
				challenges.erase(challenges.begin()+i);
			}
		}

		for (unsigned i=0;i<challenges.size();i++)
		{
			if (getSocketByName(challenges[i].kihivoNev)==&sock && challenges[i].allapot==FOLYAMATBAN)
			{
				so = getSocketByName(challenges[i].ellenfelNev);
				if (so) 
				{
					so->context.realm="";
					so->context.is1v1=false;
					SendChat(*so,lang(so->context.nyelv,45));
					SendChallenge(*so,"",2);
				}
				if (challenges[i].ellenfelPont>=challenges[i].kihivoPont*1.5 && challenges[i].ellenfelPont>=5)
					challenges[i].allapot=VEGE;
				else
				challenges.erase(challenges.begin()+i);
			}
		}

	}
	
	void SendKick(TMySocket& sock,const string& indok="Kicked without reason",bool hard=false)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_KICK);
		frame.WriteChar(hard?1:0);
		frame.WriteString(indok);
		sock.SendFrame(frame,true);
		cout<<"Kicked "<<sock.context.nev<<": "<<indok<<endl;
		AddToChatlog("Kicked "+sock.context.nev+": "+indok);
	}

	void SendMedal(TMySocket& sock,int medalid)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_MEDAL);
		frame.WriteInt(medalid);
		sock.SendFrame(frame);
	}


	void SendPlayerList(TMySocket& sock)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_PLAYERLIST);
		unsigned n=socketek.size();
		unsigned n2=0;
		for(unsigned i=0;i<n;++i)
			if( sock.context.realm   ==socketek[i]->context.realm &&
				sock.context.checksum==socketek[i]->context.checksum)
				++n2;
		frame.WriteInt(n2);
		/*!TODO legközelebbi 50 kiválasztása */
		for(unsigned i=0;i<n;++i)
		if( sock.context.realm   ==socketek[i]->context.realm &&
			sock.context.checksum==socketek[i]->context.checksum &&
			sock.context.verified==socketek[i]->context.verified )
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
			frame.WriteString(scontext.clan);
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
		if (sock.context.registered)
			frame.WriteInt(db[config.ToLowercase(sock.context.nev)].kills);

		else
		frame.WriteInt(0);
		sock.SendFrame(frame);

		
	}

	void AddToChatlog(TMySocket& sock,const string& uzenet)
	{
		time_t t = time(0);   // get time now
		struct tm * now = localtime( & t );
		chatlog.append(itoa(now->tm_year + 1900)); // év
		chatlog.append(". ");
		chatlog.append(itoa(now->tm_mon + 1)); //hónap
		chatlog.append(". ");
		chatlog.append(itoa(now->tm_mday)); //nap
		chatlog.append(" ");
		chatlog.append(itoa(now->tm_hour)); //óra
		chatlog.append(":");
		chatlog.append(itoa(now->tm_min)); //perc
		chatlog.append(" - ");
		chatlog.append(sock.context.nev);
		chatlog.append(": ");
		chatlog.append(uzenet);
		chatlog.append("\r\n");
	}

		void AddToChatlog(const string& uzenet)
	{
		time_t t = time(0);   // get time now
		struct tm * now = localtime( & t );
		chatlog.append(itoa(now->tm_year + 1900)); // év
		chatlog.append(". ");
		chatlog.append(itoa(now->tm_mon + 1)); //hónap
		chatlog.append(". ");
		chatlog.append(itoa(now->tm_mday)); //nap
		chatlog.append(" ");
		chatlog.append(itoa(now->tm_hour)); //óra
		chatlog.append(":");
		chatlog.append(itoa(now->tm_min)); //perc
		chatlog.append(" - ");
		chatlog.append(uzenet);
		chatlog.append("\r\n");
	}

	void SaveChatlog()
	{
		ofstream fil("chat.log",std::ofstream::app);
		fil<<chatlog;
		chatlog="";
		fil.close();
	}

	void SendChat(TMySocket& sock,const string& uzenet,int showglyph=0)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_CHAT);
		frame.WriteChar(0); // this is a normal chat msg
		frame.WriteString(uzenet);
		frame.WriteInt(showglyph);
		sock.SendFrame(frame);
	}

	void SendBigText(TMySocket& sock,const string& uzenet)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_CHAT);
		frame.WriteChar(1); // this is a REDTEXT
		frame.WriteString(uzenet);
		frame.WriteInt(0);
		sock.SendFrame(frame);
	}

	void SendChatToAll(const string& uzenet,int showglyph=0,bool redtext=false)
	{
		int n=socketek.size();
		if (redtext)
			for(int i=0;i<n;++i)
				SendBigText(*socketek[i],uzenet);
		else
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

	void SendUdpQuery(TMySocket& sock)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_SENDUDP);
		frame.WriteInt(sock.context.udpauth);
		sock.SendFrame(frame);
	}

	void SendEvent(TMySocket& sock,const string &nev, int phase)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_EVENT);
		frame.WriteString(nev);
		frame.WriteInt(phase);
		sock.SendFrame(frame);
	}

	void SendChallenge(TMySocket& sock,const string &nev,char started,int limit=0)
	{
		TSocketFrame frame;
		frame.WriteChar(SERVERMSG_1V1);
		frame.WriteString(nev);
		frame.WriteChar(started);
		frame.WriteInt(limit);
		sock.SendFrame(frame);
	}


	void OnMsgLogin(TMySocket& sock,TSocketFrame& msg)
	{
		int verzio=msg.ReadInt();
		int nyelv=sock.context.nyelv=msg.ReadInt();
		
		sock.context.is1v1 = false;
		sock.context.realmAdmin = false;
		
		

		if (nyelv!=14)
			nyelv=0;

		sock.context.nyelv = nyelv;

		if (verzio<config.clientversion )
		{
			SendKick(sock,lang(nyelv,1),true);
			return;
		}
		if (sock.context.loggedin)
		{
			SendKick(sock,lang(nyelv,2),true);
			return;
		}
		string& nev=sock.context.nev=msg.ReadString();
		unsigned n=nev.length();
		if (n==0)
		{
			SendKick(sock,lang(nyelv,3),false);
			return;
		}
		if (n>15)
		{
			SendKick(sock,lang(nyelv,4),false);
			return;
		}
		for(unsigned i=0;i<n;++i)
			if (config.allowedchars.find(nev[i])==string::npos)
			{
				SendKick(sock,lang(nyelv,5),false);
				return;
			}

		bool isadmin = false;
		for (unsigned i = 0; i<config.adminok.size();i++)
			if (nev==config.adminok[i]) 
			{
				isadmin = true;
				break;
			}
		sock.context.admin = isadmin;


		string jelszo=msg.ReadString();

		sock.context.fegyver=msg.ReadInt();
		sock.context.fejrevalo=msg.ReadInt();

		sock.context.port =msg.ReadChar();
		sock.context.port+=msg.ReadChar()<<8;
		sock.context.checksum=msg.ReadInt();
		if (msg.cursor!=msg.datalen) //nem jo a packetmeret
		{
			SendKick(sock,lang(nyelv,6),true);
			return;
		}
		int ip=sock.context.ip;
		cout<<"Login "<<sock.context.nev<<" from "<<((ip)&0xff)<<"."<<((ip>>8)&0xff)<<"."<<((ip>>16)&0xff)<<"."<<((ip>>24)&0xff)<<endl;
		AddToChatlog("Login "+sock.context.nev+" from "+itoa((ip)&0xff)+"."+itoa((ip>>8)&0xff)+"."+itoa((ip>>16)&0xff)+"."+itoa((ip>>24)&0xff));
		if(bans.count(config.ToLowercase(nev)))
		{
				SendKick(sock,lang(nyelv,34)+bans[config.ToLowercase(nev)],false);
				return;
		}

		if(bans.count(itoa(ip)))
		{
				SendKick(sock,lang(nyelv,34)+bans[itoa(ip)],false);
				return;
		}

#ifdef WHITELIST
		string pass;
		if (wlist.contains(config.ToLowercase(nev),&pass))//whitelisten levõ player
		{
			if (pass!=jelszo)
			{
				SendKick(sock,lang(nyelv,7),false);
				return;
			}

#else
		if (db.count(config.ToLowercase(nev)))//regisztrált player
		{
			TStickRecord& record=db[config.ToLowercase(nev)];
			if (record.jelszo!=jelszo)
			{
				SendKick(sock,lang(nyelv,7),false);
				return;
			}
#endif
			// anti multi
			for (unsigned i = 0; i<socketek.size();i++)
				if (config.ToLowercase(socketek[i]->context.nev)==config.ToLowercase(nev) && socketek[i]->context.UID!=sock.context.UID)
			{
				SendKick(sock,lang(nyelv,37),false);
				return;
			}


			#ifndef WHITELIST
			sock.context.clan=record.clan;
			#endif
			sock.context.registered=true;

			const string nev_lower=config.ToLowercase(sock.context.nev);


			#ifndef WHITELIST
			if (killdb.count(nev_lower))
			{
				if (sock.context.fegyver==record.fegyv)
					record.kills=killdb[nev_lower][fegyvtoint(sock.context.fegyver)];

				record.fegyv = fegyvtoint(sock.context.fegyver);
			}
			#endif


			SendLoginOk(sock);
			string chatuzi="\x11\x01"+lang(nyelv,8)+"\x11\x03"+nev+"\x11\x01"+lang(nyelv,9);
			/*if(record.level)
				chatuzi=chatuzi+lang(nyelv,10)+itoa(record.level)+lang(nyelv,11);*/  // olvasatlan levelek, inaktív
			SendChat(sock,chatuzi);
			

			if (sock.context.checksum!=config.datachecksum)
				SendChat(sock,lang(nyelv,50));
			SendWeather(sock,weathermost);

			//check for 5 members of the same clan

			if (!sock.context.clan.empty())
			{
				int count = 0;
				n = socketek.size();
				for (unsigned a=0;a<n;a++)
					if (socketek[a]->context.clan==sock.context.clan) count++;

				if (count>=5)
				{
					for (unsigned a=0;a<n;a++)
						if (socketek[a]->context.clan==sock.context.clan) AddMedal(*socketek[a],'T'|('M'<<8));

				}
			}




		}
		else
		if (jelszo.length()==0)
		{
			SendLoginOk(sock);
			string chatuzi="\x11\x01"+lang(nyelv,12)+"\x11\x03"+nev+"\x11\x01"+lang(nyelv,13);
			SendChat(sock,chatuzi);
			SendWeather(sock,weathermost);
			if (disablekill) SendEvent(sock,"disablekill",0);
		}
		else
		{
			SendKick(sock,lang(nyelv,14),false);
			return;
		}

		sock.context.glyph=0;
		n=nev.length();
		for(unsigned i=0;i<n;++i)
		{
			sock.context.glyph*=982451653;
			sock.context.glyph+=nev[i];
		}

		sock.context.glyph*=756065179;
		sock.context.loggedin=true;
	}

	void OnMsgStatus(TMySocket& sock,TSocketFrame& msg)
	{

		sock.context.verified=IsCryptoValid(sock,msg);

		sock.context.x=msg.ReadInt();
		sock.context.y=msg.ReadInt();

		if (msg.cursor!=msg.datalen) //nem jo a packetmeret
		{
			SendKick(sock,lang(sock.context.nyelv,15),true);
			return;
		}
		sock.context.lastrecv=GetTickCount64();
	}

	bool isSameTeam(TMySocket& sock1,TMySocket& sock2)
	{
		return ((sock1.context.fegyver>=128 && sock2.context.fegyver>=128) ||
				(sock1.context.fegyver<128 && sock2.context.fegyver<128));
	}

	void Enter1v1Games(TMySocket& sock)
	{
		if (!sock.context.registered)
		{
			SendChat(sock,"Most nem játszhatsz 1v1-et, bajnokság folyik és te nem vagy regisztrálva",0);
			return;
		}
			SendChat(sock,"Az automata 1v1 be van kapcsolva, tehát a gép sorsol majd egy játékost.",0);

			sock.context.readyFor1v1Games = true;


			if (sock.context.admin) {
				int pnum = 0;
				int n = socketek.size();
				string names;

				SendChat(sock,"Várakozó emberek:"+pnum,0);

				for (int a=0; a<n;a++)
					if (socketek[a]->context.readyFor1v1Games)
						{
							SendChat(sock,socketek[a]->context.nev,0);
							pnum++;
						}

				SendChat(sock,"összesen"+itoa(pnum)+"játékos",0);
			}

		//okké akkor keressünk valakit.
		int n = socketek.size();
		for (int a=0; a<n;a++)
		{
			TMySocket* ellen = socketek[a];

			if (ellen->context.readyFor1v1Games && !isSameTeam(sock,*ellen))
			{
				//oké akkor még megnézzük, hogy játszottak e már.
				int gn = played1v1games.size();
				bool ok = true;
				for (int b=0;b<gn;b++)
				{
					
					if (played1v1games[b].player1==sock.context.nev && played1v1games[b].player2 == ellen->context.nev)
					{
						ok = false;
						break;
					}
				}

				if (ok) 
				{
					//mehet az 1v1
					SendChallenge(*ellen,sock.context.nev,true);
					SendChallenge(sock,ellen->context.nev,true);

					sock.context.realm=ellen->context.nev+"_vs_"+sock.context.nev;
					ellen->context.realm = ellen->context.nev+"_vs_"+sock.context.nev;

					SendChat(sock,lang(sock.context.nyelv,42)+itoa(5)+lang(sock.context.nyelv,40));
					SendChat(*ellen,lang(socketek[a]->context.nyelv,42)+itoa(5)+lang(socketek[a]->context.nyelv,40));

					sock.	context.kihivo=false;
					ellen->	context.kihivo=true;
					sock.	context.is1v1 = true;
					ellen->	context.is1v1 = true;
					ellen->	context.kills=0;
					sock.	context.kills=0;
					sock.	context.realmAdmin = false;
					ellen->	context.realmAdmin = false;
					sock.	context.readyFor1v1Games = false;
					ellen->	context.readyFor1v1Games = false;

					break;
				}
			}
		}

	}



	void ChatCommand(TMySocket& sock,const string& command,const string& parameter)
	{
		/* user commandok */
		//cout << sock.context.nev << " - " << command << " "<< parameter << endl;
		AddToChatlog(sock,"/" + command + " " + parameter);

		if (command=="realm" && parameter.size()>0 && sock.context.is1v1==false)
		{
			sock.context.realm=parameter;
			SendChat(sock,lang(sock.context.nyelv,16)+parameter+lang(sock.context.nyelv,17));
		}else
			if ((command=="norealm" || (command=="realm" && parameter.size()==0)) && !sock.context.is1v1)
		{
			sock.context.realm="";
			SendChat(sock,lang(sock.context.nyelv,18));

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
		}else
		if (command=="1v1")
		{
			if (feature1v1gamesActive) Enter1v1Games(sock);
			else {
				string kinek;
				int limit = 0;
				size_t pos=0;
				int n=socketek.size();
				pos=parameter.find(string(" "));
				if (pos!=string::npos)
				{
					kinek = parameter.substr(0,pos);
					limit = atoi(parameter.substr(pos+1).c_str());
					limit = min(50,max(5,limit));
				}
				else
				{
					kinek = parameter; 
					limit=10;
				}
				bool talalt=false;
				for(int i=0;i<n;++i)
				{
					string& nev=socketek[i]->context.nev;
					if (nev==kinek && nev!=sock.context.nev)
					{
						if ((sock.context.fegyver<127 && socketek[i]->context.fegyver>127) ||
							 (sock.context.fegyver>127 && socketek[i]->context.fegyver<127) )
						{
							if (!socketek[i]->context.is1v1){

							for (unsigned c=0;c<challenges.size();c++)
							{
								if ( challenges[c].ellenfelNev==kinek && challenges[c].allapot==VARAKOZAS)
									challenges.erase(challenges.begin()+c);
							}

							T1v1Game chall;
							chall.limit = limit;
							chall.kihivoNev = sock.context.nev;
							chall.ellenfelNev = socketek[i]->context.nev;
							chall.ellenfelPont = chall.kihivoPont = 0;
							chall.allapot=VARAKOZAS;
							challenges.push_back(chall);
							SendChallenge(*socketek[i],sock.context.nev,false,limit);
							SendChat(sock, lang(sock.context.nyelv,38)+socketek[i]->context.nev+lang(sock.context.nyelv,39)+itoa(limit)+lang(sock.context.nyelv,40));
							socketek[i]->context.lastwhisp=sock.context.UID;
							talalt=true;
							break;
							}
							else SendChat(sock,socketek[i]->context.nev + lang(sock.context.nyelv,49));
						}
						else SendChat(sock, lang(sock.context.nyelv,44));
						break;
					}
				}
				if (!talalt) SendChat(sock, kinek+lang(sock.context.nyelv,41));
			}

		}else
		if (command=="accept")
		{

			for (unsigned i=0;i<challenges.size();i++)
				if (challenges[i].ellenfelNev==sock.context.nev && challenges[i].allapot==VARAKOZAS)
				{
					T1v1Game* chall = &challenges[i];
					chall->allapot=FOLYAMATBAN;

					
					
					unsigned n=socketek.size();
					for(unsigned a=0;a<n;++a)
						if (socketek[a]->context.nev==chall->kihivoNev)
						{
							SendChallenge(*socketek[a],sock.context.nev,true);
							SendChallenge(sock,socketek[a]->context.nev,true);
							sock.context.realm=chall->kihivoNev+"_vs_"+chall->ellenfelNev;
							socketek[a]->context.realm=chall->kihivoNev+"_vs_"+chall->ellenfelNev;
							SendChat(sock,lang(sock.context.nyelv,42)+itoa(chall->limit)+lang(sock.context.nyelv,40));
							SendChat(*socketek[a],lang(socketek[a]->context.nyelv,42)+itoa(chall->limit)+lang(socketek[a]->context.nyelv,40));
							sock.context.kihivo=false;
							socketek[a]->context.kihivo=true;
							sock.context.is1v1 = true;
							socketek[a]->context.is1v1 = true;
							socketek[a]->context.kills=0;
							sock.context.kills=0;
							sock.context.realmAdmin = false;
							socketek[a]->context.realmAdmin = false;
							break;
						}						

				}
		}else
		if (command=="refuse" || command=="decline")
		{
			for (unsigned i=0;i<challenges.size();i++)
				if (challenges[i].ellenfelNev==sock.context.nev && challenges[i].allapot==VARAKOZAS)
				{
					//üzenetek
					TMySocket *kihivo = getSocketByName(challenges[i].kihivoNev);
					if (kihivo)
						SendChat(*kihivo,sock.context.nev+lang(kihivo->context.nyelv,46));
					SendChallenge(sock,"",2);
					//törlés

				}
		}
		else
		if (command=="admin") // adminság realmon
		{
			if (sock.context.realm=="") 
				{
					SendChat(sock,lang(sock.context.nyelv,51));
					return;
				}

			int n = socketek.size(); // van e másik admin
			for (int a=0;a<n;a++)
				if (socketek[a]->context.realmAdmin && socketek[a]->context.realm==sock.context.realm)
				{
					SendChat(sock,lang(sock.context.nyelv,52)+socketek[a]->context.nev);
					return;
				}
				
			// ha 1v1 ezik akk nem.
			if (sock.context.is1v1) return;

			sock.context.realmAdmin = true;
			SendChat(sock,lang(sock.context.nyelv,53));			
		}
		else
		if (command=="zero")
			{
				int n = socketek.size();

				if (sock.context.admin) //adminos zero
				{			

				for(int a=0;a<n;a++)
					if (socketek[a]->context.realm=="")
						socketek[a]->context.kills = 0;
				}
				else
					if (sock.context.realm!="" && sock.context.realmAdmin)
					{
						for(int a=0;a<n;a++)
							if (socketek[a]->context.realm==sock.context.realm)
								socketek[a]->context.kills = 0;
					}

			}
		else
		if (command=="realmkick")
			{
				if (sock.context.realm!="" && sock.context.realmAdmin)
				{
					unsigned n=socketek.size();
					for(unsigned i=0;i<n;++i)
						if (config.ToLowercase(socketek[i]->context.nev)==config.ToLowercase(parameter) && sock.context.realm==socketek[i]->context.realm)
						{
							socketek[i]->context.realm = "";
							SendChat(*socketek[i],lang(socketek[i]->context.nyelv,54));
						}
				}
			}

		/* admin commandok */
			
		if (!sock.context.admin) return;

		if (command=="ann" || command=="announce")
		{
			SendChatToAll(parameter,0,1);
		}
		else
		if (command=="auto1v1")
		{
			feature1v1gamesActive = (parameter == "1");
			SendChat(sock,feature1v1gamesActive?"automatikus 1v1 sorsolás átállítva: bekapcsolva":"automatikus 1v1 sorsolás átállítva: kikapcsolva");

		}
		else	
		if (command=="weather")
		{
			weathermost=weathercel=atoi(parameter.c_str());
			unsigned n=socketek.size();
			for(unsigned i=0;i<n;++i)
				SendWeather(*socketek[i],weathermost);
		}else	
		if (command=="countdown")
		{
			string humantime=parameter.c_str();
			int ora = atoi(humantime.substr(0,2).c_str());
			int perc = atoi(humantime.substr(3,5).c_str());
			

			time_t now = time(NULL);
			timer = *localtime(&now);	

			timer.tm_hour = ora; timer.tm_min = perc; timer.tm_sec = 0;
			

			timer_active = true;

			double diffd = difftime(mktime(&timer),now);
			SendChat(sock,"Countdown started: "+itoa((int) diffd)+"sec",0);

		}else	
		if (command=="kick" || command=="ban")
		{
			unsigned n=socketek.size();
			bool kick=command=="kick";
			int kickuid=atoi(parameter.c_str());
			if (!kickuid)
				for(unsigned i=0;i<n;++i)
					if (config.ToLowercase(socketek[i]->context.nev)==config.ToLowercase(parameter))
						kickuid=socketek[i]->context.UID;

			int pos=parameter.find(' ');
			string uzenet;
			if(pos>0)
				uzenet.assign(parameter.begin()+pos,parameter.end());
			else
				uzenet=lang(sock.context.nyelv,19); //lol lang specifikus default kick
			
			for(unsigned i=0;i<n;++i)
				if (socketek[i]->context.UID==kickuid)
				{
					SendKick(*socketek[i],sock.context.nev+lang(sock.context.nyelv,kick?20:35)+uzenet,true);
					SendChatToAll("\x11\xe0"+sock.context.nev+lang(sock.context.nyelv,kick?21:36)+"\x11\x03"+
								  socketek[i]->context.nev+"\x11\xe0"+lang(sock.context.nyelv,22)+uzenet);
					if (!kick) 
						bans[config.ToLowercase(socketek[i]->context.nev)]=uzenet;
					break;
				}
		}else
		if (command=="banlist" )
		{
			for(map<string,string>::iterator i=bans.begin();i!=bans.end();++i)
				SendChat(sock,i->first+ ": "+i->second);
		}else
		if (command=="unban" )
		{
			if (bans.count(parameter))
			{
				SendChat(sock,"Unbanned "+parameter);
				bans.erase(parameter);
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
		else
		if (command=="ip")
		{
			int n=socketek.size();
			for(int i=0;i<n;++i)
			{
				string& nev=socketek[i]->context.nev;
				if (nev.find(parameter)!=string::npos)
					SendChat(sock,string(inet_ntoa(*(in_addr*)&socketek[i]->address))+" : "+nev);
			}

		}
		else
		if (command=="event")
			StartEvent(parameter);
		else
		if (command=="disablekill")
			{
				StartEvent("disablekill");
				disablekill=true;
			}
		else
		if (command=="enablekill")
			{
				StartEvent("enablekill");
				disablekill=false;
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
			SendKick(sock,lang(sock.context.nyelv,23),true);
			return;
		}

		if (uzenet.length()<=1)
			return;

		//flood ellenorzes
		if (sock.context.floodtime<GetTickCount64()-12000)
			sock.context.floodtime=GetTickCount64()-10000;
		else
			sock.context.floodtime+=2000;
		if (sock.context.floodtime>GetTickCount64())
		{
			SendKick(sock,lang(sock.context.nyelv,24),true);
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
			if (sock.context.admin)
			{
				if (sock.context.fegyver>127)	uzenet="\x11\xe0"+sock.context.nev+"\x11\x4F:\x11\x03 "+uzenet;
				else							uzenet="\x11\xe0"+sock.context.nev+"\x11\xE1:\x11\x03 "+uzenet;
			}
			else
			{
				if (sock.context.fegyver>127)	uzenet="\x11\x01"+sock.context.nev+"\x11\x4F:\x11\x03 "+uzenet;
				else							uzenet="\x11\x01"+sock.context.nev+"\x11\xE1:\x11\x03 "+uzenet;
				
			}
			if (sock.context.clan.length()>0)
			{
				uzenet="\x11\x10"+sock.context.clan+" "+uzenet;
			}

			int n=socketek.size();
			for(int i=0;i<n;++i)
				if( sock.context.realm   ==socketek[i]->context.realm &&
					sock.context.checksum==socketek[i]->context.checksum)
					SendChat(*socketek[i],uzenet,sock.context.glyph);

			AddToChatlog(sock,uzenet);
		}
	}
	
	bool IsCryptoValid(TMySocket& sock,TSocketFrame& msg)
	{
		// Minden kill után seedet xorolunk a titkos kulccsal, és egyet
		// ráhashelünk a jelenlegi crypt értékre.
		// Ez kellõen fos verifikálása a killnek, de ennyivel kell beérni
		// Thx Kirknek a security auditingért

		unsigned char newcrypto[20];
		for(int i=0;i<20;++i)
			newcrypto[i]=sock.context.crypto[i]^config.sharedkey[i];

		SHA1_Hash(newcrypto,20,sock.context.crypto);
		bool retval=true;
		for(int i=0;i<20;++i)
		if (sock.context.crypto[i]!=msg.ReadChar())
			retval=false;

		return retval;
	}

	void AddMedal(TMySocket& sock, int medalid)
	{
		if(db.count(config.ToLowercase(sock.context.nev)))
		{
			TStickRecord &dbrecord=db[config.ToLowercase(sock.context.nev)];

			if (dbrecord.medal.count(medalid)!=0)
				return;
			dbrecord.medal.insert(medalid);

			SendChat(sock,"\x11\x6c"+lang(sock.context.nyelv,47)+"\x11\x03"+medalnevek[medalid]+" \x11\x01"+lang(sock.context.nyelv,48));
			SendMedal(sock,medalid);
		}

	}

	void OnMsgKill(TMySocket& sock,TSocketFrame& msg)
	{
		int UID=msg.ReadInt();

		sock.context.verified=IsCryptoValid(sock,msg);

		

		if (msg.cursor!=msg.datalen) //nem jo a packetmeret
		{
			SendKick(sock,lang(sock.context.nyelv,26),true);
			return;
		}

		if (!sock.context.verified) return;

		unsigned n=socketek.size();
		for(unsigned i=0;i<n;++i)
		if (socketek[i]->context.UID==UID &&
			socketek[i]->context.loggedin)
		{

			if (socketek[i]->context.is1v1)
			{

				T1v1Game* chall = 0;

				for (unsigned a=0; a<challenges.size();a++)
				{
					if ((challenges[a].ellenfelNev==socketek[i]->context.nev || challenges[a].kihivoNev==socketek[i]->context.nev) && challenges[a].allapot==FOLYAMATBAN)
					{
						chall = &challenges[a];
						break;
					}
				}

				if (chall) {
					if (socketek[i]->context.kihivo) 
						chall->kihivoPont++;
					else 
						chall->ellenfelPont++;

					if (chall->kihivoPont==chall->limit || chall->ellenfelPont==chall->limit) 
					{
						//a két ember megkeresése
						TMySocket *nyertes=0,*vesztes=0;
						int winpoint, losepoint;
						if (chall->kihivoPont==chall->limit)
						{
						nyertes = getSocketByName(chall->kihivoNev);	winpoint = chall->ellenfelPont;
						vesztes = getSocketByName(chall->ellenfelNev);		losepoint = chall->kihivoPont;
						}
						else
						{
						nyertes = getSocketByName(chall->ellenfelNev);		winpoint = chall->kihivoPont;
						vesztes = getSocketByName(chall->kihivoNev);	losepoint = chall->ellenfelPont;
						}
						if (nyertes && vesztes) SendChat(*nyertes,lang(nyertes->context.nyelv,43)+nyertes->context.nev+" ("+itoa(winpoint)+":"+itoa(losepoint)+")");
						if (nyertes && vesztes) SendChat(*vesztes,lang(nyertes->context.nyelv,43)+nyertes->context.nev+" ("+itoa(winpoint)+":"+itoa(losepoint)+")");
						if (nyertes) nyertes->context.realm = "";
						if (vesztes) vesztes->context.realm = "";
						if (nyertes)cout << "nyertes:"<< nyertes->context.nev << endl;
						if (vesztes)cout << "vesztes:"<< vesztes->context.nev << endl;
						chall->allapot=VEGE; 
						if (nyertes) nyertes->context.is1v1 = false;
						if (vesztes) vesztes->context.is1v1 = false;
						if (nyertes) SendChallenge(*nyertes,"",2);
						if (vesztes) SendChallenge(*vesztes,"",2);
						cout << "1v1 vége ";
						if (nyertes) cout << nyertes->context.nev <<" nyert " <<" (" << itoa(winpoint) << ":" << itoa(losepoint) << ")";
						if (nyertes && vesztes) AddToChatlog("1v1: "+ nyertes->context.nev + " vs " + vesztes->context.nev + " (" + itoa(winpoint) + ":" + itoa(losepoint) + ")");
						cout << "\n";
					}
				}
			}
			socketek[i]->context.kills+=1;

			if(socketek[i]->context.registered)
			{
				const string nev_lower=config.ToLowercase(socketek[i]->context.nev);
				if (socketek[i]->context.realm.size()==0 && socketek[i]->context.checksum==config.datachecksum)
				{
					if (killdb.count(nev_lower))
						killdb[nev_lower][fegyvtoint(socketek[i]->context.fegyver)]+=1;
					else
					{
						killdb[nev_lower][fegyvtoint(socketek[i]->context.fegyver)]=1;
					}
					db[nev_lower].napikill+=1;
					db[nev_lower].osszkill+=1;
					db[nev_lower].kills+=1;

					const int killcnt[9]={10,30,90,250,750,2000,5000,10000,30000};
					for (int j=0;j<9;++j)
						if (db[nev_lower].osszkill>=killcnt[j])
							AddMedal(*socketek[i], 'K' | (('1'+j)<<8) );
				}
			}

			SendChat(*socketek[i],"\x11\x01"+lang(socketek[i]->context.nyelv,27)+"\x11\x03"+sock.context.nev+"\x11\x01"+lang(socketek[i]->context.nyelv,28));
			SendChat(sock,"\x11\x01"+lang(sock.context.nyelv,29)+"\x11\x03"+socketek[i]->context.nev+" \x11\x01"+lang(sock.context.nyelv,30));
			break;
		}
	}
	
	

	void OnMsgMedal(TMySocket& sock,TSocketFrame& msg)
	{
		int medalid=msg.ReadInt();

		if (!IsCryptoValid(sock,msg))
			return;
		
		if (msg.cursor!=msg.datalen) //nem jo a packetmeret
		{
			SendKick(sock,lang(sock.context.nyelv,26),true);
			return;
		}
		
		if (sock.context.realm=="")
		{
			AddMedal(sock,medalid);
			
		}
	}

	void StartEvent(const string &nev)
	{
		unsigned n=socketek.size();
		for(unsigned i=0;i<n;++i)
			if (socketek[i]->context.loggedin)
			{
				AddMedal(*socketek[i],'S'|('P'<<8));
				SendEvent(*socketek[i],nev,0);
			}
	}
	
	void UpdateDb()
	{
		cout<<"Updating database..."<<endl;
		AddToChatlog("Database update");
		//post kills
		if(killdb.size()>0)
		{
			cout<<"Uploading kills: "<<killdb.size()<<" users."<<endl;
			TBufferedSocket sock("stickman.hu",80);

			string postmsg;
			postmsg.reserve(64*1024);
			for(map<string,TKill>::iterator i=killdb.begin();i!=killdb.end();++i)
			{
				const string& nev=i->first;
				postmsg+=nev+"\r\n";
				for (int a= 0;a<8;a++)
					postmsg+=itoa(i->second[a])+"\r\n";
				for (set<WORD>::iterator j=db[nev].medal.begin();j!=db[nev].medal.end();++j)
				{
					postmsg+=((char)*j);
					postmsg+=((char)(*j>>8));
				}
				postmsg+="\r\n";
			}
			postmsg+="1v1data\r\n";
			for (unsigned a=0;a<challenges.size();a++)
				if (challenges[a].allapot==VEGE)
			{
				postmsg+=challenges[a].kihivoNev+"\r\n";
				postmsg+=challenges[a].ellenfelNev+"\r\n";
				postmsg+=itoa(challenges[a].kihivoPont)+"\r\n";
				postmsg+=itoa(challenges[a].ellenfelPont)+"\r\n";
			}
			for (unsigned a=0;a<challenges.size();a++)
				if (challenges[a].allapot==VEGE)
				{
					challenges[a] = challenges.back();
					challenges.pop_back();
					--a;
				}

			sock.SendLine("POST "+config.webinterfaceup+" HTTP/1.1");
			sock.SendLine("Host: "+config.webinterface);
			sock.SendLine("Connection: close");
			sock.SendLine("Content-Type: application/x-www-form-urlencoded");
			sock.SendLine("Content-Length: "+itoa(postmsg.length()+2));
			sock.SendLine("");
			sock.SendLine(postmsg);
			
			string lin;
			while(!sock.GetError())
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
			while(!sock.GetError())
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
				db[config.ToLowercase(nev)]=ujrec;
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
				SendKick(sock,lang(sock.context.nyelv,31),true);
			else
			switch(type)
			{
				case CLIENTMSG_LOGIN:	OnMsgLogin(sock,recvd); break;
				case CLIENTMSG_STATUS:	OnMsgStatus(sock,recvd); break;
				case CLIENTMSG_CHAT:	OnMsgChat(sock,recvd); break;
				case CLIENTMSG_KILLED:	OnMsgKill(sock,recvd); break;
				case CLIENTMSG_MEDAL:	OnMsgMedal(sock,recvd); break;
				default:
					SendKick(sock,lang(sock.context.nyelv,32),true);
			}
		}

		unsigned long long gtc=GetTickCount64();
		/* 2000-2500 msenként küldünk playerlistát */
		if (sock.context.loggedin &&
			sock.context.lastsend<gtc-2000-(rand()&511))
		{
			SendPlayerList(sock);
			sock.context.lastsend=gtc;
		}
		/* fél másodpercenként kérünk UDP-t ha nem jött még*/
		if (sock.context.loggedin &&
			sock.context.lastudpquery<gtc-500)
		{
			SendUdpQuery(sock);
			sock.context.lastudpquery=gtc;
		}
		/* 10 másodperc tétlenség után kick van. */
		if (sock.context.lastrecv<gtc-10000)
			SendKick(sock,lang(sock.context.nyelv,33),false);

	}


public:

	std::string chatlog;
	bool feature1v1gamesActive;
	std::vector<Finished1v1> played1v1games;


	StickmanServer(int port): TBufferedServer<TStickContext>(port),lang("lang.ini"),
		udp(port),lastUID(1),lastUDB(0),lastUDBsuccess(0),lastweather(0),weathermost(8),weathercel(15),
		laststatusfile(0),disablekill(0),timer_active(0)
	{
		feature1v1gamesActive = false;
		
		ifstream fil("medals.cfg");
		while(1)
		{
			char buffer1[1024];
			fil.getline(buffer1,1024);
			char buffer2[1024];
			fil.getline(buffer2,1024);
			if (fil.eof() || fil.fail())
				break;
			int medalid=buffer1[0] | (buffer1[1]<<8);
			medalnevek[medalid]=string(buffer2);
		}
		nextevent=GetTickCount64()+24*3600*1000;

		chatlog = "";
	}

	void Update()
	{
		TSocketFrame frame;
		DWORD fromip;
		WORD fromport;
		while(udp.Recv(frame,fromip,fromport))
		{
			int n=socketek.size();
			int stck=frame.ReadInt();
			if (stck!=('S'|('T'<<8)|('C'<<16)|('K'<<24)))
				continue;

			int uid=frame.ReadInt();
			int auth=frame.ReadInt();
			for(int i=0;i<n;++i)
				if (!socketek[i]->context.gotudp &&
					 socketek[i]->context.UID==uid && 
					 socketek[i]->context.udpauth==auth)
				{
					socketek[i]->context.port=fromport; //hopp, hazudott a portjáról talán.
					socketek[i]->context.gotudp=true;
				}
		}



		if (lastUDB<GetTickCount64()-300000)//5 percenként.
		{

			UpdateDb();

			SaveChatlog();
			autoMsg.load();

			config.reload();
			cout << "cfg reloaded: version " << config.clientversion << "\n";

			lastUDB=GetTickCount64();

			int n=socketek.size();
			for(int i=0;i<n;++i)
			if (socketek[i]->context.loggedin)
				socketek[i]->context.lastrecv=lastUDB;
		}

		TBufferedServer<TStickContext>::Update();

		if (lastweather<GetTickCount64()-120000)// 2 percenként üzi
		{

			SendChatToAll("\x11\x01"+autoMsg.randomMessage(),false);

			if (weathermost==weathercel)
				weathercel=rand()%23;
			if(weathermost>weathercel)
				--weathermost;
			else
				++weathermost;

			int n=socketek.size();
			for(int i=0;i<n;++i)
				if (socketek[i]->context.loggedin)
					SendWeather(*socketek[i],weathermost);
			lastweather=GetTickCount64();
		}
		if (laststatusfile<GetTickCount64()-60000)//percenként
		{
			ofstream fil("status");
			fil<<socketek.size();
			laststatusfile=GetTickCount64();
		}
		if (nextevent<GetTickCount64())
		{
			StartEvent("spaceship");
			nextevent=GetTickCount64()+12*3600*1000+rand()%(24*3600*1000);
		}
		// timer kiírás
		if (timer_active) 
		{
			time_t now = time(NULL);
			double diffd = difftime(mktime(&timer),now);
			int diff = diffd;

			if ((diff % 60)==0 && diff!=0) SendChatToAll(itoa(diff/60)+" perc van hátra",0,true);
			if (diff==15 || diff==30) SendChatToAll(itoa(diff)+" mp",0,true);
			if (diff<5 && diff!=0) SendChatToAll(itoa(diff),0,true);
			if (diff==0) 
			{
				SendChatToAll("START!",0,true);
				timer_active = false;
			}
		}


	}
};


int main(){
	#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
	#endif
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
