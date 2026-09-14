#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <string>
#include <iostream>
#include <unistd.h>

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

    // ....

    
    close(sockfd);

    return 0
}	
