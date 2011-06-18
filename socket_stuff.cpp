#include "socket_stuff.h"
#include <iostream>
using namespace std;
///////////// TSocketFrame ///////////////

void TSocketFrame::EnlargeBuffer(int mekkorara) //nem sokon múlt, hogy ne buffer, hanem penis legyen
	{
		if (mekkorara<datalen)
			return;

		int tmp=datalen>0?datalen:1;
		while (tmp<mekkorara)
			tmp<<=1;

		unsigned char* ujdata=new unsigned char[tmp];
		if (datalen>0)
			for (int i=0;i<datalen;++i)
				ujdata[i]=data[i];
		if (data)
			delete [] data;
		data=ujdata;
		datalen=tmp;
	}

unsigned char TSocketFrame::ReadChar()
{
	++cursor;
	if (cursor<=datalen)
		return data[cursor-1];
	return 0;
}

unsigned int TSocketFrame::ReadWord()
{
	cursor+=2;
	if (cursor<=datalen)
		return (((unsigned char)data[cursor-1])<<8) |
				((unsigned char)data[cursor-2]);
	return 0;
}

int TSocketFrame::ReadInt()
{
	cursor+=4;
	if (cursor<=datalen)
		return (((unsigned char)data[cursor-1])<<24) |
			   (((unsigned char)data[cursor-2])<<16) |
			   (((unsigned char)data[cursor-3])<<8)  |
			    ((unsigned char)data[cursor-4]);
	return 0;
}

string TSocketFrame::ReadString()
{
	string result;
	char hossz=ReadChar();
	int tmp=cursor+hossz;
	if (tmp<=datalen)
	{
		result.append(&(data[cursor]),&(data[tmp]));
		cursor=tmp;
	}
	return result;
}

void TSocketFrame::WriteChar(unsigned char mit)
{
	EnlargeBuffer(cursor+1);
	data[cursor]=mit;
	cursor+=1;
}

void TSocketFrame::WriteWord(unsigned int mit)
{
	EnlargeBuffer(cursor+2);
	data[cursor  ]=mit;
	data[cursor+1]=mit>>8;
	cursor+=2;
}

void TSocketFrame::WriteInt(int mit)
{
	EnlargeBuffer(cursor+4);
	data[cursor  ]=mit;
	data[cursor+1]=mit>>8;
	data[cursor+2]=mit>>16;
	data[cursor+3]=mit>>24;
	cursor+=4;
}

void TSocketFrame::WriteString(const string& mit)
{
	int len=mit.length();
	if (len>255)
		len=255;
	WriteChar(len);
	EnlargeBuffer(cursor+len);
	for (int i=0; i<len;++i)
		data[cursor+i]=mit[i];
	cursor+=len;
}

//////////// TBufferedSocket //////////////


TBufferedSocket::TBufferedSocket(const string& hostname,int port)
{
	#ifdef _WIN32
	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);
	#endif

	error=0;
	closeaftersend=false;

	hostent* he=gethostbyname(hostname.c_str());
	if (!he)
	{
		#ifdef _WIN32
		error=WSAGetLastError();
		#else
		error=-1337;
		#endif
		return;
	}

	sock=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (!sock)
	{
		#ifdef _WIN32
		error=WSAGetLastError();
		#else
		error=-1337;
		#endif
		return;
	}

	SOCKADDR_IN sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);
	sockaddr.sin_addr.s_addr = *(ULONG*)he->h_addr_list[0];
	
	

	if(connect(sock,(SOCKADDR*)&sockaddr,sizeof(sockaddr)))
	{
		#ifdef _WIN32
		error=WSAGetLastError();
		#else
		error=-1337;
		#endif
	}
}


void TBufferedSocket::Update()
{
 //send
	while(sendbuffer.size()>0 && SelectForWrite(sock))
	{
		char* mit=&(sendbuffer[0]);
		int mennyit=sendbuffer.size();
		if (mennyit>16*1024)
			mennyit=16*1024;
		int mennyilett=send(sock,mit,mennyit,0);

		if (mennyilett==SOCKET_ERROR)
			#ifdef _WIN32
			error=WSAGetLastError();
			#else
			error=-1337;
			#endif
		else
			sendbuffer.erase(sendbuffer.begin(),sendbuffer.begin()+mennyilett);

		if (mennyit!=mennyilett)
			break;
	}

 //recv
	while(SelectForRead(sock))
	{
		char hova[16*1024];//ennek ingyenes az allokálása
		int mennyit=16*1024;

		int mennyilett=recv(sock,(char*)hova,mennyit,0);
		if (mennyilett==SOCKET_ERROR)
			#ifdef _WIN32
			error=WSAGetLastError();
			#else
			error=-1337;
			#endif
		else
		if (mennyilett==0)//connection closed
			error=1;
		else
			recvbuffer.insert(recvbuffer.end(),hova,(hova+mennyilett));

		if (mennyit!=mennyilett)
			break;
	}

	if(SelectForError(sock))
		error=2;//ezt észre fogod venni.
	if (sendbuffer.empty() && closeaftersend)
		error=3;//ezt is. Ez után töröld és csá
}

bool TBufferedSocket::RecvFrame(TSocketFrame& hova)
{
	if (recvbuffer.size()<2)
		return false;
	int siz=((unsigned char)recvbuffer[0]) | ((unsigned char)(recvbuffer[1])<<8);

	if (siz<1) //faszom.
		return false;

	if ((int)recvbuffer.size()<2+siz)
		return false;
	
	if (hova.data)
		delete [] hova.data;

	hova.data=new unsigned char[siz];
	
	for (int i=0;i<siz;++i)
		hova.data[i]=recvbuffer[2+i];
	hova.datalen=siz;
	hova.cursor=0;
	recvbuffer.erase(recvbuffer.begin(),recvbuffer.begin()+2+siz);
	return true;
}

void TBufferedSocket::SendFrame(const TSocketFrame& mit,bool finalframe)
{
	if (mit.cursor<=0) // má megint balfasz voltál
		return;

	if (closeaftersend) //má volt egy final frame
		return;

	if(finalframe)
		closeaftersend=true;

	char egybyte;
	egybyte=mit.cursor;
	sendbuffer.push_back(egybyte);
	egybyte=mit.cursor>>8;
	sendbuffer.push_back(egybyte);

	sendbuffer.insert(sendbuffer.end(),mit.data,mit.data+mit.cursor);
}

bool TBufferedSocket::RecvLine(string& hova)
{
	int n=recvbuffer.size();
	if (n<2)
		return false;
	
	int siz=error?n:-1;

	for (int i=0;i<n-2;++i)
		if (recvbuffer[i]==13 && recvbuffer[i+1]==10)
		{
			siz=i;
			break;
		}
	if (siz<0)
		return false;

	hova.clear();
	hova.append( recvbuffer.begin(),recvbuffer.begin()+siz);

	if (siz+2>n)
		siz=n-2;
	recvbuffer.erase(recvbuffer.begin(),recvbuffer.begin()+siz+2);
	return true;
}

const string TBufferedSocket::RecvLine2()
{
	string hova;
	if (RecvLine(hova))
		return hova;
	else
		return "";
}

void TBufferedSocket::SendLine(const string& mit, bool final)
{
	if (closeaftersend) //má volt egy final line
		return;

	if(final)
		closeaftersend=true;
	
	sendbuffer.insert(sendbuffer.end(),mit.begin(),mit.end());
	sendbuffer.insert(sendbuffer.end(),13);
	sendbuffer.insert(sendbuffer.end(),10);
}

/////// OTHER /////////

int SelectForRead(SOCKET sock)
{
	fd_set csillamvaltozo;
	timeval csodavaltozo={0,0};
	FD_ZERO(&csillamvaltozo);
	FD_SET(sock,&csillamvaltozo);
	return select(sock+1,&csillamvaltozo,0,0,&csodavaltozo);
}

int SelectForWrite(SOCKET sock)
{
	fd_set csillamvaltozo;
	timeval csodavaltozo={0,0};
	FD_ZERO(&csillamvaltozo);
	FD_SET(sock,&csillamvaltozo);
	return select(sock+1,0,&csillamvaltozo,0,&csodavaltozo);
}

int SelectForError(SOCKET sock)
{
	fd_set csillamvaltozo;
	timeval csodavaltozo={0,0};
	FD_ZERO(&csillamvaltozo);
	FD_SET(sock,&csillamvaltozo);
	return select(sock+1,0,0,&csillamvaltozo,&csodavaltozo);
}
