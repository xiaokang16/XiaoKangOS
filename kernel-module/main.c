#include "xiao_syscall.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xiaojs");
MODULE_DESCRIPTION("xiaojs kernel module - system call bridge");
MODULE_VERSION(XIAO_MODULE_VERSION);

DEFINE_MUTEX(xiao_global_lock);

static struct proc_dir_entry *xiao_proc_dir;
static struct proc_dir_entry *xiao_proc_entry;
static struct sock *xiao_nl_sock;
static int xiao_major;

static int __init xiao_syscall_init(void)
{
    int ret;

    pr_info("xiao_syscall: initializing module version %s\n", XIAO_MODULE_VERSION);

    ret = xiao_security_init();
    if (ret) {
        pr_err("xiao_syscall: failed to initialize security subsystem\n");
        goto fail_security;
    }

    ret = xiao_fs_init();
    if (ret) {
        pr_err("xiao_syscall: failed to initialize filesystem subsystem\n");
        goto fail_fs;
    }

    ret = xiao_proc_init();
    if (ret) {
        pr_err("xiao_syscall: failed to initialize process subsystem\n");
        goto fail_proc;
    }

    ret = xiao_sys_init();
    if (ret) {
        pr_err("xiao_syscall: failed to initialize system info subsystem\n");
        goto fail_sys;
    }

    ret = xiao_net_init();
    if (ret) {
        pr_err("xiao_syscall: failed to initialize network subsystem\n");
        goto fail_net;
    }

    ret = xiao_hardware_init();
    if (ret) {
        pr_err("xiao_syscall: failed to initialize hardware subsystem\n");
        goto fail_hardware;
    }

    ret = xiao_ipc_init();
    if (ret) {
        pr_err("xiao_syscall: failed to initialize IPC subsystem\n");
        goto fail_ipc;
    }

    xiao_proc_dir = proc_mkdir(XIAO_PROC_DIR, NULL);
    if (!xiao_proc_dir) {
        pr_err("xiao_syscall: failed to create proc directory\n");
        ret = -ENOMEM;
        goto fail_proc_dir;
    }

    xiao_proc_entry = proc_create(XIAO_PROC_FILE, 0666, xiao_proc_dir, &xiao_proc_fops);
    if (!xiao_proc_entry) {
        pr_err("xiao_syscall: failed to create proc entry\n");
        ret = -ENOMEM;
        goto fail_proc_entry;
    }

    pr_info("xiao_syscall: module initialized successfully\n");
    pr_info("xiao_syscall: proc interface created at /proc/%s\n", XIAO_PROC_FILE);

    return 0;

fail_proc_entry:
    remove_proc_entry(XIAO_PROC_DIR, NULL);
fail_proc_dir:
    xiao_ipc_exit();
fail_ipc:
    xiao_hardware_exit();
fail_hardware:
    xiao_net_exit();
fail_net:
    xiao_sys_exit();
fail_sys:
    xiao_proc_exit();
fail_proc:
    xiao_fs_exit();
fail_fs:
    xiao_security_exit();
fail_security:
    return ret;
}

static void __exit xiao_syscall_exit(void)
{
    pr_info("xiao_syscall: cleaning up module\n");

    if (xiao_proc_entry)
        remove_proc_entry(XIAO_PROC_FILE, xiao_proc_dir);
    if (xiao_proc_dir)
        remove_proc_entry(XIAO_PROC_DIR, NULL);

    xiao_ipc_exit();
    xiao_hardware_exit();
    xiao_net_exit();
    xiao_sys_exit();
    xiao_proc_exit();
    xiao_fs_exit();
    xiao_security_exit();

    pr_info("xiao_syscall: module cleanup complete\n");
}

module_init(xiao_syscall_init);
module_exit(xiao_syscall_exit);
