#define _GNU_SOURCE
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>

void log_stderr(){
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
}

void parse_argv(char * URL, char ** host, char ** path, char ** protocol){
    
    *protocol = strtok(URL,"://");
    *host = strtok(NULL,"/");
    *path = strtok(NULL,"");

}

void form_get_req(char * host, char * path, char * get_req){
    snprintf(get_req, 1023, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", path, host);
}
int main(int argc, char *argv[]){
    
    int client_fd;
    int ret;
    int bytes_sent, bytes_received;

    struct addrinfo hints;
    struct addrinfo * result, * next;

    char rec_buffer[1024] = {0};
    char get_req_buffer[1024] = {0};

    if (argc != 2) {
        fprintf(stderr, "Usage: %s url\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char * host = (char *)calloc(100,1);
    char * path = (char *)calloc(100,1);
    char * protocol = (char *)calloc(10,1);
    parse_argv(argv[1], &host, &path, &protocol);
    form_get_req(host, path, get_req_buffer);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    
    
    if((ret = getaddrinfo(host, protocol, &hints, &result)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    for(next = result; next != NULL; next = result->ai_next){

        if((client_fd = socket(next->ai_family, next->ai_socktype, next->ai_protocol)) == -1){
        continue;
        }

        if ((connect(client_fd, next->ai_addr, next->ai_addrlen)) == -1) {
            close(client_fd);
            log_stderr;
        }
        else {
            break;
        }
        
    }

    freeaddrinfo(result);
    if(next == NULL){
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }
    
    // send and receive loop
    if((bytes_sent = send(client_fd, get_req_buffer, strlen(get_req_buffer), 0)) == -1){
        log_stderr;
    }
    if((bytes_received = recv(client_fd, rec_buffer, sizeof(rec_buffer) - 1, 0)) == -1){
        log_stderr;
    }
    rec_buffer[bytes_received] = '\0';
    printf("BYTES RECEIVED:\n%s",rec_buffer);



    close(client_fd);
    return 0;
}