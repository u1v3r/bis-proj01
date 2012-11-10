# Makefile pre proj01

CC = gcc
CFLAGS_APP = -Wall -ansi -pthread -g $(PNAME).c -o $(PNAME)
PNAME = proj01
obj-m += hidepid.o


build: $(PNAME).c
	$(CC) $(CFLAGS_APP)
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	
clean: $(PNAME).c
	rm -f $(PNAME)
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	
	
	