#include <iostream>
#include <thread>
#include <mutex>
#include <sstream>
#include <vector>

#define DEFAULT_PORT	12345

#ifdef _WIN32
	#include <WinSock2.h>
	#include <Ws2tcpip.h>
	#pragma comment(lib,"ws2_32.lib")

	#include <conio.h>
	//#include <synchapi.h>
	#include <windows.h>
	
	static const char Backspace = 8;
	inline void sleep(unsigned mseconds)	{Sleep(mseconds);}
#else
	#include <termios.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netdb.h>
	#include <arpa/inet.h>
	#include <string.h>
	
	typedef int		SOCKET;
	static const SOCKET	INVALID_SOCKET = -1;
	typedef addrinfo ADDRINFOA;
	
	static const char Backspace = 127;
	
	inline void closesocket(SOCKET s)	{close(s);}
	//copied from http://cboard.cprogramming.com/faq-board/27714-faq-there-getch-conio-equivalent-linux-unix.html
	int _getch( ) {
	  struct termios oldt,
					 newt;
	  int            ch;
	  tcgetattr( STDIN_FILENO, &oldt );
	  newt = oldt;
	  newt.c_lflag &= ~( ICANON | ECHO );
	  tcsetattr( STDIN_FILENO, TCSANOW, &newt );
	  ch = getchar();
	  tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
	  return ch;
	}
#endif
#undef min


/**
	@brief Converts an integer to a string
*/
std::string ToString(int val)
{
	std::ostringstream s;
	s << val;
	return s.str();
}

/**
	@brief Converts a network address to string
*/
std::string ToString(const ADDRINFOA&addr)
{
	char buffer[0x100];
	return std::string(inet_ntop(addr.ai_family,addr.ai_addr,buffer,sizeof(buffer)));

}

//who'd have thought i'd have to manually overload these operators. std::string can only do +=
	//WTF...
std::string operator+(const char*nativeString, const std::string&str)
{
	std::string rs(nativeString);
	rs += str;
	return rs;
}
std::string operator+(const std::string&str, const char*nativeString)
{
	std::string rs(str);
	rs += nativeString;
	return rs;
}
std::string operator+(const std::string&str0, const std::string&str1)
{
	std::string rs(str0);
	rs += str1;
	return rs;
}
std::string operator+(char c, const std::string&str)
{
	std::string rs;
	rs += c;
	rs += str;
	return rs;
}
std::string operator+(const std::string&str, char c)
{
	std::string rs(str);
	rs += c;
	return rs;
}
//so now std::string can do most + concatenation operations, too.


std::thread netThread;		//!< Net-read operations are outsourced into a separate thread that works independently of the input loop
std::mutex	consoleLock;	//!< Secures console output

char inputBuffer[0x100] = {0};						//!< Input buffer
size_t inputBufferUsage = 0;						//!< number of meaningful characters in the buffer
size_t maxInputBufferUsage = sizeof(inputBuffer)-1;	//!< maximum number of possible meaningful characters. since 0 must be written, -1 is applied




/**
	@brief Print line to console

	Mutex-secured line output command that makes sure any pending input is moved appropriately.
*/
void PrintLine(const std::string&line)
{
	consoleLock.lock();								//lock console
	std::cout << '\r';								//reset cursor
	for (size_t i = 0; i < inputBufferUsage+1; i++)	//fill input line with blanks
		std::cout << ' ';
	std::cout << '\r'<<line.c_str()<<std::endl;		//reset cursor, print line, issue newline
	std::cout << ':'<<inputBuffer;						//restore current input buffer
	consoleLock.unlock();							//release console
}

/**
	@brief Signals that something very bad has happend
	@param msg Error description
	@isClient true, if this error has been issued by the client read-thread, and not the main thread
*/
void Die(const std::string msg)
{
	std::cerr <<std::endl << msg<<std::endl;
	#ifdef _WIN32
		WSACleanup(); //Clean up Winsock
	#endif
	system("PAUSE");
	exit(-1);
}




std::vector<SOCKET>	clients;
std::mutex			clientLock;



void Insert(SOCKET sock)
{
	clientLock.lock();
	clients.push_back(sock);
	clientLock.unlock();
}


bool verbose = true;

/**
	@brief Handle client message
*/
void Broadcast(const std::string&input)
{
	char buffer[0x100];
	unsigned char len = (unsigned char)std::min<size_t>(input.length(),255);
	((unsigned char&)buffer[0]) = len;
	memcpy(buffer+1,input.c_str(),len);

	clientLock.lock();
	std::cout << input << std::endl;
	std::vector<size_t> toErase;
	if (verbose)
		std::cout << "iterating "<<clients.size()<< " clients "<<std::endl;
	for (std::vector<SOCKET>::iterator it = clients.begin(); it != clients.end(); ++it)
	{
		if (verbose)
			std::cout << "sending "<<(int)(len+1)<< " bytes to socket"<<std::endl;
		int rs = send(*it,buffer,len+1,0);
		if (rs <= 0)
		{
			closesocket(*it);
			toErase.push_back(it - clients.begin());
		}
	}

	size_t erased = 0;
	for (auto it = toErase.begin(); it != toErase.end(); ++it)
	{
		clients.erase(clients.begin() + *it -erased);
		erased++;
	}

	clientLock.unlock();
}

struct ClientInfo
{
	SOCKET		sock;
	std::string	initialName;
	std::thread*thread;
};



void GetClientMessage(SOCKET&sock, std::string&outMessage)
{
	char		buffer[0x100], //255 + trailing 0
				*bufferAt = buffer,
				*const bufferEnd = buffer + sizeof(buffer);

	unsigned char len;
	if (verbose)
		std::cout << "fetching length"<<std::endl;
	if (recv(sock,(char*)&len,1,0) != 1)
	{
		closesocket(sock);
		sock = INVALID_SOCKET;
		outMessage.clear();
		return;
	}
	while (len > 0)
	{
		if (verbose)
			std::cout << "fetching "<<(int)len<<" more characters"<< std::endl;
		int rc = recv(sock,bufferAt,len,0);
		if (rc <= 0)
		{
			if (verbose)
				std::cout << "invalid len"<< std::endl;
			closesocket(sock);
			sock = INVALID_SOCKET;
			outMessage.clear();
			return;
		}
		bufferAt += rc;
		len -= rc;
	}
	(*bufferAt) = 0;
	if (verbose)
		std::cout << "all done"<< std::endl;


	outMessage = std::string(buffer);

}

/**
	@brief Thread main function that handles messages from a specific client
*/
void ThreadLoop(ClientInfo info)
{
	int cnt = 0;



	std::string clientName = info.initialName;;
	SOCKET sock = info.sock;

	Broadcast(clientName+" connected");

	std::string message;

	while (sock != INVALID_SOCKET)
	{
		GetClientMessage(sock,message);
		if (message.empty())
		{
			std::cout << "message is empty"<<std::endl;
			continue;
		}
		if (message.at(0) ==':')
		{
			std::string oldName = clientName;
			clientName = message.substr(1);
			Broadcast(oldName+" renamed to "+clientName);
		}
		else
			Broadcast(clientName+": "+message);
	}
	Broadcast(clientName+" disconnected");
	info.thread->detach();
	delete info.thread;
}




int main(int argc, const char* argv[])
{
	#ifdef _WIN32
		//Start up Winsock…
		WSADATA wsadata;

		int error = WSAStartup(0x0202, &wsadata);
		if (error)	//Did something happen?
		{
			std::cerr<<"Unable to start winsock"<<std::endl;
			return -1;
		}

		if (wsadata.wVersion != 0x0202)//Did we get the right Winsock version?
		{
			Die("version mismatch");
		}
	#endif

	SOCKET sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
		Die("unable to create socket");
	
	SOCKADDR_IN addr;
	int addrlen = sizeof(addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(DEFAULT_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (::bind(sock,(SOCKADDR*)&addr, sizeof(addr)) != 0)
	{
		closesocket(sock);
		Die("unable to bind socket");
	}

	if (::listen(sock,5) != 0)
	{
		closesocket(sock);
		Die("unable to listen");
	}

	std::cout << "Server online"<<std::endl;
	
	while (sock != INVALID_SOCKET)
	{
		struct sockaddr_in addr;
		int len = sizeof(addr);
		SOCKET client = accept(sock,(sockaddr*)&addr,&len);
		if (client != INVALID_SOCKET)
		{
			char buffer[0x1000];
			inet_ntop(AF_INET,&addr,buffer,sizeof(buffer));
			Insert(client);
			ClientInfo info;
			info.sock = client;
			info.initialName = buffer;
			info.thread = new std::thread();
			info.thread->swap(std::thread(ThreadLoop,info));
		}
	}
	#ifdef _WIN32
		WSACleanup();
	#endif
	return 0;
}

