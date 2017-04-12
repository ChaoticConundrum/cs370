#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Charlie Waters");
MODULE_DESCRIPTION("Testing filesystem driver");
MODULE_VERSION("0.1");

// Init function
static int __init tmod_init(void){
    printk(KERN_INFO "TMOD: Hello world\n");
    return 0;
}

// Exit function
static void __exit tmod_exit(void){
    printk(KERN_INFO "TMOD: Goodbye world\n");
}

// Register functions
module_init(tmod_init);
module_exit(tmod_exit);

