#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <string>
#include <cstring>
#include <iostream>
#include <unistd.h>

// Helper functions to solve the puzzles
// NOT SURE ABOUT THE PARAMETERS!
void solveSecret(int sockfd, sockaddr_in destaddr) {
    // The predefined number

    uint32_t my_int = 21;

    std::string str = "S.E.C.R.E.T.:katrinth25,margretf24,";
    """
    sizeof(my_int) // get byte of int

    str.length() // get bite of string 

    &my_int // memory of int
    """

    const char *cp = (char*)&my_int;

    str.append(cp ,sizeof(my_int));

    int ret;
    
    if ((ret = sendto(sockfd, str.c_str() , str.length(), 0, (struct sockaddr*)&destaddr, sizeof(destaddr) )) < 0)
    {
        perror("Error sending");
        exit(1);
    }
    
    // create a time interval
    struct  timeval tv;
    fd_set readfds;

    tv.tv_sec = 2;
    tv.tv_usec = 500000;

    // Clear the watchlist and put new data into the it
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    char buffer[2048];
    // Store where response came from
    struct sockaddr_in srcaddr;
    socklen_t srcaddrlen = sizeof(srcaddr);

    // see if sockfd has any data, if no data after time out return 0
    int result = select(sockfd + 1, &readfds, NULL, NULL, &tv);

    if (result > 0) {
    if ((ret = recvfrom(sockfd, buffer , sizeof(buffer), 0, 
            (struct sockaddr*)&srcaddr, &srcaddrlen)) < 0){

        perror("Error receiving");

    } else {

        if (ret == 5) {
            uint8_t groupID = buffer[0];

            uint32_t challenge;

            memcpy(&challenge, &buffer[1],sizeof(challenge)); 
            
            uint32_t sigil = challenge ^ my_int;

            char sigilSent[5];

            memcpy(&sigilSent, &groupID, sizeof(groupID));

            memcpy(&sigilSent[1], &sigil, sizeof(sigil));
            
            sendto(sockfd, sigilSent, sizeof(sigilSent), 0, (struct sockaddr*)&destaddr, sizeof(destaddr) )
            
            }
        }
    }
}

void solveEvil(int sockfd, sockaddr_in destaddr) {

}

void solveGuardian(int sockfd, sockaddr_in destaddr) {

}

void solveDragon(int sockfd, sockaddr_in destaddr) {

}

/*
PUZZLE SOLVER
-------------
This program takes in an IP address and four puzzle ports.
It finds which puzzle belongs to which port and then solves each puzzle.
------------- 
*/
int main(int argc, const char* argv[]){

	// Make sure that the function call is correct
	if (argc != 6) {
		std::cerr << "Invalid number of arguments! Need: ./puzzlesolver <IPaddress> <port1> <port2> <port3> <port4>" << std::endl;
		exit(1);
	}

    // Name the arguments received
	const char *ipaddr = argv[1]; 
	int port1 = std::stoi(argv[2]);
	int port2 = std::stoi(argv[3]);
    int port3 = std::stoi(argv[4]);
    int port4 = std::stoi(argv[5]);

    int ports[4] = {port1, port2, port3, port4};    // Array containing all ports

    // Create UDP socket
	int sockfd;
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Error creating socket");
		exit(1);
	}

    // Make destination address
	struct sockaddr_in destaddr;
	destaddr.sin_family = AF_INET;

	if (inet_pton(AF_INET, ipaddr, &destaddr.sin_addr) < 1) {
		std::cerr << "Invalid ip address of address family: " << ipaddr << std::endl;
		exit(1);
	}

    // Loop through the ports to find which puzzle belongs to each port
    for (int i = 0; i < 4; i++) {

        // Set the current port as the desination address
        destaddr.sin_port = htons(ports[i]);
       
        std::string m = "Hello!"; // Message

        // Keep track of whether responses are recieved
        bool receivedResponse = false;

        int bytesSent;      // Number of bytes sendto() sent
        int bytesReceived;  // Number of bytes recvfrom() received
        char buffer[2048];

        // Try sending to port 3 times before giving up 
        for (int attempt = 0; attempt < 3; attempt++) {

            // Send message to current port
            if ((bytesSent = sendto(sockfd, m.c_str(), m.length(), 0, (struct sockaddr*)&destaddr, sizeof(destaddr) )) < 0) {
                perror("Error sending");
                exit(1);
            }

            // Set timeout for receiving a response
			struct  timeval tv;
			fd_set readfds;

			tv.tv_sec = 2;
			tv.tv_usec = 500000;

            // Clear the socket watchlist and add socket to it
			FD_ZERO(&readfds);
			FD_SET(sockfd, &readfds);

            // Wait until data is on the socket or timeout is reached
			int result = select(sockfd + 1, &readfds, NULL, NULL, &tv);

            if (result > 0) {

                // Store where response came from
                struct sockaddr_in srcaddr;
                socklen_t srcaddrlen = sizeof(srcaddr);

                // Recieve the response and store it in the buffer
                if ((bytesReceived = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&srcaddr, &srcaddrlen)) < 0) {
                    perror("Error receiving");
                    exit(1);
                };

                // Make sure response came from the IP and port that was sent to
                if (srcaddr.sin_addr.s_addr == destaddr.sin_addr.s_addr &&
        			srcaddr.sin_port == destaddr.sin_port) {
                        receivedResponse = true;
                        break;
				}        
            }
        }
        
        // Converts response into a string
        std::string response(buffer, bytesReceived);

        // Find which puzzle the port belongs to
        // NOT SURE ABOUT THE PARAMETERS
        if (response.find("Sacred Elder Cipher Relay for Enchanted Transmissions") != std::string::npos) {
            std::cout << ports[i] << " is the S.E.C.R.E.T. port" << std::endl; // JUST FOR DEBUGGING
            solveSecret(sockfd, destaddr);
        }
        else if(response.find("Evil") != std::string::npos) {
            std::cout << ports[i] << " is the Evil port" << std::endl; // JUST FOR DEBUGGING
            solveEvil(sockfd, destaddr);
        }
        else if(response.find("guardian") != std::string::npos) {
            std::cout << ports[i] << " is the Guardian of the secret spell port" << std::endl; // JUST FOR DEBUGGING
            solveGuardian(sockfd, destaddr); 
        }
        else if (response.find("D.R.A.G.O.N") != std::string::npos) {
            std::cout << ports[i] << " is the D.R.A.G.O.N. port" << std::endl; // JUST FOR DEBUGGING
            solveDragon(sockfd, destaddr);
        }
    }

    close(sockfd);
    return 0;     

}

    
   