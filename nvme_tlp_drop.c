/*
 * nvme_tlp_drop.c - Kprobe module to selectively drop NVMe completion TLPs
 * 
 * Simulates PCIe TLP loss for NVMe completion messages only
 * Usage: insmod nvme_tlp_drop.ko drop_rate=10
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/random.h>
#include <linux/pci.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Tool");
MODULE_DESCRIPTION("Selectively drop NVMe completion TLPs");

static int drop_rate = 5;  // 5% default drop rate
module_param(drop_rate, int, 0644);
MODULE_PARM_DESC(drop_rate, "Percentage of NVMe completions to drop (0-100)");

static unsigned long dropped_count = 0;
static unsigned long total_count = 0;

/* Forward declarations for NVMe internal structures */
struct nvme_queue;
struct nvme_dev;

/* Kprobe for nvme_irq */
static struct kprobe nvme_irq_kp = {
    .symbol_name = "nvme_irq",
};

/* Simplified check - just verify pointer is not NULL */
static int is_nvme_device(struct nvme_queue *nvmeq)
{
    /* Basic sanity check - if pointer is NULL, it's not valid */
    return nvmeq != NULL;
}

/* Pre-handler: called before nvme_irq executes */
static int handler_nvme_irq_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct nvme_queue *nvmeq;
    
    /* Get the nvme_queue pointer from the second argument (x86_64: RSI) */
    nvmeq = (struct nvme_queue *)regs->si;
    
    if (!is_nvme_device(nvmeq))
        return 0;  // Not a valid pointer, let it proceed
    
    total_count++;
    
    /* Randomly decide to drop this completion */
    if (get_random_u32() % 100 < drop_rate) {
        dropped_count++;
        
        /* 
         * Simulate lost TLP by:
         * 1. Not calling nvme_process_cq (skip original function)
         * 2. Return IRQ_NONE to indicate "not our interrupt"
         */
        regs->ax = IRQ_NONE;  // x86_64: RAX holds return value
        
        /* Log for debugging */
        pr_info("nvme_tlp_drop: Dropped NVMe completion (total: %lu, dropped: %lu)\n",
                total_count, dropped_count);
        
        return 1;  // Skip original function
    }
    
    return 0;  // Continue to original function
}

/* Post-handler: called after nvme_irq executes (if not skipped) */
static void handler_nvme_irq_post(struct kprobe *p, struct pt_regs *regs,
                                  unsigned long flags)
{
    /* Could add monitoring/logging here */
}

static int __init nvme_tlp_drop_init(void)
{
    int ret;
    
    if (drop_rate < 0 || drop_rate > 100) {
        pr_err("nvme_tlp_drop: drop_rate must be 0-100\n");
        return -EINVAL;
    }
    
    nvme_irq_kp.pre_handler = handler_nvme_irq_pre;
    nvme_irq_kp.post_handler = handler_nvme_irq_post;
    /* fault_handler field doesn't exist in current kernel version */
    
    ret = register_kprobe(&nvme_irq_kp);
    if (ret < 0) {
        pr_err("nvme_tlp_drop: Failed to register kprobe: %d\n", ret);
        return ret;
    }
    
    pr_info("nvme_tlp_drop: Module loaded, dropping %d%% of NVMe completions\n",
            drop_rate);
    pr_info("nvme_tlp_drop: Kprobe registered at %p\n", nvme_irq_kp.addr);
    
    return 0;
}

static void __exit nvme_tlp_drop_exit(void)
{
    unregister_kprobe(&nvme_irq_kp);
    
    pr_info("nvme_tlp_drop: Module unloaded\n");
    if (total_count) {
        /* Calculate percentage using integer arithmetic to avoid FPU */
        unsigned long percent_int = (dropped_count * 100) / total_count;
        unsigned long percent_frac = ((dropped_count * 1000) / total_count) % 10;
        pr_info("nvme_tlp_drop: Statistics - Total: %lu, Dropped: %lu (%lu.%lu%%)\n",
                total_count, dropped_count, percent_int, percent_frac);
    } else {
        pr_info("nvme_tlp_drop: Statistics - Total: %lu, Dropped: %lu (0%%)\n",
                total_count, dropped_count);
    }
}

module_init(nvme_tlp_drop_init);
module_exit(nvme_tlp_drop_exit);
