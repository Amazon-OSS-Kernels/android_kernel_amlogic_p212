#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#define REG_ISA_TIMERE (0x2662*4+0xc1100000)

static int aml_xgpt_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long off = REG_ISA_TIMERE;
	unsigned vm_size = vma->vm_end - vma->vm_start;

	if (vm_size == 0 || vm_size > 4096)
		return -EAGAIN;

	vma->vm_page_prot = phys_mem_access_prot(file, vma->vm_pgoff,
			vm_size,
			vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
		vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		pr_err("set_cached: failed remap_pfn_range\n");
		return -EAGAIN;
	}
	return 0;
}

static const struct file_operations aml_xgpt_fops = {
	.owner = THIS_MODULE,
	.mmap = aml_xgpt_mmap,
};

static struct miscdevice aml_xgpt_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "aml_xgpt",
	.fops = &aml_xgpt_fops,
};

static int __init aml_timer_mod_init(void)
{
	pr_notice("aml_timer_mod_init\n");

	/* register miscdev node for userspace accessing */
	if (misc_register(&aml_xgpt_miscdev))
	    pr_err("failed to register misc device: %s\n", "mt_xgpt");

	return 0;
}

static void __exit aml_timer_mod_exit(void)
{
	pr_info("%s : %d - Err.\n", __func__, __LINE__);
}

module_init(aml_timer_mod_init);
module_exit(aml_timer_mod_exit);

