// Define POSIX standards for strdup() and getaddrinfo() functions
#define _POSIX_C_SOURCE 200809L

// Define for GNU extension to get strcasestr() function
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Define a struct so multiple arguments can be passed with threading
typedef struct {
  int part;
  char *host;
  char *path;
  int start;
  int end;
} ThreadArguments;

void *range_download(void *arg) {
  ThreadArguments *args = (ThreadArguments *)arg;
  // Define struct for URL
  struct addrinfo hints, *res;

  // Fill hints with zeros so that we can specidy sokcet type
  memset(&hints, 0, sizeof hints);

  // Define hints, AF_INET=IPV4, SOCK_STREAM=TCP
  hints.ai_family = AF_INET;  // IPv4
  hints.ai_socktype = SOCK_STREAM;

  // Get the resolved URL
  int ip = getaddrinfo(args->host, NULL, &hints, &res);

  // Check that the URL resolved correctly
  if (ip != 0) {
    fprintf(stderr, "\nCouldn't resolve URL or IP: %s\n", gai_strerror(ip));
    return NULL;
  }

  // Define the socket, AF_INET=IPv4, SOCK_STREAM=TCP
  int sock = socket(AF_INET, SOCK_STREAM, 0);

  // Check that the socket was created successfully
  if (sock < 0) {
    printf("\nFailed to Create Socket\n");
    return NULL;
  }

  // Define struct for server address
  struct sockaddr_in server;

  // Define the server struct, htons() converts the default port for https to
  // big-endian
  server.sin_family = AF_INET;
  server.sin_port = htons(443);

  // Cast the binary IP to a sockaddr_in struct and define sin_addr for the
  // server
  server.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;

  // Connect to server, convert the sockaddr_in -> sockaddr for generality
  int conn = connect(sock, (struct sockaddr *)&server, sizeof(server));

  // Check that the connection was successful
  if (conn < 0) {
    printf("\nConnection Failed\n");
    return NULL;
  }

  // Initialize the SSL Configuration
  SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());

  // Create a new TLS session
  SSL *ssl = SSL_new(ctx);

  // Set the TLS SNI
  SSL_set_tlsext_host_name(ssl, args->host);

  // Bind the TLS session to the TCP socket
  SSL_set_fd(ssl, sock);

  // Connect the TLS session
  SSL_connect(ssl);

  // Define a buffer to hold the output file string
  char output[17];

  // Define the output string by appending the part number
  sprintf(output, "part_%d", (args->part + 1));

  // Define and open the file for saving the image
  FILE *fp = fopen(output, "wb");

  // Check that the file was opened/created successfully
  if (!fp) {
    perror("fopen");
    return NULL;
  }

  // Define a buffer for the request
  char request[1024];

  // Define a GET request with ranging with the host, path, start and end bytes
  snprintf(request, sizeof(request),
           "GET /%s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "User-Agent: Mozilla/5.0"
           "(X11; Linux x86_64) AppleWebKit/537.36 "
           "(KHTML, like Gecko) Chrome/140.0.0.0 "
           "Safari/537.36 Edg/140.0.0.0\r\n"
           "Range: bytes=%d-%d\r\n"
           "Connection: close\r\n\r\n",
           args->path, args->host, args->start, args->end);

  // Print the HTTP Request defined above
  printf("\nHTTP GET Request #%d\n---------\n%s", (args->part + 1), request);

  // Send the request
  SSL_write(ssl, request, strlen(request));

  // Define variables, buffer for response, flag for processing header, and
  // bytes for length of response
  char response[4096];
  int header_done = 0;
  int bytes;

  // Read 4096 bytes of the full response incrementally until it has been fully
  // processed
  while ((bytes = SSL_read(ssl, response, sizeof(response))) > 0) {
    // Define modifiable variables to hold location of response and length of
    // response
    char *data = response;
    int data_len = bytes;

    // Enter loop to process header if it has not been done yet
    if (!header_done) {
      // Find the end of header
      char *body = strstr(response, "\r\n\r\n");

      // If the end of the header has been found
      if (body) {
        // Find the content type in the header to only print the status
        char *status = strcasestr(response, "content-type:");

        // Print status of the request
        printf("\nHTTP GET Status #%d\n---------\n", (args->part + 1));
        fwrite(response, 1, ((status - 2) - response), stdout);
        printf("\n\n");

        // Change data to point to the end of the header
        data = body + 4;

        // Recalculate the length of the response without the header
        data_len = bytes - (data - response);

        // Set header_done flag
        header_done = 1;
      }
      // If the end of the header has not been found, restart the while loop
      else {
        continue;
      }
    }

    // Write the received binary data to the output location, not including
    // header
    fwrite(data, 1, data_len, fp);
  }

  // Close the file once all the data has been received
  fclose(fp);

  // Close the TLS session
  SSL_free(ssl);

  // Release the TLS configuration object
  SSL_CTX_free(ctx);

  // Close the Socket
  close(sock);

  return NULL;
}

int main(int argc, char *argv[]) {
  // Define command-line arguments default values
  char *url =
      "https://arxiv.org/static/browse/0.3.4/images/"
      "arxiv-logo-one-color-white.svg";
  int num_parts = 5;
  char *output = "image.jpg";

  // Parse passed arguments, if any
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-u") == 0) {
      url = argv[++i];
    } else if (strcmp(argv[i], "-n") == 0) {
      num_parts = atoi(argv[++i]);
    } else if (strcmp(argv[i], "-o") == 0) {
      output = argv[++i];
    }
  }

  // Print Arguments
  printf("\nArguments\n----------\nURL: %s\n", url);
  printf("Number of Parts: %d\n", num_parts);
  printf("Output: %s\n", output);

  // // Make a writable copy of the URL
  char *url_copy = strdup(url);

  // Point where there is "://", if any
  char *host = strstr(url_copy, "://");

  // If there is "://", change the pointer to exclude it
  if (host != NULL) {
    host += 3;
  }

  // If there is not "://", point to the beginning of the URL
  else {
    host = url;
  }

  // Find if there is a "/" after the domain name
  char *slash = strchr(host, '/');

  // Define the path pointer variable
  char *path;

  // If there is a "/"
  if (slash) {
    // Place a null terminator so the host pointer excludes the path
    *slash = '\0';
    // Define path to point to everything after the slash
    path = slash + 1;
  }

  // If there is not a "/", assign path to be empty
  else {
    path = "";
  }

  // Print host and path from URL
  printf("\nExtracted URL Info\n----------\nHost: %s\n", host);
  printf("Path: %s\n", path);

  // Define struct for URL
  struct addrinfo hints, *res;

  // Fill hints with zeros so that we can specidy sokcet type
  memset(&hints, 0, sizeof hints);

  // Define hints, AF_INET=IPV4, SOCK_STREAM=TCP
  hints.ai_family = AF_INET;  // IPv4
  hints.ai_socktype = SOCK_STREAM;

  // Get the resolved URL or IP address
  int ip = getaddrinfo(host, NULL, &hints, &res);

  // Check that the URL resolved correctly
  if (ip != 0) {
    fprintf(stderr, "\nCouldn't resolve URL or IP: %s\n", gai_strerror(ip));
    return -1;
  }

  // Define the socket, AF_INET=IPv4, SOCK_STREAM=TCP
  int sock = socket(AF_INET, SOCK_STREAM, 0);

  // Check that the socket was created successfully
  if (sock < 0) {
    printf("\nFailed to Create Socket\n");
    return -1;
  }

  // Define struct for server address
  struct sockaddr_in server;

  // Define the server struct, htons() converts the default port for https to
  // big-endian
  server.sin_family = AF_INET;
  server.sin_port = htons(443);

  // Cast the binary IP to a sockaddr_in struct and define sin_addr
  server.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;

  // Connect to server, convert the sockaddr_in -> sockaddr for generality
  int conn = connect(sock, (struct sockaddr *)&server, sizeof(server));

  // Check that the connection was successful
  if (conn < 0) {
    printf("\nConnection Failed\n");
    return -1;
  }

  // Initialize the SSL Configuration
  SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());

  // Create a new TLS session
  SSL *ssl = SSL_new(ctx);

  // Set the TLS SNI
  SSL_set_tlsext_host_name(ssl, host);

  // Bind the TLS session to the TCP socket
  SSL_set_fd(ssl, sock);

  // Connect the TLS session
  SSL_connect(ssl);

  // Define a buffer for the request
  char request[1024];

  // Define a HEAD request with the host and path
  snprintf(request, sizeof(request),
           "HEAD /%s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "User-Agent: Mozilla/5.0"
           "(X11; Linux x86_64) AppleWebKit/537.36 "
           "(KHTML, like Gecko) Chrome/140.0.0.0 "
           "Safari/537.36 Edg/140.0.0.0\r\n"
           // "Accept: */*\r\n"
           "Connection: close\r\n\r\n",
           path, host);

  // Print the HTTP Request defined above
  printf("\nHTTP Head Request\n----------\n%s", request);

  // Send the request
  SSL_write(ssl, request, strlen(request));

  // Define variables, buffer for response, bytes for length of response, and
  // file_size for returned file size
  char response[4096];
  int bytes;
  int file_size = 0;

  // Read 4096 bytes of the full response incrementally until it has been fully
  // processed
  while ((bytes = SSL_read(ssl, response, sizeof(response))) > 0) {
    // Print the received header to the terminal
    printf("HTTP Head Response\n----------\n");
    fwrite(response, 1, bytes, stdout);

    // // Make a writable copy of the response
    char *response_copy = strdup(response);

    // Find content length in the copy of the response
    char *content_length = strcasestr(response_copy, "content-length:");

    // If content_length is found in the response
    if (content_length) {
      // Add 16 to point to the first integer of content length label
      content_length += 16;

      // Find date in the response
      char *date = strstr(content_length, "date");

      // If date is found
      if (date) {
        // Subtract 2 to point to the last integer of content-length
        date -= 2;
        // Add a null terminator after the last integer so the content_length
        // string only has the file size
        *date = '\0';
      }

      // Convert the file length extracted from the header to an integer
      file_size = atoi(content_length);
    }
  }

  // Close the TLS session
  SSL_free(ssl);

  // Release the TLS configuration object
  SSL_CTX_free(ctx);

  // Close the Socket
  close(sock);

  // Calculate the size of download for each thread
  int part_size = file_size / num_parts;

  // Calculate the remainder of the file size
  int remainder = file_size - (part_size * num_parts);

  // Define number of threads equal to num_parts
  pthread_t threads[num_parts];

  // Define ThreadArguments structs equal to num_parts
  ThreadArguments args[num_parts];

  // Define variable for loop to store previous end byte
  int prev_end = 0;

  // Loop for num_parts
  for (int i = 0; i < num_parts; i++) {
    // Calculate the first byte by assigning the previous end byte
    int start = prev_end;

    // Calculate the end byte by adding the part size to the start byte
    int end = start + part_size - 1;

    // If it is the last part, add the remainder as well to the end byte
    if (i == (num_parts - 1)) {
      end += remainder;
    }

    // Assign the current end to prev_end for the next loop iteration, add 1 so
    // it starts at the next byte
    prev_end = end + 1;

    // Fill the values of the struct to pass multiple arguments to thread
    args[i].part = i;
    args[i].host = host;
    args[i].path = path;
    args[i].start = start;
    args[i].end = end;

    // Create a thread for the current loop and args for range download
    pthread_create(&threads[i], NULL, range_download, &args[i]);
  }

  // Join the threads above
  for (int i = 0; i < num_parts; i++) {
    pthread_join(threads[i], NULL);
  }

  // Open the output file for writing
  FILE *fp = fopen(output, "wb");

  // Check that the output file was opened successfully
  if (!fp) {
    perror("fopen output_file");
    return -1;
  }

  // Loop for num_parts
  for (int i = 0; i < num_parts; i++) {
    // Define the output string by appending the part number
    char file[17];
    sprintf(file, "part_%d", (i + 1));

    // Open one of the thread output files for reading
    FILE *in_fp = fopen(file, "rb");

    // Check that the current thread output file was opened successfully
    if (!in_fp) {
      perror("fopen input file");
      continue;
    }

    // Define variables, buffer for holding read info, and bytes for size of
    // file
    char buffer[4096];
    size_t bytes;

    // Read the current thread output file until finished
    while ((bytes = fread(buffer, 1, sizeof(buffer), in_fp)) > 0) {
      // Write the read info to the overall output file
      fwrite(buffer, 1, bytes, fp);
    }

    // Close the current thread output file
    fclose(in_fp);
  }

  // Close the overall output file
  fclose(fp);

  return 0;
}
