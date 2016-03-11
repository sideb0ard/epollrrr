#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include <iostream>
#include <vector>

#define ARRAY_SIZE(ra) (sizeof(ra)/sizeof(ra[0]))
#define MAXEVENTS 64
#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct fd_callback_data {
    int fd;
    epoll_event data;
};


int  epfd = -1;
int  fd_signal = -1;
int  fd_timer = -1;
bool read_from_stdin = false;


// FD_SIGNAL ////////////////////////////////
void on_signal()
{
    /* read fd_signal to handle event */
    std::cout << "Boo ya: FD_SIG: " << fd_signal << "\n";
    struct signalfd_siginfo info;
    ssize_t bytes = read(fd_signal, &info, sizeof(info));
    assert(bytes == sizeof(info));
}
struct fd_callback_data setup_signalfd()
{
    sigset_t mask;
    sigemptyset ( &mask );
    sigaddset ( &mask, SIGINT );

    // block default signal handler
    if ( sigprocmask ( SIG_BLOCK, &mask, NULL ) == 1 )
        handle_error("sigprocmask");

    fd_signal = signalfd ( -1, &mask, 0 );
    if ( fd_signal == -1 )
        handle_error ( "signalfd" );

    epoll_event fd_signal_data = { EPOLLIN, { .ptr = (void*) on_signal }};

    return (struct fd_callback_data){ fd_signal, fd_signal_data };

}
// END FD_SIGNAL ////////////////////////////////


// FD_STDIN ////////////////////////////////
void on_stdin()
{
    std::cout << "Boo ya: STDIN Input\n";
    std::string input;
    std::cin >> input;
    std::cout << "I gots :" << input << std::endl;
}
struct fd_callback_data setup_stdin()
{
    epoll_event fd_stdin_data = { EPOLLIN, { .ptr = (void*) on_stdin }};
    return (struct fd_callback_data){ STDIN_FILENO, fd_stdin_data };
}
// FD_STDIN ////////////////////////////////


// FD_TIMER ////////////////////////////////
void on_timer_up()
{
    std::cout << "Boom, timer expired!\n";
    uint64_t exp;
    ssize_t s = read(fd_timer, &exp, sizeof(uint64_t));
    assert(s == sizeof(uint64_t));
}
struct fd_callback_data setup_timerfd()
{

    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) == -1)
        handle_error("clock_gettime");

    struct itimerspec new_value;
    new_value.it_value.tv_sec = now.tv_sec + 3;
    new_value.it_value.tv_nsec = now.tv_nsec;
    new_value.it_interval.tv_nsec = 0;

    fd_timer = timerfd_create(CLOCK_REALTIME, 0);
    if (fd_timer == -1)
        handle_error("timerfd_create");

    if (timerfd_settime(fd_timer, TFD_TIMER_ABSTIME, &new_value, NULL) == -1)
        handle_error("timerfd_settime");

    epoll_event fd_timer_data = { EPOLLIN, { .ptr = (void*) on_timer_up }};

    return (struct fd_callback_data){ fd_timer, fd_timer_data };
}
// FD_TIMER ////////////////////////////////


int main(int argc, char* argv[])
{
    std::vector<fd_callback_data> polls;
    polls.push_back(setup_signalfd());
    polls.push_back(setup_timerfd());
    polls.push_back(setup_stdin());

    epfd = epoll_create ( 1 );

    for (auto poll : polls) {
        int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, poll.fd, &poll.data);
        if (ret)
            handle_error("epoll_ctl");
    }

    epoll_event events[MAXEVENTS];
    int event_count;
    for (;;) {
        event_count = epoll_wait(epfd, events, MAXEVENTS, -1);
        if ( event_count < 0 )
            handle_error("epoll_wait");

        for ( unsigned n = 0 ; n < event_count ; ++n ) {
            auto callback = reinterpret_cast<void (*)()>(events[n].data.ptr);
            callback();
        }
    }
}
