#include <iostream>
#include <thread>

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#define MAXEVENTS 64

class EventHandler {
public:
  ~EventHandler() = default;
  int fd = 0;
  void (*fire)(int fd);
};

class EVloop
{
public:
  EVloop();
  int addEvent(EventHandler* eh);
  int rmEvent(int fd);
  void run();
private:
  int epfd;
  struct epoll_event event;
  struct epoll_event *events;
};

EVloop::EVloop()
{
  epfd = epoll_create1 (0);
  if ( epfd == -1 ) {
    perror ("Epoll Create1 errrrr");
    abort();
  }
  events = (epoll_event *) calloc (MAXEVENTS, sizeof(event));
  if ( events == NULL ) {
    perror ("Cannae Calloc yer events!");
    abort();
  }
}

void EVloop::run()
{
  for (;;) {
    int nr_events, i;
    nr_events = epoll_wait (epfd, events, MAXEVENTS, -1);
    if ( nr_events < 0 ) {
      perror("Epoll_wait");
      return;
    }

    for ( i = 0; i < nr_events; i++) {
      printf ("Event %ld on fd=%d\n", events[i].events, events[i].data.fd);
      exit(0);
      //static_cast<EventHandler*>(events[i].data.ptr)->fire(events[i].data.fd);
    }
  }
}

int EVloop::addEvent(EventHandler* eh)
{
  printf("Got yer FD! %d\n", eh->fd);
  int ret;

  event.data.fd = eh->fd;
  event.data.ptr = eh;
  event.events = EPOLLIN | EPOLLOUT;

  ret = epoll_ctl (epfd, EPOLL_CTL_ADD, eh->fd, &event);
  if (ret)
    perror("Epoll_ctl_add");
}

void blah(int fd)
{
  std::cout << "Yar!: " << fd << "\n";
}

int main()
{

  EVloop ev;
  std::thread {&EVloop::run, &ev}.detach();

  std::cout << "Created an EVloop\n";

  int fd = open ( "/home/vagrant/FIFO", O_RDONLY);
  printf("FD is %d\n", fd);
  EventHandler* eh = (EventHandler *) calloc(1, sizeof(EventHandler));
  eh->fd = fd;
  eh->fire = &blah;

  printf("EH FD is %d\n", eh->fd);

  ev.addEvent(eh);

  for (;;) {}

}

