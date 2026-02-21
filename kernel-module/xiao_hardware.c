#include "xiao_syscall.h"

static DEFINE_MUTEX(xiao_hw_lock);

int xiao_get_hw_info(struct xiao_hw_info __user *info)
{
    struct xiao_hw_info hwinfo;
    struct file *fp;
    char line[256];
    char *p;
    int ret;

    if (!info)
        return -EINVAL;

    ret = xiao_check_capability(current->pid, XIAO_CAP_HW_INFO);
    if (ret)
        return ret;

    memset(&hwinfo, 0, sizeof(hwinfo));

    fp = filp_open("/proc/cpuinfo", O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        loff_t pos = 0;
        hwinfo.num_cpus = 0;
        while (kernel_read(fp, line, sizeof(line) - 1, &pos) > 0) {
            if (strncmp(line, "vendor_id", 9) == 0) {
                p = strchr(line, ':');
                if (p) {
                    p++;
                    while (*p == ' ') p++;
                    strncpy(hwinfo.vendor, p, sizeof(hwinfo.vendor) - 1);
                    hwinfo.vendor[strcspn(hwinfo.vendor, "\n")] = 0;
                }
            }
            if (strncmp(line, "model name", 10) == 0) {
                p = strchr(line, ':');
                if (p) {
                    p++;
                    while (*p == ' ') p++;
                    strncpy(hwinfo.model, p, sizeof(hwinfo.model) - 1);
                    hwinfo.model[strcspn(hwinfo.model, "\n")] = 0;
                }
            }
            if (strncmp(line, "processor", 9) == 0) {
                hwinfo.num_cpus++;
            }
            if (strncmp(line, "bogomips", 8) == 0) {
                p = strchr(line, ':');
                if (p) {
                    p++;
                    kstrtou64(p, 10, &hwinfo.bogomips);
                }
            }
        }
        filp_close(fp, NULL);
    }

    fp = filp_open("/proc/meminfo", O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        loff_t pos = 0;
        while (kernel_read(fp, line, sizeof(line) - 1, &pos) > 0) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                p = line + 9;
                kstrtou64(p, 10, &hwinfo.mem_total);
                hwinfo.mem_total *= 1024;
            }
        }
        filp_close(fp, NULL);
    }

    fp = filp_open("/sys/class/dmi/id/sys_vendor", O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        loff_t pos = 0;
        int len = kernel_read(fp, hwinfo.vendor, sizeof(hwinfo.vendor) - 1, &pos);
        if (len > 0) {
            hwinfo.vendor[len] = '\0';
            hwinfo.vendor[strcspn(hwinfo.vendor, "\n")] = 0;
        }
        filp_close(fp, NULL);
    }

    fp = filp_open("/sys/class/dmi/id/product_name", O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        loff_t pos = 0;
        int len = kernel_read(fp, hwinfo.model, sizeof(hwinfo.model) - 1, &pos);
        if (len > 0) {
            hwinfo.model[len] = '\0';
            hwinfo.model[strcspn(hwinfo.model, "\n")] = 0;
        }
        filp_close(fp, NULL);
    }

    fp = filp_open("/sys/class/dmi/id/bios_version", O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        loff_t pos = 0;
        int len = kernel_read(fp, hwinfo.bios_version, sizeof(hwinfo.bios_version) - 1, &pos);
        if (len > 0) {
            hwinfo.bios_version[len] = '\0';
            hwinfo.bios_version[strcspn(hwinfo.bios_version, "\n")] = 0;
        }
        filp_close(fp, NULL);
    }

    if (copy_to_user(info, &hwinfo, sizeof(hwinfo)))
        return -EFAULT;

    return 0;
}

static int xiao_hw_proc_show(struct seq_file *m, void *v)
{
    struct file *fp;
    char line[256];
    char *p;

    seq_printf(m, "xiao hardware info bridge\n");
    seq_printf(m, "version: %s\n", XIAO_MODULE_VERSION);
    seq_printf(m, "operations: get_hw_info\n");

    seq_printf(m, "\n--- CPU Information ---\n");

    fp = filp_open("/proc/cpuinfo", O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        loff_t pos = 0;
        while (kernel_read(fp, line, sizeof(line) - 1, &pos) > 0) {
            if (strncmp(line, "vendor_id", 9) == 0) {
                p = strchr(line, ':');
                if (p) {
                    p++;
                    seq_printf(m, "Vendor: %s", p);
                }
            }
            if (strncmp(line, "model name", 10) == 0) {
                p = strchr(line, ':');
                if (p) {
                    p++;
                    seq_printf(m, "Model: %s", p);
                }
            }
            if (strncmp(line, "processor", 9) == 0) {
                seq_printf(m, "Processor: detected\n");
            }
        }
        filp_close(fp, NULL);
    }

    seq_printf(m, "\n--- Memory Information ---\n");

    fp = filp_open("/proc/meminfo", O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        loff_t pos = 0;
        while (kernel_read(fp, line, sizeof(line) - 1, &pos) > 0) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                seq_printf(m, "%s", line);
            }
        }
        filp_close(fp, NULL);
    }

    seq_printf(m, "\n--- DMI Information ---\n");

    fp = filp_open("/sys/class/dmi/id/sys_vendor", O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        loff_t pos = 0;
        int len = kernel_read(fp, line, sizeof(line) - 1, &pos);
        if (len > 0) {
            line[len] = '\0';
            seq_printf(m, "System Vendor: %s", line);
        }
        filp_close(fp, NULL);
    }

    fp = filp_open("/sys/class/dmi/id/product_name", O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        loff_t pos = 0;
        int len = kernel_read(fp, line, sizeof(line) - 1, &pos);
        if (len > 0) {
            line[len] = '\0';
            seq_printf(m, "Product Name: %s", line);
        }
        filp_close(fp, NULL);
    }

    fp = filp_open("/sys/class/dmi/id/bios_version", O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        loff_t pos = 0;
        int len = kernel_read(fp, line, sizeof(line) - 1, &pos);
        if (len > 0) {
            line[len] = '\0';
            seq_printf(m, "BIOS Version: %s", line);
        }
        filp_close(fp, NULL);
    }

    return 0;
}

static int xiao_hw_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, xiao_hw_proc_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops xiao_hw_proc_fops = {
    .proc_open = xiao_hw_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#else
static const struct file_operations xiao_hw_proc_fops = {
    .owner = THIS_MODULE,
    .open = xiao_hw_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#endif

static struct proc_dir_entry *xiao_hw_proc_entry;

int __init xiao_hardware_init(void)
{
    xiao_hw_proc_entry = proc_create("xiao_hw", 0444, NULL, &xiao_hw_proc_fops);
    if (!xiao_hw_proc_entry) {
        pr_err("xiao_hardware: failed to create proc entry\n");
        return -ENOMEM;
    }
    pr_info("xiao_hardware: hardware info subsystem initialized\n");
    return 0;
}

void __exit xiao_hardware_exit(void)
{
    if (xiao_hw_proc_entry)
        remove_proc_entry("xiao_hw", NULL);
    pr_info("xiao_hardware: hardware info subsystem cleanup complete\n");
}
