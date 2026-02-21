#include "xiao_syscall.h"

static DEFINE_MUTEX(xiao_security_lock);
static unsigned long xiao_capabilities[256] = {0};

static inline int xiao_is_root(void)
{
    return from_kuid(current_user_ns(), current_uid()) == 0;
}

static inline int xiao_cap_enabled(u32 caps)
{
    return caps != 0;
}

int xiao_check_capability(u32 pid, u32 caps)
{
    struct task_struct *task;
    kuid_t uid;
    int ret = 0;

    if (!caps)
        return 0;

    mutex_lock(&xiao_security_lock);

    if (xiao_is_root()) {
        ret = 0;
        goto out;
    }

    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (!task) {
        ret = -ESRCH;
        goto out;
    }

    uid = task_uid(task);
    if (from_kuid(current_user_ns(), uid) == 0) {
        ret = 0;
        goto out;
    }

    if (xiao_cap_enabled(caps)) {
        if (test_bit(caps, xiao_capabilities)) {
            ret = 0;
        } else {
            ret = -EACCES;
        }
    }

out:
    mutex_unlock(&xiao_security_lock);
    return ret;
}

int xiao_request_capability(u32 pid, u32 requested_caps, u32 __user *granted_caps)
{
    struct task_struct *task;
    kuid_t uid;
    u32 granted = 0;
    int ret = 0;

    if (!granted_caps)
        return -EINVAL;

    mutex_lock(&xiao_security_lock);

    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (!task) {
        ret = -ESRCH;
        goto out;
    }

    uid = task_uid(task);

    if (from_kuid(current_user_ns(), uid) == 0) {
        granted = requested_caps;
    } else {
        if (requested_caps & (XIAO_CAP_SYS_INFO | XIAO_CAP_HW_INFO))
            granted |= requested_caps & (XIAO_CAP_SYS_INFO | XIAO_CAP_HW_INFO);

        if (xiao_is_root()) {
            granted = requested_caps;
        }
    }

    if (granted) {
        set_bit(granted, xiao_capabilities);
    }

    if (copy_to_user(granted_caps, &granted, sizeof(granted))) {
        ret = -EFAULT;
        goto out;
    }

    pr_info("xiao_security: process %d requested caps 0x%08x, granted 0x%08x\n",
            pid, requested_caps, granted);

out:
    mutex_unlock(&xiao_security_lock);
    return ret;
}

int xiao_validate_path(const char *path)
{
    const char *p;
    char *path_buf;
    char *dir;
    struct path path_info;
    int ret = 0;

    if (!path)
        return -EINVAL;

    if (strnlen(path, XIAO_MAX_PATH) >= XIAO_MAX_PATH)
        return -ENAMETOOLONG;

    if (strstr(path, ".."))
        return -EPERM;

    if (strncmp(path, "/proc/", 6) == 0 ||
        strncmp(path, "/sys/", 5) == 0 ||
        strncmp(path, "/dev/", 5) == 0 ||
        strncmp(path, "/root/", 6) == 0)
        return -EPERM;

    path_buf = kstrdup(path, GFP_KERNEL);
    if (!path_buf)
        return -ENOMEM;

    dir = dirname(path_buf);
    if (IS_ERR(dir)) {
        ret = PTR_ERR(dir);
        goto out;
    }

    ret = kern_path(dir, LOOKUP_FOLLOW, &path_info);
    if (ret) {
        goto out;
    }

    path_put(&path_info);

out:
    kfree(path_buf);
    return ret;
}

static int xiao_security_proc_show(struct seq_file *m, void *v)
{
    int i;

    seq_printf(m, "xiao security bridge\n");
    seq_printf(m, "version: %s\n", XIAO_MODULE_VERSION);
    seq_printf(m, "operations: check_capability, request_capability, validate_path\n");

    seq_printf(m, "\n--- Security Status ---\n");
    seq_printf(m, "Current UID: %d\n", from_kuid(current_user_ns(), current_uid()));
    seq_printf(m, "Is root: %s\n", xiao_is_root() ? "yes" : "no");

    seq_printf(m, "\n--- Active Capabilities ---\n");
    for (i = 0; i < 32; i++) {
        if (test_bit(i, xiao_capabilities)) {
            seq_printf(m, "Cap 0x%08x: enabled\n", 1 << i);
        }
    }

    return 0;
}

static int xiao_security_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, xiao_security_proc_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops xiao_security_proc_fops = {
    .proc_open = xiao_security_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#else
static const struct file_operations xiao_security_proc_fops = {
    .owner = THIS_MODULE,
    .open = xiao_security_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#endif

static struct proc_dir_entry *xiao_security_proc_entry;

int __init xiao_security_init(void)
{
    xiao_security_proc_entry = proc_create("xiao_security", 0444, NULL, &xiao_security_proc_fops);
    if (!xiao_security_proc_entry) {
        pr_err("xiao_security: failed to create proc entry\n");
        return -ENOMEM;
    }

    if (xiao_is_root()) {
        set_bit(XIAO_CAP_FILE_READ, xiao_capabilities);
        set_bit(XIAO_CAP_FILE_WRITE, xiao_capabilities);
        set_bit(XIAO_CAP_PROC_LIST, xiao_capabilities);
        set_bit(XIAO_CAP_PROC_KILL, xiao_capabilities);
        set_bit(XIAO_CAP_SYS_INFO, xiao_capabilities);
        set_bit(XIAO_CAP_NET_CONFIG, xiao_capabilities);
        set_bit(XIAO_CAP_HW_INFO, xiao_capabilities);
        set_bit(XIAO_CAP_IPC, xiao_capabilities);
    }

    pr_info("xiao_security: security subsystem initialized\n");
    pr_info("xiao_security: running as %s\n", xiao_is_root() ? "root" : "non-root");
    return 0;
}

void __exit xiao_security_exit(void)
{
    if (xiao_security_proc_entry)
        remove_proc_entry("xiao_security", NULL);
    memset(xiao_capabilities, 0, sizeof(xiao_capabilities));
    pr_info("xiao_security: security subsystem cleanup complete\n");
}
