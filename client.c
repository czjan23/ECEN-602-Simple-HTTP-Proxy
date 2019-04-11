#include "message.h"

static char *g_url;
static char *g_page;
static char *g_file;

static void print_err_and_exit(char *err_message) {
   fprintf(stdout, "Error: %s\n", err_message);
   exit(1);
}

void handle_respond(int socket_fd) {
   char buffer[Message_max_size] = {0};
   char *respond_head = NULL;
   char *respond_data = NULL;
   int numBytes = 0;
   int n = 0;

   while(1) {
      memset(buffer, 0, sizeof(buffer));
      numBytes = 0;

      numBytes = readline(socket_fd, buffer, Message_max_size - 1);

      if(numBytes > 0) {
         buffer[numBytes] = 0;

         if(respond_head == NULL) {
            respond_head = malloc(numBytes);
         } else {
            respond_head = realloc(respond_head, n + numBytes);
         }

         memcpy(respond_head + n, buffer, numBytes);

         n += numBytes;

         //fprintf(stdout, "%s", buffer);

         if(buffer[0] == '\r' && buffer[1] == '\n') {
            //fprintf(stdout, "read_header end here\n");
            break;
         }
      }
   }

   n = 0;

   FILE *fp = fopen(g_file, "wb");

   if(fp == NULL) {
      perror("fopen");
      return;
   }

   while(1) {
      memset(buffer, 0, sizeof(buffer));
      numBytes = 0;

      numBytes = recv(socket_fd, buffer, Message_max_size - 1, 0);

      //fprintf(stdout, "%s", buffer);

      if(numBytes <= -1) {
         break;
      } else if(numBytes == 0) {
         break;
      }

      if(respond_data == NULL) {
         respond_data = malloc(numBytes);
      } else {
         respond_data = realloc(respond_data, n + numBytes);
      }

      memcpy(respond_data + n, buffer, numBytes);

      n += numBytes;

      fwrite(buffer, 1, numBytes, fp);

      if(numBytes < (Message_max_size - 1)) {
         fprintf(stdout, "Received file successfully\n");
         break;
      }
   }

   fprintf(stdout, "The file is saved as %s\n", g_file);

   //fprintf(stdout, "\nread_data Bytes %d\n", n);

   if(respond_head != NULL) {
      free(respond_head);
   }

   if(respond_data != NULL) {
      free(respond_data);
   }

   fclose(fp);

   free(g_url);
   free(g_page);
}

//send request
void send_request(int socket_fd, char *data)
{
   send(socket_fd, data, strlen(data), 0);
}

void parse_url(char *url)
{
   char *http = strstr(url, "://");

   if(http == NULL)
   {
      print_err_and_exit("parse_url url error");
   }

   char *page = strstr(http + 3, "/"); //escape
   char *host;

   if(page == NULL) {
      print_err_and_exit("parse_url http error");
   } else {
      g_page = strdup(page);
      host = strtok(http + 3, "/");
      g_url = strdup(host);
   }

   g_file = g_page + 1;
   char *p = g_file;

   while(p) {
      p = strstr(p, "/");

      if(p != NULL) {
         p++;
         g_file = p;
      }
   }

   //fprintf(stdout, "%s %s %s\n", g_url, g_page, g_file);
}

void get_request(char *request) {
   if(request == NULL) {
      print_err_and_exit("get_request request error");
   }

   sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", g_page, g_url);

   //fprintf(stdout, "request: %s", request);
}

// receive server message
static void socket_fd_isset(int socket_fd) {
   char buf[Message_max_size] = {0};
   memset(buf, 0, sizeof(buf));

   ssize_t len = read(socket_fd, buf, Message_max_size - 1); //read server message

   if(len < 0) {
      print_err_and_exit("readline error");
   } else if(len > 0) {
      buf[len] = 0;
   } else if(len == 0) {
      print_err_and_exit("closed");
   }
}

// receive command line message
void stdin_fd_isset(int socket_fd) {
   char buf[Message_max_size] = {0};

   char *str = fgets(buf, sizeof(buf) - 1, stdin); //read command line message

   if(str != NULL) {
      puts(buf);
   }
}

int main(int argc, char *argv[]) {
   if(argc != 4) {
      print_err_and_exit("usage: echos <proxy ip> <proxy port> <URL to retrieve>\n");
   }

   fd_set readfds;
   int result;

   int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

   if(socket_fd < 0) {
      print_err_and_exit("socket error");
   }

   struct sockaddr_in client_addr;

   client_addr.sin_family = AF_INET;

   inet_pton(AF_INET, argv[1], &client_addr.sin_addr);

   client_addr.sin_port = htons(atoi(argv[2]));

   // connect to server
   int connection_fd = connect(socket_fd, (struct sockaddr *)&client_addr, sizeof client_addr);

   if(connection_fd < 0) {
      print_err_and_exit("connect error");
   }

   fprintf(stdout, "Connecting to %s sucdessfully!\n", argv[1]);
   fprintf(stdout, "Sending request to proxy server:\n%s%s\n", argv[1], argv[2]);
   fprintf(stdout, "Target file:%s\n", argv[3]);

   parse_url(argv[3]);

   char request[Message_max_size] = {0};
   get_request(request);

   send_request(socket_fd, request);

   handle_respond(socket_fd);

   while(1) {
      FD_ZERO(&readfds); //clear

      FD_SET(STDIN_FILENO, &readfds); //add SRDIN_FILENO
      FD_SET(socket_fd, &readfds); //add socket_fd

      result = select(FD_SETSIZE, &readfds, 0, 0, NULL); //check new message

      if(result < 0) {
         if(errno != EINTR) {
            fprintf(stderr, "Error in select(): %d\n", errno);
         }
      } else if(result > 0) { //receive new message
         if(FD_ISSET(STDIN_FILENO, &readfds)) { //receive command line message
            stdin_fd_isset(socket_fd);
         } else if(FD_ISSET(socket_fd, &readfds)) { //receive server message
            socket_fd_isset(socket_fd);
         }
      }
   }

   close(socket_fd);
}
