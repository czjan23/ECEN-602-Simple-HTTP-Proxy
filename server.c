#include "message.h"

static connection_t connList_head; //head for list

static http_cache_t cacheList_head;

#define MAX_CONN_NUM 100 //max connection
static short conn_num = 0; //current connection

static void print_err_and_exit(char *err_message) {
   fprintf(stdout, "Error: %s\n", err_message);
   exit(1);
}

// new client connection
void connection_new_incoming(int sock,
                             struct sockaddr *addr,
                             size_t addrLen) {
   connection_t *connP;

   connP = (connection_t *)malloc(sizeof(connection_t));

   if(connP != NULL) {
      connP->sock = sock;
      memcpy(&(connP->addr), addr, addrLen);
      connP->addrLen = addrLen;

      list_add_tail(&(connP->list), &(connList_head.list)); // save new connection to list
   }
}

// release list memory
void connection_free() {
   struct list_head *pos, *next;
   connection_t *connP;

   list_for_each_safe(pos, next, &(connList_head.list)) {
      connP = list_entry(pos, connection_t, list);
      list_del_init(pos);

      free(connP);
      connP = NULL;
   }
}

// delete a node from list
void connection_delete(int sock) {
   struct list_head *pos, *next;
   connection_t *connP;

   list_for_each_safe(pos, next, &(connList_head.list))
   {
      connP = list_entry(pos, connection_t, list);

      if(connP->sock == sock) { //delete
         // list_del_init(struct list_head * entry)
         list_del_init(pos);
         free(connP);
         connP = NULL;
      }
   }
}

// get current time
void localtime_get(char *t, int len) {
   time_t rawtime;
   struct tm *timeinfo;

   time(&rawtime);

   timeinfo = localtime(&rawtime);
   strftime(t, len, "%a, %d %b %Y %H:%M:%S %Z", timeinfo);
   //printf("%s\n", t);
}

// str to timestamp
long time_get(char *time) {
   struct tm timeinfo;

   char *r = strptime(time, "%a, %d %b %Y %H:%M:%S %Z", &timeinfo);

   if(r == NULL) {
      return 0;
   }

   long t = mktime(&timeinfo);

   //printf("%ld\n", t);

   return t;
}

// get cache list size
int cache_size() {
   struct list_head *pos;

   int i = 0;

   // list_for_each(pos, head)
   list_for_each(pos, &(cacheList_head.list)) {
      i++;
   }

   return i;
}

// clean expired cache
void cache_delete_one() {
   //fprintf(stdout, "cache_delete_one here\n");

   struct list_head *pos, *next;
   http_cache_t *cache;
   long last_accessed = 0;
   long t = 0;

   list_for_each(pos, &(cacheList_head.list)) {
      cache = list_entry(pos, http_cache_t, list);

      t = time_get(cache->last_accessed);

      if(last_accessed == 0) {
         last_accessed = t;
      } else if(last_accessed > t) { // find expired cache
         last_accessed = t;
      }
   }

   list_for_each_safe(pos, next, &(cacheList_head.list)) {
      cache = list_entry(pos, http_cache_t, list);
      if(last_accessed == time_get(cache->last_accessed)) { // delete expired cache
         list_del_init(pos);
         free(cache->respond_head);
         free(cache->respond_data);
         free(cache);
         cache = NULL;
         return;
      }
   }
}

// look up cache list to find if node is in list
int cache_find(const connection_t *connP) {
   struct list_head *pos;
   http_cache_t *cache;

   // list_for_each(pos, head)
   list_for_each(pos, &(cacheList_head.list)) {
      // list_entry(ptr, type, member)
      cache = list_entry(pos, http_cache_t, list);

      if(strcmp(cache->domain_name, connP->host) == 0 &&
         strcmp(cache->path, connP->url) == 0) {
         return 1;
      }
   }

   return 0;
}

// look up cache list to check if expires
int cache_check(const connection_t *connP) {
   int ret = 0;
   struct list_head *pos;
   http_cache_t *cache;

   // list_for_each(pos, head)
   list_for_each(pos, &(cacheList_head.list)) {
      // list_entry(ptr, type, member)
      cache = list_entry(pos, http_cache_t, list);

      if(strcmp(cache->domain_name, connP->host) == 0 &&
         strcmp(cache->path, connP->url) == 0) {
         long t1 = time_get(cache->last_accessed);
         long t2 = 0;
         long t3 = 0;

         if(strlen(cache->Expires) > 0) { //Expires is not null
            t3 = time_get(cache->Expires);

            if(t1 > t3) { // cache expires
               fprintf(stdout, "cache_find expired\n");
               ret = 0;
            } else { // not expire
               ret = 1;
            }
         }
         else { // no Expire, let proxy to determine expire or not
            time_t t;
            time(&t); //current time

            if(t1 < (t - Expires_TIME)) { //last accessd more than 24 hours ago
               ret = 0;
            } else {
               ret = 1;
            }

            if(strlen(cache->Last_Modified) > 0) { // check if it has last_modified
               t2 = time_get(cache->Last_Modified);

               if(t2 > (t - Last_Modified_TIME)) { //last modifed less than 1 month
                  ret = 0;
               } else {
                  ret = 1;
               }
            } else {
               ret = 0;
            }
         }

         break;
      }
   }

   if (ret == 0) {
      fprintf(stdout, "The requested file expires! Discard it from cache!\n");
   }

   return ret;
}

// print cache list
void cache_print() {
   struct list_head *pos;
   http_cache_t *cache;
   int i = 0;

   fprintf(stdout, "=============== Cache List ==============\n");

   list_for_each(pos, &(cacheList_head.list)) {
      cache = list_entry(pos, http_cache_t, list);

      i++;
      fprintf(stdout, "%d. %s %s\n", i, cache->domain_name, cache->path);

      if(strlen(cache->last_accessed) == 0) {
         fprintf(stdout, "last_accessed: N/A\n");
      } else {
         fprintf(stdout, "last_accessed: %s\n", cache->last_accessed);
      }

      if(strlen(cache->Last_Modified) == 0) {
         fprintf(stdout, "Last_Modified: N/A\n");
      } else {
         fprintf(stdout, "Last_Modified: %s\n", cache->Last_Modified);
      }

      if(strlen(cache->Expires) == 0) {
         fprintf(stdout, "Expires: N/A\n");
      } else {
         fprintf(stdout, "Expires: %s\n", cache->Expires);
      }
   }

   fprintf(stdout, "================ End List ===============\n");
}

void cache_init(const connection_t *connP) {
   http_cache_t *cache;

   if(cache_find(connP) == 1) { // already has this node in list
      return;
   }

   int size = cache_size();

   if(size >= CACHE_MAX_SIZE) {
      cache_delete_one();
   }

   cache = (http_cache_t *)malloc(sizeof(http_cache_t));

   if(cache != NULL) {
      strcpy(cache->domain_name, connP->host);
      strcpy(cache->path, connP->url);

      list_add_tail(&(cache->list), &(cacheList_head.list)); //add new node to list
   }
}

// add head to cache list
void cache_add_head(const connection_t *connP, char *respond_head) {
   struct list_head *pos;
   http_cache_t *cache;

   if(respond_head == NULL) {
      return;
   }

   // list_for_each(pos, head)
   list_for_each(pos, &(cacheList_head.list)) {
      // list_entry(ptr, type, member)
      cache = list_entry(pos, http_cache_t, list);

      if(strcmp(cache->domain_name, connP->host) == 0 &&
         strcmp(cache->path, connP->url) == 0) {
         cache->respond_head = respond_head;
         break;
      }
   }
}

// add data to cache list
void cache_add_data(const connection_t *connP, char *respond_data) {
   struct list_head *pos;
   http_cache_t *cache;

   if(respond_data == NULL) {
      return;
   }

   // list_for_each(pos, head)
   list_for_each(pos, &(cacheList_head.list)) {
      // list_entry(ptr, type, member)
      cache = list_entry(pos, http_cache_t, list);

      if(strcmp(cache->domain_name, connP->host) == 0 &&
         strcmp(cache->path, connP->url) == 0) {
         cache->respond_data = respond_data;
         break;
      }
   }
}

// add last_accessed to cache
void cache_add_last_accessed(const connection_t *connP) {
   struct list_head *pos;
   http_cache_t *cache;
   char last_accessed[128] = {0};

   localtime_get(last_accessed, sizeof(last_accessed));

   // list_for_each(pos, head)
   list_for_each(pos, &(cacheList_head.list)) {
      // list_entry(ptr, type, member)
      cache = list_entry(pos, http_cache_t, list);

      if(strcmp(cache->domain_name, connP->host) == 0 &&
         strcmp(cache->path, connP->url) == 0) {
         strcpy(cache->last_accessed, last_accessed);
         break;
      }
   }
}

// add last_modified to cache list
void cache_add_Last_Modified(const connection_t *connP, const char *Last_Modified) {
   struct list_head *pos;
   http_cache_t *cache;

   if(Last_Modified == NULL) {
      return;
   }

   // list_for_each(pos, head)
   list_for_each(pos, &(cacheList_head.list)) {
      // list_entry(ptr, type, member)
      cache = list_entry(pos, http_cache_t, list);

      if(strcmp(cache->domain_name, connP->host) == 0 &&
         strcmp(cache->path, connP->url) == 0) {
         strcpy(cache->Last_Modified, Last_Modified);
         break;
      }
   }
}

// add expires to cache
void cache_add_Expires(const connection_t *connP, const char *Expires) {
   struct list_head *pos;
   http_cache_t *cache;

   // list_for_each(pos, head)
   list_for_each(pos, &(cacheList_head.list)) {
      // list_entry(ptr, type, member)
      cache = list_entry(pos, http_cache_t, list);

      if(strcmp(cache->domain_name, connP->host) == 0 &&
         strcmp(cache->path, connP->url) == 0) {
         if(Expires != NULL) {
            strcpy(cache->Expires, Expires);
         }

         break;
      }
   }
}

// read http header from cache
void cache_get_head(const connection_t *connP) {
   struct list_head *pos;
   http_cache_t *cache;

   // list_for_each(pos, head)
   list_for_each(pos, &(cacheList_head.list)) {
      // list_entry(ptr, type, member)
      cache = list_entry(pos, http_cache_t, list);

      if(strcmp(cache->domain_name, connP->host) == 0 &&
         strcmp(cache->path, connP->url) == 0) {
         write(connP->sock, cache->respond_head, strlen(cache->respond_head));
         break;
      }
   }
}

// read http data from cache
void cache_get_data(const connection_t *connP) {
   struct list_head *pos;
   http_cache_t *cache;

   // list_for_each(pos, head)
   list_for_each(pos, &(cacheList_head.list)) {
      // list_entry(ptr, type, member)
      cache = list_entry(pos, http_cache_t, list);

      if(strcmp(cache->domain_name, connP->host) == 0 &&
         strcmp(cache->path, connP->url) == 0) {
         write(connP->sock, cache->respond_data, strlen(cache->respond_data));
         break;
      }
   }
}

// broad message to connected client
void send_msg(connection_t *c, const char *Payload, short Length) {
   struct list_head *pos;
   connection_t *connP;

   if(!list_empty(&(connList_head.list))) {
      // list_for_each(pos, head)
      list_for_each(pos, &(connList_head.list)) {
         // list_entry(ptr, type, member)
         connP = list_entry(pos, connection_t, list);

         if(c != connP) {
            write(connP->sock, Payload, Length);
         }
      }
   }
}

// connect
int connect_to(connection_t *connP) {
   int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

   if(socket_fd < 0) {
      print_err_and_exit("socket error!");
   }

   // init father socket sockaddr_in
   struct sockaddr_in client_addr;
   client_addr.sin_family = AF_INET;
   client_addr.sin_port = htons(atoi(connP->port));

   char host[32] = {0};
   struct hostent *phost = NULL;

   phost = gethostbyname(connP->host);

   int i = 0;

   for(i = 0; phost->h_addr_list[i]; i++) {
      inet_ntop(phost->h_addrtype,  phost->h_addr_list[i], host, sizeof(host));
   }

   inet_pton(AF_INET, host, &client_addr.sin_addr);
   int connection_fd = connect(socket_fd, (struct sockaddr *)&client_addr, sizeof client_addr);

   if(connection_fd < 0) {
      fprintf(stderr, "connect error\n");
      return connection_fd;
   }

   fprintf(stdout, "connect_to %s successfully!\n", connP->host);

   return socket_fd;
}

// handle http respond packet
void handle_request(connection_t *connP, int client_fd) {
   char buffer[Message_max_size] = {0};
   char *respond_head = NULL;
   char *respond_data = NULL;
   int numBytes = 0;
   int n = 0;

   char *Last_Modified = "Last-Modified: ";
   char Last_Modified_value[64] = {0};

   char *Expires = "Expires: ";
   char Expires_value[64] = {0};

   fprintf(stdout, "Server response: ");

   while(1) {
      memset(buffer, 0, sizeof(buffer));
      numBytes = 0;

      numBytes = readline(client_fd, buffer, Message_max_size - 1);

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

         write(connP->sock, buffer, numBytes);

         if(memcmp(buffer, Last_Modified, strlen(Last_Modified)) == 0) {
            char *p = strtok(buffer + strlen(Last_Modified), "\r");
            strcpy(Last_Modified_value, p);
         } else if(memcmp(buffer, Expires, strlen(Expires)) == 0) {
            char *p = strtok(buffer + strlen(Last_Modified), "\r");
            strcpy(Expires_value, p);
         }

         if(buffer[0] == '\r' && buffer[1] == '\n') {
            //fprintf(stdout, "handle_request read_header end\n");
            break;
         }
      }
   }

   n = 0;

   while(1) {
      memset(buffer, 0, sizeof(buffer));
      numBytes = 0;

      numBytes = recv(client_fd, buffer, Message_max_size - 1, 0);

      if(numBytes <= -1) {
         break;
      } else if(numBytes == 0) {
         break;
      }

      //fprintf(stdout, "%s", buffer);

      if(respond_data == NULL) {
         respond_data = malloc(numBytes);
      } else {
         respond_data = realloc(respond_data, n + numBytes);
      }

      memcpy(respond_data + n, buffer, numBytes);

      n += numBytes;

      write(connP->sock, buffer, numBytes);
   }

   close(client_fd);

   cache_init(connP);
   cache_add_head(connP, respond_head);
   cache_add_data(connP, respond_data);
   cache_add_last_accessed(connP);
   cache_add_Last_Modified(connP, Last_Modified_value);
   cache_add_Expires(connP, Expires_value);

   fprintf(stdout, "File is cached successfully\n");
   fprintf(stdout, "Sent file to client successfully\n");

   //fprintf(stdout, "\nread_data Bytes %d\n", n);
}

// every loop, set all to fd again
void fd_set_all(fd_set *readfds) {
   struct list_head *pos;
   connection_t *connP;

   if(!list_empty(&(connList_head.list))) {
      // list_for_each(pos, head)
      list_for_each(pos, &(connList_head.list)) {
         // list_entry(ptr, type, member)
         connP = list_entry(pos, connection_t, list);

         if(connP->sock > 0) {
            FD_SET(connP->sock, readfds);
         }
      }
   }
}

// receive cline info
void connect_fd_isset(fd_set *readfds) {
   struct list_head *pos, *next;
   connection_t *connP;
   char buffer[Message_max_size] = {0};
   int numBytes = 0;

   list_for_each_safe(pos, next, &(connList_head.list)) {
      connP = list_entry(pos, connection_t, list);

      memset(buffer, 0, sizeof(buffer));
      numBytes = 0;

      if(FD_ISSET(connP->sock, readfds)) {
         numBytes = readline(connP->sock, buffer, Message_max_size - 1);

         if(numBytes > 0) {
            buffer[numBytes] = 0;

            //fprintf(stdout, "connect_fd_isset read %d:\n%s", numBytes, buffer);

            http_read_header(connP, buffer, numBytes);
         } else if(numBytes == 0) { // client disconnect
            close(connP->sock);

            char s[INET6_ADDRSTRLEN] = {0};
            s[0] = 0;
            struct sockaddr_in *saddr = (struct sockaddr_in *)&connP->addr;
            inet_ntop(AF_INET, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
            //fprintf(stdout, "disconnect from [%d][%s]\n", connP->sock, s);

            list_del_init(pos); //delete current client from list
            free(connP);
            connP = NULL;

            conn_num--;
            break;
         }
      }
   }
}

// receive new client connection
static void socket_fd_isset(int socket_fd) {
   struct sockaddr_in connection_addr = {0};
   socklen_t connection_addr_len = INET6_ADDRSTRLEN;
   char reason[30] = {0};

   // accept connection
   int connection_fd = accept(socket_fd, (struct sockaddr *)&connection_addr, &connection_addr_len);

   if(connection_fd < 0) {
      print_err_and_exit("accept error!");
   }

   char s[INET6_ADDRSTRLEN] = {0};
   in_port_t port = 0;

   s[0] = 0;
   struct sockaddr_in *saddr = (struct sockaddr_in *)&connection_addr;
   inet_ntop(AF_INET, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
   port = saddr->sin_port;

   fprintf(stdout, "Client connected:\n%s:%hu\n", s, ntohs(port));

   if(conn_num >= MAX_CONN_NUM) { //check current connection num is max or not
      sprintf(reason, "Too many connections %d\n", conn_num);

      write(connection_fd, reason, strlen(reason));
      puts(reason);

      close(connection_fd);
   } else {
      conn_num++;
      connection_new_incoming(connection_fd, (struct sockaddr *)&connection_addr, connection_addr_len); //save new client to list
   }
}

int main(int argc, char **argv) {
   if(argc != 3) {
      print_err_and_exit("usage: echos ip port_number!");
   }

   fprintf(stdout, "Server initializing...\n");

   fd_set readfds;
   int result;

   INIT_LIST_HEAD(&(connList_head.list)); //init list
   INIT_LIST_HEAD(&(cacheList_head.list));

   int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

   if(socket_fd < 0) {
      print_err_and_exit("socket error!");
   }

   // init father socket sockaddr_in
   struct sockaddr_in server_addr;
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(atoi(argv[2]));
   inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

   // bind socket
   if(bind(socket_fd, (struct sockaddr *)&server_addr, sizeof server_addr) < 0) {
      print_err_and_exit("bind error!");
   }

   // scoket start listening
   if(listen(socket_fd, MAX_CONN_NUM + 1) < 0) {
      print_err_and_exit("listen error!");
   }

   while(1) {
      FD_ZERO(&readfds); //clear
      FD_SET(socket_fd, &readfds); //add socket_fd
      fd_set_all(&readfds); //add all client
      result = select(FD_SETSIZE, &readfds, 0, 0, NULL); //check new message

      if(result < 0) {
         if(errno != EINTR) {
            fprintf(stderr, "Error in select(): %d\n", errno);
         }
      } else if(result > 0) { // new message
         if(FD_ISSET(socket_fd, &readfds)) { // receive new connection
            socket_fd_isset(socket_fd);
         } else {
            connect_fd_isset(&readfds); //receive message from client
         }
      }
   }

   close(socket_fd);
   connection_free();
}
