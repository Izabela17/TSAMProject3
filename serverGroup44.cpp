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
#include <list>

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

#define BACKLOG 15          // Allowed length of queue of waiting connections

// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.

// MODIFICATION - We do not need a list of client connection. Only one client connects to us. We however need a list of
//servers connecting

class Server
{
public:
	int sock;              // socket of client connection
	std::string name;           // Limit length of name of client's user

	Server(int socket) : sock(socket){}

	~Server(){}            // Virtual destructor defined for base class
};

// Note: map is not necessarily the most efficient method to use here,
// especially for a server with large numbers of simulataneous connections,
// where performance is also expected to be an issue.
// MODIFICATION - Map allows us to not repeat the same connections many times.
//  Let's try first dict if we find it difficult to use update to LIST
//
// Quite often a simple array can be used as a lookup table,
// (indexed on socket no.) sacrificing memory for speed.

std::map<int, Server*> servers; // Lookup table for per Client information. MODIFICATION - storing servers info.

// Do we need anothe store for client? Do not think so


int openSocket(int portno)
{
	struct sockaddr_in sk_addr;   // address settings for bind()
	int sock;                     // socket opened for this port
	int set = 1;                  // for setsockopt

	// Create socket for connection. Set to be non-blocking, so recv will
	// return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("Failed to open socket");
      return(-1);
   }
#else
	if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
	{
		perror("Failed to open socket");
		return(-1);
	}
#endif

	// Turn on SO_REUSEADDR to allow socket to be quickly reused after
	// program exit.

	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
	{
		perror("Failed to set SO_REUSEADDR:");
	}
	set = 1;
#ifdef __APPLE__
	if(setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
   {
     perror("Failed to set SOCK_NOBBLOCK");
   }
#endif
	memset(&sk_addr, 0, sizeof(sk_addr));

	sk_addr.sin_family      = AF_INET;
	sk_addr.sin_addr.s_addr = INADDR_ANY;
	sk_addr.sin_port        = htons(portno);

	// Bind to socket to listen for connections from clients

	if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
	{
		perror("Failed to bind to socket:");
		return(-1);
	}
	else
	{
		return(sock);
	}
}

 //* CODE SIMILAR TO THIS WOULD ALLOW SERVER CONNECTING TO ANOTHER SERVERS. NEED WORK AND POLISHING. IT hangs right now.
int connectToServer() {
	 struct addrinfo hints, *svr;              // Network host entry for server
	 struct sockaddr_in serv_addr;           // Socket address for server
	 int serverSocket;                         // Socket used for server
	 int set = 1;                              // Toggle for setsocko

	 hints.ai_family = AF_INET;            // IPv4 only addresses
	 hints.ai_socktype = SOCK_STREAM;

	 memset(&hints, 0, sizeof(hints));

	 if (getaddrinfo("127.0.0.1", "4001", &hints, &svr) != 0) {
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
	 serv_addr.sin_port = htons(4001);

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
/*
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
void closeServer(int clientSocket, fd_set *openSockets, int *maxfds)
{

	printf("Client closed connection: %d\n", clientSocket);

	// If this client's socket is maxfds then the next lowest
	// one has to be determined. Socket fd's can be reused by the Kernel,
	// so there aren't any nice ways to do this.

	close(clientSocket);

	if(*maxfds == clientSocket)
	{
		for(auto const& p : servers)
		{
			*maxfds = std::max(*maxfds, p.second->sock);
		}
	}

	// And remove from the list of open sockets.

	FD_CLR(clientSocket, openSockets);

}


void serverCommand(int serverSocket, fd_set *openSockets, int *maxfds,
				   char *buffer)
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

		close(serverSocket);

		//closeClient(clientSocket, openSockets, maxfds);
	}
	else if(tokens[0].compare("QUERYSERVERS") < 0)
	{
		std::cout << "Who is logged on" << std::endl;
		std::string msg;

		msg = "*CONNECTED,P3_GROUP_44,127.0.0.1,4044#";
		// Reducing the msg length by 1 loses the excess "," - which
		// granted is totally cheating.
		send(serverSocket, msg.c_str(), msg.length()-1, 0);

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
	int serverSocketConnections;                 // Socket of connecting servers
	fd_set openSockets;             // Current open sockets 
	fd_set readSockets;             // Socket list for select()        
	fd_set exceptSockets;           // Exception socket list
	int maxfds;                     // Passed to select() as max fd in set
	struct sockaddr_in server;
	socklen_t serverLen;
	char buffer[1025];              // buffer for reading from clients
	const char *clientPortNumb = "4043";


	if(argc != 2)
	{
		printf("Usage: tsampgroup44 <ip port>\n");
		exit(0);
	}

	// Setup socket for server to listen to
	
	
	printf("Listening on port: %d\n", atoi(argv[1]));
	int serverConnections = openSocket(atoi(argv[1]));
	int clientConnections = openSocket(atoi(clientPortNumb));
	int ourServersConnection = connectToServer();

	if(listen(serverConnections, BACKLOG) < 0)
	{
		printf("Listen failed on port %s\n", argv[1]);
		exit(0);
	}
	else
		// Add listen socket to socket set we are monitoring
	{
		FD_ZERO(&openSockets);
		FD_SET(serverConnections, &openSockets);
		maxfds = serverConnections;
	}

	finished = false;

	while(!finished)
	{
		// Get modifiable copy of readSockets
		readSockets = exceptSockets = openSockets;
		memset(buffer, 0, sizeof(buffer));

		// Look at sockets and see which ones have something to be read()
		int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, NULL);

		if(n < 0)
		{
			perror("select failed - closing down\n");
			finished = true;
		}
		else
		{
			// First, accept  any new connections to the server on the listening socket
			if(FD_ISSET(serverConnections, &readSockets))
			{
				serverSocketConnections = accept(serverConnections, (struct sockaddr *)&server,
									&serverLen);
				printf("accept***\n");
				// Add new client to the list of open sockets
				FD_SET(serverSocketConnections, &openSockets);

				// And update the maximum file descriptor
				maxfds = std::max(maxfds, serverSocketConnections) ;

				// create a new client to store information.
				servers[serverSocketConnections] = new Server(serverSocketConnections);

				// Decrement the number of sockets waiting to be dealt with
				n--;

				printf("Server connected on server: %d\n", serverSocketConnections);
			}
			// Now check for commands from clients
			 std::list<Server *> disconnectedServers;
			while(n-- > 0)
			{
				for(auto const& pair : servers)
				{
					Server *server = pair.second;

					if(FD_ISSET(server->sock, &readSockets))
					{
						// recv() == 0 means client has closed connection
						if(recv(server->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
						{
							disconnectedServers.push_back(server);
							closeServer(server->sock, &openSockets, &maxfds);

						}
							// We don't check for -1 (nothing received) because select()
							// only triggers if there is something on the socket for us.
						else
						{
							std::cout << buffer << std::endl;
							serverCommand(server->sock, &openSockets, &maxfds, buffer);
						}
					}
				}
				// Remove client from the clients list
				for(auto const& s : disconnectedServers)
					servers.erase(s->sock);
			}
		}
	}
}



