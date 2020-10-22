//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000
//
// Author: Jacky Mallett (jacky@ru.is)
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
#include <ifaddrs.h>
#include <fstream>
#include <tuple>
#include <netinet/ip_icmp.h>

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

#define BACKLOG  15    // Allowed length of queue of waiting connections
#define CLIENTPORT "4045"
#define OURGROUPID "P3_GROUP_44"
// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.
class Client
{
public:
	int sock;              // socket of client connection
	std::string name;
	int portNum;
	std::string ipNum;  // Limit length of name of client's user

	Client(int socket,std::string groupName) : sock(socket){
		portNum = 0;
		ipNum = "";
		name = groupName;
	}

	~Client(){}            // Virtual destructor defined for base class
};
class Messages{
public:
	std::string groupNumberSendTo;
	std::string message;
	std::string groupNumberSendFrom;
	Messages(std::string groupNumb) : groupNumberSendTo(groupNumb){
		message = "";
		groupNumberSendFrom = "";
	}
	~Messages(){}

};

// Note: map is not necessarily the most efficient method to use here,
// especially for a server with large numbers of simulataneous connections,
// where performance is also expected to be an issue.
//
// Quite often a simple array can be used as a lookup table,
// (indexed on socket no.) sacrificing memory for speed.

std::map<int, Client*> clients; // Lookup table for per Client information
std::map<std::string, Messages*> messages; // Lookup table for messages;

// Open socket for specified port.
//
// Returns -1 if unable to create the socket for any reason.

int open_socket(int portno)
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

// Close a client's connection, remove it from the client list, and
// tidy up select sockets afterwards.

std::string getMyIP(){
	std::string ipAddress = "Unable to get IP address";
	struct ifaddrs *intefaces = NULL;
	struct ifaddrs *temp_addres = NULL;
	int retrive = getifaddrs(&intefaces);
	if (retrive == 0) {
		temp_addres = intefaces;
		while (temp_addres != NULL){
			if(temp_addres->ifa_addr->sa_family == AF_INET){
				if(strcmp(temp_addres->ifa_name,"en0")==0){
					ipAddress=inet_ntoa(((struct sockaddr_in*)temp_addres->ifa_addr)->sin_addr);
				}
			}
			temp_addres = temp_addres->ifa_next;
		}
	}
	freeifaddrs(intefaces);
	return ipAddress;
}


std::string addServTokens(std::string buffer) {
	std::string returnString = "";
	std::string str = buffer;
	std::string beginning = "*";
	std::string ending = "#";

	int position = str.find(ending);
	while (position < str.size()) {
		str.insert(position, ending);
		position = str.find(ending, position + ending.size()*2);
	}

	returnString = beginning + str + ending;
	return returnString;
}

std::vector<std::string> splitCommands(char* buffer) {
	std::string str = std::string(buffer);
	std::string commandLine;
	std::vector<std::string> commandsArr;
	std::stringstream ss(str);
	while(getline(ss, commandLine, '#')) {
		commandLine.erase(0, 1);
		commandsArr.push_back(commandLine);
	}
	return commandsArr;
}
std::vector<std::string> splitServerCommand(std::string command) {
	char delimeter = ',';
	std::stringstream ss(command);
	std::string part;
	std::vector<std::string> commandParts;
	while (getline(ss, part, delimeter))
	{
		commandParts.push_back(part);
	}

	return commandParts;
}

void closeClient(int clientSocket, fd_set *openSockets, int *maxfds)
{

	printf("Client closed connection: %d\n", clientSocket);

	// If this client's socket is maxfds then the next lowest
	// one has to be determined. Socket fd's can be reused by the Kernel,
	// so there aren't any nice ways to do this.

	close(clientSocket);

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
void serverMessagesSend(int serverConnectionSocket){
	std::string msg;
	msg = "*QUERYSERVERS,P3_GROUP_44#";
	send(serverConnectionSocket, msg.c_str(), msg.length(),0);
}

//* CODE SIMILAR TO THIS WOULD ALLOW SERVER CONNECTING TO ANOTHER SERVERS. NEED WORK AND POLISHING. IT hangs right now.
int connectToServer(std::string ip,std::string portnum) {
	struct addrinfo hints, *svr;              // Network host entry for server
	struct sockaddr_in serv_addr;           // Socket address for server
	int serverSocket;                         // Socket used for server
	int set = 1;                              // Toggle for setsocko

	hints.ai_family = AF_INET;            // IPv4 only addresses
	hints.ai_socktype = SOCK_STREAM;

	memset(&hints, 0, sizeof(hints));

	if (getaddrinfo(ip.c_str(), portnum.c_str(), &hints, &svr) != 0) {
		perror("getaddrinfo failed: ");
		exit(0);
	}

	struct hostent *server;
	server = gethostbyname(ip.c_str());
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *) server->h_addr,
		  (char *) &serv_addr.sin_addr.s_addr,
		  server->h_length);
	serv_addr.sin_port = htons(stoi(portnum));

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

// Process command from client on the server

void clientCommand(int clientSocket, fd_set *openSockets, int *maxfds,
				   char *buffer)
{
	std::vector<std::string> tokens;
	std::string token;
	std::vector <std::string> serverCommandParts;
	// Split command from client into tokens for parsing
	std::stringstream stream(buffer);
	while(stream >> token)
		tokens.push_back(token);

	std::vector<std::string> commands = splitCommands(buffer);
	for (auto const& command : commands) {
		serverCommandParts = splitServerCommand(command.c_str());
	}


	if((tokens[0].compare("CONNECT") == 0))
	{
		std::string msg = "*QUERYSERVERS,P3_GROUP_44#";
		int connectSocket = connectToServer(tokens[1],tokens[2]);
		FD_SET(connectSocket,openSockets);
		*maxfds = std::max(*maxfds,connectSocket);
		clients[connectSocket] = new Client(connectSocket," ");
		clients[connectSocket]->portNum = stoi(tokens[2]);
		clients[connectSocket]->ipNum = tokens[1];
		send(connectSocket, msg.c_str(), msg.length(),0);
	}
	else if(tokens[0].compare("LEAVE") == 0)
	{
		// Close the socket, and leave the socket handling
		// code to deal with tidying up clients etc. when
		// select() detects the OS has torn down the connection.

		closeClient(clientSocket, openSockets, maxfds);
	}
	else if(serverCommandParts[0].compare("QUERYSERVERS") == 0)
	{
		std::string msg;
		msg = "*CONNECTED,";

		for(auto const& pair : clients)
		{
			Client *client = pair.second;
			if(client->name != "OurClient") {
				std::cout << client->name << std::endl;
				std::cout << client->ipNum << std::end;
				std::cout << std::to_string(client->portNum) << std::endl;

				msg += client->name + "," + client->ipNum + "," + std::to_string(client->portNum) + ";";
			}

		}
		msg += "#";
		// Reducing the msg length by 1 loses the excess "," - which
		// granted is totally cheating.
		std::cout << msg << " message to instructor" << std::endl;
		send(clientSocket, msg.c_str(), msg.length(), 0);

	}

	else if(serverCommandParts[0].compare("CONNECTED") == 0)
	{
		clients[clientSocket]->name = serverCommandParts[1];
		clients[clientSocket]->ipNum = serverCommandParts[2];
		clients[clientSocket]->portNum = stoi(serverCommandParts[3]);

	}
		// This is slightly fragile, since it's relying on the order
		// of evaluation of the if statement.
	else if((tokens[0].compare("GETMSG,") == 0) && (tokens.size() == 2))
	{
		std::string msg;
		for(auto i = tokens.begin()+2;i != tokens.end();i++)
		{
			msg += *i + " ";
		}

		for(auto const& pair : clients)
		{
			send(pair.second->sock, msg.c_str(), msg.length(),0);
		}
	}
	else if((tokens[0].compare("SENDMSG") == 0) && (tokens.size() == 3)){
		if(tokens[1] != OURGROUPID){
			messages[tokens[1]] = new Messages(tokens[1]);
			messages[tokens[1]] -> message = tokens[2];
		}


	}
	else if(serverCommandParts[0].compare("SEND_MSG") == 0)
	{
		std::string messageToSend = "*SEND_MSG," + serverCommandParts[1] + "," + serverCommandParts[2] + "," + serverCommandParts[3] + "#";
		for(auto const& pair : clients)
		{
			if(pair.second->name.compare(serverCommandParts[2]) == 0)
			{
				if(pair.second->name.compare(OURGROUPID) || pair.second->name.compare("OurClient")){
					std::cout << "From: " << serverCommandParts[2] << "Message: " << serverCommandParts[3] << std::endl;
				}
				send(pair.second->sock, messageToSend.c_str(), messageToSend.length(),0);
			}
			else
				{
				messages[serverCommandParts[1]] = new Messages(serverCommandParts[1]);
				messages[serverCommandParts[1]] -> groupNumberSendFrom = serverCommandParts[2];
				messages[serverCommandParts[1]] -> message = serverCommandParts[3];
			}
		}
	}
	else
	{
		std::cout << "Unknown command from client:" << buffer << std::endl;
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
	struct sockaddr_in client;
	socklen_t clientLen;
	char buffer[1025];              // buffer for reading from clients
	//const char *clientPortNumb = "4045";
	struct timeval time;
	time.tv_sec = 1.0;

	if(argc != 2)
	{
		printf("Usage: chat_server <ip port>\n");
		exit(0);
	}

	// Setup socket for server to listen to

	listenSock = open_socket(atoi(argv[1]));
	int clientConnection = open_socket(atoi(CLIENTPORT));
	std::string ipNumber = getMyIP();

	printf("Listening on port: %d\n", atoi(argv[1]));
	printf("Listening on port: 4045 for our client connection \n");


	if(listen(listenSock, BACKLOG) < 0)
	{
		printf("Listen failed on port %s\n", argv[1]);
		exit(0);
	}
	else
		// Add listen socket to socket set we are monitoring
	{
		FD_ZERO(&openSockets);
		FD_SET(listenSock, &openSockets);
		clients[clientSock] = new Client(listenSock,"P3_GROUP_44");
		clients[clientSock] -> ipNum = ipNumber.c_str();
		clients[clientSock] -> portNum = atoi(argv[1]);
		maxfds = listenSock;
	}
	if(listen(clientConnection,BACKLOG) <0){
		printf("Listen failed on port 4045 \n");
		exit(0);

	}else{
		FD_SET(clientConnection, &openSockets);
		maxfds = std::max(maxfds,clientConnection);
	}

	finished = false;

	while(!finished)
	{
		// Get modifiable copy of readSockets
		readSockets = exceptSockets = openSockets;
		memset(buffer, 0, sizeof(buffer));

		// Look at sockets and see which ones have something to be read()
		int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, &time);

		if(n < 0)
		{
			perror("select failed - closing down\n");
			finished = true;
		}
		else
		{
			// First, accept  any new connections to the server on the listening socket
			if(FD_ISSET(listenSock, &readSockets))
			{
				clientSock = accept(listenSock, (struct sockaddr *)&client,
									&clientLen);

				printf("accept***\n");
				// Add new client to the list of open sockets
				FD_SET(clientSock, &openSockets);

				// And update the maximum file descriptor
				maxfds = std::max(maxfds, clientSock);


				// create a new client to store information.
				clients[clientSock] = new Client(clientSock,"Unknown yet");
				serverMessagesSend(clientSock);

				// Decrement the number of sockets waiting to be dealt with
				n--;

				printf("Client connected on server: %d\n", clientSock);
			}
			if(FD_ISSET(clientConnection,&readSockets)){

				int clientSocketConnection = accept(clientConnection,(struct sockaddr *)&client, &clientLen);
				printf("Client accepted****\n");

				FD_SET(clientSocketConnection,&openSockets);

				maxfds = std::max(maxfds,clientSocketConnection);

				clients[clientSocketConnection] = new Client(clientSocketConnection,"OurClient");
				clients[clientSocketConnection] -> ipNum = ipNumber.c_str();
				clients[clientSocketConnection] -> portNum = atoi(CLIENTPORT);
				n--;

				printf("Our client connected to server: %d\n", clientSocketConnection);
			}

			// Now check for commands from clients
			std::list<Client *> disconnectedClients;
			while(n-- > 0)
			{ 
				for(auto const& pair : clients)
				{
					Client *client = pair.second;
					if(FD_ISSET(client->sock, &readSockets))
					{
						// recv() == 0 means client has closed connection
						if(recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
						{
							disconnectedClients.push_back(client);
							closeClient(client->sock, &openSockets, &maxfds);


						}
							// We don't check for -1 (nothing received) because select()
							// only triggers if there is something on the socket for us.
						else
						{
							std::cout << buffer << std::endl;
							clientCommand(client->sock, &openSockets, &maxfds,buffer);
						}
					}
				}
				// Remove client from the clients list
				for(auto const& c : disconnectedClients)
					clients.erase(c->sock);
			}

		}
	}
}
