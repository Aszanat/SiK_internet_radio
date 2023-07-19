FLAGS=-Wall -O2 -g -o

all: A B

A: sikradio-sender.c err.h song-eater.c
	gcc $(FLAGS) sikradio-sender sikradio-sender.c
	gcc $(FLAGS) song-eater song-eater.c

B: sikradio-receiver.c err.h
	gcc -lpthread $(FLAGS) sikradio-receiver sikradio-receiver.c

clean:
	rm sikradio-receiver
	rm sikradio-sender