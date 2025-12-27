#ifndef HTTP_H
#define HTTP_H

#include "url.h"
#define GET_REQ_BUFF_SIZE 2048
#define RECV_BUFF_SIZE 4096

void process_http_response(int client_fd);

int send_http_request(int client_fd, URL_components_t URL_components);

#endif