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
#include <net/if.h>

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

// string message need to be changed to std::vector<std::>
class Messages{
public:
	std::string groupNumberSendTo;
	std::vector<std::string> message;
	std::string groupNumberSendFrom;
	int howManyMessages;
	Messages(std::string groupNumberSendTo) : groupNumberSendTo(groupNumberSendTo){
		message = {};
		groupNumberSendFrom = "";
		howManyMessages = 0;
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
int timeNow() {
	// Returns the time in MS since 01.01.1970
	return (int)time(0);
}
void timestamp() {
	auto timestamp = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(timestamp);
	std::string timestampString = std::ctime(&time);
	std::cout << timestampString << std::endl;
}

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
	struct ifaddrs *myaddrs, *ifa;
	void *in_addr;
	char buf[64];

	if(getifaddrs(&myaddrs) != 0)
	{
		perror("getifaddrs");
		exit(1);
	}

	for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr == NULL)
			continue;
		if (!(ifa->ifa_flags & IFF_UP))
			continue;

		switch (ifa->ifa_addr->sa_family)
		{
			case AF_INET:
			{
				struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;
				in_addr = &s4->sin_addr;
				break;
			}

			case AF_INET6:
			{
				struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ifa->ifa_addr;
				in_addr = &s6->sin6_addr;
				break;
			}

			default:
				continue;
		}

		if (!inet_ntop(ifa->ifa_addr->sa_family, in_addr, buf, sizeof(buf)))
		{
			printf("%s: inet_ntop failed!\n", ifa->ifa_name);
		}
		else
		{
			std::string str(buf, sizeof(buf));
			return str;
		}
	}
	freeifaddrs(myaddrs);
	return "";
}

std::string splitCommands(char* buffer) {
	std::string str = std::string(buffer);
	str.erase(remove(str.begin(), str.end(), '#'), str.end());
	str.erase(remove(str.begin(), str.end(), '*'), str.end());
	str.erase(remove(str.begin(), str.end(), '\n'), str.end());
	return str;
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

void saveMessage(std::string token, std::string msg,bool clientCommand,std::string groupFrom){
	if(clientCommand == true){
		messages[token] = new Messages(token);
		messages[token] -> groupNumberSendTo = token;
		messages[token] -> groupNumberSendFrom = std::string(OURGROUPID);
		messages[token] -> message.push_back(msg);
		messages[token] -> howManyMessages++;
	}else{
		messages[token] = new Messages(token);
		messages[token] -> groupNumberSendTo = token;
		messages[token] -> groupNumberSendFrom = groupFrom;
		messages[token] -> message.push_back(msg);
		messages[token] -> howManyMessages++;
	}
}

void sendMessage(std::vector<std::string> tokens,bool ourClientCommand){
	std::string msg;
	std::string strOurId = std::string(OURGROUPID);
	if(ourClientCommand ==true){
		for(auto i = tokens.begin()+2;i != tokens.end();i++)
		{
			msg += *i + " ";
		}
	}else{
		for(auto i = tokens.begin()+3;i != tokens.end();i++)
		{
			msg += *i + " ";
		}
	}

	unsigned msglen = msg.length();
	msg.resize(msglen-1);
	bool directlyConnect = false;
	int ourClientSocket = 0;
	for(auto const& pair : clients) {
		if(pair.second->name.compare(tokens[1]) == 0) {
			if(ourClientCommand == true){
				std::string toSend = "*SEND_MSG," + tokens[1] + "," + strOurId + "," + msg + "#";
				std::cout << toSend << " :MSG SEND" << std::endl;
				timestamp();
				send(pair.second->sock,toSend.c_str(),toSend.length(),0);
				directlyConnect = true;
			}else{
					std::string toSend = "*SEND_MSG," + tokens[1] + "," + tokens[2] + "," + msg + "#";
					std::cout << toSend << " :MSG SEND" << std::endl;
					timestamp();
					send(pair.second->sock,toSend.c_str(),toSend.length(),0);
					directlyConnect = true;
			}

		}
		if(pair.second->name.compare("OurClient") == 0) {
			ourClientSocket = pair.second->sock;
		}
	}
	if(directlyConnect) {
		if(ourClientCommand == true){
			std::string toSend = "This has been send to directly connected group: *SEND_MSG," + tokens[1] + "," + strOurId + "," + msg + "#";
			send(ourClientSocket,toSend.c_str(),toSend.length(),0);
		}
	}
	else {
		if(ourClientCommand == true){
			saveMessage(tokens[1], msg,ourClientCommand,strOurId);
		}else{
			saveMessage(tokens[1],msg,ourClientCommand,tokens[2]);
		}
		for(auto const& pair : clients) {
			if(pair.second->name.compare(strOurId) != 0 && pair.second->name.compare("OurClient") != 0) {
				if(ourClientCommand == true){
					std::string toSend = "*SEND_MSG," + tokens[1] + "," + strOurId + "," + msg + "#";
					std::cout << toSend << " :MSG SEND" << std::endl;
					timestamp();
					send(pair.second->sock,toSend.c_str(),toSend.length(),0);
					// Let client know is message was forwarded
					toSend = "This has been send forward: *SEND_MSG," + tokens[1] + "," + strOurId + "," + msg + "#";
					send(ourClientSocket,toSend.c_str(),toSend.length(),0);
				}else{
					std::string toSend = "*SEND_MSG," + tokens[1] + "," + tokens[2] + "," + msg + "#";
					std::cout << toSend << " :MSG SEND" << std::endl;
					timestamp();
					send(pair.second->sock,toSend.c_str(),toSend.length(),0);
					// Let client know is message was forwarded
					toSend = "This has been send forward: *SEND_MSG," + tokens[1] + "," + tokens[2] + "," + msg + "#";
					send(ourClientSocket,toSend.c_str(),toSend.length(),0);
				}

			}

		}
	}

}
/*void getMessage(std::vector<std::string> tokens){
	bool inOurMessages = false;
	std::string msg;
	std::
	for(auto const& pair : messages){
		if(pair.second->groupNumber){
			int count = 0;
			for(auto const& message : pair.second->message){
				std::cout << message << std::endl;
				//std::cout << pair.second->group[count] << std::endl;
				// sendMessage
				count++;
			}
			inOurMessages = true;
		}
	}

}
 */

// Process command from client on the server

void clientCommand(int clientSocket, fd_set *openSockets, int *maxfds,
				   char *buffer)
{
	std::vector<std::string> tokens;
	std::string token;
	// Split command from client into tokens for parsing
	std::stringstream stream(buffer);
	while(stream >> token)
		tokens.push_back(token);

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
	else if((tokens[0].compare("GETMSG") == 0) && (tokens.size() == 1))
	{
		if(tokens[1] != OURGROUPID){
			//getMessage(tokens);
		}else{

			std::cout << "Not sure if how that gonna go now lol"<< std::endl;
		}
	}

	else if(tokens[0].compare("SENDMSG") == 0){
		//SENDMSG,GROUP ID,<message contents>
		std::string strOurId = std::string(OURGROUPID);
		std::string msg;
		std::string sendTo = tokens[1];
		bool ourClientCommand = true;
		if(tokens[1] != strOurId){
			sendMessage(tokens,ourClientCommand);
		}else{
			for(auto i = tokens.begin()+2;i != tokens.end();i++)
			{
				msg += *i + " ";
			}
			std::cout << "Our client send us a message!: " << msg << std::endl;
		}


	}
	else if((tokens[0].find("LEAVE") != std::string::npos) && (tokens.size() == 3))
	{
		//LEAVE,SERVER IP,PORT
		for(auto const& pair : clients)
		{
			if((pair.second->ipNum.compare(tokens[1]) == 0)  && (pair.second->portNum == stoi(tokens[2]))){
				closeClient(pair.second->sock, openSockets, maxfds);
			}
		}
	}
	else if(tokens[0].compare("LISTSERVERS") == 0)
	{
		// Close the socket, and leave the socket handling
		// code to deal with tidying up clients etc. when
		// select() detects the OS has torn down the connection.
		// PUT IT IN FUCTION !
		std::string msg;
		msg = "CONNECTED,";

		for(auto const& pair : clients)
		{
			Client *client = pair.second;
			if(client->name != "OurClient") {
				msg += client->name + "," + client->ipNum + "," + std::to_string(client->portNum) + ";";
			}

		}
		send(clientSocket, msg.c_str(), msg.length(), 0);

	}
	else{

		std::vector <std::string> serverCommandParts;
		std::string commands = splitCommands(buffer);
		serverCommandParts = splitServerCommand(commands);

		if(serverCommandParts[0].compare("CONNECTED") == 0)
		{
			std::string port = serverCommandParts[3];
			port.erase(remove(port.begin(), port.end(), ';'), port.end());
			clients[clientSocket]->name = serverCommandParts[1];
			clients[clientSocket]->ipNum = serverCommandParts[2];
			clients[clientSocket]->portNum = stoi(port);

		}
		else if((serverCommandParts[0].find("QUERYSERVERS") != std::string::npos) && (serverCommandParts.size() == 2))
		{
			std::string msg;
			msg = "*CONNECTED,";

			for(auto const& pair : clients)
			{
				Client *client = pair.second;
				if(client->name != "OurClient") {

					msg += client->name + "," + client->ipNum + "," + std::to_string(client->portNum) + ";";
				}

			}
			msg += "#";
			std::cout << msg << " message to servers" << std::endl;
			send(clientSocket, msg.c_str(), msg.length(), 0);

		}

			// This is slightly fragile, since it's relying on the order
			// of evaluation of the if statement.
		else if((serverCommandParts[0].find("SEND_MSG") != std::string::npos) && (serverCommandParts.size() == 4))
		{
			//SEND MSG,<TO GROUP ID>,<FROM GROUP ID>,<Message content>
			std::string strOurId = std::string(OURGROUPID);
			bool ourClientCommand = false;
			std::string msg;
			if(serverCommandParts[1] != strOurId){
				sendMessage(serverCommandParts,ourClientCommand);
			}
			else{
				for(auto i = serverCommandParts.begin()+2;i != serverCommandParts.end();i++)
				{
					msg += *i + " ";
				}
				std::cout << "Message from: " << serverCommandParts[2] << "Message: "<< msg << std::endl;
			}

		}
		else if((serverCommandParts[0].find("LEAVE") != std::string::npos) && (serverCommandParts.size() == 3))
		{
			//LEAVE,SERVER IP,PORT
			for(auto const& pair : clients)
			{
				if((pair.second->ipNum.compare(serverCommandParts[1]) == 0)  && (pair.second->portNum == stoi(serverCommandParts[2]))){
					closeClient(pair.second->sock, openSockets, maxfds);
				}
			}
		}
		else if((serverCommandParts[0].find("STATUSREQ") != std::string::npos) && (serverCommandParts.size() == 2)){
			std::string strOurId = std::string(OURGROUPID);
			std::string msg;
			for (auto const&pair : clients) {
				Client *client = pair.second;
				msg = "*STATUSRESP," + serverCommandParts[1];
				if(messages.empty()){
					msg += "0";
				}
				for(auto const&pair : messages){
					Messages *message = pair.second;
					msg += message->groupNumberSendFrom + "," + message->groupNumberSendTo + "," + std::to_string(message->howManyMessages);
				}
				if(client->name != "OurClient" || client->name != strOurId){
					std::cout << "Do not send to us" << std::endl;
				}else{
					msg += "#";
					send(client->sock, msg.c_str(), msg.length(), 0);
				}
			}
		}
		else
		{
			std::cout << "Unknown command from client:" << buffer << std::endl;
		}
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
	int keepAliveTimer = timeNow();

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
		clients[listenSock] = new Client(listenSock,"P3_GROUP_44");
		clients[listenSock] -> ipNum = ipNumber.c_str();
		clients[listenSock] -> portNum = atoi(argv[1]);
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

		if(clients.size() != 1){
			std::string keepAliveString;
			std::string strOurId = std::string(OURGROUPID);
			if (timeNow() - keepAliveTimer >= 60) {
				for (auto const&pair : clients) {
					Client *client = pair.second;
					keepAliveString = "*KEEPALIVE,";
					if(messages.empty()){
						keepAliveString += "0";
					}
					for(auto const&pair : messages){
						Messages *message = pair.second;
						if(client->name == message->groupNumberSendTo){
							keepAliveString += std::to_string(message->howManyMessages);
						}
					}
					std::cout << strOurId << std::endl;
					if(client->name == "OurClient" || client->name == strOurId){
						std::cout << "Not send to us or client" << std::endl;
					}else{
						keepAliveString += "#";
						send(client->sock, keepAliveString.c_str(), keepAliveString.length(), 0);
						std::cout << "KEEP ALIVE SEND!" << std::endl;
					}
				}
				keepAliveTimer = timeNow();
			}
		}
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
				serverMessagesSend(clientSock);
				printf("accept***\n");
				// Add new client to the list of open sockets
				FD_SET(clientSock, &openSockets);

				// And update the maximum file descriptor
				maxfds = std::max(maxfds, clientSock);


				// create a new client to store information.
				clients[clientSock] = new Client(clientSock," ");

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
							std::cout << buffer << " : Check what is in the buffer" << std::endl;
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
