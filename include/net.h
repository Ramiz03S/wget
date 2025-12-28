#ifndef NET_H
#define NET_H
#include "url.h"

int make_connection(URL_components_t * URL_components);

void close_connection(int client_fd);

#endif