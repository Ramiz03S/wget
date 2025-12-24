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
#define GET_REQ_BUFF_SIZE 2048
#define RECV_BUFF_SIZE 4096

typedef struct {
    char * scheme;
    char * host;
    char * port;
    char * path;
} URL_components_t;

typedef struct {
    int content_length;
    int chunked_encoding;
    long status;
} response_components_t;

void init_URL_components_t(URL_components_t * URL){
    URL->scheme = NULL;
    URL->host = NULL;
    URL->port = NULL;
    URL->path = NULL;
}

void init_response_components_t(response_components_t * recv){
    recv->content_length = -1;
    recv->status = -1;
    recv->chunked_encoding = -1;
}

void free_URL_components_t(URL_components_t * URL){
    free(URL->scheme);
    free(URL->host);
    free(URL->port);
    free(URL->path);
}

long strtol_caller(char * num_str){
    char * endptr;
    return strtol(num_str,&endptr,0);
}

void log_stderr(){
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
}

void find_URL_component(mpc_ast_t *a, char * component, char ** found_contents) {
 
    for (int i = 0; i < a->children_num; i++) {
        mpc_ast_t * child = a->children[i];
        if(strstr(child->tag, component)){
          *found_contents = calloc(strlen(child->contents)+1, 1);
          memcpy(*found_contents, child->contents, strlen(child->contents)+1);
          break;
        }
    }

}

void parse_URL(char * URL, URL_components_t * URL_Componenets){
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
        "path :  /[\\/][a-zA-Z0-9.\\/_-]*/ ;",
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
        find_URL_component(output, "scheme", &(URL_Componenets->scheme));
        find_URL_component(output, "host", &(URL_Componenets->host));
        find_URL_component(output, "port", &(URL_Componenets->port));
        find_URL_component(output, "path", &(URL_Componenets->path));
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

void tree_traversal(mpc_ast_t * tree_node, response_components_t * response_components){
    char * tag = tree_node->tag;
    char * contents = tree_node->contents;
    static int in_header_line = 0;
    static int child_counter = 0;
    static char * header_type;
    mpc_ast_t * node_child;
    size_t content_len = strlen(contents);

    if(strstr(tag, "status")){
        response_components->status = strtol_caller(contents);
        in_header_line = 0;
    }
    else if (strstr(tag, "header_line"))
    {
        in_header_line = 1;
        child_counter = 0;
    }
    else if (strstr(tag, "crlf")) {in_header_line = 0;}

    if(in_header_line && child_counter==0){
        
        header_type = malloc(content_len + 1);
        strcpy(header_type, contents);
    }
    if(in_header_line && child_counter==3){
        
        if(strstr(header_type, "Content-Length")){
            response_components->content_length = strtol_caller(contents);
        }  
        if(strstr(header_type, "Transfer-Encoding")){
            if(strstr(contents, "chunked")){
                response_components->chunked_encoding = 1;
            }
            
        }  
        free(header_type);     
    }

    for(int i = 0; i < tree_node->children_num; i++){
        
        tree_traversal(tree_node->children[i], response_components);
        child_counter++;
    }
    
}

void parse_status_headers(FILE * fptr, size_t data_idx, response_components_t * response_components){
    char status_headers_buff[data_idx + 1]; // one more for \0
    int bytes_read;
    mpc_result_t r;
    mpc_ast_t * output;
    
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
        "header_line : /[a-zA-Z0-9-]+/ \":\" /[ \t]?/ /[^\\r\\n]*/;"
        "data        : /(.|\\r|\\n)*/;",
        recv, first_line, crlf, sp, status, header_line, data, NULL);


    if( err != NULL){
        fprintf(stderr, "mpca_lang() failed: %s\n",err->failure);
        exit(EXIT_FAILURE);
    };

    
    memset(status_headers_buff, 0, data_idx);
    rewind(fptr);
    bytes_read = fread(status_headers_buff, 1, data_idx, fptr);
    if(bytes_read != data_idx){
        log_stderr();
    }

    status_headers_buff[data_idx] = '\0';
    
    if(mpc_parse("status_headers_buff", status_headers_buff, recv, &r)){
        mpc_ast_print(r.output);
        output = (mpc_ast_t *)r.output;
        tree_traversal(output, response_components);
        mpc_ast_delete(r.output);
    } else {
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
        mpc_cleanup(7, recv, first_line, status, header_line, header_type, header_value, data);
        exit(EXIT_FAILURE);
        
    }
    mpc_cleanup(7, recv, first_line, status, header_line, header_type, header_value, data);
}

void get_chunked_response(int client_fd, FILE * response_fptr, char * recv_buff){
    int bytes_received;
    int total_bytes_received;
    long chunk_length;
    char * chunk_length_buff;
    int chunk_length_found = 0;
    int next_idx = 1;

    memset(recv_buff, 0, RECV_BUFF_SIZE);
    while(!chunk_length_found){
        
        bytes_received = recv(client_fd, recv_buff, 1, 0);

        if(bytes_received == -1){
            log_stderr;
        }

        if(*recv_buff == ';' || *recv_buff == '\r' || *recv_buff == '\n'){
            chunk_length_buff = calloc(next_idx , 1);
            memcpy(chunk_length_buff, recv_buff + 1, next_idx - 1);
            chunk_length_buff[next_idx - 1] = '\0';
            chunk_length = strtol_caller(chunk_length_buff);
            free(chunk_length_buff);
            chunk_length_found = 1;
        }
        else{
            recv_buff[next_idx] = recv_buff[0];
            next_idx++;
        }

    }
    
    

}

void process_response(int client_fd){
    int end_copying;
    FILE * response_fptr, * final_file_fptr;
    size_t data_idx;
    int bytes_received;
    long file_pos;
    int reading_till_headers = 0;
    int total_bytes_received = 0;
    int total_data_bytes_received = 0;
    char recv_buff[RECV_BUFF_SIZE];
    char carry_over_buff[RECV_BUFF_SIZE];
    response_components_t response_components;

    response_fptr = fopen("response", "wb+");
    if(response_fptr == NULL){
        log_stderr();
    }

    final_file_fptr = fopen("file", "wb");
    if(final_file_fptr == NULL){
        log_stderr();
    }

    memset(carry_over_buff, 0, RECV_BUFF_SIZE);
    memset(recv_buff, 0, RECV_BUFF_SIZE);
    char * temp = NULL;
    while(!reading_till_headers){
        
        recv_buff[3] = recv_buff[2];
        recv_buff[2] = recv_buff[1];
        recv_buff[1] = recv_buff[0];

        bytes_received = recv(client_fd, recv_buff, 1, 0);
        total_bytes_received += bytes_received;

        if(bytes_received == -1){
            log_stderr;
        }
        fwrite(recv_buff, 1, bytes_received, response_fptr);
        recv_buff[4] = '\0';
        temp = strstr(recv_buff, "\n\r\n\r");
        if(temp){
            data_idx = total_bytes_received;
            reading_till_headers = 1;
        }        
    }

    // parse status and headers
    memset(&response_components, 0, sizeof(response_components));
    init_response_components_t(&response_components);

    parse_status_headers(response_fptr, data_idx, &response_components);
    // now we know the response status and whether the data contents were chunked or not

    if(response_components.chunked_encoding == 1 && response_components.status == 200){
        get_chunked_response(client_fd, response_fptr, recv_buff);
    }
    else if(response_components.content_length != -1 && response_components.status == 200){
        while(total_data_bytes_received < response_components.content_length){

            memset(recv_buff, 0, RECV_BUFF_SIZE);
            bytes_received = recv(client_fd, recv_buff, RECV_BUFF_SIZE, 0);
            if(bytes_received == -1){
                log_stderr;
            }
            total_data_bytes_received += bytes_received;
            fwrite(recv_buff, 1, bytes_received, response_fptr);
        }

    }
    
    // rewrite the data portion only to a seperate file
    fseek(response_fptr, data_idx, SEEK_SET);
    end_copying = 0;
    while(!end_copying){

        clearerr(response_fptr);
        memset(recv_buff, 0, RECV_BUFF_SIZE);
        bytes_received = fread(recv_buff, 1, RECV_BUFF_SIZE, response_fptr);

        
        if(bytes_received < RECV_BUFF_SIZE){
            if(feof(response_fptr) != 0){end_copying = 1;}
            else{
                log_stderr;
            }
        }
        fwrite(recv_buff, 1, bytes_received, final_file_fptr);
    }
    
    fclose(final_file_fptr);
    fclose(response_fptr);
    /*
    if(remove("response") != 0){
        log_stderr();
    }
    */
}

int send_request(int client_fd, URL_components_t URL_components){
    int bytes_sent;
    size_t get_req_buffer_len;
    
    char get_req_buffer[GET_REQ_BUFF_SIZE] = {0};

    form_get_req(URL_components.host, URL_components.path, get_req_buffer, 1);
    get_req_buffer_len = strlen(get_req_buffer);

    bytes_sent = send(client_fd, get_req_buffer, get_req_buffer_len, 0);
    if( bytes_sent == -1 || bytes_sent < get_req_buffer_len){
        log_stderr;
    }
    printf("%d BYTES SENT:\n%s\n", bytes_sent, get_req_buffer);

}

int main(int argc, char *argv[]){
    
    int client_fd;
    int ret_getaddrinfo;

    struct addrinfo hints;
    struct addrinfo * result, * next;
    char * service;
    char ip[INET6_ADDRSTRLEN];
    uint16_t port;

    URL_components_t URL_components;
    init_URL_components_t(&URL_components);

    if(argc != 2) {
        fprintf(stderr, "Usage: %s [(http | https)://]host[:port][/path/to/file]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if(strlen(argv[1]) > 2048){
        fprintf(stderr, "Length of URL exceeds the limit of 2048 characters\n");
        exit(EXIT_FAILURE);
    }
    parse_URL(argv[1], &URL_components);
    if(URL_components.port){
        long val;
        val = strtol_caller(URL_components.port);
        if (val < 0 || val > 65535){
            fprintf(stderr, "Port value in URL is outside of expected bounds\n");
            exit(EXIT_FAILURE);
        }
    }

    service = URL_components.scheme ? URL_components.scheme : URL_components.port;
    

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    
    
    if((ret_getaddrinfo = getaddrinfo(URL_components.host, service, &hints, &result)) != 0){
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
    send_request(client_fd, URL_components);
    process_response(client_fd);

    free_URL_components_t(&URL_components);
    freeaddrinfo(result);
    close(client_fd);
    return 0;
}