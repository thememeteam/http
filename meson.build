project('http', 'c')
executable('http', 'src/main.c')

uring = dependency('liburing')

executable('async-http', 'src/async.c', dependencies: [uring])
