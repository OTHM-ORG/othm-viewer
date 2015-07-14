all:
	cc -g -Wall -o main `pkg-config --cflags --libs gtk+-3.0` \
	main.c flownetwork.c -lm
