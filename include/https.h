#ifndef HTTPS_H
#define HTTPS_H
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "url.h"

void ssl_ctx_init(SSL_CTX ** ctx);

void ssl_ssl_init(SSL_CTX * ctx, SSL ** ssl, BIO **bio, int sock, char * hostname);

void ssl_handshake(SSL * ssl);

int https_send_request(SSL * ssl, int client_fd, URL_components_t URL_components);

void https_process_response(SSL * ssl, int client_fd);

void https_shutdown_and_cleanup(SSL_CTX ** ctx, SSL ** ssl);

#endif