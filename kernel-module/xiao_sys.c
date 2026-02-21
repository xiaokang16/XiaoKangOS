#include "xiao_syscall.h"

static DEFINE_MUTEX(xiao_sys_lock);

int xiao_get_cpu_info(struct xiao_cpu_info __user *info)
{
    struct xiao_cpu_info cinfo;
    struct file *fp;
    char line[256];
    char *p;
    int ret;

    if (!info)
        return -EINVAL;

    ret = xiao_check_capability(current->pid, XIAO_CAP_SYS_INFO);
    if (ret)
        return ret;

    memset(&cinfo, 0, sizeof(cinfo));

    cinfo.num_cores = num_online_cpus();
    cinfo.num_threads = num_possible_cpus();

    fp = filp_open("/proc/cpuinfo", O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        loff_t pos = 0;
        while (kernel_read(fp, line, sizeof(line) - 1, &pos) > 0) {
            if (strncmp(line, "model name", 10) == 0) {
                p = strchr(line, ':');
                if (p) {
                    p++;
                    while (*p == ' ') p++;
                    strncpy(cinfo.model, p, sizeof(cinfo.model) - 1);
                    cinfo.model[strcspn(cinfo.model, "\n")] = 0;
                }
            }
            if (strncmp(line, "cpu MHz", 7) == 0) {
                p = strchr(line, ':');
                if (p) {
                    p++;
                    kstrtou64(p, 10, &cinfo.freq);
                    cinfo.freq *= 1000000;
                }
            }
        }
        filp_close(fp, NULL);
    }

    fp = filp_open("/proc/stat", O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        loff_t pos = 0;
        while (kernel_read(fp, line, sizeof(line) - 1, &pos) > 0) {
            if (strncmp(line, "cpu ", 4) == 0) {
                sscanf(line + 4, "%llu %llu %llu %llu %llu",
                       &cinfo.user, &cinfo.nice, &cinfo.system,
                       &cinfo.idle, &cinfo.iowait);
                break;
            }
        }
        filp_close(fp, NULL);
    }

    if (copy_to_user(info, &cinfo, sizeof(cinfo)))
        return -EFAULT;

    return 0;
}

int xiao_get_mem_info(struct xiao_mem_info __user *info)
{
    struct xiao_mem_info minfo;
    struct file *fp;
    char line[256];
    char *p;
    int ret;

    if (!info)
        return -EINVAL;

    ret = xiao_check_capability(current->pid, XIAO_CAP_SYS_INFO);
    if (ret)
        return ret;

    memset(&minfo, 0, sizeof(minfo));

    fp = filp_open("/proc/meminfo", O_RDONLY, 0);
    if (IS_ERR(fp))
        return PTR_ERR(fp);

    while (kernel_read(fp, line, sizeof(line) - 1, &fp->f_pos) > 0) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            p = line + 9;
            kstrtou64(p, 10, &minfo.total);
            minfo.total *= 1024;
        } else if (strncmp(line, "MemFree:", 8) == 0) {
            p = line + 8;
            kstrtou64(p, 10, &minfo.free);
            minfo.free *= 1024;
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            p = line + 13;
            kstrtou64(p, 10, &minfo.available);
            minfo.available *= 1024;
        } else if (strncmp(line, "Buffers:", 8) == 0) {
            p = line + 8;
            kstrtou64(p, 10, &minfo.buffers);
            minfo.buffers *= 1024;
        } else if (strncmp(line, "Cached:", 7) == 0) {
            p = line + 7;
            kstrtou64(p, 10, &minfo.cached);
            minfo.cached *= 1024;
        } else if (strncmp(line, "SwapTotal:", 10) == 0) {
            p = line + 10;
            kstrtou64(p, 10, &minfo.swap_total);
            minfo.swap_total *= 1024;
        } else if (strncmp(line, "SwapFree:", 9) == 0) {
            p = line + 9;
            kstrtou64(p, 10, &minfo.swap_free);
            minfo.swap_free *= 1024;
        }
    }

    filp_close(fp, NULL);

    if (copy_to_user(info, &minfo, sizeof(minfo)))
        return -EFAULT;

    return 0;
}

int xiao_get_network_info(struct xiao_network_info __user *info, u32 max_count, u32 __user *count)
{
    struct xiao_network_info ninfo;
    struct net_device *dev;
    struct in_device *in_dev;
    struct ifinfomsg *ifi;
    u32 count_val = 0;
    int ret;

    if (!info || !count)
        return -EINVAL;

    ret = xiao_check_capability(current->pid, XIAO_CAP_SYS_INFO);
    if (ret)
        return ret;

    mutex_lock(&xiao_sys_lock);

    rtnl_lock();

    for_each_netdev(&init_net, dev) {
        if (count_val >= max_count)
            break;

        if (dev->flags & IFF_LOOPBACK)
            continue;

        memset(&ninfo, 0, sizeof(ninfo));
        strncpy(ninfo.name, dev->name, sizeof(ninfo.name) - 1);

        in_dev = dev->ip_ptr;
        if (in_dev && in_dev->ifa_list) {
            snprintf(ninfo.ip, sizeof(ninfo.ip), "%pI4", &in_dev->ifa_list->ifa_address);
            snprintf(ninfo.netmask, sizeof(ninfo.netmask), "%pI4",
                     &in_dev->ifa_list->ifa_mask);
        }

        if (dev->addr_len <= sizeof(ninfo.mac)) {
            snprintf(ninfo.mac, sizeof(ninfo.mac), "%pM", dev->dev_addr);
        }

        ninfo.rx_bytes = dev->stats.rx_bytes;
        ninfo.tx_bytes = dev->stats.tx_bytes;
        ninfo.rx_packets = dev->stats.rx_packets;
        ninfo.tx_packets = dev->stats.tx_packets;
        ninfo.mtu = dev->mtu;
        ninfo.flags = dev->flags;

        if (copy_to_user(&info[count_val], &ninfo, sizeof(ninfo))) {
            rtnl_unlock();
            mutex_unlock(&xiao_sys_lock);
            return -EFAULT;
        }
        count_val++;
    }

    rtnl_unlock();

    mutex_unlock(&xiao_sys_lock);

    if (put_user(count_val, count))
        return -EFAULT;

    return 0;
}

static int xiao_sys_proc_show(struct seq_file *m, void *v)
{
    struct xiao_cpu_info cinfo;
    struct xiao_mem_info minfo;
    struct file *fp;
    char line[256];
    char *p;

    seq_printf(m, "xiao system info bridge\n");
    seq_printf(m, "version: %s\n", XIAO_MODULE_VERSION);

    seq_printf(m, "\n--- CPU Information ---\n");

    cinfo.num_cores = num_online_cpus();
    cinfo.num_threads = num_possible_cpus();
    seq_printf(m, "CPU cores (online): %d\n", cinfo.num_cores);
    seq_printf(m, "CPU threads (possible): %d\n", cinfo.num_threads);

    fp = filp_open("/proc/cpuinfo", O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        loff_t pos = 0;
        while (kernel_read(fp, line, sizeof(line) - 1, &pos) > 0) {
            if (strncmp(line, "model name", 10) == 0) {
                p = strchr(line, ':');
                if (p) {
                    p++;
                    while (*p == ' ') p++;
                    seq_printf(m, "Model: %s", p);
                }
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
            } else if (strncmp(line, "MemFree:", 8) == 0) {
                seq_printf(m, "%s", line);
            } else if (strncmp(line, "MemAvailable:", 13) == 0) {
                seq_printf(m, "%s", line);
            }
        }
        filp_close(fp, NULL);
    }

    return 0;
}

static int xiao_sys_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, xiao_sys_proc_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops xiao_sys_proc_fops = {
    .proc_open = xiao_sys_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#else
static const struct file_operations xiao_sys_proc_fops = {
    .owner = THIS_MODULE,
    .open = xiao_sys_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#endif

static struct proc_dir_entry *xiao_sys_proc_entry;

int __init xiao_sys_init(void)
{
    xiao_sys_proc_entry = proc_create("xiao_sys", 0444, NULL, &xiao_sys_proc_fops);
    if (!xiao_sys_proc_entry) {
        pr_err("xiao_sys: failed to create proc entry\n");
        return -ENOMEM;
    }
    pr_info("xiao_sys: system info subsystem initialized\n");
    return 0;
}

void __exit xiao_sys_exit(void)
{
    if (xiao_sys_proc_entry)
        remove_proc_entry("xiao_sys", NULL);
    pr_info("xiao_sys: system info subsystem cleanup complete\n");
}
