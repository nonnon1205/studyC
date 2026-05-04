#ifndef SHM_API_H
#define SHM_API_H

#include <stdbool.h>

// SHM を通じて転送できるペイロードの最大バイト数（呼び出し側のバッファサイズ基準）
#define MAX_PAYLOAD_SIZE 1024

// 内部構造を隠蔽するためのハンドル（Opaque Pointer）
typedef struct ShmContext* ShmHandle;

// API群
ShmHandle shm_api_init(void);                          // メモリとセマフォの確保・接続
void      shm_api_close(ShmHandle handle);             // 切断
void      shm_api_destroy(ShmHandle handle);           // 完全破棄（システム終了時）

// 読み書きインターフェース（ここで排他制御を隠蔽する）
bool      shm_api_write(ShmHandle handle, int status, const char* msg);
bool      shm_api_read(ShmHandle handle, int* out_status, char* out_msg);

#endif