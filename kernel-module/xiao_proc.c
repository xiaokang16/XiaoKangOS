#include "xiao_syscall.h"

static DEFINE_MUTEX(xiao_proc_lock);

int xiao_get_processes(struct xiao_process_info __user *buf, u32 max_count, u32 __user *count)
{
    struct task_struct *task;
    struct xiao_process_info info;
    u32 count_val = 0;
    int ret = 0;

    if (!buf || !count)
        return -EINVAL;

    ret = xiao_check_capability(current->pid, XIAO_CAP_PROC_LIST);
    if (ret)
        return ret;

    mutex_lock(&xiao_proc_lock);

    for_each_process(task) {
        if (count_val >= max_count)
            break;

        if (task->exit_state == EXIT_DEAD)
            continue;

        memset(&info, 0, sizeof(info));
        info.pid = task->pid;
        info.ppid = task->real_parent ? task->real_parent->pid : 0;
        info.uid = from_kuid_munged(current_user_ns(), task_uid(task));
        info.gid = from_kgid_munged(current_user_ns(), task_gid(task));
        info.state = task->state;
        info.state_char = task_state_to_char(task);
        info.utime = task->utime;
        info.stime = task->stime;
        info.vmsize = task->mm ? task->mm->total_vm : 0;
        info.vmrss = task->mm ? task->mm->rss : 0;
        strncpy(info.comm, task->comm, TASK_COMM_LEN - 1);

        if (copy_to_user(&buf[count_val], &info, sizeof(info))) {
            ret = -EFAULT;
            break;
        }
        count_val++;
    }

    mutex_unlock(&xiao_proc_lock);

    if (put_user(count_val, count))
        return -EFAULT;

    return 0;
}

int xiao_kill_process(u32 pid, int sig)
{
    struct task_struct *task;
    int ret;

    ret = xiao_check_capability(current->pid, XIAO_CAP_PROC_KILL);
    if (ret)
        return ret;

    mutex_lock(&xiao_proc_lock);

    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (!task) {
        ret = -ESRCH;
        goto out;
    }

    if (task->signal->flags & SIGNAL_UNKILLABLE) {
        ret = -EPERM;
        goto out;
    }

    ret = send_sig_info(sig, SEND_SIG_PRIV, task);

out:
    mutex_unlock(&xiao_proc_lock);
    return ret;
}

int xiao_get_process_info(u32 pid, struct xiao_process_info __user *info)
{
    struct task_struct *task;
    struct xiao_process_info pinfo;
    int ret;

    if (!info)
        return -EINVAL;

    ret = xiao_check_capability(current->pid, XIAO_CAP_PROC_LIST);
    if (ret)
        return ret;

    mutex_lock(&xiao_proc_lock);

    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (!task) {
        ret = -ESRCH;
        goto out;
    }

    memset(&pinfo, 0, sizeof(pinfo));
    pinfo.pid = task->pid;
    pinfo.ppid = task->real_parent ? task->real_parent->pid : 0;
    pinfo.uid = from_kuid_munged(current_user_ns(), task_uid(task));
    pinfo.gid = from_kgid_munged(current_user_ns(), task_gid(task));
    pinfo.state = task->state;
    pinfo.state_char = task_state_to_char(task);
    pinfo.utime = task->utime;
    pinfo.stime = task->stime;
    pinfo.vmsize = task->mm ? task->mm->total_vm : 0;
    pinfo.vmrss = task->mm ? task->mm->rss : 0;
    strncpy(pinfo.comm, task->comm, TASK_COMM_LEN - 1);

    if (copy_to_user(info, &pinfo, sizeof(pinfo)))
        ret = -EFAULT;
    else
        ret = 0;

out:
    mutex_unlock(&xiao_proc_lock);
    return ret;
}

static int xiao_proc_proc_show(struct seq_file *m, void *v)
{
    struct task_struct *task;
    int count = 0;

    seq_printf(m, "xiao process bridge\n");
    seq_printf(m, "version: %s\n", XIAO_MODULE_VERSION);
    seq_printf(m, "operations: get_processes, kill_process, get_process_info\n");

    seq_printf(m, "\n--- Process List (PID | PPID | UID | State | Command) ---\n");

    for_each_process(task) {
        if (count++ > 20)
            break;
        if (task->exit_state == EXIT_DEAD)
            continue;
        seq_printf(m, "%d | %d | %d | %c | %s\n",
                   task->pid,
                   task->real_parent ? task->real_parent->pid : 0,
                   from_kuid_munged(current_user_ns(), task_uid(task)),
                   task_state_to_char(task),
                   task->comm);
    }

    return 0;
}

static int xiao_proc_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, xiao_proc_proc_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops xiao_proc_proc_fops = {
    .proc_open = xiao_proc_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#else
static const struct file_operations xiao_proc_proc_fops = {
    .owner = THIS_MODULE,
    .open = xiao_proc_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#endif

static struct proc_dir_entry *xiao_proc_proc_entry;

int __init xiao_proc_init(void)
{
    xiao_proc_proc_entry = proc_create("xiao_proc", 0444, NULL, &xiao_proc_proc_fops);
    if (!xiao_proc_proc_entry) {
        pr_err("xiao_proc: failed to create proc entry\n");
        return -ENOMEM;
    }
    pr_info("xiao_proc: process management subsystem initialized\n");
    return 0;
}

void __exit xiao_proc_exit(void)
{
    if (xiao_proc_proc_entry)
        remove_proc_entry("xiao_proc", NULL);
    pr_info("xiao_proc: process management subsystem cleanup complete\n");
}
