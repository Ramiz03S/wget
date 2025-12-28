#include "util.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

long strtol_caller(char * num_str, int base){
    char * endptr;
    return strtol(num_str,&endptr, base);
}

void log_stderr(){
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
}
