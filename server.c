/*
 * 
 * This server was developed for learning purposes:
 * using socket related libraries.
 * Currently, it supports only IPv4 address family.
 * N.B: code may contain bugs or security issues. 
 *
 */

#include <stdio.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <signal.h>
#include "http.h"

//----------------global variables------------------------

// number of pending connection supported by the server
#define BACKLOG 5

#define STATIC_ROOT "files"
#define REQUEST_BUFFER_SIZE 4096
#define LOCAL_PATH_SIZE 2048

struct timespec start;
struct timespec end;
//-----------------errors and logs------------------------

void err_sys(const char *fmt, ...){

    // initialize variable arguments
    va_list args;

    // starts to read arguments after fmt
    va_start(args, fmt);
    // print the custom formatted error message to stderr
    vfprintf(stderr, fmt, args);
    // clean up variable-argument handling
    va_end(args);

    /* Convert the current errno value to a human-readable message
     * and write it to stderr after the custom error message.
     * errno is set by the kernel / C library depending on the function/syscall */

    fprintf(stderr, ": %s\n", strerror(errno));
    exit(1);
}

void log_request(const http_request_header *request, const http_response *response,
                struct timespec start, struct timespec end)
{
    double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
        (end.tv_nsec - start.tv_nsec) / 1000000.0;

    // log bad requests
    if (request == NULL) {
        printf("- - -> %d %.2fms\n", response->status_code, elapsed_ms);
        return;
    }

    // standard log
    printf("%s %s -> %d %.2fms\n",
           request->method,
           request->path,
           response->status_code,
           elapsed_ms);
}

//-----------------helper functions------------------------

int serve_file(FILE *fp, http_response *response){
    
    // set file position indicator
    if(fseek(fp, 0, SEEK_END) < 0){
        fclose(fp);
        return -1;
    } 

    // get file size
    long file_size = ftell(fp);

    if (file_size < 0) {
        fclose(fp);
        return -1;
    }

    // rewind the indicator at the start of the file
    rewind(fp);
    

    // allocate memory for body
    response->body = malloc(file_size);
    response->owns_body = 1;

    if (response->body == NULL) {
        fclose(fp);
        return -1;
    }

    // populate body reading from the file
    size_t nread = fread(response->body, 1, file_size, fp);

    if (nread != (size_t)file_size) {
        fclose(fp);
        free(response->body);
        response->body = NULL;
        response->owns_body = 0;
        return -1;
    }

    response->content_length = file_size;
    fclose(fp);
    return 0;
}

// this is a wrapper of build_response needed for succesful requests
int serve_request(http_response *response, http_request_header *request_header){

    // check not null requests and supported methods
    if (response == NULL || request_header == NULL) return -1;
    if (strcmp(request_header->method, "GET") != 0){
        response->status_code = 405;
    //split health outputs when implementing multi-thread
    } else if((strcmp(request_header->path, "/") == 0) || 
        (strcmp(request_header->path, "/healthz") == 0) || 
        (strcmp(request_header->path, "/readyz") == 0))
    {
        // serve static plain text
        response->status_code = 200;
        response->content_type = "text/plain";
        response->body = "Hello from server\n";
        response->content_length = strlen(response->body);
        response->owns_body = 0;
    } else if(strcmp(request_header->path, "/hostname")==0)
    {
        // +1 because of null terminator
        char hostname[HOST_NAME_MAX+1];
        if((gethostname(hostname, sizeof(hostname)) == 0)){
            response->status_code = 200;
            response->body = malloc(sizeof(hostname));
            if(response->body != NULL){
            //write hostname into response body, freed later by main as owns_body = 1
                snprintf(response->body, sizeof(hostname), "%s", hostname);
                response->content_type = "text/plain";
                response->content_length = strlen(response->body);
                response->owns_body = 1;
            } else  response->status_code = 500;
        } else response->status_code = 500;
    } else {
        response->status_code = 200;
        response->content_type = "text/html";

        // build the local path from the request
        char local_path[LOCAL_PATH_SIZE];

        // static root needed to search the full path (a guard for root fs)
        int path_len = snprintf(local_path, sizeof(local_path), "%s%s",
                        STATIC_ROOT, request_header->path);

        // return 400 if snprintf fails, path is too large, or contains '..'
        if (path_len < 0 || (size_t)path_len >= sizeof(local_path) || 
            strstr(request_header->path, "..") != NULL)
        {
            response->status_code = 400;
        } else {
            FILE *fp = fopen(local_path, "rb");

            // try to serve file if present in local path
            if(fp != NULL){
                if(serve_file(fp, response)<0){
                    response->status_code = 500;
                }
            } else {
                response->status_code = 404;
            }    
        }
    }

    int n = build_response(response);
    if (n < 0) return -1;
    
    return 0;
}

/* since typical TCP buffer size is 4096, send() may send only 
 * that number of bytes, send_all is needed to avoid this 
 * and will send the remaining bytes eventually */

int send_all(int fd, const char *buffer, size_t size){

    size_t total_sent = 0;

    while (total_sent < size) {

        // normal send on first iteration (total sent = 0)
        // then the buffer pointer is moved to the remaining butes to send
        ssize_t sent = send(fd, buffer + total_sent, size - total_sent, 0);

        // checks for send errors 
        if (sent <= 0) return -1;

        // increase count of bytes sent
        total_sent += sent;
    }

    return 0;
}

//---------------------server implementation-------------

int main(void){

    // unbuffer calls writing to stdout to show logs in docker
    setvbuf(stdout, NULL, _IOLBF, 0);

    // prevent terminate signals which stops the process when send fails (e.g. closed TCP connection)
    signal(SIGPIPE, SIG_IGN);

    // definition of user space socket data structure (addr. family, port, address, padding)
    struct sockaddr_in server_address; 
    char *endptr;
    long server_port;
    const char *address_string;
    const char *port_string;


    // initialize server address user space struct
    bzero(&server_address, sizeof(server_address));

    // set address family in the sockaddr_in user space socket structure
    server_address.sin_family = AF_INET;
    
    /*
     * read environmantal variables with fallback on default values.
     * value = (condition) ? value if true : value if false 
     * is the variable set? if true value is ... : if false is   
     *
     */

    port_string = (getenv("PORT") != 0) ? getenv("PORT") : "8080"; 
    address_string = (getenv("LISTEN_ADDR") != NULL) ? getenv("LISTEN_ADDR") : "127.0.0.1";

    // convert port string to a long
    server_port = strtol(port_string, &endptr, 10);

    /*
     * For the port: htons swaps bytes, because the host is little endian
     * and the network uses big endian. For the address: inet_pton converts text to binary,
     * and already gives network byte order, so no swap is needed.
     *
     */

    if((*endptr != '\0')||((server_port < 1024) || (server_port > 65535))){
        fprintf(stderr, "Invalid PORT '%s': it must be a number between 1024 and 65535\n", port_string);
        exit(1);
    } else server_address.sin_port = htons(server_port);

    if(inet_pton(AF_INET, address_string, &server_address.sin_addr)!=1) {
        fprintf(stderr, "Invalid LISTEN_ADDR '%s'\n", address_string);
        exit(1);
    }

    /*
     * Current host is little endian while network order is big endian,
     * so you need to swap bytes to read values correctly.
     * For IP there is an additional format passage as you should group the result   
     * using one byte (or 2 hex digits) before being converted to a human readable format.
     * In this case for the address the inet_pton function provides the full conversion.
     *
     * to see the final structure content:
     * printf("port: %d\n", server_address.sin_port);
     * printf("ip number: %u\n", server_address.sin_addr.s_addr);
     *
     */

    // creation of the kernel object for socket (sock), represented as a file descriptor
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd<0){
        err_sys("Socket creation failed");
    }


    /*
     * Binding of server address information to socket object. 
     * The kernel reads family port and anddress and pass the values to 
     * kernel space socket-related structures.
     *
     */

    if (bind(fd, (struct sockaddr *) &server_address,
        sizeof(server_address)) != 0){
        err_sys("failed to bind %d to port %d", fd, ntohs(server_address.sin_port));
    }

    /*
     * Start listening to accept incoming connections:
     * the state of socket structures becomes TCP_LISTEN and so the kernel
     * is able to accept a queue of incoming connections from clients
     * which are able to initialize an handshake  under the hood.
     * 
     */

    if (listen(fd, BACKLOG) < 0){
        err_sys("failed to listen on port %s and address %s",
            port_string, address_string );
    } else {
        printf("started listening on port %s and address %s\n",
            port_string, address_string);
    }

    // definition of client socket data structure (addr. family, port, address, padding)
    struct sockaddr_in client_address;

    // loop to manage client connections
    while(1){

        // save client address lenght
        socklen_t client_address_len = sizeof(client_address);

        /*
        * Accept connection from a client address:
        * the server process sleep until a child of the listening kernel socket (sock)
        * is in the queue. A new socket kernel structure is created for the child 
        * represented as a file descriptor.
        *
        * On the user space, it fills the client information of the user space socket     
        * (sockaddr_in client_address) so now the 4 tuple (client and server port and ip)
        * is complete. 
        *
        */

        int client_fd = accept(fd, (struct sockaddr *) &client_address, &client_address_len); 
        if (client_fd < 0) {
            fprintf(stderr, "failed to accept connection: %s\n", strerror(errno));
            continue;
        }

        // Application buffer where recv copies bytes from the socket.
        char buffer[REQUEST_BUFFER_SIZE];

        // read request from client_fd
        ssize_t rn = recv(client_fd, buffer, REQUEST_BUFFER_SIZE - 1, 0);
        if (rn < 0){
            fprintf(stderr, "failed to read from socket: %s\n", strerror(errno));
            close(client_fd);
            continue;
        } else if (rn == 0){
            fprintf(stderr, "client disconnected\n");
            close(client_fd);
            continue;
        } else {
            buffer[rn] = '\0';
        }


        clock_gettime(CLOCK_MONOTONIC, &start);

        /* parse buffer to implement http logic and send http responses,
        * the functions are defined in http.c */ 

        http_request_header request_header = {0};
        http_response response = {0};
        char *response_string = NULL;
        size_t response_size = 0;

        // store the parser result and then checks for error
        int parse_result = parse_http_request(buffer, rn, &request_header);

        /* error 400: bad request if parser fails due to malformed syntax,
        * invalid data, or incorrect formatting */
        if (parse_result < 0) {
            response.status_code = 400; 
            int n = build_response(&response);
            if (n < 0) {
                clock_gettime(CLOCK_MONOTONIC, &end);
                log_request(NULL, &response, start, end);
                goto cleanup;
            }
            response_string = make_response_string(&response, &response_size);
        } else {
            int n = serve_request(&response, &request_header);
            if (n < 0) {
                clock_gettime(CLOCK_MONOTONIC, &end);
                log_request(&request_header, &response, start, end);
                goto cleanup;
            }
            response_string = make_response_string(&response, &response_size);
        }

        if (response_string != NULL) {
            // wrapper for send syscall
            if(send_all(client_fd, response_string, response_size) < 0){
                fprintf(stderr, "send failed\n");
                goto cleanup;
            }
            clock_gettime(CLOCK_MONOTONIC, &end);
            log_request(&request_header, &response, start, end);
        } else {
            clock_gettime(CLOCK_MONOTONIC, &end);
            fprintf(stderr, "failed to build response string\n");
            // if parse result < 0 use NULL, else use request_header
            log_request(parse_result < 0 ? NULL : &request_header, &response, start, end);
        }
        
    // that check is needed to avoid freeing a string literal (undefined behaviour)
    cleanup:
        if (response.owns_body) {
            free(response.body);
        }

        free(response_string);
        close(client_fd);
        continue;
    }
    
    return 0;
}