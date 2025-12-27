#include <stdio.h>
#include <stdlib.h>
#include <openssl/ssl.h>

void ssl_ctx_init(SSL_CTX * ctx){
    /*
    * Create an SSL_CTX which we can use to create SSL objects from. We
    * want an SSL_CTX for creating clients so we use TLS_client_method()
    * here.
    */
    ctx = SSL_CTX_new(TLS_client_method());
    if(ctx == NULL){
        fprintf(stderr, "Failed to create the SSL_CTX\n");
        exit(EXIT_FAILURE);
    }

    /*
    * Configure the client to abort the handshake if certificate
    * verification fails. Virtually all clients should do this unless you
    * really know what you are doing.
    */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    /* Use the default trusted certificate store */
    if (!SSL_CTX_set_default_verify_paths(ctx)) {
        fprintf(stderr, "Failed to set the default trusted certificate store\n");
        exit(EXIT_FAILURE);
    }

    /*
    * TLSv1.1 or earlier are deprecated by IETF and are generally to be
    * avoided if possible. We require a minimum TLS version of TLSv1.2.
    */
    if (!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION)) {
        fprintf(stderr, "Failed to set the minimum TLS protocol version\n");
        exit(EXIT_FAILURE);
    }
}

void ssl_ssl_init(SSL_CTX * ctx, SSL * ssl, BIO *bio, int sock, char * hostname){
    /* Create an SSL object to represent the TLS connection */
    ssl = SSL_new(ctx);
    if (ssl == NULL) {
        fprintf(stderr, "Failed to create a new SSL object\n");
        exit(EXIT_FAILURE);
    }

    /* Create a BIO to wrap the socket */
    bio = BIO_new(BIO_s_socket());
    if (bio == NULL) {
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
    BIO_set_fd(bio, sock, BIO_NOCLOSE);

    SSL_set_bio(ssl, bio, bio);

    /*
    * Tell the server during the handshake which hostname we are attempting
    * to connect to in case the server supports multiple hosts.
    */
    if (!SSL_set_tlsext_host_name(ssl, hostname)) {
        fprintf(stderr, "Failed to set the SNI hostname\n");
        exit(EXIT_FAILURE);
    }

    /*
    * Ensure we check during certificate verification that the server has
    * supplied a certificate for the hostname that we were expecting.
    * Virtually all clients should do this unless you really know what you
    * are doing.
    */
    if (!SSL_set1_host(ssl, hostname)) {
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

int main(){

    SSL_CTX * ctx;
    SSL * ssl;
    BIO *bio;

    return 0;
}