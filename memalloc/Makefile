obj-m += memalloc.o
memalloc-y += memalloc-main.o memalloc-helper.o

all:
	make -C /usr/src/linux M=$(PWD) modules

clean:
	make -C /usr/src/linux M=$(PWD) clean

install:
	insmod memalloc.ko

remove:
	rmmod memalloc
