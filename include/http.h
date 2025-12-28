#ifndef HTTP_H
#define HTTP_H

#include "url.h"


void http_process_response(int client_fd);

int http_send_request(int client_fd, URL_components_t URL_components);

#endif