# SHM ラッパー設計方針

## 目的

- POSIX SHM (`shm_open` / `mmap`) と System V SHM (`shmget` / `shmat`) の両実装を学習する
- `shm_api.h` を安定インターフェースとして、実装を切り替え可能にする（Bridge パターン）

## 実装方式

`SHM/`（SystemV）と `PosixSHM/`（POSIX）がそれぞれ同名の `shm_api_*` 関数を実装している。
**Makefile のビルド変数でどちらをリンクするかを制御する**だけで切り替えが完成する。
中間ラッパー層（`shm_impl_*`）は不要。

```
SHM/src/shm_api.c      shm_api_init() { shmget...   }  ← SystemV
PosixSHM/src/shm_api.c shm_api_init() { shm_open... }  ← POSIX
    ↑ 同名関数、Makefile の SHM_IMPL 変数でどちらをリンクするか選ぶ
```

```makefile
SHM_IMPL ?= sysv   # posix に変えて make するだけで切り替わる
```

`#ifdef` も共用体も不要。切り替えはリビルドで行う。

## API

```c
ShmHandle shm_api_init(void);
bool      shm_api_write(ShmHandle handle, int status, const char* msg);
bool      shm_api_read(ShmHandle handle, int* out_status, char* out_msg);
void      shm_api_close(ShmHandle handle);
void      shm_api_destroy(ShmHandle handle);
```

- `shm_api_init`: 共有メモリの作成または接続。作成者判定を内部で行う
- `shm_api_write`: ミューテックスで保護して `status_code` と `message` を書き込む
- `shm_api_read`: ミューテックスで保護して `status_code` と `message` を読み出す
- `shm_api_close`: 自プロセスのデタッチのみ。共有メモリ領域は残る
- `shm_api_destroy`: `is_creator` の場合のみ共有メモリを削除する

## 初期化とオーナー判定

`O_CREAT | O_EXCL` のアトミック性を利用して作成者を決定する。
起動順序の固定化が不要になり、`ShmContext` 内に `is_creator` フラグを保持する。

```c
ctx->shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
if (ctx->shm_fd >= 0) {
    ctx->is_creator = true;
    ftruncate(ctx->shm_fd, sizeof(SharedData));
} else if (errno == EEXIST) {
    ctx->is_creator = false;
    ctx->shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
}
```

SystemV も同様に `IPC_CREAT | IPC_EXCL` で判定する。

### POSIX 実装: ftruncate 完了待機

POSIX 版では作成者が `ftruncate` を完了する前に接続者が `mmap` するとゼロサイズのマッピングになる。
`fstat` でサイズを確認し、完了まで 1ms ポーリングで待機する。

```c
if (!is_creator) {
    struct stat st;
    while (fstat(ctx->shm_fd, &st) == 0 && st.st_size < (off_t)sizeof(SharedData)) {
        usleep(1000);
    }
}
```

## ミューテックス設計

### プロセス間共有

`PTHREAD_PROCESS_SHARED` を設定することで、同一共有メモリ領域にマップした異なるプロセスが同一ミューテックスを利用できる。
Mutex の初期化は作成者のみが行う（二重初期化を防ぐため）。

```c
pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
```

### ロバスト Mutex（EOWNERDEAD 回復）

`PTHREAD_MUTEX_ROBUST` が利用可能な環境では、ロックを保持したままプロセスが死亡した場合に `EOWNERDEAD` が返る。
`safe_lock` ヘルパーがこれを検知して `pthread_mutex_consistent` で状態を復旧する。

```c
#ifdef PTHREAD_MUTEX_ROBUST
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
#endif
```

```c
static bool safe_lock(pthread_mutex_t *mtx) {
    int ret = pthread_mutex_lock(mtx);
    if (ret == 0) return true;
#ifdef PTHREAD_MUTEX_ROBUST
    if (ret == EOWNERDEAD) {
        pthread_mutex_consistent(mtx);
        return true;
    }
#endif
    return false;
}
```

## クリーンアップ責任

`shm_api_close()` / `shm_api_destroy()` 内部で `is_creator` を見て削除を判断する。
呼び出し側はオーナーを意識せず API を呼ぶだけ。

クラッシュ時の残骸は OS（`/dev/shm` は tmpfs、再起動で消える）に委譲する。
SystemV の場合は `ipcrm` コマンドで手動削除が必要。

## 安全性

### バッファ書き込み

`strncpy` は `src` 長 ≥ `n` のとき null 終端しない。両実装とも `snprintf` に統一した。

```c
// 書き込み
snprintf(handle->shm_ptr->message, sizeof(handle->shm_ptr->message), "%s", msg);
// 読み出し（コピー先バッファサイズを SharedData の定義に合わせる）
snprintf(out_msg, sizeof(handle->shm_ptr->message), "%s", handle->shm_ptr->message);
```

### エラーログのスレッド安全性

`strerror` はグローバルバッファを使用するため MT-Unsafe。
`debug_log.h` が提供する `safe_strerror`（`_Thread_local` バッファ + `strerror_r` ラッパー）を使用する。

### NULL ガード

`shm_api_write` / `shm_api_read` はハンドルが有効でも `shm_ptr` が NULL の場合を明示的にガードする。

```c
if (!handle || !handle->shm_ptr) return false;
```

## モジュール間のヘッダ共有

`SHM/` と `PosixSHM/` は独立したモジュールであり、共通ヘッダは不要。
各モジュールが独自に `SharedData` を定義する。

ただし両モジュールは同一の共有メモリ領域を読み書きするため、
**`SharedData` のメモリレイアウトは両実装で一致させることが実装上の規約となる**。

## データ構造

```c
typedef struct {
    pthread_mutex_t mtx;    /* プロセス間共有 Mutex */
    int             status_code;
    char            message[256];
    volatile int    updated;  /* 書き込み完了フラグ（現在未使用） */
} SharedData;
```

`updated` フィールドは将来のポーリング型読み出し向けに予約されているが現在は未使用。

## スコープ外

- クラッシュ時のクリーンアップ: OS（tmpfs）に委譲
- 実行時の動的切り替え: 対象外
- リングバッファ化（複数パケット保持）: 現実装は単一パケットモデル。MQ 通知で刈り取りを促す設計のため遅延問題はトレードオフとして受容
- MQ のラッパー化: POSIX MQ を学ぶタイミングで別途検討
