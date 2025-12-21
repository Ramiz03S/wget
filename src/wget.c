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
#include "mpc.h"
#define GET_REQ_BUFF_SIZE 1024
#define RECV_BUFF_SIZE 1024

typedef struct URL_Components_t {
    char * scheme;
    char * host;
    char * port;
    char * path;
} URL_Components_t;

void init_URL_Components_t(URL_Components_t * URL){
    URL->scheme = NULL;
    URL->host = NULL;
    URL->port = NULL;
    URL->path = NULL;
}

void free_URL_Components_t(URL_Components_t * URL){
    free(URL->scheme);
    free(URL->host);
    free(URL->port);
    free(URL->path);
}

void log_stderr(){
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
}

void find_component(mpc_ast_t *a, char * component, char ** found_contents) {
 
    for (int i = 0; i < a->children_num; i++) {
        mpc_ast_t * child = a->children[i];
        if(strstr(child->tag, component)){
          *found_contents = calloc(strlen(child->contents)+1, 1);
          memcpy(*found_contents, child->contents, strlen(child->contents)+1);
          break;
        }
    }

}

void parse_URL(char * URL, URL_Components_t * URL_Componenets){
    mpc_parser_t *url  = mpc_new("url");
    mpc_parser_t *scheme  = mpc_new("scheme");
    mpc_parser_t *host  = mpc_new("host");
    mpc_parser_t *port = mpc_new("port");
    mpc_parser_t *path = mpc_new("path");    
    
    mpc_err_t * err = mpca_lang(MPCA_LANG_DEFAULT,
        "url : /^/ (<scheme>\"://\")? <host> (\":\"<port>)? <path>? (\"?\" /[^#\\r\\n]*/)? (\"#\" /[^\\r\\n]*/)? /$/;"
        "scheme : \"http\" | \"https\";"
        "host : /[a-zA-Z0-9-.]+/;"
        "port : /[0-9]+/;"
        "path :  /[\\/][a-zA-Z0-9-.\\/]*/ ;",
        url, scheme, host, port, path, NULL);

    if( err != NULL){
        fprintf(stderr, "mpca_lang() failed: %s\n",err->failure);
        exit(EXIT_FAILURE);
    };

    mpc_result_t r;
    mpc_ast_t * output;
    
    if(mpc_parse("url input", URL, url, &r)){
        mpc_ast_print(r.output);
        output = (mpc_ast_t *)r.output;
        find_component(output, "scheme", &(URL_Componenets->scheme));
        find_component(output, "host", &(URL_Componenets->host));
        find_component(output, "port", &(URL_Componenets->port));
        find_component(output, "path", &(URL_Componenets->path));
        mpc_ast_delete(r.output);
    } else {
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
        mpc_cleanup(5, url, scheme, host, port, path);
        exit(EXIT_FAILURE);
    }
    mpc_cleanup(5, url, scheme, host, port, path);
}

void form_get_req(char * host, char * path, char * get_req_buffer, int connection_flag){
    if(path == NULL){
        path = "/";
    }
    if (connection_flag){
        snprintf(get_req_buffer, GET_REQ_BUFF_SIZE - 1, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\n\r\n", path, host);
    }
    else{
        snprintf(get_req_buffer, GET_REQ_BUFF_SIZE - 1, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    }
}

void parse_recv(char * recv_buffer){
    mpc_parser_t *recv  = mpc_new("recv");
    mpc_parser_t *first_line  = mpc_new("first_line");
    mpc_parser_t *crlf  = mpc_new("crlf");
    mpc_parser_t *sp  = mpc_new("sp");
    mpc_parser_t *status  = mpc_new("status");
    mpc_parser_t *header_line  = mpc_new("header_line");
    mpc_parser_t *header_type  = mpc_new("header_type");
    mpc_parser_t *header_value  = mpc_new("header_value");
    mpc_parser_t *data  = mpc_new("data");

    mpc_err_t *err = mpca_lang(MPCA_LANG_WHITESPACE_SENSITIVE,
        "recv        : /^/ <first_line> <crlf> (<header_line> <crlf>)* <crlf>+ <data>? /$/;"
        "crlf        : /(\\r\\n|\\n|\\r)/;"
        "first_line  : \"HTTP/1.1\" <sp> <status> <sp> /[^\\r\\n]*/;"
        "sp          : / /;"
        "status      : /[0-9]{3}/;"
        "header_line : <header_type> \":\" /[ \t]?/ <header_value>;"
        "header_type : /[a-zA-Z0-9-]+/;"
        "header_value: /[^\\r\\n]*/;"
        "data        : /(.|\\r|\\n)*/;",
        recv, first_line, crlf, sp, status, header_line, header_type, header_value, data, NULL);


    if( err != NULL){
        fprintf(stderr, "mpca_lang() failed: %s\n",err->failure);
        exit(EXIT_FAILURE);
    };

    mpc_result_t r;
    mpc_ast_t * output;
    
    if(mpc_parse("recv buff", recv_buffer, recv, &r)){
        mpc_ast_print(r.output);
        mpc_ast_delete(r.output);
    } else {
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
        
    }
    mpc_cleanup(7, recv, first_line, status, header_line, header_type, header_value, data);
}

int send_recv_loop(int client_fd, URL_Components_t URL_Components, char * get_req_buffer, char * recv_buffer){
    int bytes_sent, bytes_received;
    int total_bytes_received = 0;
    size_t get_req_buffer_len;

    form_get_req(URL_Components.host, URL_Components.path, get_req_buffer, 1);
    get_req_buffer_len = strlen(get_req_buffer);

    bytes_sent = send(client_fd, get_req_buffer, get_req_buffer_len, 0);
    if( bytes_sent == -1 || bytes_sent < get_req_buffer_len){
        log_stderr;
        return -1;
    }

    bytes_received = recv(client_fd, recv_buffer, RECV_BUFF_SIZE - 1, 0);
    total_bytes_received += bytes_received;
    if(bytes_received == -1){
        log_stderr;
        return -1;
    }
    
    recv_buffer[bytes_received] = '\0';
    printf("%d BYTES SENT:\n%s\n", bytes_sent, get_req_buffer);
    printf("%d BYTES RECEIVED:\n%s\n\n", bytes_received, recv_buffer);
    parse_recv(recv_buffer);
    

}

int main(int argc, char *argv[]){
    
    int client_fd;
    int ret_getaddrinfo;
    
    

    struct addrinfo hints;
    struct addrinfo * result, * next;
    char * service;
    char ip[INET6_ADDRSTRLEN];
    uint16_t port;

    char recv_buffer[RECV_BUFF_SIZE] = {0};
    char get_req_buffer[GET_REQ_BUFF_SIZE] = {0};

    URL_Components_t URL_Components;
    init_URL_Components_t(&URL_Components);

    if(argc != 2) {
        fprintf(stderr, "Usage: %s [(http | https)://]host[:port][/path/to/file]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if(strlen(argv[1]) > 2048){
        fprintf(stderr, "Length of URL exceeds the limit of 2048 characters\n");
        exit(EXIT_FAILURE);
    }
    parse_URL(argv[1], &URL_Components);
    if(URL_Components.port){
        char * endptr, * str;
        long val;
        str = URL_Components.port;
        val = strtol(str,&endptr,0);
        if (val < 0 || val > 65535){
            fprintf(stderr, "Port value in URL is outside of expected bounds\n");
            exit(EXIT_FAILURE);
        }
    }

    service = URL_Components.scheme ? URL_Components.scheme : URL_Components.port;
    

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    
    
    if((ret_getaddrinfo = getaddrinfo(URL_Components.host, service, &hints, &result)) != 0){
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
            log_stderr;
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
    
    // send and receive loop  
    send_recv_loop(client_fd, URL_Components, get_req_buffer, recv_buffer);

    free_URL_Components_t(&URL_Components);
    freeaddrinfo(result);
    close(client_fd);
    return 0;
}