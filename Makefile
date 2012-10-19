# Makefile pre proj01

CC = gcc
CFLAGS = -Wall -ansi -pthread -g $(PNAME).c -o $(PNAME)
PNAME = proj01

build: $(PNAME).c
	$(CC) $(CFLAGS)

clean: $(PNAME).c
	rm -f $(PNAME)
