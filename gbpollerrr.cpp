#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include <iostream>

#define ARRAY_SIZE(ra) (sizeof(ra)/sizeof(ra[0]))
#define MAXEVENTS 64
#define handle_error(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

int epfd = -1;
int fd_signal = -1;

struct callback_wrapper {
    // as far as I can tell, no way to cast a data pointer to
    // a function pointer, so using a wrapper instead
    void (*fire)();
};

void on_signal()
{
    /* read fd_signal to handle event */
    std::cout << "Boo ya: FD_SIG: " << fd_signal << "\n";
    struct signalfd_siginfo info;
    ssize_t bytes = read(fd_signal, &info, sizeof(info));
    assert(bytes == sizeof(info));
}

int setup_fd_signal()
{
    sigset_t mask;
    sigemptyset ( &mask );
    sigaddset ( &mask, SIGINT );

    // block default signal handler
    if ( sigprocmask ( SIG_BLOCK, &mask, NULL ) == 1 )
      handle_error("sigprocmask");

    int fd_signal = signalfd ( -1, &mask, 0 );
    if ( fd_signal == -1 )
      handle_error ( "signalfd" );

    return fd_signal;
}

 
  
int main(int argc, char* argv[])
{
    epfd = epoll_create ( 1 );
    int ret = 0;

    fd_signal = setup_fd_signal();
    struct callback_wrapper* cb;
    cb = (struct callback_wrapper *) calloc(1, sizeof(struct callback_wrapper));
    cb->fire = &on_signal;
    epoll_event fd_signal_data = { EPOLLIN, { .ptr = cb }};

    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd_signal, &fd_signal_data);
    if (ret)
      handle_error("epoll_ctl");

    epoll_event events[MAXEVENTS];
    int event_count;
    for (;;) {
        event_count = epoll_wait(epfd, events, MAXEVENTS, -1);
        if ( event_count < 0 )
          handle_error("epoll_wait");

        for ( unsigned n = 0 ; n < event_count ; ++n ) {
            auto callback = static_cast<struct callback_wrapper*>(events[n].data.ptr)->fire;
            callback();
        }
    }

    return 0;
}
