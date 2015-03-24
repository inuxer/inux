/*This is a simple example epoll, just show you how to use epoll, 
  referenced some open source code when I write this code.
  However, examples are all similar to others. May you like it*/
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#define MAXEVENTS 128

static int 
make_socket_non_blocking (int sockfd);

static int 
create_and_bind (char *port);


static int
make_socket_non_blocking (int sockfd)
{
    int flags, status;
    
    flags = fcntl (sockfd, F_GETFL, 0); /*get sockfd's status*/
    if (flags == -1)
    {
        perror ("fcntl");
        return -1;
    }
    
    flags |= O_NONBLOCK;
    status = fcntl (sockfd, F_SETFL, flags); /*set sockfd to no-blocking*/
    if (status == -1)
    {
        perror ("fcntl");
        return -1;
    }
    
    return 0;
}

static int
create_and_bind (char *port)
{
    struct addrinfo hints; 
    struct addrinfo *result, *rp;
    int status, sockfd;
    
    bzero(&hints, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* we want a TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* all interfaces */
    
    status = getaddrinfo(NULL, port, &hints, &result);
    if (status != 0)
    {
        fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (status));
        return -1;
    }
    
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {/*traverse all returned IP addresses before one bind succeed*/
        sockfd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1)
          continue;
    
        status = bind (sockfd, rp->ai_addr, rp->ai_addrlen);
        if (status == 0)
        {
            /* we managed to bind successfully! */
            break;
        }
    }
    
    if (rp == NULL)
    {
        fprintf (stderr, "Could not bind\n");
        return -1;
    }
    
    freeaddrinfo (result);
    
    return sockfd;
}

int
main (int argc, char *argv[])
{
    int sockfd, status;
    int epollfd;
    struct epoll_event event;
    struct epoll_event *events;
    
    if (argc != 2)
    {
        fprintf (stderr, "Usage: %s [port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    sockfd = create_and_bind (argv[1]); /*get a new socket fd if succeed*/
    if (sockfd == -1) /*kill this process if bind failed*/
      abort();
    
    status = make_socket_non_blocking (sockfd);
    if (status == -1)
      abort();
    
    status = listen (sockfd, SOMAXCONN); /*SOMAXCONN is the max backlog*/
    if (status == -1)
    {
        perror ("listen");
        abort();
    }
    
    epollfd = epoll_create1 (0); /*epoll_create() replaced by epoll_create1(0)*/
    if (epollfd == -1)
    {
        perror("epoll_create");
        abort();
    }
    
    event.data.fd = sockfd;
    event.events = EPOLLIN | EPOLLET; /*listening sockfds ready for read, edge trigger*/
    status = epoll_ctl (epollfd, EPOLL_CTL_ADD, sockfd, &event); /*add the sockfd to epoll monitor epollfd*/
    if (status == -1)
    {
        perror ("epoll_ctl");
        abort();
    }
    
    /* buffer where events are returned */
    events = calloc (MAXEVENTS, sizeof event);
    
    /* The event loop */
    while (1)
    {
        int n, i;
    
	    /*blocking until notification(s) on the listening sockets*/
        n = epoll_wait (epollfd, events, MAXEVENTS, -1);
        for (i = 0; i < n; i++)
	    {
	        if ((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN)))
	        {
                    /* an error has occured on this fd, or the socket is not
                       ready for reading (why were we notified then?) */
	            fprintf (stderr, "epoll error\n");
	            close (events[i].data.fd);
	            continue;
	        }
            
	        else if (sockfd == events[i].data.fd)
	             {
                    /* We have a notification on the listening socket, which
                       means one or more incoming connections. */
                    while (1)
                    {
                        struct sockaddr in_addr;
                        socklen_t in_len;
                        int infd;
                        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
            
                        in_len = sizeof in_addr;
                        infd = accept (sockfd, &in_addr, &in_len); /*accept the connect request from client*/
                        if (infd == -1)
                        {
                            if ((errno == EAGAIN) ||
                                (errno == EWOULDBLOCK))
                            {
                                /* we have processed all incoming
                                   connections. */
                                break;
                            }
                            else
                            {
                                perror ("accept");
                                break;
                            }
                        }
            
                        status = getnameinfo (&in_addr, in_len,
                                         hbuf, sizeof hbuf,
                                         sbuf, sizeof sbuf,
                                         NI_NUMERICHOST | NI_NUMERICSERV);
                        if (status == 0)
                        {
                            printf("Accepted connection on descriptor %d "
                                   "(host=%s, port=%s)\n", infd, hbuf, sbuf);
                        }
            
                        /* make the incoming socket non-blocking and add it to the
                           list of fds to monitor. */
                        status = make_socket_non_blocking (infd); 
                        if (status == -1)
                          abort ();
            
                        event.data.fd = infd;
                        event.events = EPOLLIN | EPOLLET;
                        status = epoll_ctl (epollfd, EPOLL_CTL_ADD, infd, &event); 
                        if (status == -1)
                        {
                            perror ("epoll_ctl");
                            abort ();
                        }
                    }
                    continue;
                }
                else
                {
                    /* We have data on the fd waiting to be read. Read and
                       display it. We must read whatever data is available
                       completely, as we are running in edge-triggered mode
                       and won't get a notification again for the same
                       data. */
                    int done = 0;
            
                    while (1)
                    {
                        ssize_t count;
                        char buf[512];
            
                        count = read (events[i].data.fd, buf, sizeof buf);
                        if (count == -1)
                        {
                            /* If errno == EAGAIN, that means we have read all
                               data. So go back to the main loop. */
                            if (errno != EAGAIN)
                            {
                                perror ("read");
                                done = 1;
                            }
                            break;
                        }
                        else if (count == 0)
                        {
                            /* End of file. The remote has closed the
                               connection. */
                            done = 1;
                            break;
                        }
            
                        /* Write the buffer to standard output */
                        status = write (1, buf, count);
                        if (status == -1)
                        {
                            perror ("write");
                            abort ();
                        }
                    }
            
                    if (done)
                    {
                        printf ("Closed connection on descriptor %d\n",
                                events[i].data.fd);
            
                        /* Closing the descriptor will make epoll remove it
                           from the set of descriptors which are monitored. */
                        close (events[i].data.fd);
                    }
                }
        }
    }
    
    free (events);
    close (sockfd);
    return EXIT_SUCCESS;
}
