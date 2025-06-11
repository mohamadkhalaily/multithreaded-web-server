#include "segel.h"
#include "request.h"

struct request_m {
    int fd;
    Time arrive_time;
    Time dispatch_time;
    StatThread st;
};

void requestError(Request req, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    int fd = getFdRequest(req);
    char buf[MAXLINE], body[MAXBUF];

    // Create the body of the error message
    sprintf(body, "<html><title>OS-HW3 Error</title>");
    sprintf(body, "%s<body bgcolor=""fffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr>OS-HW3 Web Server\r\n", body);

    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Stat-Req-Arrival:: %lu.%06lu\r\n", req->arrive_time->tv_sec, req->arrive_time->tv_usec);
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Stat-Req-Dispatch:: %lu.%06lu\r\n", req->dispatch_time->tv_sec, req->dispatch_time->tv_usec);
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Stat-Thread-Id:: %d\r\n",  getThreadId(req->st));
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Stat-Thread-Count:: %d\r\n", getThreadCount(req->st));
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Stat-Thread-Static:: %d\r\n", getThreadStaticCount(req->st));
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Stat-Thread-Dynamic:: %d\r\n", getThreadDynamicCount(req->st));
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Content-Type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "Content-Length: %lu\r\n\r\n", strlen(body));
    Rio_writen(fd, buf, strlen(buf));

    // Write out the content
    Rio_writen(fd, body, strlen(body));
}

void requestReadhdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n"))
    {
        Rio_readlineb(rp, buf, MAXLINE);
    }
    return;
}

int requestParseURI(char *uri, char *filename, char *cgiargs)
{

    char *ptr;

    if (strstr(uri, ".."))
    {
        sprintf(filename, "./public/home.html");
        return 1;
    }

    if (!strstr(uri, "cgi"))
    {
        strcpy(cgiargs, "");
        sprintf(filename, "./public/%s", uri);
        if (uri[strlen(uri) - 1] == '/')
        {
            strcat(filename, "home.html");
        }
        return 1;
    } else
    {
        ptr = index(uri, '?');
        if (ptr)
        {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        } else
        {
            strcpy(cgiargs, "");
        }
        sprintf(filename, "./public/%s", uri);
        return 0;
    }}

void requestGetFiletype(char *filename, char *filetype)
{
    const char *extension = strrchr(filename, '.'); // Find the last occurrence of '.' in filename

    if (extension != NULL) {
        extension++;

        switch(extension[0]) {
            case 'h':
                if (strcmp(extension, "html") == 0)
                    strcpy(filetype, "text/html");
                else
                    strcpy(filetype, "text/plain");
                break;
            case 'g':
                if (strcmp(extension, "gif") == 0)
                    strcpy(filetype, "image/gif");
                else
                    strcpy(filetype, "text/plain");
                break;
            case 'j':
                if (strcmp(extension, "jpg") == 0)
                    strcpy(filetype, "image/jpeg");
                else
                    strcpy(filetype, "text/plain");
                break;
            default:
                strcpy(filetype, "text/plain");
                break;
        }
    }
    else {
        strcpy(filetype, "text/plain"); // No extension found
    }
}

// Serve dynamic content for a request
void requestServeDynamic(Request req, char *filename, char *cgiargs)
{
    int fd = getFdRequest(req);
    char buf[MAXLINE], *emptylist[] = {NULL};

    increaseDynamicCount(req->st);

    // The server does only a little bit of the header.
    // The CGI script has to finish writing out the header.
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: OS-HW3 Web Server\r\n", buf);

    sprintf(buf, "%sStat-Req-Arrival:: %lu.%06lu\r\n", buf, req->arrive_time->tv_sec, req->arrive_time->tv_usec);
    sprintf(buf, "%sStat-Req-Dispatch:: %lu.%06lu\r\n", buf, req->dispatch_time->tv_sec, req->dispatch_time->tv_usec);
    sprintf(buf, "%sStat-Thread-Id:: %d\r\n", buf, getThreadId(req->st));
    sprintf(buf, "%sStat-Thread-Count:: %d\r\n", buf, getThreadCount(req->st));
    sprintf(buf, "%sStat-Thread-Static:: %d\r\n", buf, getThreadStaticCount(req->st));
    sprintf(buf, "%sStat-Thread-Dynamic:: %d\r\n", buf, getThreadDynamicCount(req->st));
    Rio_writen(fd, buf, strlen(buf));

    int pid = Fork();
    if (pid == 0)
    {
        /* Child process */
        Setenv("QUERY_STRING", cgiargs, 1);
        /* When the CGI process writes to stdout, it will instead go to the socket */
        Dup2(fd, STDOUT_FILENO);
        Execve(filename, emptylist, environ);
    }
    WaitPid(pid, NULL, 0);}

// Serve static content for a request
void requestServeStatic(Request req, char *filename, int filesize)
{
    int fd = getFdRequest(req);
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    increaseStaticCount(req->st);

    requestGetFiletype(filename, filetype);

    srcfd = Open(filename, O_RDONLY, 0);

    // Rather than call read() to read the file into memory,
    // which would require that we allocate a buffer, we memory-map the file
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);

    // put together response
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: OS-HW3 Web Server\r\n", buf);
    sprintf(buf, "%sContent-Length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-Type: %s\r\n", buf, filetype);

    sprintf(buf, "%sStat-Req-Arrival:: %lu.%06lu\r\n", buf, req->arrive_time->tv_sec, req->arrive_time->tv_usec);
    sprintf(buf, "%sStat-Req-Dispatch:: %lu.%06lu\r\n", buf, req->dispatch_time->tv_sec, req->dispatch_time->tv_usec);
    sprintf(buf, "%sStat-Thread-Id:: %d\r\n", buf, getThreadId(req->st));
    sprintf(buf, "%sStat-Thread-Count:: %d\r\n", buf, getThreadCount(req->st));
    sprintf(buf, "%sStat-Thread-Static:: %d\r\n", buf, getThreadStaticCount(req->st));
    sprintf(buf, "%sStat-Thread-Dynamic:: %d\r\n\r\n", buf, getThreadDynamicCount(req->st));

    Rio_writen(fd, buf, strlen(buf));

    //  Writes out to the client socket the memory-mapped file
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);}

// Handle a request
void requestHandle(Request req)
{
    int fd = getFdRequest(req);
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    printf("%s %s %s\n", method, uri, version);

    if (strcasecmp(method, "GET"))
    {
        requestError(req, method, "501", "Not Implemented", "OS-HW3 Server does not implement this method");
        return;
    }
    requestReadhdrs(&rio);

    is_static = requestParseURI(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0)
    {
        requestError(req, filename, "404", "Not found", "OS-HW3 Server could not find this file");
        return;
    }

    if (is_static)
    {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
        {
            requestError(req, filename, "403", "Forbidden", "OS-HW3 Server could not read this file");
            return;
        }
        requestServeStatic(req, filename, sbuf.st_size);
    } else
    {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
        {
            requestError(req, filename, "403", "Forbidden", "OS-HW3 Server could not run this CGI program");
            return;
        }
        requestServeDynamic(req, filename, cgiargs);
    }}

// Create a new request
Request createRequest(int fd, Time arrive_time)
{
    Request newRequest = malloc(sizeof(*newRequest));
    if (!newRequest)
    {
        return NULL;
    }

    newRequest->fd = fd;
    newRequest->arrive_time = arrive_time;
    newRequest->dispatch_time = malloc(sizeof(*newRequest->dispatch_time));
    newRequest->st = NULL;

    return newRequest;
}

// Copy a request
void *copyRequest(void *request)
{
    if (!request) {
        return NULL;
    }
    return createRequest(getFdRequest(request), getArriveTimeRequest(request));
}

// Destroy a request
void destroyRequest(void *request)
{
    free(request);
}

// Get the file descriptor associated with a request
int getFdRequest(Request request)
{
    return request->fd;
}

// Get the arrival time of a request
Time getArriveTimeRequest(Request request)
{
    return request->arrive_time;
}

// Get the thread associated with a request
StatThread getThreadRequest(Request request)
{
    return request->st;
}

// Set the dispatch time for a request
void setDispatchRequest(Request request, Time new_dispatch_time)
{
    Time dispatch= malloc(sizeof(*dispatch));
    timersub (new_dispatch_time , request->arrive_time, dispatch);
    request -> dispatch_time->tv_sec = dispatch->tv_sec;
    request -> dispatch_time->tv_usec = dispatch->tv_usec;
    free(dispatch);
}

// Set the thread associated with a request
void requestSetThread(Request request, StatThread new_thread)
{
    request->st = new_thread;
}