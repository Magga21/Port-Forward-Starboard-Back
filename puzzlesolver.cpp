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
#include <netinet/ip6.h>
#include <sstream>

/*
--------------------
UDP HELPER FUNCTIONS
--------------------
*/

// Sends data to a destination address through a UDP socket and returns whether it was successful
bool sendMessage(int sockfd, sockaddr_in destaddr, const void *data, size_t dataSize)
{
    int bytesSent = sendto(sockfd, data, dataSize, 0, (struct sockaddr *)&destaddr, sizeof(destaddr));

    if (bytesSent < 0)
    {
        perror("Error sending");
        return false;
    }

    return true;
}

// Receives data from the expected destination address and returns the number of bytes received
int receiveMessage(int sockfd, sockaddr_in destaddr, char *buffer, int bufferSize)
{
    // Set maximum time to wait for a response
    struct timeval tv;
    fd_set readfds;

    tv.tv_sec = 2;
    tv.tv_usec = 500000;

    // Clear the socket watchlist and add the socket to it
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    // Wait until data is on the socket or timeout is reached
    int result = select(sockfd + 1, &readfds, NULL, NULL, &tv);

    if (result <= 0)
    {
        return -1;
    }

    // Create address structure to store where the response came from
    struct sockaddr_in srcaddr;
    socklen_t srcaddrlen = sizeof(srcaddr);

    // Receive the response and store it in the buffer
    int bytesReceived = recvfrom(sockfd, buffer, bufferSize, 0, (struct sockaddr *)&srcaddr, &srcaddrlen);

    if (bytesReceived < 0)
    {
        perror("Error receiving");
        return -1;
    }

    // Make sure the response came from the IP and port that was sent to
    if (srcaddr.sin_addr.s_addr != destaddr.sin_addr.s_addr ||
        srcaddr.sin_port != destaddr.sin_port)
    {
        return -1;
    }

    return bytesReceived;
}

// Sends data and waits for a response and retries up to 3 times if needed
int retryMessage(int sockfd, sockaddr_in destaddr, const void *data, size_t dataSize, char *buffer, int bufferSize)
{
    // Try sending the data up tp 3 times before giving up
    for (int attempt = 0; attempt < 3; attempt++)
    {
        if (!sendMessage(sockfd, destaddr, data, dataSize))
        {
            return -1;
        }

        int bytesReceived = receiveMessage(sockfd, destaddr, buffer, bufferSize);

        if (bytesReceived > 0)
        {
            return bytesReceived;
        }
    }
    return -1;
}

/*
----------------------
UDP CHECKSUM FUNCTIONS
----------------------
*/

// Combines pairs of bytes into 16 bit values and adds them to the checksum
uint32_t addBytePairs(uint32_t sum, const uint8_t *data, size_t length)
{
    // Read data in pairs of two bytes and add their values to sum
    for (size_t i = 0; i + 1 < length; i += 2)
    {
        // Combine the two bytes into one 16 bit number
        uint16_t bytePair = (static_cast<uint16_t>(data[i]) << 8) | data[i + 1];

        sum += bytePair;
    }

    // If there is an odd number of bytes, pad the last byte with a zero
    if (length % 2 != 0)
    {
        uint16_t bytePair = static_cast<uint16_t>(data[length - 1]) << 8;
        sum += bytePair;
    }
    return sum;
}

// Calculates the UDP checksum
uint16_t udpChecksum(const ip6_hdr *ipv6, const udphdr *udp, const char *payload, size_t payloadLength)
{
    uint32_t sum = 0;

    // Add the IPv6 source and destination addresses to the checksum
    sum = addBytePairs(sum, reinterpret_cast<const uint8_t *>(&ipv6->ip6_src), sizeof(ipv6->ip6_src));
    sum = addBytePairs(sum, reinterpret_cast<const uint8_t *>(&ipv6->ip6_dst), sizeof(ipv6->ip6_dst));

    // Set the UDP length and protocol in network byte order
    uint32_t udpLength = htonl(sizeof(struct udphdr) + payloadLength);
    uint32_t protocol = htonl(IPPROTO_UDP);

    // Add the UDP length and protocol
    sum = addBytePairs(sum, reinterpret_cast<const uint8_t *>(&udpLength), sizeof(udpLength));
    sum = addBytePairs(sum, reinterpret_cast<const uint8_t *>(&protocol), sizeof(protocol));

    // Add the UDP header and payload
    sum = addBytePairs(sum, reinterpret_cast<const uint8_t *>(udp), sizeof(struct udphdr));
    sum = addBytePairs(sum, reinterpret_cast<const uint8_t *>(payload), payloadLength);

    // Add any bits that are above the 16 bits back into the lower 16 bits to handle overflow
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // Invert the bits to get the checksum
    uint16_t checksum = static_cast<uint16_t>(~sum);

    // Return checksum in network byte order
    return htons(checksum);
}

/*
-----------------------
PUZZLE HELPER FUNCTIONS
-----------------------
*/

// Extracts a number from a message by reading backwards from a given index
int extractNumber(const std::string &message, int startIndex)
{
    std::string numberString = "";

    for (int i = startIndex; i >= 0; --i)
    {
        if (std::isdigit(message[i]))
        {
            numberString += message[i];
        }
        else
        {
            // Stop searching once a non digit is found
            break;
        }
    }
    // Reverse the number since the digits were gathered in reverse order
    std::reverse(numberString.begin(), numberString.end());
    // Convert the string to an integer or return 0 if no digits were found
    return numberString.empty() ? 0 : std::stoi(numberString);
}

/*
--------------------
SECRET PUZZLE SOLVER
--------------------
*/

// Stores the sigil message and hidden port found by the SECRET puzzle
struct SecretResult
{
    std::array<char, 5> sigilMessage{};
    int hiddenPort1 = -1;
};

// Solves the SECRET puzzle and returns the sigil message and the first hidden port
SecretResult solveSecret(int sockfd, sockaddr_in destaddr)
{
    std::cout << "\n=== Solving the S.E.C.R.E.T. puzzle ===" << std::endl;

    SecretResult result;

    uint32_t secretNumber = 21;
    std::string message = "S.E.C.R.E.T.:katrinth25,margretf24,";

    // Append the 32 bit secret number as 4 bytes to the end of the message
    const char *secretNumberBytes = (const char *)&secretNumber;
    message.append(secretNumberBytes, sizeof(secretNumber));

    char buffer[2048];

    int bytesReceived = retryMessage(sockfd, destaddr, message.c_str(), message.length(), buffer, sizeof(buffer));

    if (bytesReceived != 5)
    {
        std::cout << "-> Failed to receive 5 byte challenge" << std::endl;
        return result;
    }

    std::cout << "-> Received group ID and challenge number" << std::endl;

    // First byte of the response is the group ID
    uint8_t groupID = buffer[0];

    uint32_t challengeNumber;

    // Copy the next 4 bytes from the response into challengeNumber
    memcpy(&challengeNumber, &buffer[1], sizeof(challengeNumber));

    // XOR the challenge number with the secret number
    uint32_t sigil = challengeNumber ^ secretNumber;

    // Build the 5 byte sigil message from the group ID and calculated sigil
    memcpy(&result.sigilMessage[0], &groupID, sizeof(groupID));
    memcpy(&result.sigilMessage[1], &sigil, sizeof(sigil));

    if (!sendMessage(sockfd, destaddr, result.sigilMessage.data(), result.sigilMessage.size()))
    {
        std::cout << "-> Failed to send sigil message" << std::endl;
        return result;
    }

    std::cout << "-> Sent sigil message" << std::endl;

    // Receive final response containing the hidden port
    int secretResponse = receiveMessage(sockfd, destaddr, buffer, sizeof(buffer));

    if (secretResponse <= 0)
    {
        std::cout << "-> Failed to receive hidden port" << std::endl;
        return result;
    }
    std::string hiddenSecret(buffer, secretResponse);

    result.hiddenPort1 = extractNumber(hiddenSecret, hiddenSecret.size() - 2);

    std::cout << "-> Found first hidden port: " << result.hiddenPort1 << std::endl;

    return result;
}

/*
------------------
EVIL PUZZLE SOLVER
------------------
*/

size_t makeEvilPacket(char *packet, sockaddr_in destaddr, sockaddr_in localaddr, const char *payload, size_t payloadSize)
{

    std::memset(packet, 0, 4096);

    // Pointer to the IP header
    struct iphdr *iph = (struct iphdr *)packet;

    iph->version = 4;                      // IPv4 v.
    iph->ihl = 5;                          // Header length (5 * 32 bits = 20 bytes)
    iph->tos = 0;                          // Type of service / DSCP
    iph->id = htons(12345);                // id num
    iph->frag_off = htons(0x8000);         // Fragment offset aka where our evil bit is
    iph->ttl = 64;                         // hop limit / time to live
    iph->protocol = IPPROTO_UDP;           // next layer protocol
    iph->daddr = destaddr.sin_addr.s_addr; // destination ip addr

    // UPD Header is straight after IPv4 header
    struct udphdr *udph = (struct udphdr *)(packet + sizeof(struct iphdr));

    udph->source = localaddr.sin_port;
    udph->dest = destaddr.sin_port;

    // pointer to where the payload starts in the packet, after the IP and UDP header
    char *PayloadPrt = packet + sizeof(struct iphdr) + sizeof(struct udphdr);

    // copy the payload into the packet
    std::memcpy(PayloadPrt, payload, payloadSize);

    iph->saddr = 0;  // kernel chooses source IP
    iph->check = 0;  // kernel calculates IPv4 checksum
    udph->check = 0; // valid IPv4 UDP: checksum disabled

    size_t packetSize = sizeof(struct iphdr) + sizeof(struct udphdr) + payloadSize;

    udph->len = htons(sizeof(struct udphdr) + payloadSize);

    iph->tot_len = htons(packetSize);

    return packetSize;
}

struct EvilResult
{
    int hiddenPort2 = -1;
};

EvilResult solveEvil(int sockfd, sockaddr_in destaddr, const std::array<char, 5> &sigilMessage)
{

    std::cout << "\n=== Solving the Evil puzzle ===" << std::endl;

    EvilResult result;

    std::string startStr = "Hello!";

    // Create empty socket
    int sockEvil = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);

    // If socket is not empty abort
    if (sockEvil < 0)
    {
        perror("Error creating evil socket");
        exit(1);
    }

    int one = 1;

    // Define that we'll make our own IPV4 header
    if (setsockopt(sockEvil, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0)
    {
        perror("setsockopt IP_HDRINCL error");
        close(sockEvil);
        exit(1);
    }

    char packet[4096];

    sockaddr_in localaddr{};
    socklen_t localLen = sizeof(localaddr);

    // Get Local address and port for UDP socket
    if (getsockname(
            sockfd,
            (struct sockaddr *)&localaddr,
            &localLen) < 0)
    {
        perror("getsockname");
        close(sockEvil);
        return result;
    }

    size_t packetSize = makeEvilPacket(
        packet,
        destaddr,
        localaddr,
        startStr.data(),
        startStr.size());

    if (!sendMessage(sockEvil, destaddr, packet, packetSize))
    {
        close(sockEvil);
        return result;
    }

    char buffer[2048];

    int bytesReceived = receiveMessage(sockfd, destaddr, buffer, sizeof(buffer));

    if (bytesReceived > 0)
    {
        std::string response(buffer, bytesReceived);
        std::cout << "Evil respons: " << response << std::endl;

        // Packet2 with groupID and sigil
        packetSize = makeEvilPacket(
            packet,
            destaddr,
            localaddr,
            sigilMessage.data(),
            sigilMessage.size());

        if (!sendMessage(sockEvil, destaddr, packet, packetSize))
        {
            std::cerr << "Failed to send group ID and sigil" << std::endl;
            close(sockEvil);
            return result;
        }

        int finalBytes = receiveMessage(sockfd, destaddr, buffer, sizeof(buffer));

        if (finalBytes > 0)
        {
            std::string finalResponse(buffer, finalBytes);
            std::cout << "Evil final response: " << finalResponse << std::endl;

            result.hiddenPort2 = extractNumber(finalResponse, finalResponse.size() - 1);
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
        std::cout << "No response from Evil port" << std::endl;
    }

    close(sockEvil);
    return result;
}

/*
----------------------
GUARDIAN PUZZLE SOLVER
----------------------
*/
// Stores the secret spell found by the Guardian puzzle
struct GuardianResult
{
    std::string spell;
};

// Solves the Guardian puzzle and returns the secret spell
GuardianResult solveGuardian(int sockfd, sockaddr_in destaddr, const std::array<char, 5> &sigilMessage)
{
    std::cout << "\n=== Solving the Guardian of the secret spell puzzle ===" << std::endl;

    GuardianResult result;

    std::string message = "Hello!";

    char buffer[2048];

    // Send initial message and receive the nested IPv6/UDP packet
    int bytesReceived = retryMessage(sockfd, destaddr, message.c_str(), message.size(), buffer, sizeof(buffer));

    if (bytesReceived <= 0)
    {
        std::cout << "-> Failed to receive response after 3 attempts" << std::endl;
        return result;
    }

    if (bytesReceived <= sizeof(struct ip6_hdr) + sizeof(struct udphdr))
    {
        std::cout << "-> Received response is too short" << std::endl;
        return result;
    }

    std::cout << "-> Received nested IPv6/UDP packet" << std::endl;

    // Interpret the beginning of the response as a IPv6 header
    struct ip6_hdr *receivedIpv6 = reinterpret_cast<struct ip6_hdr *>(buffer);
    // Interpret the part after the IPv6 header as a UDP header
    struct udphdr *receivedUdp = reinterpret_cast<struct udphdr *>(buffer + sizeof(struct ip6_hdr));

    // Calculate length of the nested UDP packet and size of the entire packet
    size_t udpLength = sizeof(struct udphdr) + sigilMessage.size();
    size_t packetSize = sizeof(struct ip6_hdr) + udpLength;

    // Create a buffer for the reply packet and set all bytes to 0
    char packetBuffer[53];
    std::memset(packetBuffer, 0, sizeof(packetBuffer));

    // Interpret the beginning of the packet buffer as a IPv6 header
    struct ip6_hdr *replyIpv6 = reinterpret_cast<struct ip6_hdr *>(packetBuffer);

    // Copy the received IPv6 header into the reply
    std::memcpy(replyIpv6, receivedIpv6, sizeof(struct ip6_hdr));

    // Set the IPv6 payload length to the length of the UDP packet
    replyIpv6->ip6_plen = htons(udpLength);

    // Swap IPv6 source and destination addresses so packet is sent back to sender
    replyIpv6->ip6_src = receivedIpv6->ip6_dst;
    replyIpv6->ip6_dst = receivedIpv6->ip6_src;

    // Set IPv6 next header as UDP
    replyIpv6->ip6_nxt = IPPROTO_UDP;

    // Interpret the part after the IPv6 header as a UDP header
    struct udphdr *replyUdp = reinterpret_cast<struct udphdr *>(packetBuffer + sizeof(struct ip6_hdr));

    // Swap the UDP source and destination ports
    replyUdp->source = receivedUdp->dest;
    replyUdp->dest = receivedUdp->source;

    // Set the length of the UDP packet
    replyUdp->len = htons(udpLength);

    // Find where the payload starts in the packet buffer
    char *replyPayload = packetBuffer + sizeof(struct ip6_hdr) + sizeof(struct udphdr);

    // Copy the sigil message into the payload
    std::memcpy(replyPayload, sigilMessage.data(), sigilMessage.size());

    // Set checksum to 0 and then calculate it for the UDP packet
    replyUdp->check = 0;
    replyUdp->check = udpChecksum(replyIpv6, replyUdp, replyPayload, sigilMessage.size());

    // Send the constructed IPv6/UDP packet through a normal UDP socket
    if (!sendMessage(sockfd, destaddr, packetBuffer, packetSize))
    {
        std::cout << "-> Failed to send constructed IPv6/UDP packet" << std::endl;
        return result;
    }

    std::cout << "-> Sent constructed IPv6/UDP packet" << std::endl;

    // Save the IPv6 addresses from the initial response
    in6_addr initialSource = receivedIpv6->ip6_src;
    in6_addr initialDestination = receivedIpv6->ip6_dst;

    // Keep track of responses and loop through them to find the secret spell
    int responseCount = 0;
    for (int i = 0; i < 4; i++)
    {
        int bytesReceived = receiveMessage(sockfd, destaddr, buffer, sizeof(buffer));

        if (bytesReceived <= 0)
        {
            std::cout << "-> Failed to receive Guardian responses" << std::endl;
            break;
        }

        // Interpret the beginning of the response as an IPv6 header
        struct ip6_hdr *responseIpv6 = reinterpret_cast<struct ip6_hdr *>(buffer);

        // Calculate the size of the IPv6 and UDP headers
        size_t headerSize = sizeof(struct ip6_hdr) + sizeof(struct udphdr);

        // Extract the text that comes after the headers
        std::string responseMessage(buffer + headerSize, bytesReceived - headerSize);

        // Check whether the response has the same source and destination address as the initial response
        bool sameSource = std::memcmp(&responseIpv6->ip6_src, &initialSource, sizeof(in6_addr)) == 0;
        bool sameDestination = std::memcmp(&responseIpv6->ip6_dst, &initialDestination, sizeof(in6_addr)) == 0;

        // The response with the matching IPv6 addresses contains the spell
        if (sameSource && sameDestination)
        {
            // Find text between the quotation marks
            size_t openingQuote = responseMessage.find('"');
            size_t closingQuote = responseMessage.find('"', openingQuote + 1);

            if (openingQuote != std::string::npos && closingQuote != std::string::npos)
            {
                // Extract the secret spell
                result.spell = responseMessage.substr(openingQuote + 1, closingQuote - openingQuote - 1);
            }
        }
        responseCount++;
    }

    if (responseCount == 4)
    {
        std::cout << "-> Received all 4 responses" << std::endl;
    }

    if (!result.spell.empty())
    {
        std::cout << "-> Found which one is the secret spell: " << result.spell << std::endl;
    }

    return result;
}

/*
----------------------
DRAGON PUZZLE SOLVER
----------------------
*/
void solveDragon(int sockfd, sockaddr_in destaddr, int hiddenPort1, int hiddenPort2, const std::string &spell, const std::array<char, 5> &sigilMessage)
{

    std::cout << "\n=== Solving the D.R.A.G.O.N. puzzle ===" << std::endl;

    std::string hellostr = "Hello!";

    // Print the ports to see if they are -1
    std::cout << "hiddenPort1 = " << hiddenPort1 << std::endl;
    std::cout << "hiddenPort2 = " << hiddenPort2 << std::endl;

    // If they're 1 one then retriving the ports failed and we opt out of the program
    if (hiddenPort1 == -1 || hiddenPort2 == -1)
    {
        std::cerr << "Invalid hidden ports" << std::endl;
        exit(1);
    }

    std::string portList = (std::to_string(hiddenPort1)) + "," + (std::to_string(hiddenPort2));

    sendMessage(sockfd, destaddr, portList.c_str(), portList.length());

    char buffer[2048];

    // Create empty list of ints
    std::vector<int> vals;

    int bytesReceived = receiveMessage(sockfd, destaddr, buffer, sizeof(buffer));

    if (bytesReceived > 0)
    {
        std::string response(buffer, bytesReceived);
        std::cout << "Dragon response: " << response << std::endl;

        // convert response into a list of integers
        std::stringstream ss(response);
        std::string item;

        while (std::getline(ss, item, ','))
        {
            vals.push_back(std::stoi(item));
        }
    }

    else
    {
        std::cout << "No response from Dragon" << std::endl;
        return;
    }

    std::vector<char> knock;

    knock.insert(
        knock.end(),
        sigilMessage.begin(),
        sigilMessage.end());

    knock.insert(
        knock.end(),
        spell.begin(),
        spell.end());

    struct sockaddr_in firstdestaddr = destaddr;
    struct sockaddr_in seconddestaddr = destaddr;

    firstdestaddr.sin_port = htons(hiddenPort1);
    seconddestaddr.sin_port = htons(hiddenPort2);

    // make this a forloop that checks which port it has to send to and then send to said port.
    for (int i = 0; i < vals.size(); i++)
    {
        struct sockaddr_in targaddr;

        if (vals[i] == hiddenPort1)
        {
            targaddr = firstdestaddr;
        }
        else if (vals[i] == hiddenPort2)
        {
            targaddr = seconddestaddr;
        }

        sendMessage(sockfd, targaddr, knock.data(), knock.size());
        int resp = receiveMessage(sockfd, targaddr, buffer, sizeof(buffer));
        if (resp > 0)
        {
            std::string response(buffer, resp);
            std::cout << "Dragon response: " << response << std::endl;
        }
    }
}

/*
-------------
PUZZLE SOLVER
-------------
* This program takes in an IP address and four puzzle ports.
* It finds which puzzle belongs to which port and then solves each puzzle.
*/
int main(int argc, const char *argv[])
{
    // Make sure the correct number of command line arguments was given
    if (argc != 6)
    {
        std::cerr << "Invalid number of arguments! Need: ./puzzlesolver <IPaddress> <port1> <port2> <port3> <port4>" << std::endl;
        exit(1);
    }

    // Store the IP address and port number from the command line
    const char *ipaddr = argv[1];
    int port1 = std::stoi(argv[2]);
    int port2 = std::stoi(argv[3]);
    int port3 = std::stoi(argv[4]);
    int port4 = std::stoi(argv[5]);

    int ports[4] = {port1, port2, port3, port4}; // Array containing all ports

    // Create UDP socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Error creating socket");
        exit(1);
    }

    // Make destination address
    struct sockaddr_in destaddr;
    destaddr.sin_family = AF_INET;

    if (inet_pton(AF_INET, ipaddr, &destaddr.sin_addr) < 1)
    {
        std::cerr << "Invalid IP address of address family: " << ipaddr << std::endl;
        exit(1);
    }

    // Store the identified ports for each puzzle
    int secretPort = -1;
    int evilPort = -1;
    int guardianPort = -1;
    int dragonPort = -1;

    std::cout << "\n=== Finding which puzzle each port is ===" << std::endl;

    // Loop through the ports to find which puzzle belongs to which port
    for (int i = 0; i < 4; i++)
    {
        // Set the current port as the desination port
        destaddr.sin_port = htons(ports[i]);

        std::string m = "Hello!";

        char buffer[2048];

        int bytesReceived = retryMessage(sockfd, destaddr, m.data(), m.size(), buffer, sizeof(buffer));

        if (bytesReceived < 0)
        {
            std::cout << "NO RESPONSE from port " << ports[i] << std::endl;
            continue;
        }

        // Converts response into a string so its contents can be checked
        std::string response(buffer, bytesReceived);

        // Find which puzzle the port belongs to
        if (response.find("Sacred Elder Cipher Relay for Enchanted Transmissions") != std::string::npos)
        {
            std::cout << ports[i] << " is the S.E.C.R.E.T. port" << std::endl;
            secretPort = ports[i];
        }
        else if (response.find("Evil") != std::string::npos)
        {
            std::cout << ports[i] << " is the Evil port" << std::endl;
            evilPort = ports[i];
        }
        else if (response.find("guardian") != std::string::npos)
        {
            std::cout << ports[i] << " is the Guardian of the secret spell port" << std::endl;
            guardianPort = ports[i];
        }
        else if (response.find("D.R.A.G.O.N") != std::string::npos)
        {
            std::cout << ports[i] << " is the D.R.A.G.O.N. port" << std::endl;
            dragonPort = ports[i];
        }
    }

    if (secretPort == -1 || evilPort == -1 || guardianPort == -1 || dragonPort == -1)
    {
        std::cout << "\n-> Failed to identify all puzzle ports. Check the provided ports and try again." << std::endl;
        close(sockfd);
        return 1;
    }

    // Solve the SECRET puzzle
    destaddr.sin_port = htons(secretPort);
    SecretResult secretResult = solveSecret(sockfd, destaddr);

    if (secretResult.hiddenPort1 == -1)
    {
        std::cout << "\n-> SECRET puzzle failed. The program cannot continue. Please try again." << std::endl;
        close(sockfd);
        return 1;
    }

    // Solve the Evil puzzle
    destaddr.sin_port = htons(evilPort);
    EvilResult evilResult = solveEvil(sockfd, destaddr, secretResult.sigilMessage);

    if (evilResult.hiddenPort2 == -1)
    {
        std::cout << "\n-> Evil puzzle failed. The program cannot continue. Please try again." << std::endl;
        close(sockfd);
        return 1;
    }

    // Solve the Guardian puzzle
    destaddr.sin_port = htons(guardianPort);
    GuardianResult guardianResult = solveGuardian(sockfd, destaddr, secretResult.sigilMessage);

    if (guardianResult.spell.empty())
    {
        std::cout << "\n-> Guardian puzzle failed. The program cannot continue. Please try again." << std::endl;
        close(sockfd);
        return 1;
    }

    // Solve DRAGON puzzle
    destaddr.sin_port = htons(dragonPort);
    solveDragon(sockfd, destaddr, secretResult.hiddenPort1, evilResult.hiddenPort2, guardianResult.spell, secretResult.sigilMessage);

    close(sockfd);
    return 0;
}
