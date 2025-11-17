// Define POSIX standards for strdup() and getaddrinfo() functions
#define _POSIX_C_SOURCE 200809L

// Define for GNU extension
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

unsigned short calculate_checksum(unsigned short *address, int bytes) {
  // Define the sum and checksum variables to be incremented
  long sum = 0;
  unsigned short checksum = 0;

  // While the number of bytes is greater than 1
  while (bytes > 1) {
    // Increment sum by the 16-bit word "address" is pointing to
    sum += (unsigned short)*address++;

    // Decrement bytes by 2
    bytes -= 2;
  }

  // If there is a byte left over
  if (bytes > 0) {
    // Add the last byte after converting
    sum += htons((unsigned short)*address);
  }

  // While the sum is larger than 16 bits
  while (sum >> 16) {
    // Handle the overflowed packet by wrapping it around
    sum = (sum & 0xffff) + (sum >> 16);
  }

  // Take the one's complement of the sum (bitwise NOT)
  checksum = ~sum;

  return checksum;
}

// Define pseudo header struct
struct pseudo_header {
  u_int32_t source_address;
  u_int32_t dest_address;
  u_int8_t reserved;
  u_int8_t protocol_type;
  u_int16_t segment_length;
};

struct pseudo_header *calculate_pseudo_header(uint32_t src_addr,
                                              uint32_t dest_addr) {
  // Allocate memory for the pseudo_header struct and zero it all
  struct pseudo_header *header = malloc(sizeof(struct pseudo_header));
  memset(header, 0, sizeof(struct pseudo_header));

  // Fill in the pseudo header values
  // Source IP
  header->source_address = src_addr;
  // Destination IP
  header->dest_address = dest_addr;
  // Protocol
  header->protocol_type = IPPROTO_TCP;
  // Reserve 8 bytes
  header->reserved = 0;
  // Segment Length
  header->segment_length = htons(sizeof(struct tcphdr));

  return header;
}

int main(int argc, char *argv[]) {
  // Define defaults for command-line arguments
  int max_hops = 30;
  int dst_port = 80;
  char *target = "google.com";
  bool help = false;

  // Parse passed arguments, if any
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-m") == 0) {
      max_hops = atoi(argv[++i]);
    } else if (strcmp(argv[i], "-p") == 0) {
      dst_port = atoi(argv[++i]);
    } else if (strcmp(argv[i], "-t") == 0) {
      target = argv[++i];
    } else if (strcmp(argv[i], "-h") == 0) {
      help = true;
    }
  }

  // Display message and return if "-h" specified
  if (help) {
    printf(
        "usage: tcp_traceroute [-m MAX_HOPS] [-p DST_PORT] -t TARGET\n\n"
        "optional arguments:\n"
        "-h, --help   show this help message and exit\n"
        "-m   MAX_HOPS  Max hops to probe (default = 30)\n"
        "-p   DST_PORT  TCP destination port (default = 80)\n"
        "-t   TARGET    Target domain or IP\n");
    return 0;
  }

  // // Make a writable copy of the target domain/IP
  char *target_copy = strdup(target);

  // Point where there is "://", if any
  char *host = strstr(target_copy, "://");

  // If there is "://", change the pointer to exclude it
  if (host != NULL) {
    host += 3;
  }

  // If there is not "://", point to the beginning of the domain/IP
  else {
    host = target;
  }

  // Find if there is a "/" after the domain name
  char *slash = strchr(host, '/');

  // If there is a "/"
  if (slash) {
    // Place a null terminator so the host pointer excludes the path
    *slash = '\0';
  }

  // Define struct for domain
  struct addrinfo hints, *res;

  // Fill hints with zeros so that we can specidy sokcet type
  memset(&hints, 0, sizeof hints);

  // Define hints, AF_INET=IPV4, SOCK_STREAM=TCP
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  // Get the resolved URL or IP address
  int ip = getaddrinfo(host, NULL, &hints, &res);

  // Check that the URL resolved correctly
  if (ip != 0) {
    fprintf(stderr, "\nCouldn't resolve URL or IP: %s\n", gai_strerror(ip));
    return -1;
  }

  // Create a temporary UDP socket
  int temp_sock = socket(AF_INET, SOCK_DGRAM, 0);

  // Check that the temporary socket was created successfully
  if (temp_sock < 0) {
    printf("\nFailed to create temporary socket\n");
    return -1;
  }

  // Define struct for temporary destination address
  struct sockaddr_in temp_addr;

  // Define temporary destination address
  temp_addr.sin_family = AF_INET;
  temp_addr.sin_port = htons(80);
  inet_pton(AF_INET, "8.8.8.8", &temp_addr.sin_addr);

  // Connect to the temporary destination address
  connect(temp_sock, (struct sockaddr *)&temp_addr, sizeof(temp_addr));

  // Define struct for Local IP address and length
  struct sockaddr_in src_addr;
  socklen_t src_len = sizeof(src_addr);

  // Get socket name of temp socket to extract local IP and then close it
  getsockname(temp_sock, (struct sockaddr *)&src_addr, &src_len);
  close(temp_sock);

  // Define a raw socket to send, AF_INET=IPv4, IPPROTO_RAW=Manual IP header
  int raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);

  // Check that the raw socket was created successfully
  if (raw_sock < 0) {
    printf(
        "\nFailed to create raw socket, make sure you are executing as root\n");
    return -1;
  }

  // Tell OS that we are building our own IP headers
  int one = 1;
  setsockopt(raw_sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

  // Define socket to listen for TTL expirations, IPPROTO_ICMP=ICMP packets
  int icmp_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

  // Check that the raw socket was created successfully
  if (icmp_sock < 0) {
    printf(
        "\nFailed to create raw socket, make sure you are executing as root\n");
    return -1;
  }

  // Define a raw socket to listen for TCP SYN-ACK, IPPROTO_TCP=TCP packets
  int tcp_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);

  // Check that the raw socket was created successfully
  if (tcp_sock < 0) {
    printf(
        "\nFailed to create raw socket, make sure you are executing as root\n");
    return -1;
  }

  // Define struct for destination address
  struct sockaddr_in destination;

  // Define destination, htons() converts destination port to big-endian
  destination.sin_family = AF_INET;
  destination.sin_port = htons(dst_port);

  // Cast the binary IP to a sockaddr_in struct and define sin_addr
  destination.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;

  printf("traceroute to %s (%s), %d hops max, TCP SYN to port %d\n", target,
         inet_ntoa(destination.sin_addr), max_hops, dst_port);

  // Start from 1 and iterate until max_hops
  for (int hop = 1; hop <= max_hops; hop++) {
    // Define variables for ending early
    bool synack = false;
    bool rst = false;
    bool terminate = false;

    // Print the hop number
    printf("%2d  ", hop);

    // Create a buffer for the packet and fill it with zeros
    char packet[4096];
    memset(packet, 0, sizeof(packet));

    // Define the IP header structure and point it to the beginning of buffer
    struct iphdr *ip_header = (struct iphdr *)packet;
    // Define the TCP header structure and point it to the end of the IP header
    struct tcphdr *tcp_header =
        (struct tcphdr *)(packet + sizeof(struct iphdr));

    // Fill in the IP header values
    // IPv4
    ip_header->version = 4;
    // Header Length: 5 = 20 bytes
    ip_header->ihl = 5;
    // Type of Service
    ip_header->tos = 0;
    // Total Size of Packet (16-bit)
    ip_header->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    // No Fragmentation
    ip_header->frag_off = 0;
    // Time to Live
    ip_header->ttl = hop;
    // Identification (16-bit)
    ip_header->id = htons(54321);
    // Upper layer protocol
    ip_header->protocol = IPPROTO_TCP;
    // Checksum (calculate later)s
    ip_header->check = 0;
    // Source IP
    ip_header->saddr = src_addr.sin_addr.s_addr;
    // Destination IP
    ip_header->daddr = destination.sin_addr.s_addr;

    // Fill in the TCP header values
    // Source Port (16-bit)
    tcp_header->source = htons(12345 + hop);
    // Destination Port (16-bit)
    tcp_header->dest = htons(dst_port);
    // Sequence Number (32 bit)
    tcp_header->seq = htonl(hop);
    // SYN Flag set to send SYN packet
    tcp_header->syn = 1;
    // Window Buffer Size (16-bit)
    tcp_header->window = htons(5840);
    // Data Offset = 5 bytes
    tcp_header->doff = 5;
    // Checksum (calculate later)
    tcp_header->check = 0;

    // Calculate the IP header checksum and apply it
    ip_header->check =
        calculate_checksum((unsigned short *)ip_header, sizeof(struct iphdr));

    // Calculate the pseudo header for the TCP header
    struct pseudo_header *pseudo_header =
        calculate_pseudo_header(ip_header->saddr, ip_header->daddr);

    // Allocate memory for pseudo_header struct and TCP header and zero it all
    unsigned short *double_header =
        malloc(sizeof(struct pseudo_header) + sizeof(struct tcphdr));
    memset(double_header, 0,
           sizeof(struct pseudo_header) + sizeof(struct tcphdr));

    // Copy the pseudo header to the beginning
    memcpy(double_header, pseudo_header, sizeof(struct pseudo_header));
    // Then, copy the TCP header after
    memcpy(double_header + (unsigned short)(sizeof(struct pseudo_header) /
                                            sizeof(unsigned short)),
           tcp_header, sizeof(struct tcphdr));
    // memcpy((char *)double_header + sizeof(struct pseudo_header), tcp_header,
    // sizeof(struct tcphdr));

    // Calculate the TCP header checksum
    tcp_header->check = calculate_checksum(
        (unsigned short *)double_header,
        (sizeof(struct pseudo_header) + sizeof(struct tcphdr)));

    // Free memory allocations
    free(double_header);
    free(pseudo_header);

    // Define variables for next loop
    double first_time;
    double second_time;
    char first_addr[INET_ADDRSTRLEN];
    char second_addr[INET_ADDRSTRLEN];

    // Loop for three probes
    for (int probe = 1; probe <= 3; probe++) {
      // Define local variables
      double rtt = 0.0;
      struct timeval send_time, recv_time;
      char addrstr[INET_ADDRSTRLEN];

      // Get the current time and apply it to send_time before sending
      gettimeofday(&send_time, NULL);

      // Send the packet to the destination
      int sent = sendto(raw_sock, packet, ntohs(ip_header->tot_len), 0,
                        (struct sockaddr *)&destination, sizeof(destination));

      // Check if the packet was sent successfully
      if (sent < 0) {
        perror("sendto");
        return 0;
      }

      // Define variable for storing sockets to listen to
      fd_set readfds;

      // Clear out the set, then add the ICMP and TCP sockets
      FD_ZERO(&readfds);
      FD_SET(icmp_sock, &readfds);
      FD_SET(tcp_sock, &readfds);

      // Define the max file descriptor then add 1 for select(), tcp_sock
      // because created last
      int maxfd = tcp_sock + 1;

      // Define a time interval structure for setting the timeout for select()
      struct timeval tv;

      // Define the timeout as 10.5 seconds
      tv.tv_sec = 3;
      tv.tv_usec = 500000;

      // Listen to the ICMP and TCP sockets simultaneously
      int rv = select(maxfd, &readfds, NULL, NULL, &tv);

      // Handle the select() function return
      if (rv == -1) {
        perror("select");
      } else if (rv == 0) {
        printf("Timeout occurred\n");
      } else {
        // If the ICMP socket has data
        if (FD_ISSET(icmp_sock, &readfds)) {
          // Define local variables
          char icmp_buffer[4096];
          struct sockaddr_in recv_addr;
          socklen_t addr_length = sizeof recv_addr;

          // Read the data on the ICMP sockeet
          int read = recvfrom(icmp_sock, icmp_buffer, sizeof(icmp_buffer), 0,
                              (struct sockaddr *)&recv_addr, &addr_length);

          // Check if the data was received successfully
          if (read < 0) {
            perror("sendto");
            return 0;
          }

          // Get the current time and apply it to recv_time before receiving
          gettimeofday(&recv_time, NULL);

          // Calculate the Round Trip Time (RTT) in milliseconds
          rtt = (recv_time.tv_sec - send_time.tv_sec) * 1000.0;
          rtt += (recv_time.tv_usec - send_time.tv_usec) / 1000.0;

          // Convert the binary address to a string
          inet_ntop(AF_INET, &recv_addr.sin_addr, addrstr, sizeof addrstr);
        }

        // If the TCP socket has data
        if (FD_ISSET(tcp_sock, &readfds)) {
          // Define local variables
          char tcp_buffer[4096];
          struct sockaddr_in recv_addr;
          socklen_t addr_length = sizeof recv_addr;

          // Read the data on the TCP socket
          int read = recvfrom(tcp_sock, tcp_buffer, sizeof(tcp_buffer), 0,
                              (struct sockaddr *)&recv_addr, &addr_length);

          // Check if the data was received successfully
          if (read < 0) {
            perror("sendto");
            return 0;
          }

          // Get the current time and apply it to recv_time before receiving
          gettimeofday(&recv_time, NULL);

          // Calculate the Round Trip Time (RTT) in milliseconds
          rtt = (recv_time.tv_sec - send_time.tv_sec) * 1000.0;
          rtt += (recv_time.tv_usec - send_time.tv_usec) / 1000.0;

          // Format the beginning of the received message to an IP header
          struct iphdr *tcp_ip_header = (struct iphdr *)tcp_buffer;

          // Get the length of the IP header
          int tcp_ip_header_length = tcp_ip_header->ihl * 4;

          // Format the message following the IP header to a TCP header
          struct tcphdr *tcp_tcp_header =
              (struct tcphdr *)(tcp_buffer + tcp_ip_header_length);

          // printf("%d\n", src_addr.sin_addr.s_addr);
          // printf("%d\n", tcp_ip_header->saddr);

          // if (tcp_ip_header->saddr == src_addr.sin_addr.s_addr) {
          //   printf("HERE\n");
          //   continue;
          // }

          // Convert the binary address to a string
          inet_ntop(AF_INET, &recv_addr.sin_addr, addrstr, sizeof addrstr);

          // Extract the SYN, ACK, and RST flags from the TCP header
          // SYN/ACK or RST means we have reached the server
          synack = (tcp_tcp_header->syn && tcp_tcp_header->ack);
          rst = tcp_tcp_header->rst;
        }
      }

      // If this is the first probe
      if (probe == 1) {
        // Assign the rtt and copy the address to the "first" variables
        first_time = rtt;
        strncpy(first_addr, addrstr, INET_ADDRSTRLEN);
        // strncpy(first_addr, addrstr, INET_ADDRSTRLEN - 1);
        // second_addr[INET_ADDRSTRLEN - 1] = '\0';
      }

      // If this is the second probe
      else if (probe == 2) {
        // Assign the rtt and copy the address to the "second" variables
        second_time = rtt;
        strncpy(second_addr, addrstr, INET_ADDRSTRLEN);
        // strncpy(second_addr, addrstr, INET_ADDRSTRLEN - 1);
        // second_addr[INET_ADDRSTRLEN - 1] = '\0';
      }

      // If this is the third probe
      else if (probe == 3) {
        // Define booleans comparing the different addresses
        bool first_second = strcmp(first_addr, second_addr) == 0;
        bool first_third = strcmp(first_addr, addrstr) == 0;
        bool second_third = strcmp(second_addr, addrstr) == 0;

        // Print result based on different addresses
        if (first_second && first_third && second_third) {
          printf("(%s)  %.3f ms %.3f ms %.3f ms\n", addrstr, first_time,
                 second_time, rtt);
          if (synack || rst) {
            terminate = true;
          }
        } else if (first_second && !first_third && !second_third) {
          printf("(%s)  %.3f ms  %.3f ms  (%s)  %.3f ms\n", first_addr,
                 first_time, second_time, addrstr, rtt);
        } else if (!first_second && first_third && !second_third) {
          printf("(%s)  %.3f ms  %.3f ms  (%s)  %.3f ms\n", addrstr, first_time,
                 rtt, second_addr, second_time);
        } else if (!first_second && !first_third && second_third) {
          printf("(%s)  %.3f ms  (%s)  %.3f ms  %.3f ms\n", first_addr,
                 first_time, addrstr, second_time, rtt);
        } else if (!first_second && !first_third && !second_third) {
          printf("(%s)  %.3f ms  (%s)  %.3f ms  (%s) %.3f ms\n", first_addr,
                 first_time, second_addr, second_time, addrstr, rtt);
        }
      }
    }

    // If we have received a SYNACK or RST, break
    if (terminate) {
      break;
    }
  }

  return 0;
}
