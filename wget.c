#define _GNU_SOURCE
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>
#include <curl/curl.h>
#define PORT 8080

void log_stderr(){
    write(STDERR_FILENO, strerror(errno), strlen(strerror(errno)));
    write(STDERR_FILENO, "\n", 1);
}

int main(int argc, char *argv[]){
    
    int client_fd;
    int bytes_sent, bytes_received;
    struct sockaddr_in server_address;
    
    
    char get_request[] = "GET /contact HTTP/1.1\r\nHost: example.com\r\n\r\n";
    char buffer[1024] = {0};
    

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    if(inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) != 1){
        log_stderr();
        return -1;
    }

    if((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        log_stderr();
        return -1;
    }

    if ((connect(client_fd, (struct sockaddr*)&server_address,
                   sizeof(server_address)))
        == -1) {
        log_stderr;
        return -1;
    }

    if((bytes_sent = send(client_fd, get_request, strlen(get_request), 0)) == -1){
        log_stderr;
        return -1;
    }
    if((bytes_received = recv(client_fd, rec_buffer, sizeof(rec_buffer) - 1, 0)) == -1){
        log_stderr;
        return -1;
    }
    rec_buffer[bytes_received] = '\0';
    printf("BYTES RECEIVED:\n%s",rec_buffer);


    close(client_fd);
    return 0;
}