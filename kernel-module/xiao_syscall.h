#ifndef XIAO_SYSCALL_H
#define XIAO_SYSCALL_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/dirent.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/if_arp.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/capability.h>

#define XIAO_MODULE_NAME "xiao_syscall"
#define XIAO_MODULE_VERSION "1.0.0"
#define XIAO_PROC_DIR "xiao"
#define XIAO_PROC_FILE "xiao/bridge"
#define XIAO_NETLINK_FAMILY "xiao_bridge"
#define XIAO_NETLINK_GROUP 1
#define XIAO_MAX_PAYLOAD 4096
#define XIAO_MAX_PATH 4096
#define XIAO_MAX_PROCESSES 4096
#define XIAO_BUFFER_SIZE 8192

#define XIAO_CMD_READ_FILE     1
#define XIAO_CMD_WRITE_FILE    2
#define XIAO_CMD_LIST_DIR      3
#define XIAO_CMD_GET_PROCESSES 4
#define XIAO_CMD_KILL_PROCESS  5
#define XIAO_CMD_GET_PROC_INFO 6
#define XIAO_CMD_GET_CPU_INFO  7
#define XIAO_CMD_GET_MEM_INFO  8
#define XIAO_CMD_GET_NETWORK   9
#define XIAO_CMD_GET_HW_INFO   10
#define XIAO_CMD_CHECK_PERM    11
#define XIAO_CMD_REQUEST_CAP   12
#define XIAO_CMD_GET_NET_CONFIG 13
#define XIAO_CMD_SET_NET_CONFIG 14

#define XIAO_CAP_FILE_READ     0x0001
#define XIAO_CAP_FILE_WRITE    0x0002
#define XIAO_CAP_PROC_LIST     0x0004
#define XIAO_CAP_PROC_KILL     0x0008
#define XIAO_CAP_SYS_INFO      0x0010
#define XIAO_CAP_NET_CONFIG    0x0020
#define XIAO_CAP_HW_INFO       0x0040
#define XIAO_CAP_IPC           0x0080

struct xiao_request {
    u32 cmd;
    u32 flags;
    u32 uid;
    u32 pid;
    u32 data_len;
    char data[XIAO_MAX_PATH];
};

struct xiao_response {
    s32 error;
    u32 data_len;
    char data[XIAO_MAX_PAYLOAD];
};

struct xiao_file_data {
    char path[XIAO_MAX_PATH];
    u64 offset;
    u64 size;
    u32 flags;
};

struct xiao_process_info {
    u32 pid;
    u32 ppid;
    u32 uid;
    u32 gid;
    u64 state;
    u64 utime;
    u64 stime;
    u64 vmsize;
    u64 vmrss;
    char comm[TASK_COMM_LEN];
    char state_char;
};

struct xiao_cpu_info {
    u32 num_cores;
    u32 num_threads;
    u64 freq;
    char model[128];
    u64 user;
    u64 nice;
    u64 system;
    u64 idle;
    u64 iowait;
};

struct xiao_mem_info {
    u64 total;
    u64 free;
    u64 available;
    u64 buffers;
    u64 cached;
    u64 swap_total;
    u64 swap_free;
};

struct xiao_network_info {
    char name[32];
    char ip[16];
    char netmask[16];
    char mac[18];
    u64 rx_bytes;
    u64 tx_bytes;
    u64 rx_packets;
    u64 tx_packets;
    u32 mtu;
    u32 flags;
};

struct xiao_hw_info {
    char vendor[64];
    char model[64];
    char bios_version[64];
    u64 mem_total;
    u32 num_cpus;
    u64 bogomips;
};

struct xiao_capability {
    u32 caps;
    u32 requested_caps;
    u32 granted_caps;
    u32 pid;
    u32 uid;
};

extern struct mutex xiao_global_lock;

int xiao_fs_init(void);
void xiao_fs_exit(void);
int xiao_read_file(const char __user *path, char __user *buf, size_t count, loff_t *offset);
int xiao_write_file(const char __user *path, const char __user *buf, size_t count, loff_t *offset);
int xiao_list_dir(const char __user *path, char __user *buf, size_t count);

int xiao_proc_init(void);
void xiao_proc_exit(void);
int xiao_get_processes(struct xiao_process_info __user *buf, u32 max_count, u32 __user *count);
int xiao_kill_process(u32 pid, int sig);
int xiao_get_process_info(u32 pid, struct xiao_process_info __user *info);

int xiao_sys_init(void);
void xiao_sys_exit(void);
int xiao_get_cpu_info(struct xiao_cpu_info __user *info);
int xiao_get_mem_info(struct xiao_mem_info __user *info);
int xiao_get_network_info(struct xiao_network_info __user *info, u32 max_count, u32 __user *count);

int xiao_security_init(void);
void xiao_security_exit(void);
int xiao_check_capability(u32 pid, u32 caps);
int xiao_request_capability(u32 pid, u32 requested_caps, u32 __user *granted_caps);
int xiao_validate_path(const char *path);

int xiao_net_init(void);
void xiao_net_exit(void);
int xiao_get_net_config(struct xiao_network_info __user *info, u32 max_count, u32 __user *count);
int xiao_set_net_config(const char __user *ifname, u32 flags);

int xiao_hardware_init(void);
void xiao_hardware_exit(void);
int xiao_get_hw_info(struct xiao_hw_info __user *info);

int xiao_ipc_init(void);
void xiao_ipc_exit(void);
int xiao_handle_netlink(struct sk_buff *skb);
int xiao_send_netlink_response(struct sk_buff *skb, struct xiao_response *resp);

ssize_t xiao_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
ssize_t xiao_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
int xiao_proc_open(struct inode *inode, struct file *file);
int xiao_proc_release(struct inode *inode, struct file *file);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops xiao_proc_fops = {
    .proc_read = xiao_proc_read,
    .proc_write = xiao_proc_write,
    .proc_open = xiao_proc_open,
    .proc_release = xiao_proc_release,
};
#else
static const struct file_operations xiao_proc_fops = {
    .owner = THIS_MODULE,
    .read = xiao_proc_read,
    .write = xiao_proc_write,
    .open = xiao_proc_open,
    .release = xiao_proc_release,
};
#endif

#endif
