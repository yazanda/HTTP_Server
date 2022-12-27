#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "threadpool.h"
#define FUNCTION_SUCCESS 0
#define FUNCTION_FAIL (-1)

/*Function checks if a given text is all digits*/
int isNumber(char *txt){
    for (int i = 0; i < strlen(txt); i++) {
        if((int)txt[i] < 48 || (int)txt[i] > 57)
            return FUNCTION_FAIL;
    }
    return FUNCTION_SUCCESS;
}
int parseRequest(char **request, int *port, int *size, int *number){
    if(isNumber(request[1]) == FUNCTION_FAIL || isNumber(request[2]) == FUNCTION_FAIL || isNumber(request[3]) == FUNCTION_FAIL)
        return FUNCTION_FAIL;
    *port = (int)strtol(request[1], NULL, 10);
    *size = (int)strtol(request[2], NULL, 10);
    *number = (int)strtol(request[3], NULL, 10);
    if(*port == 0 || *size == 0 || *number < 1)
        return FUNCTION_FAIL;
    return FUNCTION_SUCCESS;
}

int main(int argc, char *argv[]){
    int port, poolSize, maxNum;
    if(argc != 4 || parseRequest(argv, &port, &poolSize, &maxNum) == FUNCTION_FAIL){
        printf("Usage: server <port> <pool-size>\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}