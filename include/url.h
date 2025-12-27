#ifndef URL_H
#define URL_H

typedef struct {
    char * scheme;
    char * host;
    char * port;
    char * path;
} URL_components_t;


void init_URL_components_t(URL_components_t * URL);

void free_URL_components_t(URL_components_t * URL);

void parse_URL(char * URL, URL_components_t * URL_components);



#endif