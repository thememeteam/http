project('http', 'c')
executable('http', 'src/main.c')

uring = dependency('liburing')

executable('uring-http', 'src/uring.c', dependencies: [uring])

executable('epoll-http', 'src/epoll.c')
