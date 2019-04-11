#ifndef MESSAGE_H__
#define MESSAGE_H__

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <netdb.h>
#include "list.h"
#include <time.h>

enum states {
   METHOD,
   URL,
   VERSION,
   DONE
};

// http request type
enum http_methods_index {
   OPTIONS,
   GET,
   HEAD,
   POST,
   PUT,
   DELETE,
   TRACE,
   CONNECT,
   UNKNOWN
};

#define CACHE_MAX_SIZE 10 // lru size
#define Expires_TIME (60 * 60 * 24) //24 hours period for last_accessed
#define Last_Modified_TIME (60 * 60 * 24 * 30) //1 month period for Last_Modified
#define http_max_size 2048
#define Message_max_size 512 // default message max size

typedef struct {
   struct list_head list;
   char *key;
   char *value;
} http_data_t;

typedef struct { // data structure for cache data
   struct list_head list;
   char domain_name[64];
   char path[128];
   char *respond_head;
   char *respond_data;
   char last_accessed[64];
   char Last_Modified[64];
   char Expires[64];
} http_cache_t;

// data structure for list
typedef struct {
   struct list_head        list;
   int                     sock;
   struct sockaddr_in6     addr;
   size_t                  addrLen;
   enum http_methods_index index;
   char *url;
   char *host;
   char *port;
   char *version;
   http_data_t data_head;
} connection_t;

void http_read_header(connection_t *connP, const char *data, int len);
int connect_to(connection_t *connP);
void handle_request(connection_t *connP, int client_fd);
ssize_t readline(int socket_fd, void *buffer, size_t len);
int cache_check(const connection_t *connP);
void cache_print();
void cache_get_head(const connection_t *connP);
void cache_get_data(const connection_t *connP);
void cache_add_last_accessed(const connection_t *connP);

#endif
