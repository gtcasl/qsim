initrd.cpio: sbin/test_threads sbin/qsim_io sbin/mark_app etc/fstab init
	find dev sbin proc init etc sys data | cpio -H newc -o > initrd.cpio

sbin/mark_app : mark_app.c
sbin/test_threads : test_threads.c
sbin/qsim_io : qsim_io.c

sbin/% : %.c
	gcc -o $@ -pthread -static $<

clean: 
	rm -f sbin/qsim_io sbin/test_threads sbin/mark_app initrd.cpio *~ \#*
