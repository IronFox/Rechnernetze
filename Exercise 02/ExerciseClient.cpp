#include <iostream>
#include <thread>
#include <mutex>
#include <sstream>

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


volatile SOCKET sock = INVALID_SOCKET;


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


/**
	@brief Handle user-input
*/
void Submit(const std::string&input)
{
	char buffer[0x100];
	unsigned char len = (unsigned char)std::min<size_t>(input.length(),255);
	((unsigned char&)buffer[0]) = len;
	memcpy(buffer+1,input.c_str(),len);
	int rs = send(sock,buffer,len+1,0);
	if (rs < 0)
		Die("Failed to send message",false);

}

/**
	@brief Thread main function that handles messages from the server
*/
void NetLoop()
{
	int cnt = 0;
	unsigned char buffer[0x1000], 
				*bufferAt = buffer,
				*const bufferEnd = buffer + sizeof(buffer);
	while (sock != INVALID_SOCKET)
	{
		int rs = recv(sock,(char*)bufferAt,bufferEnd-bufferAt,0);
		if (rs <= 0)
		{
			Die("Connection lost",true);
		}
		else
		{
			bufferAt += rs;	//also received length in bytes
		
			unsigned char length = buffer[0];
			while ((size_t)(bufferAt - buffer) >= (size_t)length + 1)
			{
				PrintLine(std::string((const char*)buffer+1,(const char*)buffer+1+length));
				unsigned char*const readBegin = buffer + length+1;
				for (unsigned char*read = readBegin; read != bufferAt; read++)
					buffer[read - readBegin] = *read;
				bufferAt -= length + 1;
				length = bufferAt > buffer ? buffer[0] : 255;
			}
		}
	}
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

	std::cout << "Enter your Nickname: ";
	char nickname[0x100] = ":";
	std::cin.getline(nickname+1,sizeof(nickname)-1);

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
	hints.ai_protocol = IPPROTO_TCP;
    hints.ai_family = AF_UNSPEC;
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
			if (!connect(sock,result->ai_addr,result->ai_addrlen))	//attempt connnection
			{
				//connected
				PrintLine("Connected to "+ToString(*result));	//yay, it worked
				connected = true;
				break;
			}
			else
			{
				closesocket(sock);		//these aren't the droids we're looking for.
				sock = INVALID_SOCKET;
			}

		}
		
		result = result->ai_next;	//moving on.
	}
	if (!connected)
		Die("Failed to connect to "+surl+":"+sport,false);	//so, yeah, none of them worked.

	Submit(nickname);	//includes leading ':', so that the server knows this is a name, not a message

	netThread = std::thread(NetLoop);	//start read-thread
	
	while (sock != INVALID_SOCKET)
	{
		char c = _getch();	//keep reading characters
		{
			if (c == 3)	//this only works on windows, but ctrl-c is handled better on linux anyway
			{
				Die("Ctrl+C",false);
				break;
			}
			consoleLock.lock();
			if (c == '\n' || c == '\r')							//user pressed enter/return:
			{
				std::string submit = inputBuffer;				//copy buffer to string
				std::cout << '\r';								//move cursor to line beginning
				for (size_t i = 0; i < inputBufferUsage+1; i++)	//overwrite line with as many blanks as there were characters
					std::cout << ' ';
				std::cout << '\r';								//move cursor to line beginning again
				inputBufferUsage = 0;							//reset character pointer
				inputBuffer[0] = 0;								//write terminating zero to first char
				consoleLock.unlock();							//release console lock
			
				Submit(submit);									//process input
			}
			else
			{
				if (c == Backspace)										//user pressed backspace
				{
					if (inputBufferUsage > 0)
					{
						inputBuffer[--inputBufferUsage] = 0;	//decrement character pointer and overwrite with 0
						std::cout << '\r'<<':'<<inputBuffer<<" \r"<<':'<<inputBuffer;
					}
				}
				else
				{
					if (c != -32 && (isalnum(c) || c == ' '))	//ordinary character
					{
						if (inputBufferUsage+1 < maxInputBufferUsage)	//only allow up to 255 characters though
						{
							inputBuffer[inputBufferUsage++] = c;	//write to the end of the buffer
							inputBuffer[inputBufferUsage] = 0;		//and terminate properly
						}
						std::cout << c;		//update console
					}
				}
				consoleLock.unlock();
			}
		}
	}
	#ifdef _WIN32
		WSACleanup();
	#endif
	return 0;
}

