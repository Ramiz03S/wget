#ifndef HTTPS_H
#define HTTPS_H
#include <openssl/ssl.h>

void ssl_ctx_init(SSL_CTX * ctx);
void ssl_ssl_init(SSL_CTX * ctx, SSL * ssl, BIO *bio, int sock, char * hostname);
void ssl_handshake(SSL * ssl);



#endif