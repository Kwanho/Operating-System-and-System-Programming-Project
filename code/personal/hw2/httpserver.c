#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */
void handle_files_request(int fd) {

  /* YOUR CODE HERE (Feel free to delete/modify the existing code below) */

  struct http_request *request = http_request_parse(fd);
  
  char *default_file_name = "/index.html";
  char *full_path = (char *)malloc(strlen(server_files_directory)+strlen(request->path)+1);
  strcpy(full_path, server_files_directory);
  strcat(full_path, "/");
  strcat(full_path, request->path);

  struct stat stat_buf;
  int stat_result = stat(full_path, &stat_buf);
  
  //data needed in future code
  int status_code = 0;
  int local_fd;
  char data[1024];
  ssize_t bytes_read;
  char *content_type;
  char *content_length_str = malloc(10 * sizeof(char));

  //int DATA_MAX_SIZE=9999999;

  
  if (stat_result==0 && S_ISREG(stat_buf.st_mode)) {  //request's path corresponds to a file
    status_code = 200;
    content_type = http_get_mime_type(full_path);
    sprintf(content_length_str, "%i", (int)stat_buf.st_size);  //int to string
    
    http_start_response(fd, status_code);
    http_send_header(fd, "Content-type", content_type);
    http_send_header(fd, "Content-Length", content_length_str);
    http_end_headers(fd);

    local_fd = open(full_path, O_RDONLY); //open the file

    if (strcmp(content_type, "text/html")==0 || strcmp(content_type, "text/css")==0) {
      bytes_read = read(local_fd, data, sizeof(data));
      while(bytes_read!=0){
        http_send_string(fd, data);
        bytes_read = read(local_fd, data, sizeof(data));
      } 
    } else {
      bytes_read = read(local_fd, data, sizeof(data));
      while(bytes_read!=0){
        http_send_data(fd, data, bytes_read);
        bytes_read = read(local_fd, data, sizeof(data)); //read in file content
      }    
    }
    close(local_fd);  //close the file

    close(fd);
  } else if (stat_result==0 && S_ISDIR(stat_buf.st_mode)) {  //request's path corresponds to a directory
    
    strcat(full_path, default_file_name); // file_name = "directory/index.html"

    if (access(full_path, F_OK)==0) { //index.html is found in directory
      status_code = 200;
      stat_result = stat(full_path, &stat_buf);
      content_type = http_get_mime_type(full_path);
      sprintf(content_length_str, "%i", (int)stat_buf.st_size);  //int to string

      http_start_response(fd, status_code);
      http_send_header(fd, "Content-type", content_type);
      http_send_header(fd, "Content-Length", content_length_str);
      http_end_headers(fd);

      local_fd = open(full_path, O_RDONLY);
      bytes_read = read(local_fd, data, sizeof(data));
      while(bytes_read!=0){
        http_send_string(fd, data);
        bytes_read = read(local_fd, data, sizeof(data));
      }
      close(local_fd);   
    } else {  //index.html is not found in directory
      status_code = 200;
      strcpy(full_path, server_files_directory);
      strcat(full_path, "/");
      strcat(full_path, request->path);

      DIR *directory_p = opendir(full_path);
      struct dirent *directory_entry_p;

      http_start_response(fd, status_code);
      http_send_header(fd, "Content-type", "text/html");

      sprintf(data, "<a href='../'>Parent directory</a>\n");
      http_send_string(fd, data);

      directory_entry_p = readdir(directory_p);
      while(directory_entry_p != NULL) {
        sprintf(data, "<a href='%s/'>/%s</a>\n", directory_entry_p->d_name, directory_entry_p->d_name);
        http_send_string(fd, data);
        directory_entry_p = readdir(directory_p);
      }     
      closedir(directory_p);
    }
  } else {
    status_code = 404;
    http_start_response(fd, status_code);
  }

}


/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  /* YOUR CODE HERE */

  //lookup IP address of the hostname
  struct hostent *he = gethostbyname(server_proxy_hostname);
  struct in_addr *IP_addr = (struct in_addr *)he->h_addr;

  //setting proxy target
  struct sockaddr_in target_addr;
  memset(&target_addr, 0, sizeof(target_addr));
  target_addr.sin_family = AF_INET;
  target_addr.sin_addr = *IP_addr;
  target_addr.sin_port = htons(server_proxy_port);

  //create a network socket and connect it to the IP address
  int target_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (connect(target_fd, (struct sockaddr *) &target_addr, sizeof(target_addr))==-1){
    dprintf(fd, "connection fail\n");
  }

  int max_fd;
  if (fd>target_fd) {
    max_fd = fd;
  } else {
    max_fd = target_fd;
  }

  fd_set read_fds;
  int num_ready_fd;
  char buffer[1024];
  int bytes_read;

  while(1) {
    //listen
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    FD_SET(target_fd, &read_fds);
    
    num_ready_fd = select(max_fd+1, &read_fds, NULL, NULL, NULL);

    if(num_ready_fd==-1) {
      perror("select error");
      exit(0);
      break;
    }

    //read then write
    if (FD_ISSET(fd, &read_fds)) {
      bytes_read = read(fd, buffer, sizeof(buffer));
      if (bytes_read <= 0) {
        break;
      }
      write(target_fd, buffer, bytes_read);
    } 
    if (FD_ISSET(target_fd, &read_fds)) {
      bytes_read = read(target_fd, buffer, sizeof(buffer));
      if (bytes_read <= 0) {
        break;
      }
      write(fd, buffer, bytes_read);
    }
  }
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;
  pid_t pid;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  while (1) {

    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);

    pid = fork();
    if (pid > 0) {
      close(client_socket_number);
    } else if (pid == 0) {
      // Un-register signal handler (only parent should have it)
      signal(SIGINT, SIG_DFL);
      close(*socket_number);
      request_handler(client_socket_number);
      close(client_socket_number);
      exit(EXIT_SUCCESS);
    } else {
      perror("Failed to fork child");
      exit(errno);
    }
  }

  close(*socket_number);

}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);

  /* Default settings */
  server_port = 8000;
  server_files_directory = malloc(1024);
  getcwd(server_files_directory, 1024);
  server_proxy_hostname = "inst.eecs.berkeley.edu";
  server_proxy_port = 80;

  void (*request_handler)(int) = handle_files_request;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
