#include "http.h"
#include "http_common.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

void static get_chunked_response(int client_fd, FILE * response_fptr, char * recv_buff){
    int bytes_received;
    int next_idx;
    long chunk_length;
    char * chunk_length_buff;
    int total_bytes_received;
    int chunk_length_found;
    int terminating_chunk_found = 0;

    while(!terminating_chunk_found){

        memset(recv_buff, 0, RECV_BUFF_SIZE);
        next_idx = 1;
        chunk_length_found = 0;
        total_bytes_received = 0;
        while(!chunk_length_found){
            bytes_received = recv(client_fd, recv_buff, 1, 0);

            if(bytes_received == -1){
                log_stderr();
            }

            if(*recv_buff == ';' || *recv_buff == '\r' || *recv_buff == '\n'){
                chunk_length_buff = calloc(next_idx , 1);
                memcpy(chunk_length_buff, recv_buff + 1, next_idx - 1);
                chunk_length_buff[next_idx - 1] = '\0';
                chunk_length = strtol_caller(chunk_length_buff, 16);
                
                chunk_length_found = 1;
            }
            else{
                recv_buff[next_idx] = recv_buff[0];
                next_idx++;
            }
        }

        while(recv_buff[0] != '\n'){
            bytes_received = recv(client_fd, recv_buff, 1, 0);
            if(bytes_received == -1){
                log_stderr();
            }
        }
        if(chunk_length == 0){
            terminating_chunk_found = 1;
        }
        else if(chunk_length <= RECV_BUFF_SIZE){

            while(chunk_length != total_bytes_received){

                bytes_received = recv(client_fd, recv_buff, chunk_length - total_bytes_received, 0);
                if(bytes_received == -1){
                    log_stderr();
                }
                total_bytes_received += bytes_received;
                
                fwrite(recv_buff, 1, bytes_received, response_fptr);
            }
        }

        
        else{
            ldiv_t div = ldiv(chunk_length, (long)RECV_BUFF_SIZE);
            for(long i = 0; i < div.quot; i++){
                total_bytes_received = 0;
                while(RECV_BUFF_SIZE != total_bytes_received){
                    bytes_received = recv(client_fd, recv_buff, RECV_BUFF_SIZE - total_bytes_received, 0);
                    if(bytes_received == -1){
                        log_stderr();
                    }
                    total_bytes_received += bytes_received;
                    
                    fwrite(recv_buff, 1, bytes_received, response_fptr);

                }
            }

            total_bytes_received = 0;
            while(div.rem != total_bytes_received){

                bytes_received = recv(client_fd, recv_buff, div.rem - total_bytes_received, 0);
                if(bytes_received == -1){
                    log_stderr();
                }
                total_bytes_received += bytes_received;
                
                fwrite(recv_buff, 1, bytes_received, response_fptr);
            }
        }

        while(recv_buff[0] != '\n'){
            bytes_received = recv(client_fd, recv_buff, 1, 0);
            if(bytes_received == -1){
                log_stderr();
            }
        }

        free(chunk_length_buff);

    }
    

}

void static get_content_length_response(int client_fd, FILE * response_fptr, char *  recv_buff, int content_length){
    int bytes_received;
    int total_data_bytes_received;
    if(content_length <= RECV_BUFF_SIZE){
        total_data_bytes_received = 0;
        while(total_data_bytes_received != content_length){

            bytes_received = recv(client_fd, recv_buff, content_length - total_data_bytes_received, 0);
            if(bytes_received == -1){
                log_stderr();
            }
            total_data_bytes_received += bytes_received;
            fwrite(recv_buff, 1, bytes_received, response_fptr);
        }
    }
    else{
        ldiv_t div = ldiv((long)content_length, (long)RECV_BUFF_SIZE);
        for(long i = 0; i < div.quot; i++){
            total_data_bytes_received = 0;
            while(RECV_BUFF_SIZE != total_data_bytes_received){
                bytes_received = recv(client_fd, recv_buff, RECV_BUFF_SIZE - total_data_bytes_received, 0);
                if(bytes_received == -1){
                    log_stderr();
                }
                total_data_bytes_received += bytes_received;
                
                fwrite(recv_buff, 1, bytes_received, response_fptr);
            }
        }

        total_data_bytes_received = 0;
        while(div.rem != total_data_bytes_received){

            bytes_received = recv(client_fd, recv_buff, div.rem - total_data_bytes_received, 0);
            if(bytes_received == -1){
                log_stderr();
            }
            total_data_bytes_received += bytes_received;
            
            fwrite(recv_buff, 1, bytes_received, response_fptr);
        }
    }
}

void process_http_response(int client_fd){
    int end_copying;
    FILE * response_fptr, * final_file_fptr;
    size_t data_idx;
    int bytes_received;
    long file_pos;
    int reading_till_headers = 0;
    int total_bytes_received = 0;
    
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
            log_stderr();
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
    if(response_components.status != 200){
        fprintf(stderr, "Error: response status was %ld\n",response_components.status);
    }
    else if(response_components.chunked_encoding == 1 && response_components.status == 200){
        get_chunked_response(client_fd, response_fptr, recv_buff);
    }
    else if(response_components.content_length != -1 && response_components.status == 200){
        get_content_length_response(client_fd, response_fptr, recv_buff, response_components.content_length);
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
                log_stderr();
            }
        }
        fwrite(recv_buff, 1, bytes_received, final_file_fptr);
    }
    
    fclose(final_file_fptr);
    fclose(response_fptr);
    
    if(remove("response") != 0){
        log_stderr();
    }
    
}

int send_http_request(int client_fd, URL_components_t URL_components){
    int bytes_sent;
    size_t get_req_buffer_len;
    
    char get_req_buffer[GET_REQ_BUFF_SIZE] = {0};

    form_get_req(URL_components.host, URL_components.path, get_req_buffer);
    get_req_buffer_len = strlen(get_req_buffer);

    bytes_sent = send(client_fd, get_req_buffer, get_req_buffer_len, 0);
    if( bytes_sent == -1 || bytes_sent < get_req_buffer_len){
        log_stderr();
    }
    printf("%d BYTES SENT:\n%s\n", bytes_sent, get_req_buffer);

}
