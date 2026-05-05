#ifndef POSIX_SHM_API_H
#define POSIX_SHM_API_H

#include <stdbool.h>

// 不透明ポインタとして扱うためのハンドル
typedef struct ShmContext *ShmHandle;

ShmHandle shm_api_init(void);
bool shm_api_write(ShmHandle handle, int status, const char *msg);
bool shm_api_read(ShmHandle handle, int *out_status, char *out_msg);
void shm_api_close(ShmHandle handle);
void shm_api_destroy(ShmHandle handle);

#endif // POSIX_SHM_API_H