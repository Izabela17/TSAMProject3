//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000
//
// Author: Jacky Mallett (jacky@ru.is)
//
// Modified by Izabela Kinga Nieradko (izabela17@ru.is) && Aron Úlfarsson (aronu17@ru.is)
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

// Class for handling messages from clients
class Messages{
public:
	std::string groupNumberSendTo;
	std::vector<std::string> message;
	std::vector<std::string> groupNumberSendFrom;
	int howManyMessages;
	Messages(std::string groupNumberSendTo) : groupNumberSendTo(groupNumberSendTo){
		message = {};
		groupNumberSendFrom = {};
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

//timeNow used to calculate KeepAlive timer.
int timeNow() {
	// Returns the time in MS since 01.01.1970
	return (int)time(0);
}
// Function to print timestamp when reciving a message.
void timestamp() {
	auto timestamp = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(timestamp);
	std::string timestampString = std::ctime(&time);
	std::cout << timestampString << std::endl;
}

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

//Function to find my ID number
// Returns empty if unable to fetch it.
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

//Erasing the begging * and # to prepare to split on ,
std::string splitCommands(char* buffer) {
	std::string str = std::string(buffer);
	if ((str.find('#') != std::string::npos) && (str.find('#') != std::string::npos)){
		str.erase(remove(str.begin(), str.end(), '#'), str.end());
		str.erase(remove(str.begin(), str.end(), '*'), str.end());
		str.erase(remove(str.begin(), str.end(), '\n'), str.end());
	}else{
		std::cout << "Badly formatted string!" << std::endl;
	}

	return str;
}
// Splitting the prepared string on , and append it to vector of commands
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
// Close a client's connection, remove it from the client list, and
// tidy up select sockets afterwards.
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
// First message send to any connected client to get their CONNECTED string and extract their group_number,ip and port
void serverMessagesSend(int serverConnectionSocket){
	std::string msg;
	msg = "*QUERYSERVERS,P3_GROUP_44#";
	send(serverConnectionSocket, msg.c_str(), msg.length(),0);
}

// Function to connect to other servers. Each socket allows one connection.
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
		printf("Failed to set SO_REUSEADDR for port \n");
		perror("setsockopt failed: ");
	}

	if (connect(serverSocket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		printf("Failed to open socket to server: \n");
		perror("Connect failed: ");
	}
	return serverSocket;
}
// Saving the messages if the group is not directly connected to our group.
void saveMessage(std::string token, std::string msg,bool clientCommand,std::string groupFrom) {
	int inMap = messages.count(token);
	std::string sender;
	if (clientCommand) {
		sender = OURGROUPID;
	} else {
		sender = groupFrom;
	}
	//If a person to send to already exist in the map we increase their messages count. Otherwise a new class object
	// is appended to the vector.
	if(inMap){
		messages[token] -> groupNumberSendFrom.push_back(sender);
		messages[token] -> message.push_back(msg);
		messages[token] -> howManyMessages += 1;
	}else{
		messages[token] = new Messages(token);
		messages[token] -> groupNumberSendTo = token;
		messages[token] -> groupNumberSendFrom.push_back(sender);
		messages[token] -> message.push_back(msg);
		messages[token] -> howManyMessages += 1;
	}
}
// Send message to handle both SEND MSG from our client and commands from other servers.
void sendMessage(std::vector<std::string> tokens,bool ourClientCommand) {
	std::string msg;
	std::string strOurId = std::string(OURGROUPID);
	// change message tokens into string.
	for (auto i = tokens.begin() + 3; i != tokens.end(); i++) {
		msg += *i + " ";
	}
	//Removing blank character in case it may break somebody code.
	unsigned msglen = msg.length();
	msg.resize(msglen - 1);
	bool directlyConnect = false;
	int ourClientSocket = 0;
	for (auto const &pair : clients) {
		//If we found the name that means that the group is directly connected to us.
		if (pair.second->name.compare(tokens[1]) == 0) {
			std::string toSend = "*SEND_MSG," + tokens[1] + "," + tokens[2] + "," + msg + "#";
			std::cout << toSend << " <---MSG SEND" << std::endl;
			timestamp();
			send(pair.second->sock, toSend.c_str(), toSend.length(), 0);
			directlyConnect = true;
			if (pair.second->name.compare("OurClient") == 0) {
				ourClientSocket = pair.second->sock;
			}
		}
	}
	if (directlyConnect) {
		std::string toSend =
				"This has been send to directly connected group: *SEND_MSG," + tokens[1] + "," + strOurId + "," +
				msg + "#";
		send(ourClientSocket, toSend.c_str(), toSend.length(), 0);
	}else{
		// If the group is not directly connected we save the messages and re-send them forward.
		saveMessage(tokens[1], msg, ourClientCommand, tokens[2]);
		for (auto const &pair : clients) {
			if (pair.second->name.compare(strOurId) != 0 && pair.second->name.compare("OurClient") != 0) {
				std::string toSend = "*SEND_MSG," + tokens[1] + "," + tokens[2] + "," + msg + "#";
				std::cout << toSend << " <---MSG SEND" << std::endl;
				timestamp();
				if (pair.second->name.compare(tokens[2]) != 0) {
					send(pair.second->sock, toSend.c_str(), toSend.length(), 0);
					// Let client know is message was forwarded
					toSend = "This has been send forward: *SEND_MSG," + tokens[1] + "," + tokens[2] + "," + msg +
							 "#";
					send(ourClientSocket, toSend.c_str(), toSend.length(), 0);
				}
			}
		}
	}
}
// Finding the messages for group.
void getMessage(std::vector<std::string> tokens, int socket){
	std::string strOurId = std::string(OURGROUPID);
	bool inOurMessages = false;
	std::string msg;
	for(auto const& pair : messages){
		Messages *message = pair.second;
		//If message was found we send back send msg command as many as
		// there are messages stored for the group
		if(message->groupNumberSendTo.compare(tokens[1]) ==0){
			inOurMessages = true;
			for(int i = 0; i < message->howManyMessages; i++){
				std::cout << "Found message from : " << message->groupNumberSendFrom[i] << " " << message->message[i] << std::endl;
				std::string toSend = "*SEND_MSG," + tokens[1] + "," + message->groupNumberSendFrom[i] + "," + message->message[i] + "#";
				send(socket, toSend.c_str(), toSend.length(), 0);
			}
		}
	}
	if(!inOurMessages){
		// Otherwise we send no messages found.
		std::string toSend = "*SEND_MSG," + tokens[1] + "," + strOurId + "," + "No message found #";
		send(socket, toSend.c_str(), toSend.length(), 0);
	}
}
// Process command from client on the server
// in the first if we deal with all commands from ourClient then we deal with the server commands.
void clientCommand(int clientSocket, fd_set *openSockets, int *maxfds,
				   char *buffer)
{
	std::vector<std::string> tokens;
	std::string token;
	std::stringstream stream(buffer);
	while(stream >> token)
		tokens.push_back(token);
	//Command to connect to another server
	if((tokens[0].compare("CONNECT") == 0))
	{
		std::string msg = "*QUERYSERVERS,P3_GROUP_44#";
		int connectSocket = connectToServer(tokens[1],tokens[2]);
		FD_SET(connectSocket,openSockets);
		*maxfds = std::max(*maxfds,connectSocket);
		// Create a new client instance and keep the name blank until we get back CONNECTED and update it.
		clients[connectSocket] = new Client(connectSocket," ");
		clients[connectSocket]->portNum = stoi(tokens[2]);
		clients[connectSocket]->ipNum = tokens[1];
		send(connectSocket, msg.c_str(), msg.length(),0);
	}
	// Handle GETMSG command send from our client
	else if((tokens[0].compare("GETMSG") == 0) && (tokens.size() == 2))
	{
		if(tokens[1] != OURGROUPID){
			getMessage(tokens,clientSocket);
		}
	}
	// Handle SENDMSG command send from our client.
	else if(tokens[0].compare("SENDMSG") == 0){
		//SENDMSG,GROUP ID,<message contents>
		std::string strOurId = std::string(OURGROUPID);
		std::string msg;
		bool ourClientCommand = true;
		if(tokens[1] != strOurId){
			sendMessage(tokens,ourClientCommand);
		}else{
			for(auto i = tokens.begin()+2;i != tokens.end();i++)
			{
				msg += *i + " ";
			}
			std::cout << " Our client send us a message!: " << msg << std::endl;
		}
	}
	//Close connection with a socket on specified ip number and port number
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
	// We list all connected servers to our client.
	else if(tokens[0].compare("LISTSERVERS") == 0)
	{
		// Close the socket, and leave the socket handling
		// code to deal with tidying up clients etc. when
		// select() detects the OS has torn down the connection.
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
	// Handling commands from servers
	else{
		std::vector <std::string> commandsFromServers;
		std::string commands = splitCommands(buffer);
		commandsFromServers = splitServerCommand(commands);

		if((commandsFromServers[0].compare("CONNECTED") == 0) && (commandsFromServers.size() >= 4))
		{
			//If we got CONNECTED from socket we update the socket with the information from connected query
			std::string port = commandsFromServers[3];
			port.erase(remove(port.begin(), port.end(), ';'), port.end());
			clients[clientSocket]->name = commandsFromServers[1];
			clients[clientSocket]->ipNum = commandsFromServers[2];
			clients[clientSocket]->portNum = stoi(port);

		}
		//Provide server with the CONNECTED query of servers currently connected to me.
		else if((commandsFromServers[0].find("QUERYSERVERS") != std::string::npos) && (commandsFromServers.size() == 2))
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
			std::cout << msg << " <--Connected query to servers!" << std::endl;
			send(clientSocket, msg.c_str(), msg.length(), 0);

		}
		else if((commandsFromServers[0].find("SEND_MSG") != std::string::npos) && (commandsFromServers.size() >= 4))
		{
			//SEND MSG,<TO GROUP ID>,<FROM GROUP ID>,<Message content>
			std::string strOurId = std::string(OURGROUPID);
			bool ourClientCommand = false;
			std::string msg;
			if(commandsFromServers[1] != strOurId){
				sendMessage(commandsFromServers,ourClientCommand);
			}
			else{
				for(auto i = commandsFromServers.begin()+2;i != commandsFromServers.end();i++)
				{
					msg += *i + " ";
				}
				timestamp();
				std::cout << "Message from---> " << commandsFromServers[2] << " Message---> "<< msg << std::endl;
			}

		}
		//Handle GET_MSG commands
		else if((commandsFromServers[0].find("GET_MSG") != std::string::npos) && (commandsFromServers.size() == 2)){
			getMessage(commandsFromServers, clientSocket);
		}
		//Handle LEAVE commands from servers.
		else if((commandsFromServers[0].find("LEAVE") != std::string::npos) && (commandsFromServers.size() == 3))
		{
			//Trying to prevent error if stoi() gets a string that is not a number.
			int value;
			try {
				value = std::stoi(commandsFromServers[2]);
			}
			catch (const std::invalid_argument& ia) {
				std::cout << "Invalid argument: " << ia.what() << std::endl;
			}
			//Look for connection and close it if found.
			for(auto const& pair : clients)
			{
				if((pair.second->ipNum.compare(commandsFromServers[1]) == 0)  && (pair.second->portNum == value)){
					closeClient(pair.second->sock, openSockets, maxfds);
				}
			}
		}
		//Reply for STATUSREQ
		else if((commandsFromServers[0].find("STATUSREQ") != std::string::npos) && (commandsFromServers.size() == 2)){
			std::string strOurId = std::string(OURGROUPID);
			std::string msg;
			for (auto const&pair : clients) {
				Client *client = pair.second;
				msg = "*STATUSRESP," + strOurId + "," + commandsFromServers[1];
				// Check if vector is empty - no messages send 0
				if(messages.empty()){
					msg += "0";
				}
				for(auto const&pair : messages){
					Messages *message = pair.second;
					msg += message->groupNumberSendTo + "," + std::to_string(message->howManyMessages);
				}
				if(client->name != "OurClient" || client->name != strOurId){
					std::cout << "Do not send to us" << std::endl;
				}else{
					msg += "#";
					std::cout << msg << "<---StatusResp to servers" << std::endl;
					send(client->sock, msg.c_str(), msg.length(), 0);
				}
			}
			// Response for KEEPALIVE query
		}else if((commandsFromServers[0].find("KEEPALIVE") != std::string::npos)){
			std::string msg;
			int value;
			try {
				value = std::stoi(commandsFromServers[1]);
			}
			catch (const std::invalid_argument& ia) {
				std::cout << "Invalid argument: " << ia.what() << std::endl;
			}
			// We respond to keepalive only if the keepalive number of messages is bigger than 0
			if(value > 0) {
				for (int i = 0; i < value; i++) {
					msg = "GET_MSG," + clients[clientSocket]->name;
					send(clientSocket, msg.c_str(), msg.length(), 0);
				}
			}
		}
		// If different command send discard it.
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
		//First socket it's our server socket that we are listening on.
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
		//If client connected we add it to open sockets and update the maxfds.
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
		// KeepAlive function. Starts sending only if we have somebody connected.
		if(clients.size() != 1){
			std::string keepAliveString;
			std::string strOurId = std::string(OURGROUPID);
			bool found = false;
			if (timeNow() - keepAliveTimer >= 60) {
				for (auto const&pair : clients) {
					Client *client = pair.second;
					keepAliveString = "*KEEPALIVE,";
					// If messages are empty we send 0 all the time.
					if(messages.empty()){
						keepAliveString += "0";
						found = true;
					}
					for(auto const&pair : messages){
						Messages *message = pair.second;
						//If client found in our message class send the number of messages we have for them.
						if(client->name == message->groupNumberSendTo){
							found = true;
							keepAliveString += std::to_string(message->howManyMessages);
						}
					}
					if(client->name == "OurClient" || client->name == strOurId){
						std::string empty;
					}else{
						//If client was not found in our messages we send 0;
						if(found==false){
							keepAliveString += "0";
						}
						keepAliveString += "#";
						send(client->sock, keepAliveString.c_str(), keepAliveString.length(), 0);
						std::cout << keepAliveString << " <-- KEEP ALIVE SEND!" << std::endl;
					}
				}
				//Restard the timer after message been send
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
			// Accept our client connection and add it to the list of socket we monitor
			if(FD_ISSET(clientConnection,&readSockets)){

				int clientSocketConnection = accept(clientConnection,(struct sockaddr *)&client, &clientLen);
				printf("Client accepted****\n");

				FD_SET(clientSocketConnection,&openSockets);

				maxfds = std::max(maxfds,clientSocketConnection);
				// Store info about our client.
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
							//Check what command we got from the connected clients we are monitoring and deal
							// with it in clientCommand function
							std::cout << buffer << " <--- :Check what is in the buffer" << std::endl;
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
