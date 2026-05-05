#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "pollIo_common.h"
#include <signal.h>
#include "shared_ipc.h"
#include "shm_api.h"
#include "unified_logger.h"

extern volatile sig_atomic_t g_keep_running;

// --- 1. 初期化系 ---
int setup_udp_socket(uint16_t port)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
	{
		GLOG_ERR("[Setup] UDPソケット作成失敗: %s", safe_strerror(errno));
		return -1;
	}
	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		GLOG_ERR("[Setup] UDPポート(%d) バインド失敗: %s", port,
				 safe_strerror(errno));
		close(fd);
		return -1;
	}
	GLOG_INFO("[Setup] 監視用UDPポート(%d) バインド完了", port);
	return fd;
}

void close_udp_socket(int fd)
{
	if (fd >= 0)
		close(fd);
}

// --- 2. イベントハンドラ系 ---
void handle_udp_read(int udp_fd, int ipc_msqid, ShmHandle shm_handle)
{
	char buffer[MAX_PAYLOAD_SIZE];
	struct sockaddr_in cliaddr;
	socklen_t len = sizeof(cliaddr);

	ssize_t n = recvfrom(udp_fd, buffer, sizeof(buffer) - 1, 0,
						 (struct sockaddr *)&cliaddr, &len);
	if (n > 0)
	{
		buffer[n] = '\0';
		char src_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &cliaddr.sin_addr, src_ip, sizeof(src_ip));
		GLOG_INFO("[Collector] %s:%d からUDP受信: %s", src_ip,
				  ntohs(cliaddr.sin_port), buffer);

		int status_id = 100;

		if (shm_api_write(shm_handle, status_id, buffer))
		{
			GLOG_DEBUG("[SHM] ペイロード書き込み成功 (StatusID: %d)",
					   status_id);

			IpcNotifyMessage notify;
			notify.mtype = MSG_TYPE_SHM_NOTIFY;
			notify.shm_status_id = status_id;
			// GLOG_DEBUG("notify.mtype=%ld, notify.shm_status_id=%d sizeof
			// IpcNotifyMessage=%zu", notify.mtype, notify.shm_status_id,
			// sizeof(IpcNotifyMessage) - sizeof(long));
			// GLOG_DEBUG("ipc_msqid=%d", ipc_msqid);

			if (msgsnd(ipc_msqid, &notify,
					   sizeof(IpcNotifyMessage) - sizeof(long),
					   IPC_NOWAIT) == 0)
			{
				GLOG_DEBUG("[MQ] Routerへ通知完了");
			}
			else
			{
				GLOG_ERR("[MQ] 通知失敗: %s", safe_strerror(errno));
			}
		}
		else
		{
			GLOG_WARN("[SHM] 書き込み失敗 (Mutexロック等による)");
		}
	}
}

// --- 3. メインループ (Reactorコア) ---
void run_event_loop(int udp_fd, int ipc_msqid, ShmHandle shm_handle,
					MgmtSocketHandle mgmt_sock)
{
	struct pollfd fds[2];

	fds[0].fd = udp_fd;
	fds[0].events = POLLIN;

	int nfds = 1;
	if (mgmt_sock)
	{
		fds[1].fd = mgmt_socket_get_fd(mgmt_sock);
		fds[1].events = POLLIN;
		nfds = 2;
	}

	GLOG_INFO("[Collector] 準備完了 — イベント待機中");

	while (g_keep_running)
	{
		int ret = poll(fds, nfds, -1);

		if (ret < 0)
		{
			if (errno == EINTR)
				break;
			GLOG_ERR("[Collector] poll error: %s", safe_strerror(errno));
			break;
		}

		if (fds[0].revents & POLLIN)
		{
			handle_udp_read(udp_fd, ipc_msqid, shm_handle);
		}

		if (nfds == 2 && (fds[1].revents & POLLIN))
		{
			mgmt_socket_process_one(mgmt_sock, 0);
		}
	}
}