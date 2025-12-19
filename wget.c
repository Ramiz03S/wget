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
#define GET_REQ_BUFF_SIZE 1024
#define REC_BUFF_SIZE 1024

void log_stderr(){
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
}

void parse_argv(char * URL, char ** host, char ** path, char ** protocol){
    
    *protocol = strtok(URL,"://");
    *host = strtok(NULL,"/");
    *path = strtok(NULL,"");

}

void form_get_req(char * host, char * path, char * get_req_buffer, int connection_flag){
    if (connection_flag){
        snprintf(get_req_buffer, GET_REQ_BUFF_SIZE - 1, "GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\n\r\n", path, host);
    }
    else{
        snprintf(get_req_buffer, GET_REQ_BUFF_SIZE - 1, "GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    }
}
int main(int argc, char *argv[]){
    
    int client_fd;
    int ret;
    int bytes_sent, bytes_received;
    int total_bytes_received = 0;

    struct addrinfo hints;
    struct addrinfo * result, * next;

    char rec_buffer[REC_BUFF_SIZE] = {0};
    char get_req_buffer[GET_REQ_BUFF_SIZE] = {0};

    if (argc != 2) {
        fprintf(stderr, "Usage: %s protocol://host/path\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char * host = (char *)calloc(100,1);
    char * path = (char *)calloc(100,1);
    char * protocol = (char *)calloc(10,1);
    parse_argv(argv[1], &host, &path, &protocol);
    

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    
    
    if((ret = getaddrinfo(host, protocol, &hints, &result)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    for(next = result; next != NULL; next = next->ai_next){
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

    if(next == NULL){
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }
    
    // send and receive loop
    form_get_req(host, path, get_req_buffer, 1);
    bytes_sent = send(client_fd, get_req_buffer, strlen(get_req_buffer), 0);
    if( bytes_sent == -1 || bytes_sent < strlen(get_req_buffer)){
        log_stderr;
    }
    bytes_received = recv(client_fd, rec_buffer, sizeof(rec_buffer) - 1, 0);
    total_bytes_received += bytes_received;
    if(bytes_received == -1){
        log_stderr;
    }
    rec_buffer[bytes_received] = '\0';
    printf("%d BYTES SENT:\n%s\n", bytes_sent, get_req_buffer);
    printf("%d BYTES RECEIVED:\n%s\n", bytes_received, rec_buffer);


    free(protocol);
    free(host);
    free(path);
    freeaddrinfo(result);
    close(client_fd);
    return 0;
}