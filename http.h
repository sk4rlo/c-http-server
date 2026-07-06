/*
 * Public interface for the HTTP module.
 * Defines HTTP-related structs and function prototypes
 * used by the server.
 */

//if HTTP_H has not been defined yet, continue
#ifndef HTTP_H

#include <stddef.h>

#define HTTP_H

/*
 * Parsed request line.
 * Fixed arrays are used because these are small bounded HTTP tokens
 * copied from the request buffer (client has ownership on content).
 */
typedef struct{
    char method[16];
    char path[1024];
    char version[16];
}  http_request_header;

/*
 * HTTP response metadata and body.
 * String pointers can refer to fixed literals defined by the http server logic
 * so a max size is not needed.
 */
typedef struct {
    const char *version;
    int status_code;
    const char *status_text;
    char *content_type;
    size_t content_length;
    char *connection;
    char *body;
    int owns_body;
} http_response;


int parse_http_request(const char *buffer, size_t len, http_request_header *request_header);
int build_response(http_response *response);
char *make_response_string(http_response *response, size_t *out_size); 


#endif
