#include "net.h"
#include "util.h"
#define _GNU_SOURCE
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

int make_connection(URL_components_t * URL_components){
    int client_fd;
    int ret_getaddrinfo;
    struct addrinfo hints;
    struct addrinfo * result, * next;
    char * service;
    char ip[INET6_ADDRSTRLEN];
    uint16_t port;

    service = URL_components->scheme ? URL_components->scheme : URL_components->port;
    

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    
    
    if((ret_getaddrinfo = getaddrinfo(URL_components->host, service, &hints, &result)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret_getaddrinfo));
        exit(EXIT_FAILURE);
    }
    
    printf("Canonname: %s\n", result->ai_canonname ? result->ai_canonname : "Not Found");
    for(next = result; next != NULL; next = next->ai_next){
        printf("Creating Socket for family: %d, type: %d, protocol: %d\n",next->ai_family, next->ai_socktype, next->ai_protocol);
        if((client_fd = socket(next->ai_family, next->ai_socktype, next->ai_protocol)) == -1){
            printf("Socket creating failed\n");
            continue;
        }
        inet_ntop(next->ai_family, next->ai_addr, ip, sizeof(ip));
        printf("Attempting connection to address: %s\n", ip);
        if ((connect(client_fd, next->ai_addr, next->ai_addrlen)) == -1) {
            close(client_fd);
            log_stderr();
        }
        else {
            printf("Connection Successful to address: %s\n", ip);
            break;
        }
    }

    if(next == NULL){
        fprintf(stderr, "Could not create socket.\n");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(result);

    return client_fd;
}

void close_connection(int client_fd){
    close(client_fd);
}