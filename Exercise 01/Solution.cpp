#include <iostream>
#include <thread>
#include <mutex>
#include <sstream>

#define UDP

#ifdef UDP
#define DEFAULT_PORT	12349
#else
#define DEFAULT_PORT	12348
#endif




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


volatile SOCKET sock = INVALID_SOCKET;


/**
	@brief Print line to console

	Mutex-secured line output command that makes sure any pending input is moved appropriately.
*/
void PrintLine(const std::string&line)
{
	std::cout << line << std::endl;

	//consoleLock.lock();								//lock console
	//std::cout << '\r';								//reset cursor
	//for (size_t i = 0; i < inputBufferUsage+1; i++)	//fill input line with blanks
	//	std::cout << ' ';
	//std::cout << '\r'<<line.c_str()<<std::endl;		//reset cursor, print line, issue newline
	//std::cout << ':'<<inputBuffer;						//restore current input buffer
	//consoleLock.unlock();							//release console
}

/**
	@brief Signals that something very bad has happend
	@param msg Error description
	@isClient true, if this error has been issued by the client read-thread, and not the main thread
*/
void Die(const std::string msg, bool isClient)
{
	SOCKET old = sock;
	sock = INVALID_SOCKET;
	if (old)
		closesocket(old);
	if (isClient && netThread.joinable())
		netThread.join();
	std::cerr <<std::endl << msg<<std::endl;
	if (isClient)
		return;
	#ifdef _WIN32
		WSACleanup(); //Clean up Winsock
	#endif
	system("PAUSE");
	exit(-1);
}

bool CanDisplay(char c)
{
	if (c >= 0 && isalnum(c))
		return true;
	const char otherOkay[] = " <>'\"!@#$%^&*()_+=-[]{};:,./?\\~\t\n\r";
	return strchr(otherOkay,c) != NULL;
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
			Die("version mismatch",false);
		}
	#endif


	std::cout << "Enter Server URL: ";
	char url[0x100];
	std::cin.getline(url,sizeof(url));

	char*colon = strchr(url,':');
	std::string surl,sport;
	
	if (colon != NULL)
	{
		*colon = 0;
		sport = colon+1;
	}
	else
	{
		sport = ToString(DEFAULT_PORT);
	}
	surl = url;
	
	PrintLine("Attempting to connect to "+surl);

	ADDRINFOA hints;
	memset(&hints,0,sizeof(hints));
	#ifdef UDP
		hints.ai_protocol = IPPROTO_UDP;
		hints.ai_family = AF_INET;
	#else
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_family = AF_UNSPEC;
	#endif
	//hints.
	ADDRINFOA*result;
	if (getaddrinfo(surl.c_str(),sport.c_str(),&hints,&result) != 0)
	{
		Die("Call to getaddrinfo() failed",false);
	}

	bool connected = false;
	while (result)		//search through all available results until one allows connection. 
						//Chances are we get IPv4 and IPv6 results here but the server will only answer to one of those
	{
		sock = socket(result->ai_family,result->ai_socktype,result->ai_protocol);
		if (sock != INVALID_SOCKET)	//if we can create a socket then we can attempt a connection. It would be rather unusual for this not to work but...
		{
			#ifndef UDP
				if (!connect(sock,result->ai_addr,result->ai_addrlen))	//attempt connnection
			#endif
			{
				//connected
				PrintLine("Connected to "+ToString(*result));	//yay, it worked
				connected = true;
				break;
			}
			#ifndef UDP
				else
				{
					closesocket(sock);		//these aren't the droids we're looking for.
					sock = INVALID_SOCKET;
				}
			#endif

		}
		
		result = result->ai_next;	//moving on.
	}
	if (!connected)
		Die("Failed to connect to "+surl+":"+sport,false);	//so, yeah, none of them worked.


	
	while (sock != INVALID_SOCKET)
	{
		char buffer[0x1000];
		#ifndef UDP
			std::cin.getline(buffer,sizeof(buffer));
			int rs = send(sock,buffer,strlen(buffer),0);
		#else
			int pack[2];
			std::cin.getline(buffer,sizeof(buffer));
			pack[0] = atoi(buffer);
			std::cin.getline(buffer,sizeof(buffer));
			pack[1] = atoi(buffer);
			int rs = sendto(sock,(const char*)pack,sizeof(pack),0,(const sockaddr*)result->ai_addr,result->ai_addrlen);
			
			//std::cin.getline(buffer,sizeof(buffer));
			//int rs = sendto(sock,buffer,strlen(buffer),0,(const sockaddr*)result->ai_addr,result->ai_addrlen);

		#endif
		if (rs <= 0)
			Die("Bad send",false);
		#ifndef UDP
			rs = recv(sock,buffer,sizeof(buffer)-1,0);
		#else
			sockaddr_in from;
			int len = sizeof(from);
			rs = recvfrom(sock,buffer,sizeof(buffer)-1,0,(sockaddr*)&from,&len);
		#endif
		if (rs <= 0)
			Die("Bad receive",false);
		buffer[rs] = 0;
		bool allgood = true;
		for (int i = 0; i < rs; i++)
			if (!CanDisplay(buffer[i]))
				allgood = false;
		if (allgood)
			std::cout<<"response: '"<<buffer<<'\''<<std::endl;
		else
		{
			std::cout << "response:";
			for (int i = 0; i < rs; i++)
				std::cout << ' '<< (unsigned)buffer[i];
			std::cout << std::endl;
		}
	}
	#ifdef _WIN32
		WSACleanup();
	#endif
	return 0;
}

