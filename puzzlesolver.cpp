#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <string>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <vector>
#include <cstdint>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <cctype>
#include <algorithm>
#include <array>

/*
--------------------
UDP HELPER FUNCTIONS
--------------------
*/
bool sendMessage(
    int sockfd, 
    sockaddr_in destaddr, 
    const void* data, 
    size_t dataSize
) {    
    int bytesSent = sendto(
        sockfd, 
        data,
        dataSize,
        0,
        (struct sockaddr*)&destaddr,
        sizeof(destaddr)
    );

    if (bytesSent < 0) {
        perror("Error sending");
        return false;
    }

    return true;
}

int receiveMessage(
    int sockfd,
    sockaddr_in destaddr,
    char* buffer,
    int bufferSize
) {
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

    if (result <= 0) { 
        return -1;
    }

    // Store where response came from
    struct sockaddr_in srcaddr;
    socklen_t srcaddrlen = sizeof(srcaddr);

    // Recieve the response and store it in the buffer
    int bytesReceived = recvfrom(
        sockfd, 
        buffer, 
        bufferSize, 
        0, 
        (struct sockaddr*)&srcaddr, 
        &srcaddrlen
    );

    if (bytesReceived < 0) {
        perror("Error receiving");
        return -1;
    }

    // Make sure response came from the IP and port that was sent to
    if (srcaddr.sin_addr.s_addr != destaddr.sin_addr.s_addr || 
        srcaddr.sin_port != destaddr.sin_port) {
        return -1;
	} 

    return bytesReceived;
}

int retryMessage(
    int sockfd,
    sockaddr_in destaddr,
    const void* data,
    size_t dataSize,
    char* buffer,
    int bufferSize
) { 
    // Try sending to port 3 times before giving up 
    for (int attempt = 0; attempt < 3; attempt++) {

        if (!sendMessage(sockfd, destaddr, data, dataSize)) {
            return -1;
        }

        int bytesReceived = receiveMessage(sockfd, destaddr, buffer, bufferSize);

        if (bytesReceived > 0) {
            return bytesReceived;
        }
    }

    return -1;
}

/*
-----------------------
PUZZLE HELPER FUNCTIONS
-----------------------
*/
int extractNumbers(const std::string&message, int startIndex) {
    std::string numberString = "";

    
    for (int i = startIndex; i >= 0; --i) {
        if (std::isdigit(message[i])) {
            numberString += message[i];
        } else {
            // Stop searching once there is a non digit
            break;
        }
    }
    // Reverse back to normal since digits were gathered in reverse order 
    std::reverse(numberString.begin(), numberString.end());
    // Convert string to an integer (returns 0 if no digits found)
    return numberString.empty() ? 0 : std::stoi(numberString);                
}


/*
----------------
PUZZLE FUNCTIONS
----------------
*/
struct SecretResult {
    std::array<char, 5> sigilMessage{};
    int hiddenPort1 = -1;
};

SecretResult solveSecret(int sockfd, sockaddr_in destaddr) {

    SecretResult result;

    uint32_t secretNumber = 21;
    std::string message = "S.E.C.R.E.T.:katrinth25,margretf24,";

    // 
    const char* secretNumberBytes = reinterpret_cast<const char*>(&secretNumber);

    /* 
    Getum líka skrifað  ^ á þennan hátt:
    const char *secretNumberBytes = (const char *)&secretNumber;

    hvað hann er að segja:
    secretNumberBytes should point to the same memory address as secretNumber,
    but treat the data at that address as char bytes.

    */
    
    // 
    message.append(secretNumberBytes ,sizeof(secretNumber));

    char buffer[2048];

    int bytesReceived = retryMessage(sockfd, destaddr, message.c_str(), message.length(),buffer, sizeof(buffer));

    // Debugging
    std::cout << "S.E.C.R.E.T. bytes received: " << bytesReceived << std::endl;

    if (bytesReceived == 5) {

        // First byte of the response is the group ID
        uint8_t groupID = buffer[0];

        uint32_t challengeNumber;

        // Copy the next 4 bytes from the response into challengeNumber
        memcpy(&challengeNumber, &buffer[1], sizeof(challengeNumber));
        
        // XOR the challenge number with the secret number
        uint32_t sigil = challengeNumber ^ secretNumber;

        // Debugging
        std::cout << "Group ID: " << static_cast<int>(groupID) << std::endl;
        std::cout << "Challenge: " << challengeNumber << std::endl;
        std::cout << "Sigil: " << sigil << std::endl;

        // 
        memcpy(&result.sigilMessage[0], &groupID, sizeof(groupID));

        // 
        memcpy(&result.sigilMessage[1], &sigil, sizeof(sigil));

        if (!sendMessage(sockfd, destaddr, result.sigilMessage.data(), result.sigilMessage.size())) {
            return result;
        }

        // Receive hidden secret
        int secretResponse = receiveMessage(sockfd, destaddr, buffer, sizeof(buffer));
        
        if (secretResponse > 0) {
            std::string hiddenSecret(buffer, secretResponse);
            result.hiddenPort1 = extractNumbers(hiddenSecret, hiddenSecret.size() - 2 );
            // Debugging
            std::cout << "Hidden secret: " << hiddenSecret << std::endl;
            std::cout << "Hidden port 1: " << result.hiddenPort1  << std::endl;
        }
    }
    return result;
}

size_t makeEvilPacket(char *packet, sockaddr_in destaddr, sockaddr_in localaddr, const char *payload, size_t payloadSize){

    std::memset(packet, 0, 4096);

    //Pointer to the IP header
    struct iphdr *iph = (struct  iphdr *)packet;
   
    iph->version  = 4; //IPv4 v. 
    iph->ihl      = 5; // Header length (5 * 32 bits = 20 bytes)
    iph->tos      = 0; // Type of service / DSCP
    iph->id       = htons(12345); //id num
    iph->frag_off = htons(0x8000); //Fragment offset aka where our evil bit is
    iph->ttl      = 64; //hop limit
    iph->protocol = IPPROTO_UDP; //next layer protocol
    iph->daddr    = destaddr.sin_addr.s_addr; // destination ip addr


    //UPD Header is straight after IPv4 header
    struct udphdr *udph = (struct udphdr *)(packet + sizeof (struct iphdr));

    udph->source = localaddr.sin_port;
    udph->dest = destaddr.sin_port;
    
    char *PayloadPrt = packet + sizeof(struct iphdr) + sizeof(struct udphdr);

    std::memcpy(PayloadPrt, payload, payloadSize);

    udph->len   = htons(sizeof(struct udphdr) + payloadSize);

    iph->tot_len = htons(sizeof(struct iphdr)+ sizeof(struct udphdr) + payloadSize);

    iph->saddr  = 0;  // kernel chooses source IP
    iph->check  = 0;  // kernel calculates IPv4 checksum
    udph->check = 0;  // valid IPv4 UDP: checksum disabled

    size_t packetSize = sizeof(struct iphdr) + sizeof(struct udphdr) + payloadSize;

    udph->len = htons(sizeof(struct udphdr) + payloadSize);

    iph->tot_len = htons(packetSize);

    return packetSize;

}


struct EvilResult
{
    int hiddenPort2 = -1;
};

EvilResult solveEvil(int sockfd, sockaddr_in destaddr, const std::array<char, 5>& sigilMessage) {
    
    EvilResult result;

    std::string startStr = "Hello!";
    
    // Create empty socket
    int sockEvil = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);

    // If not empty abort
    if(sockEvil < 0){
        perror("Error creating evil socket");
		exit(1);
    }

    int one = 1;
    // Define that we'll make our own IPV4 header
    if (setsockopt(sockEvil, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0 ){
        perror("setsockopt IP_HDRINCL error");
        close(sockEvil);
        exit(1);
    }

    char packet[4096];

    
    sockaddr_in localaddr{};
    socklen_t localLen = sizeof(localaddr);

    if (getsockname(
            sockfd,
            (struct sockaddr*)&localaddr,
            &localLen) < 0)
    {
        perror("getsockname");
        close(sockEvil);
        return result;
    }

    std::cout << "Local UDP port: " << ntohs(localaddr.sin_port) << std::endl;

    size_t packetSize = makeEvilPacket(
        packet,
        destaddr,
        localaddr,
        startStr.data(),
        startStr.size()
    );

    if (!sendMessage(sockEvil, destaddr, packet, packetSize))
    {
        close(sockEvil);
        return result;
    }

    char buffer[2048];

    int byteReceived = receiveMessage(sockfd, destaddr, buffer, sizeof(buffer));

    if (byteReceived > 0)
    {
        std::string response(buffer, byteReceived);
        std::cout << "Evil respons: " << response << std::endl;

        // Packet2 with groupID and sigil
        packetSize = makeEvilPacket(
            packet,
            destaddr,
            localaddr,
            sigilMessage.data(),
            sigilMessage.size()
        );

        if(!sendMessage(sockEvil, destaddr, packet, packetSize))
        {
            std::cerr << "Failed to send group ID and sigil" << std::endl;
            close(sockEvil);
            return result;
        }

        int finalBytes = receiveMessage(sockfd, destaddr, buffer, sizeof(buffer));

        if(finalBytes > 0)
        {
            std::string finalResponse(buffer, finalBytes);
            std::cout << "Evil final response: " << finalResponse << std::endl;

            
            result.hiddenPort2 = extractNumbers(finalResponse, finalResponse.size() - 1);
            // Debugging
            std::cout << "Hidden port 2: " << result.hiddenPort2 << std::endl;
        }
        else
        {
            std::cout << "No response after sending sigil" << std::endl;
        }

    }
    else
    {
        std::cout << "No response from Evil port"  << std:: endl;
    }
    
    
    close(sockEvil);
    return result; 
}

struct GuardianResult
{
    int hiddenPort3 = -1;
};

// NOT SURE ABOUT THE PARAMETERS!
GuardianResult solveGuardian(int sockfd, sockaddr_in destaddr, const std::array<char, 5>& sigilMessage) {

}

// NOT SURE ABOUT THE PARAMETERS!
void solveDragon(int sockfd, sockaddr_in destaddr, int hiddenPort1, int hiddenPort2, int hiddenPort3) {

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

    // Variables to store port numbers
    int secretPort = -1;
    int evilPort = -1;
    int guardianPort = -1;
    int dragonPort = -1;

    // Loop through the ports to find which puzzle belongs to which port
    for (int i = 0; i < 4; i++) {

        // Set the current port as the desination address
        destaddr.sin_port = htons(ports[i]);
       
        std::string m = "Hello!"; // Message

        char buffer[2048];

        int bytesReceived = retryMessage (sockfd, destaddr, m.data(), m.size(), buffer, sizeof(buffer));
        

        if (bytesReceived < 0) {
            std::cout << "No response from port" << ports[i] << std::endl;
            continue;
        }

        // Converts response into a string
        std::string response(buffer, bytesReceived);

        // Find which puzzle the port belongs to
        // NOT SURE ABOUT THE PARAMETERS
        if (response.find("Sacred Elder Cipher Relay for Enchanted Transmissions") != std::string::npos) {
            std::cout << ports[i] << " is the S.E.C.R.E.T. port" << std::endl; // JUST FOR DEBUGGING
            secretPort = ports[i];
        }
        else if(response.find("Evil") != std::string::npos) {
            std::cout << ports[i] << " is the Evil port" << std::endl; // JUST FOR DEBUGGING
            evilPort = ports[i];
        }
        else if(response.find("guardian") != std::string::npos) {
            std::cout << ports[i] << " is the Guardian of the secret spell port" << std::endl; // JUST FOR DEBUGGING
            guardianPort = ports[i];
        }
        else if (response.find("D.R.A.G.O.N") != std::string::npos) {
            std::cout << ports[i] << " is the D.R.A.G.O.N. port" << std::endl; // JUST FOR DEBUGGING
            dragonPort = ports[i];
        }
    }

    destaddr.sin_port = htons(secretPort);
    SecretResult secretResult = solveSecret(sockfd, destaddr);

    destaddr.sin_port = htons(evilPort);
    EvilResult evilResult = solveEvil(sockfd, destaddr, secretResult.sigilMessage);

    destaddr.sin_port = htons(guardianPort);
    GuardianResult guardianResult = solveGuardian(sockfd, destaddr, secretResult.sigilMessage); 

    destaddr.sin_port = htons(dragonPort);
    solveDragon(sockfd, destaddr, secretResult.hiddenPort1, evilResult.hiddenPort2, guardianResult.hiddenPort3);

    close(sockfd);
    return 0;  
}

       

    
   