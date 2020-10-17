//
// Created by Izabela Kinga Nieradko on 14/10/2020.
//

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>

#include <unistd.h>

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG  5          // Allowed length of queue of waiting connections

// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.

// MODIFICATION - We do not need a list of client connection. Only one client connects to us. We however need a list of
//servers connecting
class Servers
{
public:
	int sock;              // socket of server connection
	std::string name;           // Limit length of name of server's user

	Servers(int socket) : sock(socket){}

	~Servers(){}            // Virtual destructor defined for base class
};

// Note: map is not necessarily the most efficient method to use here,
// especially for a server with large numbers of simulataneous connections,
// where performance is also expected to be an issue.
// MODIFICATION - Map allows us to not repeat the same connections many times.
//  Let's try first dict if we find it difficult to use update to LIST
//
// Quite often a simple array can be used as a lookup table,
// (indexed on socket no.) sacrificing memory for speed.

std::map<int, Servers*> servers; // Lookup table for per Client information. MODIFICATION - storing servers info.

// Do we need anothe store for client? Do not think so


int openSocketForClientConnectionAndServerResponses() {
	struct sockaddr_in sk_addr;
	int clientOrServerSock;
	int set = 1;
#ifdef __APPLE__
	if((clientOrServerSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("Failed to open socket");
      return(-1);
   }
#else
	if((clientOrServerSock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
	{
		perror("Failed to open socket");
		return(-1);
	}
#endif
	//SOCK NON BLOCK IS SUPER IMPORTANT IN THIS ASSIGMENT ! Possibly may be needed in all connections established.
	// Turn on SO_REUSEADDR to allow socket to be quickly reused after
	// program exit.

	if(setsockopt(clientOrServerSock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
	{
		perror("Failed to set SO_REUSEADDR:");
	}
	set = 1;
#ifdef __APPLE__
	if(setsockopt(clientOrServerSock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
   {
     perror("Failed to set SOCK_NOBBLOCK");
   }
#endif
	memset(&sk_addr, 0, sizeof(sk_addr));

	sk_addr.sin_family      = AF_INET;
	sk_addr.sin_addr.s_addr = INADDR_ANY;
	sk_addr.sin_port        = htons(4033);

	// INADDR_ANY is used when you don't need to bind a socket to a specific IP.
	// When you use this value as the address when calling bind() ,
	// the socket accepts connections to all the IPs of the machine. - IMPORTANT ! Need to check if this is needed both
	// for client connections or only server connections. Probably both.
	// Bind to socket to listen for connections from clients.
	// BIND IS NECESSARY BOTH FOR CLIENT AND SERVER CONNECTIONS

	if(bind(clientOrServerSock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
	{
		perror("Failed to bind to socket:");
		return(-1);
	}
	else
	{
		return(clientOrServerSock);
	}

}
int openSocketForServers(int portnum) {
	struct sockaddr_in sk_addr;
	int clientOrServerSock;
	int set = 1;
#ifdef __APPLE__
	if((clientOrServerSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("Failed to open socket");
      return(-1);
   }
#else
	if((clientOrServerSock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
	{
		perror("Failed to open socket");
		return(-1);
	}
#endif
	//SOCK NON BLOCK IS SUPER IMPORTANT IN THIS ASSIGMENT ! Possibly may be needed in all connections established.
	// Turn on SO_REUSEADDR to allow socket to be quickly reused after
	// program exit.

	if(setsockopt(clientOrServerSock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
	{
		perror("Failed to set SO_REUSEADDR:");
	}
	set = 1;
#ifdef __APPLE__
	if(setsockopt(clientOrServerSock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
   {
     perror("Failed to set SOCK_NOBBLOCK");
   }
#endif
	memset(&sk_addr, 0, sizeof(sk_addr));

	sk_addr.sin_family      = AF_INET;
	sk_addr.sin_addr.s_addr = INADDR_ANY;
	sk_addr.sin_port        = htons(portnum);

	// INADDR_ANY is used when you don't need to bind a socket to a specific IP.
	// When you use this value as the address when calling bind() ,
	// the socket accepts connections to all the IPs of the machine. - IMPORTANT ! Need to check if this is needed both
	// for client connections or only server connections. Probably both.
	// Bind to socket to listen for connections from clients.
	// BIND IS NECESSARY BOTH FOR CLIENT AND SERVER CONNECTIONS

	if(bind(clientOrServerSock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
	{
		perror("Failed to bind to socket:");
		return(-1);
	}
	else
	{
		return(clientOrServerSock);
	}

}


 //* CODE SIMILAR TO THIS WOULD ALLOW SERVER CONNECTING TO ANOTHER SERVERS. NEED WORK AND POLISHING. IT hangs right now.
int connectToServer() {
	 struct addrinfo hints, *svr;              // Network host entry for server
	 struct sockaddr_in serv_addr;           // Socket address for server
	 int serverSocket;                         // Socket used for server
	 int nwrite;                               // No. bytes written to server
	 char buffer[1025];                        // buffer for writing to server
	 bool finished;
	 int set = 1;                              // Toggle for setsocko

	 hints.ai_family = AF_INET;            // IPv4 only addresses
	 hints.ai_socktype = SOCK_STREAM;

	 memset(&hints, 0, sizeof(hints));

	 if (getaddrinfo("127.0.0.1", "4010", &hints, &svr) != 0) {
		 perror("getaddrinfo failed: ");
		 exit(0);
	 }

	 struct hostent *server;
	 server = gethostbyname("127.0.0.1");

	 bzero((char *) &serv_addr, sizeof(serv_addr));
	 serv_addr.sin_family = AF_INET;
	 bcopy((char *) server->h_addr,
		   (char *) &serv_addr.sin_addr.s_addr,
		   server->h_length);
	 serv_addr.sin_port = htons(4011);

	 serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	// Turn on SO_REUSEADDR to allow socket to be quickly reused after
	// program exit.



	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0) {
		printf("Failed to set SO_REUSEADDR for port %s\n");
		perror("setsockopt failed: ");
	}


	if (connect(serverSocket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		printf("Failed to open socket to server: %s\n");
		perror("Connect failed: ");
	}
	return serverSocket;
}
void listenToServerResponse(int serverSocket,char *buffer){
	int nread;
	while(true)
	{
		memset(buffer,0,sizeof(buffer));
		nread = read(serverSocket,buffer,sizeof(buffer));

		if(nread == 0)
		{
			printf("No messages incoming");
		}
		else{
			printf("%s\n",buffer);
			std::cout << "What is in the buffer " << buffer;
		}
	}
}

/*
// No needed when working with only one client. Has to be modified to accept server sockets and server connections.
void closeClient(int clientSocket, fd_set *openSockets, int *maxfds)
{
	// Remove client from the clients list
	clients.erase(clientSocket);

	// If this client's socket is maxfds then the next lowest
	// one has to be determined. Socket fd's can be reused by the Kernel,
	// so there aren't any nice ways to do this.

	if(*maxfds == clientSocket)
	{
		for(auto const& p : clients)
		{
			*maxfds = std::max(*maxfds, p.second->sock);
		}
	}

	// And remove from the list of open sockets.

	FD_CLR(clientSocket, openSockets);
}
*/

// Process command from client on the server
// MODIFIED - SUPER IMPORTANT BUT HAS TO BE MODIFIED. We do not have to have clients in list but servers.
// But we still need something similar to this to deal with client commands that would be then send to next groups.
void serverMessagesSend(int serverConnectionSocket){
	std::string msg;
	msg = "*QUERYSERVERS,P3_GROUP_44#";
	send(serverConnectionSocket, msg.c_str(), msg.length(),0);
}
//Need to bind to receive back


void clientCommand(int clientSocket,char *buffer)
{
	std::vector<std::string> tokens;
	std::string token;

	// Split command from client into tokens for parsing
	std::stringstream stream(buffer);

	while(stream >> token)
		tokens.push_back(token);

	if((tokens[0].compare("GETMSG") == 0) && (tokens.size() == 2))
	{
		std::string msg;
		msg = "Not working properly yet!";
	}
	else if(tokens[0].compare("LEAVE") == 0)
	{
		// Close the socket, and leave the socket handling
		// code to deal with tidying up clients etc. when
		// select() detects the OS has torn down the connection.

		close(clientSocket);

		//closeClient(clientSocket, openSockets, maxfds);
	}
	else if(tokens[0].compare("QUERYSERVERS") < 0)
	{
		std::cout << "Who is logged on" << std::endl;
		std::string msg;

		msg = "*CONNECTED,P3_GROUP_44,127.0.0.1,4044#";
		// Reducing the msg length by 1 loses the excess "," - which
		// granted is totally cheating.
		send(clientSocket, msg.c_str(), msg.length()-1, 0);

	}
		// This is slightly fragile, since it's relying on the order
		// of evaluation of the if statement.
	else if((tokens[0].compare("MSG") == 0) && (tokens[1].compare("ALL") == 0))
	{
		std::string msg;
		for(auto i = tokens.begin()+2;i != tokens.end();i++)
		{
			msg += *i + " ";
		}

		for(auto const& pair : servers)
		{
			send(pair.second->sock, msg.c_str(), msg.length(),0);
		}
	}
	else if(tokens[0].compare("MSG") == 0)
	{
		for(auto const& pair : servers)
		{
			if(pair.second->name.compare(tokens[1]) == 0)
			{
				std::string msg;
				for(auto i = tokens.begin()+2;i != tokens.end();i++)
				{
					msg += *i + " ";
				}
				send(pair.second->sock, msg.c_str(), msg.length(),0);
			}
		}
	}
	else
	{
		std::cout << "Unknown command from servers:" << buffer << std::endl;
	}

}

int main(int argc, char* argv[])
{
	bool finished;
	int listenSock;                 // Socket for connections to server
	int clientSock;                 // Socket of connecting client
	fd_set openSockets;             // Current open sockets
	fd_set readSockets;             // Socket list for select()
	fd_set exceptSockets;           // Exception socket list
	int maxfds;                     // Passed to select() as max fd in set
	struct sockaddr_in server;
	socklen_t serverLen;
	struct sockaddr_in server1;
	socklen_t serverLen1;
	char buffer[1025];              // buffer for reading from clients

	if(argc != 2)
	{
		printf("Usage: chat_server <ip port>\n");
		exit(0);
	}

	// Setup socket for server to listen to

	//Socket for client connection FOR NOW
	listenSock = openSocketForClientConnectionAndServerResponses();
	int serverConnectionSocker = openSocketForServers(atoi(argv[1]));
	// SOCKET FOR SERVER CL
	int socketServerClient = connectToServer();
	//int serverSock = openServerSocket();
	printf("Listening on port: %d\n", atoi(argv[1]));

	if(listen(listenSock, BACKLOG) < 0)
	{
		printf("Listen failed on port %s\n", "4033");
		exit(0);
	}
	if(listen(serverConnectionSocker, BACKLOG) < 0)
	{
		printf("Listen failed on port %s\n", "4033");
		exit(0);
	}

	else
		// Add listen socket to socket set we are monitoring
	{
		FD_ZERO(&openSockets);
		FD_SET(listenSock, &openSockets);
		maxfds = listenSock;
	}
		// Get modifiable copy of readSockets
		readSockets = exceptSockets = openSockets;
		memset(buffer, 0, sizeof(buffer));
		serverMessagesSend(socketServerClient);
		accept(listenSock,(struct sockaddr *)&server,&serverLen);
		finished = false;
		if(recv(listenSock,buffer,sizeof(buffer),MSG_DONTWAIT) == 0){
		}
		else{
			std::cout << buffer << std::endl;
		}
		while(!finished){
			accept(serverConnectionSocker,(struct sockaddr *)&server1,&serverLen1);
			if(recv(socketServerClient,buffer,sizeof(buffer),MSG_DONTWAIT) == 0){
			}else{
				std::cout << buffer << std::endl;
				clientCommand(socketServerClient,buffer);
			}
		}

}


