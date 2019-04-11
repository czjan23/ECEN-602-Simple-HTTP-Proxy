#include "message.h"

int http_methods_len = 9;
const char *http_methods[] = {
   "OPTIONS",
   "GET",
   "HEAD",
   "POST",
   "PUT",
   "DELETE",
   "TRACE",
   "CONNECT",
   "INVALID"
};

void http_free(connection_t *connP) {
   free(connP->url);
   free(connP->host);
   free(connP->port);
   free(connP->version);

   http_data_t *data;
   struct list_head *pos, *next;

   list_for_each_safe(pos, next, &(connP->data_head.list)) {
      data = list_entry(pos, http_data_t, list);
      list_del_init(pos);
      free(data->key);
      free(data->value);
      free(data);
      data = NULL;
   }
}

// send cache info to server
void http_respond(const connection_t *connP) {
   //fprintf(stdout, "http_respond here\n");
   cache_get_head(connP);
   cache_get_data(connP);
}

// send http request to server
void http_request(connection_t *connP) {
   char buffer[http_max_size] = {0};
   sprintf(buffer, "GET %s %s\r\n", connP->url, connP->version);

   http_data_t *data;
   struct list_head *pos;

   if(!list_empty(&(connP->data_head.list))) {
      list_for_each(pos, &(connP->data_head.list)) {
         data = list_entry(pos, http_data_t, list);
         sprintf(buffer, "%s%s: %s\r\n", buffer, data->key, data->value);
      }
   }

   if(strcmp(connP->version, "HTTP/1.1") == 0) {
      strcat(buffer, "Connection: close\r\n");
   }

   strcat(buffer, "\r\n"); //end

   //fprintf(stdout, "http_request %zu:\n%s\n", strlen(buffer), buffer);

   int client_fd = connect_to(connP);

   if(client_fd < 0) {
      goto ERR;
   }

   write(client_fd, buffer, strlen(buffer));

   handle_request(connP, client_fd);

ERR:
   http_free(connP);
}

void http_parse_Host(connection_t *connP) {
   char *host = NULL;

   http_data_t *data;
   struct list_head *pos;

   if(!list_empty(&(connP->data_head.list))) {
      list_for_each(pos, &(connP->data_head.list)) {
         data = list_entry(pos, http_data_t, list);
         if(strcmp(data->key, "Host") == 0) {
            host = data->value;
            break;
         }
      }
   }

   char *port = strstr(host, ":");

   if(port == NULL) {
      port = "80";
   } else {
      host = strtok(host, ":");
      port++;
   }

   connP->port = strdup(port);
   connP->host = strdup(host);

   fprintf(stdout, "Client is requesting file:\n%s:%s\n", connP->host, connP->url);
}

int http_parse_method(connection_t *connP, const char *data) {
   char *str = NULL;
   char *p = strdup(data);
   char *head = p;
   int ret = 0;

   int i = 0;
   int s = METHOD;

   while((str = strsep(&p, " \r\n")) != NULL) {
      switch(s) {
         case METHOD:
            for(i = 0; i < http_methods_len; i++) {
               if(strcmp(str, http_methods[i]) == 0) {
                  connP->index = i;
                  break;
               }
            }

            if(i == http_methods_len) {
               connP->index = http_methods_len - 1;
               goto ERR;
            }

            s++;
            break;

         case URL:
            connP->url = strdup(str);
            s++;
            break;

         case VERSION:
            if(strcmp(str, "HTTP/1.0") != 0) {
               fprintf(stderr, "http_parse_method VERSION err: %s\n", str);
            }

            connP->version = strdup(str);

            s++;
            break;

         case DONE:
            break;
      }
   }

ERR:
   free(head);
   return ret;
}

void http_parse_data(connection_t *connP, const char *data) {
   char *p = strdup(data);

   char *key = strdup(strtok(p, ":"));
   char *value = strtok(NULL, "\r");

   char *q = value;

   while(*q == ' ') {
      q++;
   }

   value = strdup(q);

   free(p);

   http_data_t *d = malloc(sizeof(http_data_t));
   d->key = key;
   d->value = value;

   list_add_tail(&(d->list), &(connP->data_head.list));
}

void http_read_header(connection_t *connP, const char *data, int len) {
   char buffer[Message_max_size] = {0};
   int numBytes = 0;
   int ret = 0;

   ret = http_parse_method(connP, data);

   if(ret == 1) {
      return;
   }

   INIT_LIST_HEAD(&(connP->data_head.list));

   while(1) {
      memset(buffer, 0, sizeof(buffer));
      numBytes = 0;

      numBytes = readline(connP->sock, buffer, Message_max_size - 1);

      if(numBytes > 0) {
         buffer[numBytes] = 0;

         fprintf(stdout, "%s", buffer);

         if(buffer[0] == '\r' && buffer[1] == '\n') {
            fprintf(stdout, "http_read_header end here\n");

            break;
         }

         http_parse_data(connP, buffer);
      }
   }

   http_parse_Host(connP);

   if(cache_check(connP) == 1) { //check cache
      cache_add_last_accessed(connP); //update last accessed

      fprintf(stdout, "Found file in cache and it's not expired\n");
      http_respond(connP);
      fprintf(stdout, "Sent file to client successfully\n");
      http_free(connP);

      cache_print();

      return;
   }

   fprintf(stdout, "Cannot find requested file in cache\n");
   fprintf(stdout, "Downloading file from: %s\n", connP->host);
   fprintf(stdout, "File path: %s\n", connP->url);

   http_request(connP);

   cache_print(); //print cache list
}
