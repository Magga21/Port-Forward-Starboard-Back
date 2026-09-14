#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <string>
#include <iostream>
#include <unistd.h>

// Helper functions
// NOT SURE ABOUT THE PARAMETERS!
void solveSecret(int sockfd, sockaddr_in destaddr) {

}

void solveEvil(int sockfd, sockaddr_in destaddr) {

}

void solveGuardian(int sockfd, sockaddr_in destaddr) {

}

void solveDragon(int sockfd, sockaddr_in destaddr) {

}

// Main function
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

    // Array containing all ports from command line
    int ports[4] = {port1, port2, port3, port4};

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

    // Loop through the ports (array) to find each puzzle
    for (int i = 0; i < 4; i++) {
        destaddr.sin_port = htons(ports[i]);

        // Send default message
        // Recieve response
        // Check which puzzle it is
        
        // NOT SURE ABOUT THE PARAMETERS
        // if response contains S.E.C.R.E.T?
        /if () {
            solveSecret(sockfd, destaddr);
        }

        // if response contains Evil?
        else if () {
            solveEvil(sockfd, destaddr);

        }
        // if response contains Guardian?
        else if () {
            solveGuardian(sockfd, destaddr);
        }

        // if response contains D.R.A.G.O.N?
        else if () {
            solveDragon(sockfd, destaddr);
        }

        }

    
    close(sockfd);

    return 0;
}	
