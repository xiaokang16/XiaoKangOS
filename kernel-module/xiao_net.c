#include "xiao_syscall.h"

static DEFINE_MUTEX(xiao_net_lock);

int xiao_get_net_config(struct xiao_network_info __user *info, u32 max_count, u32 __user *count)
{
    struct xiao_network_info ninfo;
    struct net_device *dev;
    struct in_device *in_dev;
    u32 count_val = 0;
    int ret;

    if (!info || !count)
        return -EINVAL;

    ret = xiao_check_capability(current->pid, XIAO_CAP_NET_CONFIG);
    if (ret)
        return ret;

    mutex_lock(&xiao_net_lock);

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
            mutex_unlock(&xiao_net_lock);
            return -EFAULT;
        }
        count_val++;
    }

    rtnl_unlock();

    mutex_unlock(&xiao_net_lock);

    if (put_user(count_val, count))
        return -EFAULT;

    return 0;
}

int xiao_set_net_config(const char __user *ifname, u32 flags)
{
    struct net_device *dev;
    char ifname_buf[IFNAMSIZ];
    int ret;

    if (!ifname)
        return -EINVAL;

    ret = xiao_check_capability(current->pid, XIAO_CAP_NET_CONFIG);
    if (ret)
        return ret;

    if (strnlen_user(ifname, IFNAMSIZ) >= IFNAMSIZ)
        return -ENAMETOOLONG;

    if (copy_from_user(ifname_buf, ifname, IFNAMSIZ))
        return -EFAULT;

    ifname_buf[IFNAMSIZ - 1] = '\0';

    mutex_lock(&xiao_net_lock);

    rtnl_lock();

    dev = dev_get_by_name(&init_net, ifname_buf);
    if (!dev) {
        ret = -ENODEV;
        goto out;
    }

    if ((flags & IFF_UP) && !(dev->flags & IFF_UP)) {
        ret = dev_open(dev, NULL);
        if (ret) {
            dev_put(dev);
            goto out;
        }
    } else if (!(flags & IFF_UP) && (dev->flags & IFF_UP)) {
        dev_close(dev);
    }

    dev_put(dev);
    ret = 0;

out:
    rtnl_unlock();
    mutex_unlock(&xiao_net_lock);

    return ret;
}

static int xiao_net_proc_show(struct seq_file *m, void *v)
{
    struct net_device *dev;
    struct in_device *in_dev;

    seq_printf(m, "xiao network bridge\n");
    seq_printf(m, "version: %s\n", XIAO_MODULE_VERSION);
    seq_printf(m, "operations: get_net_config, set_net_config\n");

    seq_printf(m, "\n--- Network Interfaces ---\n");

    rtnl_lock();

    for_each_netdev(&init_net, dev) {
        seq_printf(m, "\nInterface: %s\n", dev->name);
        seq_printf(m, "  MTU: %d\n", dev->mtu);
        seq_printf(m, "  Flags: 0x%08x\n", dev->flags);
        seq_printf(m, "  MAC: %pM\n", dev->dev_addr);
        seq_printf(m, "  RX: %llu bytes, %llu packets\n",
                   dev->stats.rx_bytes, dev->stats.rx_packets);
        seq_printf(m, "  TX: %llu bytes, %llu packets\n",
                   dev->stats.tx_bytes, dev->stats.tx_packets);

        in_dev = dev->ip_ptr;
        if (in_dev && in_dev->ifa_list) {
            seq_printf(m, "  IP: %pI4\n", &in_dev->ifa_list->ifa_address);
            seq_printf(m, "  Netmask: %pI4\n", &in_dev->ifa_list->ifa_mask);
        }
    }

    rtnl_unlock();

    return 0;
}

static int xiao_net_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, xiao_net_proc_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops xiao_net_proc_fops = {
    .proc_open = xiao_net_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#else
static const struct file_operations xiao_net_proc_fops = {
    .owner = THIS_MODULE,
    .open = xiao_net_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#endif

static struct proc_dir_entry *xiao_net_proc_entry;

int __init xiao_net_init(void)
{
    xiao_net_proc_entry = proc_create("xiao_net", 0444, NULL, &xiao_net_proc_fops);
    if (!xiao_net_proc_entry) {
        pr_err("xiao_net: failed to create proc entry\n");
        return -ENOMEM;
    }
    pr_info("xiao_net: network subsystem initialized\n");
    return 0;
}

void __exit xiao_net_exit(void)
{
    if (xiao_net_proc_entry)
        remove_proc_entry("xiao_net", NULL);
    pr_info("xiao_net: network subsystem cleanup complete\n");
}
