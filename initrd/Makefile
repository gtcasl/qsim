initrd.cpio: sbin/test_threads sbin/test_inst sbin/test_mem sbin/qsim_io sbin/mark_app etc/fstab init sbin/hexdump dev proc sys data
	find dev sbin proc init etc sys data -path "*/.*" -o -print |\
          cpio -H newc -o > initrd.cpio

sbin/mark_app : mark_app.c
sbin/test_threads : test_threads.c
sbin/qsim_io : qsim_io.c
sbin/test_inst: test_inst.c
sbin/test_mem: test_mem.c

sbin/hexdump : sbin/busybox
	bash -c 'cd sbin; for a in `./busybox --list-full`; do ln -sf ./busybox $$a; done; cd ..'

sbin/% : %.c
	gcc -o $@ -pthread -static $<

dev proc sys data:
	mkdir -p dev proc sys data


clean: 
	rm -f sbin/test_inst sbin/test_mem sbin/qsim_io sbin/test_threads sbin/mark_app initrd.cpio *~ \#*
