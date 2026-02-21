#include "xiao_syscall.h"

static DEFINE_MUTEX(xiao_ipc_lock);
static struct sock *xiao_nl_sock;
static u32 xiao_nl_pid;
static DECLARE_WAIT_QUEUE_HEAD(xiao_ipc_wq);

#define XIAO_NETLINK_FAMILY_NAME "xiao_bridge"
#define XIAO_NETLINK_MULTICAST_GROUP 1

static void xiao_netlink_rcv(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    struct xiao_request *req;
    struct xiao_response resp;
    int ret;

    nlh = nlmsg_hdr(skb);
    req = nlmsg_data(nlh);

    memset(&resp, 0, sizeof(resp));

    switch (req->cmd) {
    case XIAO_CMD_READ_FILE:
        ret = xiao_read_file(req->data, resp.data, XIAO_MAX_PAYLOAD, (loff_t *)&req->offset);
        resp.error = ret;
        resp.data_len = ret > 0 ? ret : 0;
        break;

    case XIAO_CMD_WRITE_FILE:
        ret = xiao_write_file(req->data, req->data + strlen(req->data) + 1,
                              req->data_len, (loff_t *)&req->offset);
        resp.error = ret;
        break;

    case XIAO_CMD_LIST_DIR:
        ret = xiao_list_dir(req->data, resp.data, XIAO_MAX_PAYLOAD);
        resp.error = ret;
        resp.data_len = ret > 0 ? ret : 0;
        break;

    case XIAO_CMD_GET_PROCESSES:
        ret = xiao_get_processes((struct xiao_process_info *)resp.data,
                                  XIAO_MAX_PROCESSES, &resp.data_len);
        resp.error = ret;
        resp.data_len = ret == 0 ? resp.data_len * sizeof(struct xiao_process_info) : 0;
        break;

    case XIAO_CMD_KILL_PROCESS:
        ret = xiao_kill_process(req->pid, req->flags);
        resp.error = ret;
        break;

    case XIAO_CMD_GET_PROC_INFO:
        ret = xiao_get_process_info(req->pid, (struct xiao_process_info *)resp.data);
        resp.error = ret;
        resp.data_len = ret == 0 ? sizeof(struct xiao_process_info) : 0;
        break;

    case XIAO_CMD_GET_CPU_INFO:
        ret = xiao_get_cpu_info((struct xiao_cpu_info *)resp.data);
        resp.error = ret;
        resp.data_len = ret == 0 ? sizeof(struct xiao_cpu_info) : 0;
        break;

    case XIAO_CMD_GET_MEM_INFO:
        ret = xiao_get_mem_info((struct xiao_mem_info *)resp.data);
        resp.error = ret;
        resp.data_len = ret == 0 ? sizeof(struct xiao_mem_info) : 0;
        break;

    case XIAO_CMD_GET_NETWORK:
        ret = xiao_get_network_info((struct xiao_network_info *)resp.data,
                                    32, &resp.data_len);
        resp.error = ret;
        resp.data_len = ret == 0 ? resp.data_len * sizeof(struct xiao_network_info) : 0;
        break;

    case XIAO_CMD_GET_HW_INFO:
        ret = xiao_get_hw_info((struct xiao_hw_info *)resp.data);
        resp.error = ret;
        resp.data_len = ret == 0 ? sizeof(struct xiao_hw_info) : 0;
        break;

    case XIAO_CMD_CHECK_PERM:
        ret = xiao_check_capability(req->pid, req->caps);
        resp.error = ret;
        break;

    case XIAO_CMD_REQUEST_CAP:
        ret = xiao_request_capability(req->pid, req->requested_caps,
                                       (u32 *)resp.data);
        resp.error = ret;
        resp.data_len = ret == 0 ? sizeof(u32) : 0;
        break;

    case XIAO_CMD_GET_NET_CONFIG:
        ret = xiao_get_net_config((struct xiao_network_info *)resp.data,
                                   32, &resp.data_len);
        resp.error = ret;
        resp.data_len = ret == 0 ? resp.data_len * sizeof(struct xiao_network_info) : 0;
        break;

    case XIAO_CMD_SET_NET_CONFIG:
        ret = xiao_set_net_config(req->data, req->flags);
        resp.error = ret;
        break;

    default:
        resp.error = -ENOTSUPP;
        pr_warn("xiao_ipc: unknown command: %d\n", req->cmd);
        break;
    }

    xiao_send_netlink_response(skb, &resp);
}

int xiao_send_netlink_response(struct sk_buff *skb, struct xiao_response *resp)
{
    struct nlmsghdr *nlh;
    struct sk_buff *nskb;
    int ret;

    if (!skb || !resp)
        return -EINVAL;

    nskb = nlmsg_new(sizeof(struct xiao_response), GFP_ATOMIC);
    if (!nskb)
        return -ENOMEM;

    nlh = nlmsg_put(nskb, 0, 0, NLMSG_DONE, sizeof(struct xiao_response), 0);
    if (!nlh) {
        kfree_skb(nskb);
        return -ENOMEM;
    }

    memcpy(nlmsg_data(nlh), resp, sizeof(struct xiao_response));

    ret = nlmsg_unicast(xiao_nl_sock, nskb, xiao_nl_pid);

    return ret;
}

int xiao_proc_open(struct inode *inode, struct file *file)
{
    mutex_lock(&xiao_ipc_lock);
    return 0;
}

int xiao_proc_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&xiao_ipc_lock);
    return 0;
}

ssize_t xiao_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    char *kbuf;
    int ret;

    if (count < sizeof(struct xiao_response))
        return -EINVAL;

    kbuf = kzalloc(XIAO_MAX_PAYLOAD + sizeof(struct xiao_response), GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    mutex_lock(&xiao_ipc_lock);

    ret = xiao_get_cpu_info((struct xiao_cpu_info *)kbuf);
    if (ret == 0) {
        ret = xiao_get_mem_info((struct xiao_mem_info *)(kbuf + sizeof(struct xiao_cpu_info)));
    }

    mutex_unlock(&xiao_ipc_lock);

    if (ret == 0) {
        if (copy_to_user(buf, kbuf, count))
            ret = -EFAULT;
        else
            ret = count;
    }

    kfree(kbuf);
    return ret;
}

ssize_t xiao_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct xiao_request *req;
    struct xiao_response *resp;
    char *kbuf;
    int ret;

    if (count < sizeof(struct xiao_request))
        return -EINVAL;

    if (count > XIAO_MAX_PAYLOAD + sizeof(struct xiao_request))
        return -EINVAL;

    kbuf = kzalloc(count, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }

    req = (struct xiao_request *)kbuf;
    resp = kzalloc(sizeof(struct xiao_response), GFP_KERNEL);
    if (!resp) {
        kfree(kbuf);
        return -ENOMEM;
    }

    mutex_lock(&xiao_ipc_lock);

    switch (req->cmd) {
    case XIAO_CMD_READ_FILE:
        ret = xiao_read_file(req->data, (char *)(resp->data),
                              XIAO_MAX_PAYLOAD, (loff_t *)&req->offset);
        resp->error = ret;
        resp->data_len = ret > 0 ? ret : 0;
        break;

    case XIAO_CMD_WRITE_FILE:
        ret = xiao_write_file(req->data, req->data + XIAO_MAX_PATH,
                              count - XIAO_MAX_PATH, (loff_t *)&req->offset);
        resp->error = ret;
        break;

    case XIAO_CMD_LIST_DIR:
        ret = xiao_list_dir(req->data, resp->data, XIAO_MAX_PAYLOAD);
        resp->error = ret;
        resp->data_len = ret > 0 ? ret : 0;
        break;

    case XIAO_CMD_GET_PROCESSES:
        ret = xiao_get_processes((struct xiao_process_info *)resp->data,
                                  XIAO_MAX_PROCESSES, &resp->data_len);
        resp->error = ret;
        resp->data_len = ret == 0 ? resp->data_len * sizeof(struct xiao_process_info) : 0;
        break;

    case XIAO_CMD_KILL_PROCESS:
        ret = xiao_kill_process(req->pid, req->flags);
        resp->error = ret;
        break;

    case XIAO_CMD_GET_PROC_INFO:
        ret = xiao_get_process_info(req->pid, (struct xiao_process_info *)resp->data);
        resp->error = ret;
        resp->data_len = ret == 0 ? sizeof(struct xiao_process_info) : 0;
        break;

    case XIAO_CMD_GET_CPU_INFO:
        ret = xiao_get_cpu_info((struct xiao_cpu_info *)resp->data);
        resp->error = ret;
        resp->data_len = ret == 0 ? sizeof(struct xiao_cpu_info) : 0;
        break;

    case XIAO_CMD_GET_MEM_INFO:
        ret = xiao_get_mem_info((struct xiao_mem_info *)resp->data);
        resp->error = ret;
        resp->data_len = ret == 0 ? sizeof(struct xiao_mem_info) : 0;
        break;

    case XIAO_CMD_GET_NETWORK:
        ret = xiao_get_network_info((struct xiao_network_info *)resp->data,
                                    32, &resp->data_len);
        resp->error = ret;
        resp->data_len = ret == 0 ? resp->data_len * sizeof(struct xiao_network_info) : 0;
        break;

    case XIAO_CMD_GET_HW_INFO:
        ret = xiao_get_hw_info((struct xiao_hw_info *)resp->data);
        resp->error = ret;
        resp->data_len = ret == 0 ? sizeof(struct xiao_hw_info) : 0;
        break;

    case XIAO_CMD_CHECK_PERM:
        ret = xiao_check_capability(req->pid, req->caps);
        resp->error = ret;
        break;

    case XIAO_CMD_REQUEST_CAP:
        ret = xiao_request_capability(req->pid, req->requested_caps,
                                       (u32 *)resp->data);
        resp->error = ret;
        resp->data_len = ret == 0 ? sizeof(u32) : 0;
        break;

    case XIAO_CMD_GET_NET_CONFIG:
        ret = xiao_get_net_config((struct xiao_network_info *)resp->data,
                                   32, &resp->data_len);
        resp->error = ret;
        resp->data_len = ret == 0 ? resp->data_len * sizeof(struct xiao_network_info) : 0;
        break;

    case XIAO_CMD_SET_NET_CONFIG:
        ret = xiao_set_net_config(req->data, req->flags);
        resp->error = ret;
        break;

    default:
        resp->error = -ENOTSUPP;
        break;
    }

    mutex_unlock(&xiao_ipc_lock);

    ret = copy_to_user(buf, resp, sizeof(struct xiao_response));
    if (ret)
        ret = -EFAULT;
    else
        ret = sizeof(struct xiao_response);

    kfree(resp);
    kfree(kbuf);

    return ret;
}

static int xiao_ipc_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "xiao IPC bridge\n");
    seq_printf(m, "version: %s\n", XIAO_MODULE_VERSION);
    seq_printf(m, "netlink: %s\n", xiao_nl_sock ? "active" : "inactive");
    seq_printf(m, "operations: read, write (synchronous IPC)\n");
    seq_printf(m, "netlink: async IPC (family: %s)\n", XIAO_NETLINK_FAMILY_NAME);
    return 0;
}

static int xiao_ipc_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, xiao_ipc_proc_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops xiao_ipc_proc_fops = {
    .proc_open = xiao_ipc_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#else
static const struct file_operations xiao_ipc_proc_fops = {
    .owner = THIS_MODULE,
    .open = xiao_ipc_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#endif

static struct proc_dir_entry *xiao_ipc_proc_entry;

int __init xiao_ipc_init(void)
{
    struct netlink_kernel_cfg cfg = {
        .input = xiao_netlink_rcv,
    };

    xiao_nl_sock = netlink_kernel_create(&init_net, NETLINK_USERSOCK, &cfg);
    if (!xiao_nl_sock) {
        pr_err("xiao_ipc: failed to create netlink socket\n");
    } else {
        pr_info("xiao_ipc: netlink socket created successfully\n");
    }

    xiao_ipc_proc_entry = proc_create("xiao_ipc", 0444, NULL, &xiao_ipc_proc_fops);
    if (!xiao_ipc_proc_entry) {
        pr_err("xiao_ipc: failed to create proc entry\n");
        if (xiao_nl_sock)
            netlink_kernel_release(xiao_nl_sock);
        return -ENOMEM;
    }

    pr_info("xiao_ipc: IPC subsystem initialized\n");
    return 0;
}

void __exit xiao_ipc_exit(void)
{
    if (xiao_ipc_proc_entry)
        remove_proc_entry("xiao_ipc", NULL);

    if (xiao_nl_sock) {
        netlink_kernel_release(xiao_nl_sock);
        xiao_nl_sock = NULL;
    }

    pr_info("xiao_ipc: IPC subsystem cleanup complete\n");
}
