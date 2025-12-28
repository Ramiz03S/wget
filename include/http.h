#ifndef HTTP_H
#define HTTP_H

#include "url.h"


void process_http_response(int client_fd);

int send_http_request(int client_fd, URL_components_t URL_components);

#endif