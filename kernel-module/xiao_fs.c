#include "xiao_syscall.h"

static DEFINE_MUTEX(xiao_fs_lock);

static int xiao_fs_validate_path(const char *path)
{
    if (!path)
        return -EINVAL;

    if (strnlen(path, XIAO_MAX_PATH) >= XIAO_MAX_PATH)
        return -ENAMETOOLONG;

    if (strstr(path, ".."))
        return -EPERM;

    if (strncmp(path, "/proc/", 6) == 0 || strncmp(path, "/sys/", 5) == 0)
        return -EPERM;

    return 0;
}

int xiao_read_file(const char __user *path, char __user *buf, size_t count, loff_t *offset)
{
    struct file *filp;
    loff_t pos;
    int ret;

    if (!path || !buf || !offset)
        return -EINVAL;

    ret = xiao_fs_validate_path(path);
    if (ret)
        return ret;

    ret = xiao_validate_path(path);
    if (ret)
        return ret;

    mutex_lock(&xiao_fs_lock);

    filp = filp_open(path, O_RDONLY, 0);
    if (IS_ERR(filp)) {
        ret = PTR_ERR(filp);
        pr_err("xiao_fs: failed to open file: %d\n", ret);
        goto out;
    }

    if (!filp->f_op->read) {
        ret = -EIO;
        goto close_file;
    }

    pos = *offset;
    ret = kernel_read(filp, buf, count, &pos);
    if (ret < 0) {
        pr_err("xiao_fs: failed to read file: %d\n", ret);
        goto close_file;
    }

    *offset = pos;

close_file:
    filp_close(filp, NULL);
out:
    mutex_unlock(&xiao_fs_lock);
    return ret;
}

int xiao_write_file(const char __user *path, const char __user *buf, size_t count, loff_t *offset)
{
    struct file *filp;
    loff_t pos;
    int ret;

    if (!path || !buf || !offset)
        return -EINVAL;

    ret = xiao_fs_validate_path(path);
    if (ret)
        return ret;

    ret = xiao_validate_path(path);
    if (ret)
        return ret;

    mutex_lock(&xiao_fs_lock);

    filp = filp_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (IS_ERR(filp)) {
        ret = PTR_ERR(filp);
        pr_err("xiao_fs: failed to open file for write: %d\n", ret);
        goto out;
    }

    if (!filp->f_op->write) {
        ret = -EIO;
        goto close_file;
    }

    pos = *offset;
    ret = kernel_write(filp, buf, count, &pos);
    if (ret < 0) {
        pr_err("xiao_fs: failed to write file: %d\n", ret);
        goto close_file;
    }

    *offset = pos;
    vfs_fsync(filp, 0);

close_file:
    filp_close(filp, NULL);
out:
    mutex_unlock(&xiao_fs_lock);
    return ret;
}

int xiao_list_dir(const char __user *path, char __user *buf, size_t count)
{
    struct file *filp;
    struct dir_context ctx;
    struct inode *inode;
    char *tmp_buf;
    int ret;
    size_t written = 0;

    if (!path || !buf)
        return -EINVAL;

    if (count < 256)
        return -EINVAL;

    ret = xiao_fs_validate_path(path);
    if (ret)
        return ret;

    ret = xiao_validate_path(path);
    if (ret)
        return ret;

    tmp_buf = kzalloc(count, GFP_KERNEL);
    if (!tmp_buf)
        return -ENOMEM;

    mutex_lock(&xiao_fs_lock);

    filp = filp_open(path, O_RDONLY | O_DIRECTORY, 0);
    if (IS_ERR(filp)) {
        ret = PTR_ERR(filp);
        pr_err("xiao_fs: failed to open directory: %d\n", ret);
        goto out;
    }

    inode = file_inode(filp);
    if (!S_ISDIR(inode->i_mode)) {
        ret = -ENOTDIR;
        goto close_file;
    }

    ret = kernel_read_dir(filp, 0, tmp_buf, count);
    if (ret < 0) {
        pr_err("xiao_fs: failed to read directory: %d\n", ret);
        goto close_file;
    }

    written = ret;
    if (written > count - 1)
        written = count - 1;

    if (copy_to_user(buf, tmp_buf, written)) {
        ret = -EFAULT;
        goto close_file;
    }

    buf[written] = '\0';
    ret = written;

close_file:
    filp_close(filp, NULL);
out:
    kfree(tmp_buf);
    mutex_unlock(&xiao_fs_lock);
    return ret;
}

static int xiao_fs_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "xiao filesystem bridge\n");
    seq_printf(m, "version: %s\n", XIAO_MODULE_VERSION);
    seq_printf(m, "operations: read, write, list_dir\n");
    return 0;
}

static int xiao_fs_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, xiao_fs_proc_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops xiao_fs_proc_fops = {
    .proc_open = xiao_fs_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#else
static const struct file_operations xiao_fs_proc_fops = {
    .owner = THIS_MODULE,
    .open = xiao_fs_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#endif

static struct proc_dir_entry *xiao_fs_proc_entry;

int __init xiao_fs_init(void)
{
    xiao_fs_proc_entry = proc_create("xiao_fs", 0444, NULL, &xiao_fs_proc_fops);
    if (!xiao_fs_proc_entry) {
        pr_err("xiao_fs: failed to create proc entry\n");
        return -ENOMEM;
    }
    pr_info("xiao_fs: filesystem subsystem initialized\n");
    return 0;
}

void __exit xiao_fs_exit(void)
{
    if (xiao_fs_proc_entry)
        remove_proc_entry("xiao_fs", NULL);
    pr_info("xiao_fs: filesystem subsystem cleanup complete\n");
}
