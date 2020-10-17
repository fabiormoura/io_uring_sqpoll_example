all: sqpoll_main

sqpoll_main: sqpoll_main.c
	gcc -Wall -Werror -o $@ sqpoll_main.c -luring

clean:
	@rm -v sqpoll_main
