#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/random.h>
#include <linux/blkdev.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Inject EIO errors into blk_update_request for kernel 6.14+");

static int injection_probability = 10;
module_param(injection_probability, int, 0644);
MODULE_PARM_DESC(injection_probability, "Probability of EIO injection (0-100)");

static struct kprobe kp;
static unsigned long injection_count = 0;

// For kernel 6.14+, use the kernel's get_random_u32 directly
static u32 get_random_wrapper(void)
{
    return get_random_u32();
}

// For kernel 6.14+, use req_op
#include <linux/blk-mq.h>
static inline bool is_fs_request(struct request *req)
{
    return req_op(req) == REQ_OP_READ || req_op(req) == REQ_OP_WRITE;
}

static int pre_handler(struct kprobe *p, struct pt_regs *regs)
{
    struct request *req;
    int error;
    unsigned int nr_bytes;
    
    // Get function parameters (x86_64 architecture)
    req = (struct request *)regs->di;
    error = (int)regs->si;
    nr_bytes = (unsigned int)regs->dx;
    
    // Safety check: ensure req pointer is valid
    if (!req) {
        return 0;
    }
    
    // Check if injection conditions are met
    if (error == 0) {
        // Check if request type is a filesystem request
        if (is_fs_request(req)) {
            // Generate random number
            u32 random = get_random_wrapper() % 100;
            
            if (random < injection_probability) {
                injection_count++;
                printk(KERN_INFO "EIO-Inject: Injected #%lu (original error: %d)\n", 
                       injection_count, error);
                
                // Modify error parameter to -EIO
                regs->si = -EIO;
            }
        }
    }
    
    return 0;
}

static int __init inject_init(void)
{
    int ret;
    
    kp.pre_handler = pre_handler;
    kp.symbol_name = "blk_update_request";
    
    ret = register_kprobe(&kp);
    if (ret < 0) {
        printk(KERN_ERR "EIO-Inject: Failed to register kprobe: %d\n", ret);
        return ret;
    }
    
    printk(KERN_INFO "EIO-Inject: Module loaded for kernel 6.14+, probability: %d%%\n", 
           injection_probability);
    return 0;
}

static void __exit inject_exit(void)
{
    unregister_kprobe(&kp);
    printk(KERN_INFO "EIO-Inject: Module unloaded, total injections: %lu\n", 
           injection_count);
}

module_init(inject_init);
module_exit(inject_exit);
