project('http', 'c')
executable('http', 'main.c')

uring = dependency('liburing')

executable('async-http', 'async.c', dependencies: [uring])
