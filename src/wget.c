#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "url.h"
#include "net.h"
#include "http.h"


int main(int argc, char *argv[]){
    
    int client_fd;
    URL_components_t URL_components;

    init_URL_components_t(&URL_components);

    // input checks
    if(argc != 2) {
        fprintf(stderr, "Usage: %s [(http | https)://]host[:port][/path/to/file]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if(strlen(argv[1]) > 2048){
        fprintf(stderr, "Length of URL exceeds the limit of 2048 characters\n");
        exit(EXIT_FAILURE);
    }

    parse_URL(argv[1], &URL_components);

    client_fd = make_connection(&URL_components);

    send_http_request(client_fd, URL_components);

    process_http_response(client_fd);

    free_URL_components_t(&URL_components);
    
    close_connection(client_fd);

    return 0;
}