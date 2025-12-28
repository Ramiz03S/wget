#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "url.h"
#include "net.h"
#include "http.h"
#include "https.h"


int main(int argc, char *argv[]){
    
    int client_fd;
    char * service;
    SSL_CTX * ctx;
    SSL * ssl;
    BIO *bio;
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

    service = URL_components.scheme ? URL_components.scheme : URL_components.port;

    if((strcmp(service,"http") == 0) ||(strcmp(service,"80") == 0) || (strcmp(service,"8080") == 0)){
        
        http_send_request(client_fd, URL_components);

        http_process_response(client_fd);

        close_connection(client_fd);
    }

    else if((strcmp(service,"https") == 0) || (strcmp(service,"443") == 0)){
        
        ssl_ctx_init(&ctx);

        ssl_ssl_init(ctx, &ssl, &bio, client_fd, URL_components.host);

        ssl_handshake(ssl);

        https_send_request(ssl, client_fd, URL_components);

        https_process_response(ssl, client_fd);

        https_shutdown_and_cleanup(&ctx, &ssl);
    }
  
    free_URL_components_t(&URL_components);
 
    return 0;
}