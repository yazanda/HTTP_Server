#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "threadpool.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>

#define FUNCTION_SUCCESS 0
#define FUNCTION_FAIL (-1)

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define MAX_REQ 500
#define MAX_RES 1024

int isNumber(char *);
char *get_mime_type(char *);
int parseRequest(char**, int*, int*, int*, int*);
int parseHeader(char*, char*, char*);
int startSocketListener(int*, struct sockaddr_in*,int, int);
void constructResponse(char*, char*, char*);
int socketRead(int, char*);
int socketWrite(int, char*);
int dispatchHandle(void *);
int dirResponse(int , char*, char*);
int fileResponse(int, char*, char*);

int response; //variable saves the response code.
/*Function checks if a given text is all digits*/
int isNumber(char *txt){
    for (int i = 0; i < strlen(txt); i++) {
        if((int)txt[i] < 48 || (int)txt[i] > 57)
            return FUNCTION_FAIL;
    }
    return FUNCTION_SUCCESS;
}
/*Function parses input in the argv*/
int parseRequest(char **request, int *port, int *size, int *number, int *flag){
    if(strncmp(request[1], "server", strlen(request[1])) != 0) { //first index
        *flag = 0;
        return FUNCTION_FAIL;
    }
    if(isNumber(request[2]) == FUNCTION_FAIL || isNumber(request[3]) == FUNCTION_FAIL || isNumber(request[4]) == FUNCTION_FAIL) { //check if inputs is digits.
        *flag = 1;
        return FUNCTION_FAIL;
    }
    *port = (int)strtol(request[2], NULL, 10);
    *size = (int)strtol(request[3], NULL, 10);
    *number = (int)strtol(request[4], NULL, 10);
    if(*port < 1 || *number < 1 || *size < 1 || *size > MAXT_IN_POOL) { //input limits.
        *flag = 1;
        return FUNCTION_FAIL;
    }
    *flag = 0;
    return FUNCTION_SUCCESS;
}
/*Function returns type of file in a directory.*/
char *get_mime_type(char *name) {
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}
/*Function that connects to the server, creates the welcome socket and do listen*/
int startSocketListener(int *socket_fd, struct sockaddr_in* server, int port, int max){
    if ((*socket_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {//open socket
        perror("socket() failed");
        return FUNCTION_FAIL;
    }
    //initializing struct values.
    server->sin_family = AF_INET;
    server->sin_addr.s_addr = htonl(INADDR_ANY);
    server->sin_port = htons(port);
    if (bind(*socket_fd, (struct sockaddr*)server, sizeof(struct sockaddr_in)) < 0){//bind
        perror("bind() failed");
        close(*socket_fd);
        return FUNCTION_FAIL;
    }
    if (listen(*socket_fd, max) < 0){//listen
        perror("listen() failed");
        close(*socket_fd);
        return FUNCTION_FAIL;
    }
    return FUNCTION_SUCCESS;
}
/*Function that reads socket request*/
int socketRead(int socket_fd, char* msg){
    int reader;
    char *temp = (char *) calloc(MAX_REQ, sizeof(char));
    while (1){
        bzero(temp, MAX_REQ);
        reader = (int)read(socket_fd, temp, sizeof(temp));
        if (reader < 0) {
            perror("read() failed");
            free(temp);
            return FUNCTION_FAIL;
        }
        else if (reader > 0) {
            sprintf(msg + strlen(msg), "%s", temp);
            if (strstr(temp, "\r\n"))
                break;
        }
        else break;
    }
    free(temp);
    return FUNCTION_SUCCESS;
}
/*Function parses the first line of socket's request*/
int parseHeader(char* header, char* path, char* protocol){
    char method[MAX_REQ];
    int count = 0;
    char tmp[strlen(header)+1];
    strncpy(tmp, header, strlen(header)+1);
    char *token = strtok(tmp, " ");
    while(token != NULL){ //get first three tokens.
        switch (count) {
            case 0:
                strncpy(method, token, strlen(token)+1);
                break;
            case 1:
                strncpy(path, token, strlen(token)+1);
                break;
            case 2:
                strncpy(protocol, token, strlen(token)+1);
                break;
            default:
                break;
        }
        count++;
        token = strtok(NULL, " ");
    }
    if(count != 3){ //if more than three tokens.
        response = 400;
        return FUNCTION_FAIL;
    }
    if(strcmp(method, "GET") != 0){//check supported method.
        response = 501;
        return FUNCTION_FAIL;
    }
    if(protocol != strstr(protocol, "HTTP/1.0") && protocol != strstr(protocol, "HTTP/1.1")){//check supported protocol.
        response = 400;
        return FUNCTION_FAIL;
    }
    /*******Parsing Path*******************************************************************************************/
    struct stat fileInfo;
    struct dirent *fileEntity;
    DIR *directory;
    if(lstat(path, &fileInfo) < 0){//search for path.
        if(errno == ENOENT)
            response = 404;
        else if(errno == EACCES)
            response = 403;
        else response = 500;
        return FUNCTION_FAIL;
    }
    if(S_ISDIR(fileInfo.st_mode)){//if directory.
        if(path[strlen(path)-1] != '/'){//ends with '/'.
            response = 302;
            return FUNCTION_FAIL;
        }
        directory = opendir(path);
        if(directory == NULL){
            if(errno == EACCES)
                response = 403;
            else response = 500;
            return FUNCTION_FAIL;
        }
        int found = 0; //looking for index.html.
        fileEntity = readdir(directory);
        while (fileEntity != NULL){
            if(strcmp(fileEntity->d_name, "index.html") == 0){
                strcat(path, "index.html");
                found = 1;
                if(lstat(path, &fileInfo) < 0){
                    if(errno == EACCES)
                        response = 403;
                    else response = 500;
                    return FUNCTION_FAIL;
                }
                else break;
            }
            fileEntity = readdir(directory);
        }
        if(found) response = 201;
        else response = 202;
        closedir(directory);
    } else if(S_ISREG(fileInfo.st_mode))
        response = 201;
    else{
        response = 500;
        return FUNCTION_FAIL;
    }
    return FUNCTION_SUCCESS;
}
/*Function that construct response to be sent to the socket.*/
void constructResponse(char* toSend, char *path, char *timeBuf){
    char *status = NULL;
    switch (response) { //check response status.
        case 302:
            status = "302 Found";
            break;
        case 400:
            status = "400 Bad Request";
            break;
        case 403:
            status = "403 Forbidden";
            break;
        case 404:
            status = "404 Not Found";
            break;
        case 500:
            status = "500 Internal Server Error";
            break;
        case 501:
            status = "501 Not supported";
            break;
        default:
            break;
    }
    if(response != 201 && response != 202) {
        char header[MAX_RES], body[MAX_RES];
        char *type = get_mime_type("index.html");
        sprintf(body, "<HTML><HEAD><TITLE>%s</TITLE></HEAD>\r\n<BODY><H4>%s</H4>\r\n%s\r\n</BODY></HTML>\r\n\r\n",
                status, status + 4, response == 302 ? "Directories must end with a slash." : response == 400 ? "Bad Request." :
                                    response == 403 ? "Access denied." : response == 404 ? "File not found." :
                                    response == 500 ? "Some server side error." : "Method is not supported.");
        sprintf(header, "HTTP/1.0 %s\r\nServer: webserver/1.0\r\nDate: %s\r\n%s%s%s%sContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
                status, timeBuf, response == 302 ? "Location: " : "", response == 302 ? path : "", response == 302 ? "/" : "", response == 302 ? "\r\n" : "", type, (int) strlen(body));
        sprintf(toSend, "%s%s", header, body);
    }
}
/*Function that writes response to the socket*/
int socketWrite(int socket_fd, char *toSend){
    int toWrite = (int)strlen(toSend);
    while (toWrite > 0) {
        if (write(socket_fd, toSend, 1) < 0) {
            perror("write() failed");
            return FUNCTION_FAIL;
        }
        toSend++;
        toWrite--;
    }
    return FUNCTION_SUCCESS;
}
/*Function that constructs file response and sends to the socket*/
int fileResponse(int socket_fd, char *path, char *timeBuf) {
    struct stat fileInfo;
    char toSend[MAX_RES], file_data[MAX_RES*10] = {0}, newTimeBuf[128];
    int file_fd, reader, i;

    //open the file for read only.
    file_fd = open(path, O_RDONLY, S_IRUSR);
    if (file_fd < 0) {
        if (errno == EACCES)
            response = 403;
        else
            response = 500;
        return FUNCTION_FAIL;
    }
    if (lstat(path, &fileInfo) < 0) {//search for the path.
        response = 500;
        close(file_fd);
        return FUNCTION_FAIL;
    }
    //get the file name
    for (i = (int)strlen(path)-1; i >= 0; i--) {
        if (path[i] == '/') break;
    }
    char *mimeType = get_mime_type(path + i + 1);
    if (!mimeType) { //file type.
        response = 403;
        close(file_fd);
        return FUNCTION_FAIL;
    }
    //constructing response.
    sprintf(toSend,"HTTP 1.0 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: ", timeBuf);
    strftime(newTimeBuf, sizeof(newTimeBuf), RFC1123FMT, gmtime(&fileInfo.st_mtime));//last modified time of the file.
    sprintf(toSend + strlen(toSend),"%s\r\nContent-length: %lu\r\nLast-Modified: %s\r\nConnection: close\r\n\r\n",
              mimeType, fileInfo.st_size, newTimeBuf);

    //send response.
    if (socketWrite(socket_fd, toSend) < 0) {
        close(file_fd);
        return FUNCTION_FAIL;
    }
    //reading data from file to send.
    while (1){
        reader = (int)read(file_fd, file_data, sizeof(file_data));
        if (reader < 0) {
            perror("read");
            response = 500;
            close(file_fd);
            return FUNCTION_FAIL;
        } else if (reader > 0) {
            if (socketWrite(socket_fd, file_data) < 0){
                break;
            }
        } else break;
    }
    socketWrite(socket_fd, "\r\n\r\n");
    close(file_fd);
    return FUNCTION_SUCCESS;
}
/*Function constructs and sends directory response.*/
int dirResponse(int socket_fd, char *path,char *timeBuf){
    char header[MAX_RES], newTimeBuf[128], dirTb[128];
    struct stat fileInfo;
    struct dirent *fileEntity = NULL;
    DIR *folder;
    char body[MAX_RES*10];

    folder = opendir(path); //open directory
    if (!folder) {
        response = 500;
        return FUNCTION_FAIL;
    }
    if (lstat(path, &fileInfo) < 0){//find path.
        response = 500;
        closedir(folder);
        return FUNCTION_FAIL;
    }
    //construct response.
    sprintf(header,"HTTP 1.0 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: text/html\r\n",timeBuf);
    //last modified time of the folder.
    strftime(dirTb, sizeof(dirTb), RFC1123FMT, gmtime(&fileInfo.st_mtime));
    sprintf(body,"<HTML>\r\n<HEAD><TITLE> Index of %s</TITLE></HEAD>\r\n<BODY>\r\n<H4>Index of %s</H4>\r\n", path + 1, path + 1);
    sprintf(body + strlen(body),"<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n");

    //passing on directory's files.
    fileEntity = readdir(folder);
    while (fileEntity) {
        if (!strcmp (fileEntity->d_name, ".")) {
            fileEntity = readdir(folder);
            continue;
        }
        strcat(path, fileEntity->d_name);
        if (lstat(path, &fileInfo) < 0){//find path.
            if (errno == EACCES)
                response = 403;
            else
                response = 500;
            return FUNCTION_FAIL;
        }
        //last modified time of the current file
        strftime(newTimeBuf, sizeof(newTimeBuf), RFC1123FMT, gmtime(&fileInfo.st_mtime));
        //add file information to the body.
        sprintf(body + strlen(body),"<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td><td>", fileEntity->d_name, fileEntity->d_name, newTimeBuf);
        //check if the current entity is a file (not a directory)
        if(S_ISREG(fileInfo.st_mode))
            sprintf(body + strlen(body), "%lu", fileInfo.st_size);
        sprintf(body + strlen(body), "</td></tr>\r\n");
        path[strlen(path) - strlen(fileEntity->d_name)] = '\0'; //get path only.
        fileEntity = readdir(folder);
    }

    sprintf(body + strlen(body),"</table>\r\n<HR>\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n</HR>\r\n</BODY></HTML>\r\n\r\n");
    sprintf(header + strlen(header),"Content-Length: %lu\r\nLast-Modified: %s\r\nConnection: close\r\n\r\n", strlen(body), dirTb);
    char toSend[strlen(body) + strlen(header)];
    sprintf(toSend, "%s%s", header, body);

    //send response.
    socketWrite(socket_fd, toSend);
    closedir(folder);
    return FUNCTION_SUCCESS;
}
/*Function to be dispatched by threads*/
int dispatchHandle(void *arg){
    if(arg == NULL)
        return FUNCTION_FAIL;
    int socket_fd = *((int*)arg);// get socket's fd.
    free(arg);
    //construct time.
    time_t now;
    char timeBuffer[128];
    now = time(NULL);
    strftime(timeBuffer, sizeof(timeBuffer), RFC1123FMT, gmtime(&now));
    //read request.
    char message[5000] = {0};
    if(socketRead(socket_fd, message) == FUNCTION_FAIL){
        return FUNCTION_FAIL;
    }
    char header[MAX_REQ] = {0};
    char path[MAX_REQ] = {0}, protocol[MAX_REQ] = {0}, toSend[MAX_RES] = {0};
    char *check = strstr(message, "\r\n");
    if(check == NULL){//error in request.
        response = 400;
        constructResponse(toSend, NULL, timeBuffer);
        socketWrite(socket_fd, toSend);
        return FUNCTION_FAIL;
    }
    strncpy(header, message, check-message);
    if(parseHeader(header, path, protocol) == FUNCTION_FAIL){//error in request.
        constructResponse(toSend, path, timeBuffer);
        socketWrite(socket_fd, toSend);
        return FUNCTION_FAIL;
    }
    if(response == 201){//file response.
        if(fileResponse(socket_fd, path, timeBuffer) == FUNCTION_FAIL){
            constructResponse(toSend, path, timeBuffer);
            socketWrite(socket_fd, toSend);
            return FUNCTION_FAIL;
        }
    } else if(response == 202){//directory response.
        if(dirResponse(socket_fd, path, timeBuffer) == FUNCTION_FAIL){
            constructResponse(toSend, path, timeBuffer);
            socketWrite(socket_fd, toSend);
            return FUNCTION_FAIL;
        }
    }
    return FUNCTION_SUCCESS;
}
/**Main**/
int main(int argc, char *argv[]){
    int port, poolSize, maxNum, flag;
    //parsing input.
    if(argc != 5 || (parseRequest(argv, &port, &poolSize, &maxNum, &flag) == FUNCTION_FAIL && flag == 0)){
        printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
        exit(EXIT_FAILURE);
    } else if(parseRequest(argv, &port, &poolSize, &maxNum, &flag) == FUNCTION_FAIL && flag == 0){
        printf("incorrect input!\n");
        exit(EXIT_FAILURE);
    }
    int welcomeSocket;
    struct sockaddr_in *server;
    server = (struct sockaddr_in*)calloc(1, sizeof(struct sockaddr_in*));
    if(server == NULL)
        perror("alloc() failed");
    if(startSocketListener(&welcomeSocket, server, port, maxNum) == FUNCTION_FAIL) {
        free(server);
        exit(EXIT_FAILURE);
    }
    //create thread_pool.
    threadpool *th_p = create_threadpool(poolSize);
    if(th_p == NULL) {
        free(server);
        close(welcomeSocket);
        exit(EXIT_FAILURE);
    }
    //dispatching.
    int request = 0, accept_socket, *arg;
    struct sockaddr_in address;
    socklen_t socketLength = sizeof(struct sockaddr_in);
    while(request < maxNum) {
        bzero((char*)&address, sizeof(struct sockaddr_in));
        accept_socket = accept(welcomeSocket, (struct sockaddr*)&address, &socketLength); //accept connection.
        if (accept < 0)
            perror("opening new socket\n");
        else { //dispatch.
            arg = (int*)calloc(1, sizeof(int));
            if (arg == NULL)
                perror("error while allocating memory\n");
            else {
                *arg = accept_socket;
                dispatch(th_p, dispatchHandle, (void*)arg);
                request++;
            }
        }
    }
    free(server);
    destroy_threadpool(th_p);
    close(welcomeSocket);
    return 0;
}