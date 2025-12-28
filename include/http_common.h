#ifndef HTTP_COMMON_H
#define HTTP_COMMON_H
#include <stdio.h>
#include <stddef.h>
#define GET_REQ_BUFF_SIZE 2048
#define RECV_BUFF_SIZE 4096

typedef struct {
    int content_length;
    int chunked_encoding;
    long status;
} response_components_t;

void init_response_components_t(response_components_t * recv);
void form_get_req(char * host, char * path, char * get_req_buffer);
void parse_status_headers(FILE * fptr, size_t data_idx, response_components_t * response_components);


#endif