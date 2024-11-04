all: friend
.PHONY: all clean
friend: friend.c hw2.h
	gcc -o friend friend.c
clean:
	rm -rf friend
