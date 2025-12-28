#include "http_common.h"
#include "mpc.h"
#include "util.h"


void init_response_components_t(response_components_t * recv){
    recv->content_length = -1;
    recv->status = -1;
    recv->chunked_encoding = -1;
}

void form_get_req(char * host, char * path, char * get_req_buffer){
    if(path == NULL){
        path = "/";
    }
    snprintf(get_req_buffer, GET_REQ_BUFF_SIZE - 1, "GET %s HTTP/1.1\r\n"
                                                    "Host: %s\r\nConnection: keep-alive\r\n"
                                                    "User-Agent: Wget/1.21.4\r\n"
                                                    "Accept: */*\r\n"
                                                    "Accept-Encoding: identity\r\n"
                                                    "\r\n", path, host);
    
}

void static tree_traversal(mpc_ast_t * tree_node, response_components_t * response_components){
    char * tag = tree_node->tag;
    char * contents = tree_node->contents;
    static int in_header_line = 0;
    static int child_counter = 0;
    static char * header_type;
    mpc_ast_t * node_child;
    size_t content_len = strlen(contents);

    if(strstr(tag, "status")){
        response_components->status = strtol_caller(contents, 10);
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
            response_components->content_length = strtol_caller(contents, 10);
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
        //mpc_ast_print(r.output);
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
