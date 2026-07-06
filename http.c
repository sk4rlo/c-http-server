/*
 *
 * This section contains HTTP logic:
 * parsing request and build HTTP response.
 * The supported requested is get,
 * the server can serve to client static files.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "http.h"

//-------------------HTTP request logic--------------------------

/* helper to copy tokens on the http request header struct*/
int copy_token(char *dest, size_t dest_size, const char *src, size_t token_len){
    
    if (dest == NULL || src == NULL || dest_size == 0) {
        return -1;
    }

    if( token_len >= dest_size || token_len == 0) {
        return -1;
    } 

    memcpy(dest, src, token_len);
    dest[token_len] = '\0';
    return 0;
}

/* read the buffer and fill the fields of http_request struct, as defined in http.h
 * As an input, it receives a pointer to the start of buffer, the buffer lenght,
 * and an empty request header that the function will populate .
 * Returns 0 or error codes. */

int parse_http_request(const char *buffer, size_t len, http_request_header *request_header){
    
    /* check the content of the request */
    if (buffer == NULL || len == 0 || request_header == NULL) {
    return -1;
    }

    /* since the standard does not specify a max request line,
     * the size of the line is dynamically inferred from the loop */
    size_t end_index = 0;
    int found = 0;

    /* populate the http request line reading the first line from the buffer.
     * sicne the loop reads i+1 index, it can read the element buffer[len] 
     * which is outside the boundary. For this reason the for loop uses len-1 */
    for(size_t i = 0; i + 1 < len; i++){
        if(buffer[i] == '\r' && buffer[i+1] == '\n'){
            end_index = i;
            found = 1;
            break;     
        }     
    }
    
    if(!found) return -1;

    /* save request line and copy buffer after the first line is found */
    size_t line_len = end_index;
    char *request_line = malloc(line_len + 1);
    //safety check for malloc
    if (request_line == NULL) return -1;

    /* copies line_len bytes from buffer to request_line */
    memcpy(request_line, buffer, line_len);
    request_line[line_len] = '\0';

    size_t word_start = 0;
    int word_index = 0;
    size_t word_length = 0;  

    /* save request_line words in http request header struct fields */
    for(size_t i = 0; i < line_len; i++){
         
        //advance the index and save a word every time you find a space
        if(request_line[i] == ' '){
            word_length = i - word_start;

            //first word is method, second is path, third is version
            if (word_index == 0){
                if(copy_token(request_header->method, sizeof(request_header->method),
                 request_line + word_start, word_length) < 0) {
                    free(request_line);
                    return -1;
                 }
            } else if (word_index == 1){
                if(copy_token(request_header->path, sizeof(request_header->path),
                 request_line + word_start, word_length) < 0) {
                    free(request_line);
                    return -1;
                 }
            } 

            word_start = i + 1;
            word_index++;
        }        
    }

    //if there are not 2 spaces in request header return error
    if (word_index!=2) {
        free(request_line);
        return -1;
    }

    //final word is handled after the loop as it has different conditions 
    if(copy_token(request_header->version, sizeof(request_header->version),
                 request_line + word_start, line_len - word_start) < 0) {
    free(request_line);
    return -1;
    }

    free(request_line);
    return 0;
}

//-------------------HTTP response logic--------------------------

/* This function gets a filled http_response and convert its field
 * to a string */
char *make_response_string(http_response *response, size_t *out_size){

    /* snprintf writes to a memory buffer (first parameter) and 
     * returns number of bytes written. Since header_len buffer is NULL,
     * it is just used to estimate how much memory I need */
    int header_len = snprintf(
    NULL,
    0,
    "%s %d %s\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %zu\r\n"
    "Connection: %s\r\n"
    "\r\n",
    response->version,
    response->status_code,
    response->status_text,
    response->content_type,
    response->content_length,
    response->connection
    ); 
    
    /* allocates memory for http response string (metadata + body)
     * header + null term + length of body content*/
    char *response_string = malloc(header_len + 1 + response->content_length);
    if (response_string == NULL) return NULL;

    /* save the content of http response in response string */
    int written = snprintf(
    response_string,
    header_len + 1,
    "%s %d %s\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %zu\r\n"
    "Connection: %s\r\n"
    "\r\n",
    response->version,
    response->status_code,
    response->status_text,
    response->content_type,
    response->content_length,
    response->connection
    );

    /* attach the bytes from the body to the end of the response_string */
    memcpy(response_string + written, response->body, response->content_length); 
    /* save header lenght for sending parameters */
    *out_size = written + response->content_length;
    response_string[written + response->content_length] = '\0';
    return response_string;

}

/* This function gets a response with the status code as an input 
 * from the client request and populates its fields.
 * Returns 0 for success or error codes. */
 
int build_response(http_response *response){

    if (response == NULL) return -1;

    response->version = "HTTP/1.0";
    response->connection = "close";

    switch(response->status_code){
        case 400:
            response->status_text = "Bad Request";
            response->content_type = "text/plain";
            response->body = "Bad Request\n";
            response->content_length = strlen(response->body);
            response->owns_body = 0;
            break;

        case 404:
            response->status_text = "Not Found";
            response->content_type = "text/plain";
            response->body = "Not Found\n";
            response->content_length = strlen(response->body);
            response->owns_body = 0;
            break;

        case 405:
            response->status_text = "Method Not Allowed";
            response->content_type = "text/plain";
            response->body = "Method Not Allowed\n";
            response->content_length = strlen(response->body);
            response->owns_body = 0;
            break;
            
        case 500:
            response->status_text = "Internal Server Error";
            response->content_type = "text/plain";
            response->body = "Internal Server Error\n";
            response->content_length = strlen(response->body);
            response->owns_body = 0;
            break;

        case 200:
            /* succesful requests body and type will be populated 
            by an handle_request function in server.c so it is more easy to add services */

            /* this makes a check, assuming body and content type are populated by the handle request */
            if(response->content_type == NULL || response->body == NULL) return -1;

            response->status_text = "OK";
            break;

        default:
            return -1;
    }
    return 0;
}


