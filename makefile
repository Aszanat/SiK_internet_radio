FLAGS=-Wall -O2 -g -o

all: A B

A: sikradio-sender.c err.h
	gcc $(FLAGS) sikradio-sender sikradio-sender.c

B: sikradio-receiver.c err.h
	gcc $(FLAGS) sikradio-receiver sikradio-receiver.c