//
// Simple chat client for TSAM-409
//
// Command line: ./chat_client 4000 
//
// Author: Jacky Mallett (jacky@ru.is)
// Modified by Izabela Kinga Nieradko
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
#include <thread>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>
#include <ctime>
#include <chrono>
#include <ctime>
#include <limits>


void timestamp() {
	auto timestamp = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(timestamp);
	std::string timestampString = std::ctime(&time);
	std::cout << timestampString << std::endl;
}

void listenServer(int serverSocket)
{
    int nread;                                  // Bytes read from socket
    char buffer[1025];                          // Buffer for reading input

    while(true)
    {
       memset(buffer, 0, sizeof(buffer));
       nread = read(serverSocket, buffer, sizeof(buffer));

       if(nread == 0)                      // Server has dropped us
       {
          printf("Over and Out\n");
          exit(0);
       }
       else if(nread > 0)
       {
       	  timestamp();
          std::cout << buffer << std::endl;
       }
    }
}

int main(int argc, char* argv[])
{
   struct addrinfo hints, *svr;              // Network host entry for server
   struct sockaddr_in serv_addr;           // Socket address for server
   int serverSocket;                         // Socket used for server 
   int nwrite;                               // No. bytes written to server
   char buffer[1025];                        // buffer for writing to server
   bool finished;                   
   int set = 1;                              // Toggle for setsockopt



   if(argc != 3)
   {
        printf("Usage: chat_client <ip  port>\n");
        printf("Ctrl-C to terminate\n");
        exit(0);
   }

   hints.ai_family   = AF_INET;            // IPv4 only addresses
   hints.ai_socktype = SOCK_STREAM;

   memset(&hints,   0, sizeof(hints));

   if(getaddrinfo(argv[1], argv[2], &hints, &svr) != 0)
   {
       perror("getaddrinfo failed: ");
       exit(0);
   }

   struct hostent *server;
   server = gethostbyname(argv[1]);

   bzero((char *) &serv_addr, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   bcopy((char *)server->h_addr,
      (char *)&serv_addr.sin_addr.s_addr,
      server->h_length);
   serv_addr.sin_port = htons(atoi(argv[2]));

   serverSocket = socket(AF_INET, SOCK_STREAM, 0);

   // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
   // program exit.

   if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
   {
       printf("Failed to set SO_REUSEADDR for port %s\n", argv[2]);
       perror("setsockopt failed: ");
   }

   
   if(connect(serverSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr) )< 0)
   {
       if(errno != EINPROGRESS)
       {
       	printf("Failed to open socket to server: %s\n", argv[1]);
       	perror("Connect failed: ");
       	exit(0);
       }
   }

    // Listen and print replies from server
   std::thread serverThread(listenServer, serverSocket);

   finished = false;
   int input;
   int groupNumb;
   std::string ipNumber;
   std::string portNumber;
   std::string commandToSend;
   std::string messageToSend;

   while(!finished)
   {

	   std::cout << "Enter number for command: 1 --> GETMSG, 2 --> SENDMSG, 3 --> CONNECT_TO_ANOTHER_SERVER, 4 --> LISTSERVERS , 5 --> LEAVE SERVER \n";
	   std::cin >> input;
	   if (std::cin.fail()) {
		   std::cout << "Only ints accepted! \n";
	   }
	   if(input == 1){
	   		std::cout << "Enter group number you want to get message from \n";
	   		std::cin >> groupNumb;
		   if (std::cin.fail()) {
			   std::cout << "Only ints accepted! \n";
		   }
		   commandToSend = std::string("GETMSG") + std::string(" ") + std::string("GROUP_") + std::to_string(groupNumb);
	   }else if(input == 2){
		   std::cin.clear();
		   std::cout << "Enter group number you want to send the message to \n";
		   std::cin >> groupNumb;
		   if (std::cin.fail()) {
			   std::cout << "Only ints accepted! \n";
		   }
		   std::cin.clear();
		   std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n' );
		   std::cout << "Enter the message to send: \n";
		   getline(std::cin, messageToSend);
		   std::cin.clear();
		   commandToSend = std::string("SENDMSG") + std::string(" ") + std::string("P3_GROUP_") + std::to_string(groupNumb) + std::string(" ") + messageToSend;
		   std::cout << commandToSend << " command to send" << std::endl;
	   }else if(input == 3){
		   std::cout << "Enter the ip number of group you want to connect to: \n";
		   std::cin >> ipNumber;
		   std::cout << "Enter the port number of the group you want to connect to: \n";
		   std::cin >> portNumber;
		   commandToSend = std::string("CONNECT") + std::string(" ") + ipNumber + std::string(" ") + portNumber;

	   }else if(input == 4){
		   std::cout << "Enter the ip number of group you want to disconnect from: \n";
		   std::cin >> ipNumber;
		   std::cout << "Enter the port number of the group you want to disconnect from: \n";
		   std::cin >> portNumber;
		   commandToSend = std::string("LEAVE") + std::string(" ") + ipNumber + std::string(" ") + portNumber;
	   }
	   	else{
	   	commandToSend = "*QUERYSERVERS_PG_44#";
	   }
	   bzero(buffer, sizeof(buffer));
	   timestamp();
       nwrite = send(serverSocket, commandToSend.c_str(), commandToSend.length(),0);
       if(nwrite  == -1)
       {
           perror("send() to server failed: ");
           finished = true;
       }


   }
}
