#include "csapp.h"

void doit(int fd);
void read_reqheaders(rio_t *rio);
int parse_uri(char *uri, char *filename, char *cgiargs);
void client_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);

int main(int arg, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if(argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
    }

    listenfd = Open_listenfd(argv[1]);
    
    while(1)
    {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)", hostname, port);
        doit(connfd);
        Close(connfd);
    }
}

void doit(int fd)
{
    struct stat sbuf;
    int is_static;
    rio_t rio;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];

    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s\n", buf);
    Rio_writen(fd, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if(strcasecmp(method, "GET") != 0)
    {
        client_error(fd, method, "501", "Not implemented", "Tiny does not support this method");
        return;
    }
    read_reqheaders(&rio);

    is_static = parse_uri(uri, filename, cgiargs);
    if(stat(filename, &sbuf) < 0)
    {
        client_error(fd, filename, "404", "Not found", "File couldn't be found");
        return;
    }

    if(is_static)
    {
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
        {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size);
    }
    else
    {
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
        {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read this CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
    
}

void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *scrp, filetype[MAXLINE], buf[MAXBUF];

    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf. strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}

void get_filetype(char *filename, char *filetype)
{
    if(strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if(strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if(strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if(strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
    
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if(Fork() == 0)
    {
        setenv("QUERY_STRING", cgiargs, 1);
        Dup2(fd, STDOUT_FILENO);
        Execve(filename, emptylist, environ);
    }
    Wait(NULL);
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if(!strstr(uri, "cgi-bin"))
    {
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcpy(filename, uri);
        if(uri[strlen(uri)-1] == "/")
            strcat(filename, "home.html");
        return 1;
    }
    else
    {
        ptr = index(uri, "?")
        if(ptr)
        {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }
        else
        {
            strcpy(cgiargs, "");
        }
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

void client_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    sprintf(body, "<html><title>Title Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);

    sprintf(buf, "HTTP/1.0 %s.%s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %s\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

void read_reqheaders(rio_t *rio)
{
    char buf[MAXLINE];

    Rio_readlineb(rio, buf, MAXLINE);
    while(strcmp(buf, "\r\n"))
    {
        Rio_readlineb(rio, buf, MAXLINE);
        printf("%s", buf);
    }

    return;
}
