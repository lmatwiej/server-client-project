/* handler.c: HTTP Request Handlers */

#include "spidey.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* Internal Declarations */
Status handle_browse_request(Request *request);
Status handle_file_request(Request *request);
Status handle_cgi_request(Request *request);
Status handle_error(Request *request, Status status);

/**
 * Handle HTTP Request.
 *
 * @param   r           HTTP Request structure
 * @return  Status of the HTTP request.
 *
 * This parses a request, determines the request path, determines the request
 * type, and then dispatches to the appropriate handler type.
 *
 * On error, handle_error should be used with an appropriate HTTP status code.
 **/
Status  handle_request(Request *r) {

    log("entered handle_request\n");

    Status result;
    struct stat sb;

    /* Parse request */
    log("began to parse the request");
    if (parse_request(r) < 0){
        log("parse_request failed\n");
        result = handle_error(r, HTTP_STATUS_BAD_REQUEST);
        return result;
    }

    /* Determine request path */
    r->path = determine_request_path(r->uri);

    if(!r->path){
      result = handle_error(r, HTTP_STATUS_NOT_FOUND);
      return result;
    }

    debug("HTTP REQUEST PATH: %s", r->path);

    /* Dispatch to appropriate request handler type based on file type */
    if(stat(r->path, &sb) < 0){
        log("Request: handle_error\n");
        result = handle_error(r, HTTP_STATUS_NOT_FOUND);
    }
    else if((sb.st_mode & S_IFMT) == S_IFDIR){
        log("Request: handle_browse_request\n");
        result = handle_browse_request(r);
    }
    else if(S_ISREG(sb.st_mode)) {
        if(!access(r->path, X_OK)) {
            log("Request: handle_cgi_request\n");
            result = handle_cgi_request(r);
        }
        else if(!access(r->path, R_OK)){
        log("Request: handle_file_request\n");
        result = handle_file_request(r);
      }
    }
    else{
      log("Request: handle_error\n");
      result = handle_error(r, HTTP_STATUS_NOT_FOUND);
    }

    log("HTTP REQUEST STATUS: %s\n", http_status_string(result));
    return result;
}

/**
 * Handle browse request.
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP browse request.
 *
 * This lists the contents of a directory in HTML.
 *
 * If the path cannot be opened or scanned as a directory, then handle error
 * with HTTP_STATUS_NOT_FOUND.
 **/
Status  handle_browse_request(Request *r) {
    log("entered handle_browse_request\n");
    struct dirent **entries;
    int numHeader;

    /* Open a directory for reading or scanning */
    numHeader = scandir(r->path, &entries, NULL, alphasort);
    if(numHeader < 0){
      return handle_error(r, HTTP_STATUS_NOT_FOUND);
    }

    /* Write HTTP Header with OK Status and text/html Content-Type */
    fprintf(r->stream, "HTTP/1.0 200 OK\r\n");
    fprintf(r->stream, "Content-Type: text/html\r\n");
   	fprintf(r->stream, "\r\n");

    /* For each entry in directory, emit HTML list item */
    fprintf(r->stream, "<ul>\n");
    for(int i = 0; i < numHeader; i++){
      if(!streq(entries[i]->d_name, ".")){
        fprintf(r->stream, "<li><a href=\"%s/%s\">%s</a></li>\n",
        streq(r->uri, "/") ? "" : r->uri, entries[i]->d_name, entries[i]->d_name);
      }
      free(entries[i]);
    }
    fprintf(r->stream, "</ul>\n");


    /* Return OK */
    free(entries);
    fflush(r->stream);
    return HTTP_STATUS_OK;
}

/**
 * Handle file request.
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP file request.
 *
 * This opens and streams the contents of the specified file to the socket.
 *
 * If the path cannot be opened for reading, then handle error with
 * HTTP_STATUS_NOT_FOUND.
 **/
Status  handle_file_request(Request *r) {
    log("entered handle_file_request\n");
    FILE *file_stream;
    char buffer[BUFSIZ];
    char *mtype = NULL;
    size_t nread;

    /* Open file for reading */
    file_stream = fopen(r->path, "r");
    if( !file_stream ){
      fprintf(stderr, "fopen failed: %s\n", strerror(errno));
      log("fopen failed\n");
      return handle_error(r, HTTP_STATUS_NOT_FOUND);
    }

    /* Determine mimetype */
    mtype = determine_mimetype(r->path);
    debug("determine_mimetype -file_request");

     /* Write HTTP Headers with OK status and determined Content-Type */
    fprintf(r->stream, "HTTP/1.0 200 OK\r\n");
    fprintf(r->stream, "Content-Type: %s\r\n", mtype);
    fprintf(r->stream, "\r\n");

    /* Read from file and write to socket in chunks */
    nread = fread(buffer, 1, BUFSIZ, file_stream);
    while ( nread > 0 ) {
        if ( ! fwrite(buffer, 1, nread, r->stream) ) {
            fprintf(stderr, "%s\n", strerror(errno));
            goto fail;
        }
        nread = fread(buffer, 1, BUFSIZ, file_stream);
    }

     /* Close file, deallocate mimetype, return OK */
    fclose(file_stream);
    free(mtype);
    return HTTP_STATUS_OK;

fail:
    /* Close file, free mimetype, return INTERNAL_SERVER_ERROR */
    fclose(file_stream);
    free(mtype);
    return HTTP_STATUS_INTERNAL_SERVER_ERROR;
}

/**
 * Handle CGI request
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP file request.
 *
 * This popens and streams the results of the specified executables to the
 * socket.
 *
 * If the path cannot be popened, then handle error with
 * HTTP_STATUS_INTERNAL_SERVER_ERROR.
 **/
Status handle_cgi_request(Request *r) {
    log("entered handle_cgi_request\n");
    FILE *pfs;
    char buffer[BUFSIZ];
    struct header *header = r->headers;

    /* Export CGI environment variables from request:
     * http://en.wikipedia.org/wiki/Common_Gateway_Interface */
     if (setenv("DOCUMENT_ROOT", RootPath, 1))
         fprintf(stderr, "Error: Unable to set %s\n", strerror(errno));
     if (setenv("QUERY_STRING", r->query, 1))
         fprintf(stderr, "Error: Unable to set %s\n", strerror(errno));
     if (setenv("REMOTE_ADDR", r->host, 1))
         fprintf(stderr, "Error: Unable to set %s\n", strerror(errno));
     if (setenv("REMOTE_PORT", r->port, 1))
         fprintf(stderr, "Error: Unable to set %s\n", strerror(errno));
     if (setenv("REQUEST_METHOD", r->method, 1))
         fprintf(stderr, "Error: Unable to set %s\n", strerror(errno));
     if (setenv("REQUEST_URI", r->uri, 1))
         fprintf(stderr, "Error: Unable to set %s\n", strerror(errno));
     if (setenv("SCRIPT_FILENAME", r->path, 1))
         fprintf(stderr, "Error: Unable to set %s\n", strerror(errno));
     if (setenv("SERVER_PORT", Port, 1))
         fprintf(stderr, "Error: Unable to set %s\n", strerror(errno));

    /* Export CGI environment variables from request headers */
    while(header->name != NULL){
      if (streq(header->name, "Accept"))
			   setenv("HTTP_ACCEPT", header->data, 1);
		  if (streq(header->name, "Accept-Encoding"))
  			 setenv("HTTP_ACCEPT_ENCODING", header->data, 1);
  		if (streq(header->name, "Accept-Language"))
			   setenv("HTTP_ACCEPT_LANGUAGE", header->data, 1);
		  if (streq(header->name, "Connection"))
	       setenv("HTTP_CONNECTION", header->data, 1);
		  if (streq(header->name, "Host"))
			   setenv("HTTP_HOST", header->data, 1);
		  if (streq(header->name, "User-Agent"))
			   setenv("HTTP_USER_AGENT", header->data, 1);
      header = header->next;
    }

    /* POpen CGI Script */
    pfs = popen(r->path, "r");
    if(!pfs){
      pclose(pfs);
      return handle_error(r, HTTP_STATUS_INTERNAL_SERVER_ERROR);
    }

    /* Copy data from popen to socket */

    while(fgets(buffer, BUFSIZ, pfs)){
      fputs(buffer, r->stream);
    }

    /* Close popen, return OK */
    pclose(pfs);
    fflush(r->stream);

    return HTTP_STATUS_OK;
}

/**
 * Handle displaying error page
 *
 * @param   r           HTTP Request structure.
 * @return  Status of the HTTP error request.
 *
 * This writes an HTTP status error code and then generates an HTML message to
 * notify the user of the error.
 **/
Status  handle_error(Request *r, Status status) {
    log("entered handle_error\n");
    const char *statString = http_status_string(status);

    /* Write HTTP Header */
    fprintf(r->stream, "HTTP/1.0 %s\n", statString);
    fprintf(r->stream, "Content-Type: text/html\r\n");
    fprintf(r->stream, "\r\n");

    /* Write HTML Description of Error*/
    fprintf(r->stream, "<html><body>");
    fprintf(r->stream, "<h1>%s</h1>\n",statString);
    fprintf(r->stream, "<p>:( >:( ;,( Error!</p>\n");
    fprintf(r->stream, "<html><body>");

    /* Return specified status */
    fflush(r->stream);
    return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
