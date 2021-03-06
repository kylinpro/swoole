#ifndef SW_SERVER_H_
#define SW_SERVER_H_

#include "swoole.h"
#include "buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SW_THREAD_NUM          2
#define SW_WRITER_NUM          2  //写线程数量
#define SW_PIPES_NUM           (SW_WORKER_NUM/SW_WRITER_NUM + 1) //每个写线程pipes数组大小
#define SW_WORKER_NUM          4 //Worker进程数量

#define SW_BACKLOG             512

#define SW_TCP_KEEPCOUNT         5
#define SW_TCP_KEEPIDLE          3600 //1小时
#define SW_TCP_KEEPINTERVAL      60

#define SW_EVENT_DATA            0 //普通事件
#define SW_EVENT_CLOSE           5 //关闭连接
#define SW_EVENT_CONNECT         6 //连接到来
#define SW_EVENT_TIMER           7 //时钟事件
#define SW_EVENT_CONTROL         8 //控制指令，特殊事件

#define SW_HOST_MAXSIZE          48
#define SW_MAX_TMP_PKG           1000
#define SW_LOG_FILENAME          128


typedef struct _swUdpFd{
	struct sockaddr addr;
	int sock;
} swUdpFd;

typedef struct _swThreadPoll
{
	pthread_t ptid; //线程ID
	swReactor reactor;
	swUdpFd *udp_addrs;
	swDataBuffer data_buffer;
	int c_udp_fd;
} swThreadPoll;

typedef struct _swListenList_node
{
	struct _swListenList_node *next, *prev;
	int type;
	int port;
	int sock;
	char host[SW_HOST_MAXSIZE];
} swListenList_node;

typedef struct _swTimerList_node
{
	struct _swTimerList_node *next, *prev;
	int interval;
	int lasttime;
} swTimerList_node;

typedef struct _swConnBuffer swConnBuffer;

struct _swConnBuffer
{
	swEventData data;
	swConnBuffer *next;
};

typedef struct _swConnection {
	uint8_t tag; //状态0表示未使用，1表示正在使用
	int fd; //文件描述符
	uint16_t from_id; //Reactor Id
	uint16_t from_fd; //从哪个ServerFD引发的
	uint8_t buffer_num; //buffer的数量
	struct sockaddr_in addr; //socket的地址
	swConnBuffer *buffer; //缓存区
} swConnection;

typedef struct swServer_s swServer;
struct swServer_s
{
	uint16_t backlog;
	uint8_t factory_mode;
	uint8_t poll_thread_num;
	uint16_t writer_num;
	uint16_t worker_num;
	uint8_t daemonize;
	uint8_t dispatch_mode; //分配模式，1平均分配，2按FD取摸固定分配，3,使用抢占式队列(IPC消息队列)分配

	int worker_uid;
	int worker_groupid;
	int max_conn;

	int connect_count; //连接计数
	int max_request;
	int timeout_sec;
	int timeout_usec;

	int sock_client_buffer_size;    //client的socket缓存区设置
	int sock_server_buffer_size;    //server的socket缓存区设置

	int data_buffer_max_num;             //数据缓存最大个数(超过此数值的连接会被当作坏连接，将清除缓存&关闭连接)
	uint8_t max_trunk_num;               //每个请求最大允许创建的trunk数
	char data_eof[SW_DATA_EOF_MAXLEN];   //数据缓存结束符
	uint8_t data_eof_len;                //数据缓存结束符长度
	char log_file[SW_LOG_FILENAME];      //日志文件

	int timer_fd;
	int signal_fd;
	int event_fd;

	int timer_interval;
	int ringbuffer_size;

	int c_pti;           //schedule
	int udp_max_tmp_pkg; //UDP临时包数量，超过数量未处理将会被丢弃

	uint8_t open_udp;          //是否有UDP监听端口
	uint8_t open_cpu_affinity; //是否设置CPU亲和性
	uint8_t open_tcp_nodelay;  //是否关闭Nagle算法
	uint8_t open_eof_check;    //检测数据EOF

	/* tcp keepalive */
	uint8_t open_tcp_keepalive; //开启keepalive
	uint16_t tcp_keepidle;      //如该连接在规定时间内没有任何数据往来,则进行探测
	uint16_t tcp_keepinterval;  //探测时发包的时间间隔
	uint16_t tcp_keepcount;     //探测尝试的次数

	void *ptr2;

	swPipe main_pipe;
	swReactor reactor;
	swFactory factory;
	swThreadPoll *poll_threads;
	swListenList_node *listen_list;
	swTimerList_node *timer_list;

	swConnection *connection_list; //连接列表
	int connection_list_capacity;  //超过此容量，会自动扩容

	swReactor *reactor_ptr; //Main Reactor
	swFactory *factory_ptr; //Factory

	void (*onStart)(swServer *serv);
	int (*onReceive)(swFactory *factory, swEventData *data);
	void (*onClose)(swServer *serv, int fd, int from_id);
	void (*onConnect)(swServer *serv, int fd, int from_id);
	void (*onMasterClose)(swServer *serv, int fd, int from_id);
	void (*onMasterConnect)(swServer *serv, int fd, int from_id);
	void (*onShutdown)(swServer *serv);
	void (*onTimer)(swServer *serv, int interval);
	void (*onWorkerStart)(swServer *serv, int worker_id); //Only process mode
	void (*onWorkerStop)(swServer *serv, int worker_id);  //Only process mode
	void (*onWorkerEvent)(swServer *serv, swEventData *data);   //Only process mode
};
int swServer_onFinish(swFactory *factory, swSendData *resp);
int swServer_onFinish2(swFactory *factory, swSendData *resp);
int swServer_onClose(swReactor *reactor, swDataHead *event);
int swServer_onAccept(swReactor *reactor, swDataHead *event);

void swServer_init(swServer *serv);
int swServer_start(swServer *serv);
int swServer_addListen(swServer *serv, int type, char *host,int port);
int swServer_create(swServer *serv);
int swServer_free(swServer *serv);
int swServer_close(swServer *factory, swDataHead *event);
int swServer_process_close(swServer *serv, swDataHead *event);
int swServer_shutdown(swServer *serv);
int swServer_addTimer(swServer *serv, int interval);
int swServer_reload(swServer *serv);

int swServer_new_connection(swServer *serv, swEvent *ev);
#define SW_SERVER_MAX_FD_INDEX        0
#define SW_SERVER_MIN_FD_INDEX        1

//使用connection_list[0]表示最大的FD
#define swServer_set_maxfd(serv,maxfd) (serv->connection_list[SW_SERVER_MAX_FD_INDEX].fd=maxfd)
#define swServer_get_maxfd(serv) (serv->connection_list[SW_SERVER_MAX_FD_INDEX].fd)
#define swServer_get_connection(serv,fd) ((fd>serv->max_conn|| fd<= swServer_get_minfd(serv))?NULL:&serv->connection_list[fd])
//使用connection_list[1]表示最小的FD
#define swServer_set_minfd(serv,maxfd) (serv->connection_list[SW_SERVER_MIN_FD_INDEX].fd=maxfd)
#define swServer_get_minfd(serv) (serv->connection_list[SW_SERVER_MIN_FD_INDEX].fd)
swConnBuffer* swConnection_get_buffer(swConnection *conn);
void swConnection_clear_buffer(swConnection *conn);

#ifdef __cplusplus
}
#endif

#endif /* SW_SERVER_H_ */
