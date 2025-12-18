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
#define PORT 8080

void log_stderr(){
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
}

void parse_argv(char * argv){

}

void form_get_req(char * host, char * path, char * get_req){
    sscanf(get_req, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", path, host);
}
// wget https://wordpress.org/latest.zip
int main(int argc, char *argv[]){
    
    int client_fd;
    int ret;
    int bytes_sent, bytes_received;

    struct addrinfo hints;
    struct addrinfo * result, * next;

    //har get_request[] = "GET /contact HTTP/1.1\r\nHost: example.com\r\n\r\n";
    char rec_buffer[1024] = {0};

    if (argc != 2) {
        fprintf(stderr, "Usage: %s url\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    
    
    if((ret = getaddrinfo(NULL, argv[1], &hints, &result)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    for(next = result; next != NULL; next = result->ai_next){

        if((client_fd = socket(next->ai_family, next->ai_socktype, next->ai_protocol)) == -1){
        continue;
        }

        if ((connect(client_fd, next->ai_addr, next->ai_addrlen)) == -1) {
            log_stderr;
        }
        else {
            break;
        }
        break;
        
    }
    

/*
    if((bytes_sent = send(client_fd, get_request, strlen(get_request), 0)) == -1){
        log_stderr;
    }
    if((bytes_received = recv(client_fd, rec_buffer, sizeof(rec_buffer) - 1, 0)) == -1){
        log_stderr;
    }
    rec_buffer[bytes_received] = '\0';
    printf("BYTES RECEIVED:\n%s",rec_buffer);
*/


    close(client_fd);
    freeaddrinfo(result);
    return 0;
}