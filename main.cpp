#include <winsock.h>
#include <iostream>
#include "socket_stuff.h"
#include "windows.h"
#include <string>


using namespace std;

#define APP_PORT 25252

struct TStickContext{
	string nev;
	int UID;
	bool loggedin;
};

class StickmanServer: public TBufferedServer<TStickContext>{
public:
	StickmanServer(int port): TBufferedServer(port){}

	virtual void OnConnect(TMySocket& sock)
	{
		sock.context.loggedin=false;
 		sock.context.UID=rand();
	}

	virtual void OnUpdate(TMySocket& sock)
	{
		TSocketFrame recvd;
		while(sock.RecvFrame(recvd))
		{
			cout<<recvd.ReadChar()<<endl;
			cout<<recvd.ReadInt()<<endl;
			cout<<recvd.ReadString();

			TSocketFrame sendthis;
			sendthis.WriteChar('A');
			sendthis.WriteInt(-313370000);
			sendthis.WriteString("Udvozollek foldi halando, beleptel a csodaszigetre.");
			sock.closeaftersend=true;
			sock.SendFrame(sendthis);
		}
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