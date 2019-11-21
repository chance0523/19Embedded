#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
//#include <stdio.h>

static int hello_init(void)
{	
	printk("hello word\n");
	return 0;
}

static void hello_exit(void)
{
	printk("GoodBye \n");

}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("Daul BSD/GPL");
	

