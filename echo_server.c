#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched/signal.h>
#include <linux/tcp.h>

#include "fastecho.h"

#define BUF_SIZE 4096

static struct workqueue_struct *my_wq;

struct work_struct_data {
    struct work_struct my_work;
    struct socket *client;
};

static int handle_request(struct socket *sock, unsigned char *buf, size_t size)
{
    struct msghdr msg;
    struct kvec vec;
    int length;

    /* kvec setting */
    vec.iov_len = size;
    vec.iov_base = buf;

    /* msghdr setting */
    msg.msg_name = 0;
    msg.msg_namelen = 0;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

    /* get msg */
    length = kernel_recvmsg(sock, &msg, &vec, size, size, msg.msg_flags);
    if (length == 0)
        return length;

    printk("len,msg: %s, %d\n", buf, length);
    length = kernel_sendmsg(sock, &msg, &vec, 1, length);
    return length;
}

static void work_handler(struct work_struct *work)
{
    unsigned char *buf;
    struct work_struct_data *wsdata = (struct work_struct_data *) work;
    int res;

    buf = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!buf) {
        printk("server: recvbuf kmalloc error!\n");
        return;
    }
    memset(buf, 0, BUF_SIZE);

    while (1) {
        res = handle_request(wsdata->client, buf, BUF_SIZE - 1);
        if (res <= 0) {
            if (res) {
                printk(KERN_ERR MODULE_NAME ": get request error = %d\n", res);
            }
            break;
        }
    }

    kfree(buf);
    kernel_sock_shutdown(wsdata->client, SHUT_RDWR);
    sock_release(wsdata->client);
    printk("exiting work_handler...\n");
}

int echo_server_daemon(void *arg)
{
    struct echo_server_param *param = arg;
    struct socket *sock;
    int error;

    allow_signal(SIGKILL);
    allow_signal(SIGTERM);


#ifdef HIGH_PRI
    struct workqueue_attrs *attr;
    attr = alloc_workqueue_attrs(__GFP_HIGH);
    apply_workqueue_attrs(my_wq, attr);
#else
    my_wq = create_workqueue("my_queue");
#endif

    // my_wq = alloc_workqueue("my_queue",WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);

    while (1) {
        struct work_struct_data *wsdata;
        /* using blocking I/O */
        error = kernel_accept(param->listen_sock, &sock, 0);
        printk("accepted socket\n");
        if (error < 0) {
            if (signal_pending(current))
                break;
            printk(KERN_ERR MODULE_NAME ": socket accept error = %d\n", error);
            continue;
        }

        if (my_wq) {
            /*set workqueue data*/
            wsdata = (struct work_struct_data *) kmalloc(
                sizeof(struct work_struct_data), GFP_KERNEL);
            wsdata->client = sock;

            /*put task into workqueue*/
            if (wsdata) {
                printk("starting work\n");
                INIT_WORK(&wsdata->my_work, work_handler);
                error = queue_work(my_wq, &wsdata->my_work);
            }
        }
        printk("server: accept ok, Connection Established.\n");
    }

    return 0;
}
