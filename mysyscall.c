#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/sched.h>
#include<linux/syscalls.h>

long (*address)(int *,char **) = NULL;;
EXPORT_SYMBOL(address);

asmlinkage long sys_cs695sc(int *argv,char **argc) {
	if(address != NULL) {
		printk(KERN_INFO "System call is diverted to LKM function");
		address(argv,argc);
	}else {
		printk(KERN_INFO "Default cs695sc system call has been invoked !\n");
	}
	return 0;
}
