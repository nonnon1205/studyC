#define _POSIX_C_SOURCE 200809L

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // EOWNERDEAD などのマクロを有効にするため
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <syslog.h>
#include "shm_common.h"
#include "shm_api.h"
#define MODULE_NAME "PosixShmApi"
#include "debug_log.h"

struct ShmContext
{
	int shm_fd;
	SharedData *shm_ptr;
	bool is_creator;
};

// --- ヘルパー：安全なロック処理（ロバスト回復付き） ---
static bool safe_lock(pthread_mutex_t *mtx)
{
	int ret = pthread_mutex_lock(mtx);

	if (ret == 0)
		return true;

#ifdef PTHREAD_MUTEX_ROBUST
	if (ret == EOWNERDEAD)
	{
		syslog(LOG_WARNING,
			   "[POSIX_SHM] ロック保持者の死亡を検知。状態を復旧します。");
		pthread_mutex_consistent(mtx);
		return true;
	}
#endif

	syslog(LOG_ERR, "[POSIX_SHM] Mutexロック失敗: %s", safe_strerror(ret));
	return false;
}

// --- API: 初期化と接続 ---
ShmHandle shm_api_init(void)
{
	struct ShmContext *ctx = malloc(sizeof(struct ShmContext));
	if (!ctx)
		return NULL;

	bool is_creator = false;

	// O_EXCL を使って「自分が最初の作成者か」を判定する
	ctx->shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
	if (ctx->shm_fd >= 0)
	{
		is_creator = true;
		// 作成者はサイズを設定する
		if (ftruncate(ctx->shm_fd, sizeof(SharedData)) == -1)
		{
			syslog(LOG_ERR, "[POSIX_SHM] ftruncate失敗: %m");
			close(ctx->shm_fd);
			shm_unlink(SHM_NAME);
			free(ctx);
			return NULL;
		}
		DBG("POSIX共有メモリ新規作成: shm_fd=%d", ctx->shm_fd);
		syslog(LOG_INFO, "[POSIX_SHM] 共有メモリを新規作成しました。");
	}
	else if (errno == EEXIST)
	{
		// 既に存在する場合は単に開く
		ctx->shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
	}

	if (ctx->shm_fd < 0)
	{
		syslog(LOG_ERR, "[POSIX_SHM] shm_open失敗: %m");
		free(ctx);
		return NULL;
	}

	// 作成者以外は、作成者がftruncateを完了してサイズが確保されるまで待機する
	if (!is_creator)
	{
		struct stat st;
		while (fstat(ctx->shm_fd, &st) == 0 &&
			   st.st_size < (off_t)sizeof(SharedData))
		{
			usleep(1000); // 1ms待機
		}
	}

	// メモリマッピング
	ctx->shm_ptr =
		(SharedData *)mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE,
						   MAP_SHARED, ctx->shm_fd, 0);
	if (ctx->shm_ptr == MAP_FAILED)
	{
		syslog(LOG_ERR, "[POSIX_SHM] mmap失敗: %m");
		close(ctx->shm_fd);
		free(ctx);
		return NULL;
	}

	// 作成者のみが Mutex の初期化を行う
	if (is_creator)
	{
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

#ifdef PTHREAD_MUTEX_ROBUST
		pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
#endif

		pthread_mutex_init(&ctx->shm_ptr->mtx, &attr);
		pthread_mutexattr_destroy(&attr);
	}

	ctx->is_creator = is_creator;
	return ctx;
}

// --- API: 書き込み ---
bool shm_api_write(ShmHandle handle, int status, const char *msg)
{
	assert(msg != NULL);
	if (!handle || !handle->shm_ptr || !msg)
		return false;
	if (!safe_lock(&handle->shm_ptr->mtx))
		return false;

	handle->shm_ptr->status_code = status;
	snprintf(handle->shm_ptr->message, sizeof(handle->shm_ptr->message), "%s",
			 msg);
	DBG("SHM書込み: status=%d, msg=\"%s\"", status, msg);

	pthread_mutex_unlock(&handle->shm_ptr->mtx);
	return true;
}

// --- API: 読み取り ---
bool shm_api_read(ShmHandle handle, int *out_status, char *out_msg)
{
	if (!handle || !handle->shm_ptr)
		return false;
	if (!safe_lock(&handle->shm_ptr->mtx))
		return false;

	if (out_status)
		*out_status = handle->shm_ptr->status_code;
	if (out_msg)
	{
		size_t max_len = sizeof(handle->shm_ptr->message);
		if (max_len < 1)
		{
			pthread_mutex_unlock(&handle->shm_ptr->mtx);
			return false;
		}
		snprintf(out_msg, max_len, "%.*s", (int)(max_len - 1),
				 handle->shm_ptr->message);
	}
	DBG("SHM読出し: status=%d, msg=\"%s\"", out_status ? *out_status : -1,
		out_msg ? out_msg : "(null)");

	pthread_mutex_unlock(&handle->shm_ptr->mtx);
	return true;
}

// --- API: 切断（プロセス終了時） ---
void shm_api_close(ShmHandle handle)
{
	if (handle)
	{
		munmap(handle->shm_ptr, sizeof(SharedData));
		close(handle->shm_fd);
		free(handle);
	}
}

// --- API: 完全破棄（システム全体の終了時） ---
void shm_api_destroy(ShmHandle handle)
{
	if (!handle)
		return;
	bool creator = handle->is_creator;
	shm_api_close(handle);
	if (creator)
	{
		shm_unlink(SHM_NAME);
		syslog(LOG_INFO, "[POSIX_SHM] 共有メモリをシステムから破棄しました。");
	}
}