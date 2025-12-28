#include "https.h"
#include <stdio.h>
#include <stdlib.h>
#include "http_common.h"
#include "util.h"
#include "url.h"

void ssl_ctx_init(SSL_CTX ** ctx){
    /*
    * Create an SSL_CTX which we can use to create SSL objects from. We
    * want an SSL_CTX for creating clients so we use TLS_client_method()
    * here.
    */
    *ctx = SSL_CTX_new(TLS_client_method());
    if(*ctx == NULL){
        fprintf(stderr, "Failed to create the SSL_CTX\n");
        exit(EXIT_FAILURE);
    }

    /*
    * Configure the client to abort the handshake if certificate
    * verification fails. Virtually all clients should do this unless you
    * really know what you are doing.
    */
    SSL_CTX_set_verify(*ctx, SSL_VERIFY_PEER, NULL);

    /* Use the default trusted certificate store */
    if (!SSL_CTX_set_default_verify_paths(*ctx)) {
        fprintf(stderr, "Failed to set the default trusted certificate store\n");
        exit(EXIT_FAILURE);
    }

    /*
    * TLSv1.1 or earlier are deprecated by IETF and are generally to be
    * avoided if possible. We require a minimum TLS version of TLSv1.2.
    */
    if (!SSL_CTX_set_min_proto_version(*ctx, TLS1_2_VERSION)) {
        fprintf(stderr, "Failed to set the minimum TLS protocol version\n");
        exit(EXIT_FAILURE);
    }
}

void ssl_ssl_init(SSL_CTX * ctx, SSL ** ssl, BIO ** bio, int sock, char * hostname){
    /* Create an SSL object to represent the TLS connection */
    *ssl = SSL_new(ctx);
    if (*ssl == NULL) {
        fprintf(stderr, "Failed to create a new SSL object\n");
        exit(EXIT_FAILURE);
    }

    /* Create a BIO to wrap the socket */
    *bio = BIO_new(BIO_s_socket());
    if (*bio == NULL) {
        fprintf(stderr, "Failed to create a new BIO object\n");
        exit(EXIT_FAILURE);
    }
    
    /*
    * Associate the newly created BIO with the underlying socket. By
    * passing BIO_CLOSE here the socket will be automatically closed when
    * the BIO is freed. Alternatively you can use BIO_NOCLOSE, in which
    * case you must close the socket explicitly when it is no longer
    * needed.
    */
    BIO_set_fd(*bio, sock, BIO_CLOSE);

    SSL_set_bio(*ssl, *bio, *bio);

    /*
    * Tell the server during the handshake which hostname we are attempting
    * to connect to in case the server supports multiple hosts.
    */
    if (!SSL_set_tlsext_host_name(*ssl, hostname)) {
        fprintf(stderr, "Failed to set the SNI hostname\n");
        exit(EXIT_FAILURE);
    }

    /*
    * Ensure we check during certificate verification that the server has
    * supplied a certificate for the hostname that we were expecting.
    * Virtually all clients should do this unless you really know what you
    * are doing.
    */
    if (!SSL_set1_host(*ssl, hostname)) {
        fprintf(stderr, "Failed to set the certificate verification hostname\n");
        exit(EXIT_FAILURE);
    }
}

void ssl_handshake(SSL * ssl){
    /* Do the handshake with the server */
    if (SSL_connect(ssl) < 1) {
        fprintf(stderr, "Failed to connect to the server\n");
        /*
        * If the failure is due to a verification error we can get more
        * information about it from SSL_get_verify_result().
        */
        if (SSL_get_verify_result(ssl) != X509_V_OK)
            fprintf(stderr, "Verify error: %s\n",
                X509_verify_cert_error_string(SSL_get_verify_result(ssl)));

        exit(EXIT_FAILURE);
    }
}

int https_send_request(SSL * ssl, int client_fd, URL_components_t URL_components){
    int ret;
    size_t bytes_sent;
    size_t get_req_buffer_len;
    
    char get_req_buffer[GET_REQ_BUFF_SIZE] = {0};

    form_get_req(URL_components.host, URL_components.path, get_req_buffer);
    get_req_buffer_len = strlen(get_req_buffer);

    ret = SSL_write_ex(ssl, get_req_buffer, get_req_buffer_len, &bytes_sent);
    if(ret != 1){
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if(bytes_sent != get_req_buffer_len){
        fprintf(stderr, "SSL_write_ex: could not write all the bytes.\n");
        exit(EXIT_FAILURE);
    }
    printf("%ld BYTES SENT:\n%s\n", bytes_sent, get_req_buffer);

}

void static https_get_chunked_response(SSL * ssl, int client_fd, FILE * response_fptr, char * recv_buff){
    int ret;
    size_t bytes_received;
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

            ret = SSL_read_ex(ssl, recv_buff, 1, &bytes_received);

            if(ret != 1){
                ERR_print_errors_fp(stderr);
                exit(EXIT_FAILURE);
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
            ret = SSL_read_ex(ssl, recv_buff, 1, &bytes_received);

            if(ret != 1){
                ERR_print_errors_fp(stderr);
                exit(EXIT_FAILURE);
            }
        }
        if(chunk_length == 0){
            terminating_chunk_found = 1;
        }
        else if(chunk_length <= RECV_BUFF_SIZE){

            while(chunk_length != total_bytes_received){

                ret = SSL_read_ex(ssl, recv_buff, chunk_length - total_bytes_received, &bytes_received);

                if(ret != 1){
                    ERR_print_errors_fp(stderr);
                    exit(EXIT_FAILURE);
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

                    ret = SSL_read_ex(ssl, recv_buff, RECV_BUFF_SIZE - total_bytes_received, &bytes_received);

                    if(ret != 1){
                        ERR_print_errors_fp(stderr);
                        exit(EXIT_FAILURE);
                    }
                    
                    total_bytes_received += bytes_received;
                    
                    fwrite(recv_buff, 1, bytes_received, response_fptr);

                }
            }

            total_bytes_received = 0;
            while(div.rem != total_bytes_received){

                ret = SSL_read_ex(ssl, recv_buff, div.rem - total_bytes_received, &bytes_received);

                if(ret != 1){
                    ERR_print_errors_fp(stderr);
                    exit(EXIT_FAILURE);
                }

                total_bytes_received += bytes_received;
                
                fwrite(recv_buff, 1, bytes_received, response_fptr);
            }
        }

        while(recv_buff[0] != '\n'){

            ret = SSL_read_ex(ssl, recv_buff, 1, &bytes_received);

            if(ret != 1){
                ERR_print_errors_fp(stderr);
                exit(EXIT_FAILURE);
            }
        }

        free(chunk_length_buff);

    }
    

}

void static https_get_content_length_response(SSL * ssl, int client_fd, FILE * response_fptr, char *  recv_buff, int content_length){
    int ret;
    size_t bytes_received;
    size_t total_data_bytes_received;
    if(content_length <= RECV_BUFF_SIZE){
        total_data_bytes_received = 0;
        while(total_data_bytes_received != content_length){

            ret = SSL_read_ex(ssl, recv_buff, content_length - total_data_bytes_received, &bytes_received);

            if(ret != 1){
                ERR_print_errors_fp(stderr);
                exit(EXIT_FAILURE);
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

                ret = SSL_read_ex(ssl, recv_buff, RECV_BUFF_SIZE - total_data_bytes_received, &bytes_received);

                if(ret != 1){
                    ERR_print_errors_fp(stderr);
                    exit(EXIT_FAILURE);
                }
                
                total_data_bytes_received += bytes_received;
                
                fwrite(recv_buff, 1, bytes_received, response_fptr);
            }
        }

        total_data_bytes_received = 0;
        while(div.rem != total_data_bytes_received){

            ret = SSL_read_ex(ssl, recv_buff, div.rem - total_data_bytes_received, &bytes_received);

            if(ret != 1){
                ERR_print_errors_fp(stderr);
                exit(EXIT_FAILURE);
            }

            total_data_bytes_received += bytes_received;
            
            fwrite(recv_buff, 1, bytes_received, response_fptr);
        }
    }
}

void https_process_response(SSL * ssl, int client_fd){
    int ret;
    int end_copying;
    FILE * response_fptr, * final_file_fptr;
    size_t data_idx;
    size_t bytes_received;
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

        ret = SSL_read_ex(ssl, recv_buff, 1, &bytes_received);

        if(ret != 1){
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }

        total_bytes_received += bytes_received;

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
        https_get_chunked_response(ssl, client_fd, response_fptr, recv_buff);
    }
    else if(response_components.content_length != -1 && response_components.status == 200){
        https_get_content_length_response(ssl, client_fd, response_fptr, recv_buff, response_components.content_length);
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

void https_shutdown_and_cleanup(SSL_CTX ** ctx, SSL ** ssl){
    int ret;

    ret = SSL_shutdown(*ssl);
    if (ret != 1) {
        if(ret == 0){
            ret = SSL_shutdown(*ssl);
            if( ret != 1){
                fprintf(stderr, "Error shutting down\n");
                exit(EXIT_FAILURE);
            }
        }
        else{
            fprintf(stderr, "Error shutting down\n");
            exit(EXIT_FAILURE);
        }
    }

    SSL_free(*ssl);
    SSL_CTX_free(*ctx);
}