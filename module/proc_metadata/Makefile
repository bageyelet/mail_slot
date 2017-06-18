KBUILD_EXTRA_SYMBOLS := ../actual_ms/Module.symvers
obj-m += mail_spot_proc_metadata.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
