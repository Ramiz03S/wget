#include "url.h"
#include "util.h"
#include "mpc.h"


void init_URL_components_t(URL_components_t * URL){
    URL->scheme = NULL;
    URL->host = NULL;
    URL->port = NULL;
    URL->path = NULL;
}

void free_URL_components_t(URL_components_t * URL){
    free(URL->scheme);
    free(URL->host);
    free(URL->port);
    free(URL->path);
}

static void find_URL_component(mpc_ast_t *a, char * component, char ** found_contents) {
 
    for (int i = 0; i < a->children_num; i++) {
        mpc_ast_t * child = a->children[i];
        if(strstr(child->tag, component)){
          *found_contents = calloc(strlen(child->contents)+1, 1);
          memcpy(*found_contents, child->contents, strlen(child->contents)+1);
          break;
        }
    }

}

void parse_URL(char * URL, URL_components_t * URL_components){
    mpc_parser_t *url  = mpc_new("url");
    mpc_parser_t *scheme  = mpc_new("scheme");
    mpc_parser_t *host  = mpc_new("host");
    mpc_parser_t *port = mpc_new("port");
    mpc_parser_t *path = mpc_new("path");    
    
    mpc_err_t * err = mpca_lang(MPCA_LANG_DEFAULT,
        "url : /^/ (<scheme>\"://\")? <host> (\":\"<port>)? <path>? (\"?\" /[^#\\r\\n]*/)? (\"#\" /[^\\r\\n]*/)? /$/;"
        "scheme : \"https\" | \"http\";"
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
        find_URL_component(output, "scheme", &(URL_components->scheme));
        find_URL_component(output, "host", &(URL_components->host));
        find_URL_component(output, "port", &(URL_components->port));
        find_URL_component(output, "path", &(URL_components->path));
        mpc_ast_delete(r.output);
    } else {
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
        mpc_cleanup(5, url, scheme, host, port, path);
        exit(EXIT_FAILURE);
    }
    
    if(URL_components->port){
        long val;
        val = strtol_caller(URL_components->port, 10);
        if (val < 0 || val > 65535){
            fprintf(stderr, "Port value in URL is outside of expected bounds\n");
            exit(EXIT_FAILURE);
        }
    }

    if(!URL_components->scheme && !URL_components->port){
        // we default to https if not provided by user
        char https_port[4] = "443";
        URL_components->port = (char *)malloc(4);
        strcpy(URL_components->port, https_port);
    }

    mpc_cleanup(5, url, scheme, host, port, path);
}
