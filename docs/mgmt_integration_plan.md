# Mgmt コマンド統合 実装計画

## 目的

Collector / Router / Viewer の各モジュールに管理インターフェースを組み込み、
実行中のプロセスへ外部から UNIX ドメインソケット経由でコマンドを送受信できるようにする。

---

## 設計方針

### モジュール別統合方式

| モジュール | 方式 | 根拠 |
|-----------|------|------|
| **Collector** | **poll-based**（スレッドなし） | シングルスレッド設計を維持する。mgmt FD を既存の `select()` に追加するだけで対応可能 |
| **Router** | スレッド追加 | 既存マルチスレッド設計に自然に追加できる |
| **Viewer** | スレッド追加 | 既存マルチスレッド設計に自然に追加できる |

### コマンド遅延の許容性

GET 系コマンド（PING / GET_STATUS / GET_METRICS）のハンドラ処理は
メモリ読み取り + JSON 整形のみで約 10μs。
OS のソケットバッファが吸収するため Collector の UDP 受信への実用的な影響はない。

セッションリセットを伴うコマンド（SET_BUFFER_SIZE 等）は
処理後に接続が切れるため、直前の遅延は問題にならない。

### SHM 書き込み方式を採用しない理由

GET 系を SHM 経由にすると hot path に毎イベントの書き込みオーバーヘッドが乗る。
また、SHM はウォッチドッグの liveness 確認には使えない
（プロセスが固まっていても SHM は読める）。
コマンド方式（PING への応答）の方がウォッチドッグ用途に適している。

---

## 現状の問題点（着手前に修正必要）

| # | 問題 | 影響 | 対処方針 |
|---|------|------|----------|
| 1 | `mgmt_worker` が `mgmt_socket.c`（L194）と `mgmt_worker.c`（L24）の2箇所で重複定義 | `libmgmt.a` リンク時にシンボル衝突 | `mgmt_socket.c` 側の定義を削除 |
| 2 | `MGMT_SOCKET_PATH` が1つ（`/tmp/sutdyc_mgmt.sock`）しかない | 3プロセスが同じパスに bind → 2番目以降が失敗 | モジュール別パスを定義し `mgmt_socket_create()` に渡す |
| 3 | `mgmt_worker.c` が `extern atomic_bool g_keep_running` に依存 | Router / Viewer への組み込み時に外部シンボル依存が残る | `mgmt_worker` の引数を構造体にして依存を内部化 |
| 4 | `metrics_collector.h` が旧モジュール名（`MetricsPollio`, `MetricsTestMsgRcv`, `MetricsTestUdpkill`）を使用 | 新命名規則と乖離 | 型名を `MetricsCollector`, `MetricsRouter`, `MetricsViewer` に更新 |
| 5 | poll-based 統合に必要な mgmt ソケット FD を取得する手段がない | Collector の `select()` に mgmt FD を追加できない | `mgmt_socket_get_fd()` を追加 |

---

## アーキテクチャ概要

```
[オペレーター]
       |
       | mgmtctl collector get_metrics
       v
+-------------+
|  MgmtCtl   |  mgmtctl バイナリ
|  (CLI)     |  libmgmt.a をリンク（プロトコル部品を共有）
+------+------+
       |  sendto("/tmp/studyc_<module>.sock")
       |
  +---------+---------+
  |         |         |
  v         v         v
+----------+ +----------+ +----------+
| Collector| |  Router  | |  Viewer  |
|          | |          | |          |
| select() | | msgrcv   | | cond_wait|
| udp_fd   | | loop     | | loop     |
| mgmt_fd  | |          | |          |
| ↓ dispatch| |mgmt_wrkr | |mgmt_wrkr |
| handlers | | handlers | | handlers |
+----------+ +----------+ +----------+
  ↕libmgmt.a   ↕libmgmt.a   ↕libmgmt.a
```

各モジュールが独立したソケットを持ち、`libmgmt.a` を静的リンクする。
`MgmtCtl` は `libmgmt.a` のプロトコル部品（`mgmt_request_init()` 等）を使い、
sendto / recvfrom で各モジュールのソケットに直接コマンドを送る。

---

## コマンド対応表

| コマンド | Collector | Router | Viewer | レスポンス形式 |
|----------|-----------|--------|--------|----------------|
| `PING` | ✓ | ✓ | ✓ | `{"status":"pong"}` |
| `GET_STATUS` | ✓ | ✓ | ✓ | 各モジュール固有の状態 JSON |
| `SET_LOG_LEVEL` | ✓ | ✓ | ✓ | `{"result":"ok","level":"<new>"}` |
| `GET_METRICS` | ✓ | ✓ | ✗ | メトリクス JSON |
| `RESET_METRICS` | ✓ | ✓ | ✗ | `{"result":"ok"}` |
| `SHUTDOWN` | ✓ | ✓ | ✓ | `{"result":"ok"}` |
| `GET_CONFIG` | ✓ | ✓ | ✗ | 設定値 JSON |
| `SET_BUFFER_SIZE` | ✓ | ✗ | ✗ | セッションリセット後に有効 |

---

## 実装フェーズ

### Phase 0: Mgmt モジュール修正（前提作業）

他フェーズの前に完了させる。

**0-1. `mgmt_socket.c` の重複 `mgmt_worker` を削除**

```diff
- /* Worker Thread ============================================================ */
- void* mgmt_worker(void* arg) { ... }   ← L194-222 を削除
```

**0-2. `mgmt_socket_create()` にソケットパス引数を追加**

```c
// mgmt_socket.h
MgmtSocketHandle mgmt_socket_create(const char* socket_path);

// mgmt_socket.c
// ・socket_path を struct MgmtSocket に保持（unlink に使うため）
// ・bind の対象を socket_path に変更
// ・mgmt_socket_destroy() も保持した path で unlink
```

**0-3. `mgmt_socket_get_fd()` を追加（poll-based 統合用）**

Collector の `select()` に mgmt FD を渡すために必要。

```c
// mgmt_socket.h
int mgmt_socket_get_fd(MgmtSocketHandle handle);

// mgmt_socket.c
int mgmt_socket_get_fd(MgmtSocketHandle handle) {
    return handle ? handle->socket_fd : -1;
}
```

**0-4. `mgmt_worker.c` の引数構造体化（Router / Viewer 向け）**

`extern atomic_bool g_keep_running` への依存を除去する。

```c
// mgmt_worker.h（新規）
typedef struct {
    const char*  socket_path;
    atomic_bool* keep_running;
} MgmtWorkerArg;

void* mgmt_worker(void* arg);

// mgmt_worker.c
void* mgmt_worker(void* arg) {
    MgmtWorkerArg* a = (MgmtWorkerArg*)arg;
    MgmtSocketHandle sock = mgmt_socket_create(a->socket_path);
    while (atomic_load_explicit(a->keep_running, memory_order_acquire)) {
        mgmt_socket_process_one(sock, 1000);
    }
    mgmt_socket_destroy(sock);
    return NULL;
}
```

**0-5. `metrics_collector.h` の型名更新**

```diff
- typedef struct { ... } MetricsPollio;
+ typedef struct { ... } MetricsCollector;

- typedef struct { ... } MetricsTestMsgRcv;
+ typedef struct { ... } MetricsRouter;

- typedef struct { ... } MetricsTestUdpkill;
+ typedef struct { ... } MetricsViewer;

  typedef struct {
-     MetricsPollio      pollio;
-     MetricsTestMsgRcv  test_msgrcv;
-     MetricsTestUdpkill test_udpkill;
+     MetricsCollector   collector;
+     MetricsRouter      router;
+     MetricsViewer      viewer;
  } MetricsSnapshot;
```

`metrics_update_pollio()` → `metrics_update_collector()` 等、関数名も合わせて変更する。

**0-6. モジュール別ソケットパス定義**

```c
// Mgmt/include/mgmt_paths.h（新規）
#ifndef MGMT_PATHS_H
#define MGMT_PATHS_H

#define MGMT_SOCKET_PATH_COLLECTOR "/tmp/studyc_collector.sock"
#define MGMT_SOCKET_PATH_ROUTER    "/tmp/studyc_router.sock"
#define MGMT_SOCKET_PATH_VIEWER    "/tmp/studyc_viewer.sock"

#endif
```

---

### Phase 1: 共通 Makefile 変更（全モジュール）

各モジュールの Makefile に `libmgmt.a` を追加する。

```makefile
LIBS    := $(LIBDIR)/libmgmt.a $(LIBDIR)/libshm.a $(LIBDIR)/libcommon.a
LDFLAGS := -pthread -L$(LIBDIR) -lmgmt -lshm -lcommon
CFLAGS  += -I$(LIBDIR)/include
```

トップレベル `Makefile` のビルド順序に `Mgmt` を追加する。

```
依存順: SHM → Common → Mgmt → Collector / Router / Viewer
```

---

### Phase 2: Collector 統合（poll-based）

**変更ファイル**: `Collector/src/main.c`、`Collector/src/poll_io.c`（`run_event_loop`）、
新規 `Collector/src/mgmt_handlers.c`

`g_keep_running` の型変更・スレッド追加は**不要**。シングルスレッド設計を維持する。

**2-1. `Collector/src/mgmt_handlers.c` を新規作成**

```c
#include "event_handler.h"
#include "mgmt_protocol.h"
#include "unified_logger.h"
#include <stdio.h>

static int handle_ping(const MgmtCommandRequest* req,
                       MgmtCommandResponse* resp, void* ctx) {
    (void)ctx;
    mgmt_response_init(resp, req->request_id, MGMT_RESULT_OK,
                       "collector", "{\"status\":\"pong\"}", 16);
    return 0;
}

// GET_STATUS / SET_LOG_LEVEL / SHUTDOWN / GET_METRICS / RESET_METRICS / GET_CONFIG
// ... 各ハンドラ実装

int collector_register_handlers(void* ctx) {
    handler_register("collector", MGMT_CMD_PING,          handle_ping,           ctx);
    handler_register("collector", MGMT_CMD_GET_STATUS,    handle_get_status,     ctx);
    handler_register("collector", MGMT_CMD_SET_LOG_LEVEL, handle_set_log_level,  ctx);
    handler_register("collector", MGMT_CMD_SHUTDOWN,      handle_shutdown,       ctx);
    handler_register("collector", MGMT_CMD_GET_METRICS,   handle_get_metrics,    ctx);
    handler_register("collector", MGMT_CMD_RESET_METRICS, handle_reset_metrics,  ctx);
    handler_register("collector", MGMT_CMD_GET_CONFIG,    handle_get_config,     ctx);
    return 0;
}
```

**2-2. `run_event_loop()` のシグネチャ変更と select 追加**

```c
// poll_io.h
void run_event_loop(int udp_fd, int msqid, ShmHandle shm,
                    MgmtSocketHandle mgmt_sock);  // 引数追加

// poll_io.c
void run_event_loop(int udp_fd, int msqid, ShmHandle shm,
                    MgmtSocketHandle mgmt_sock)
{
    int mgmt_fd = mgmt_socket_get_fd(mgmt_sock);

    while (g_keep_running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(udp_fd, &rfds);
        FD_SET(mgmt_fd, &rfds);
        int maxfd = udp_fd > mgmt_fd ? udp_fd : mgmt_fd;

        struct timeval tv = {1, 0};
        int n = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (n < 0) { if (errno == EINTR) continue; break; }

        if (FD_ISSET(udp_fd, &rfds)) {
            /* 既存の UDP 処理 */
        }
        if (FD_ISSET(mgmt_fd, &rfds)) {
            mgmt_socket_process_one(mgmt_sock, 0);  // FD が ready なので即返る
        }
    }
}
```

**2-3. `main.c` への統合**

```c
#include "event_handler.h"
#include "mgmt_paths.h"
#include "mgmt_socket.h"

int main() {
    // (既存) sigaction
    log_init("Collector");

    handler_registry_init();
    collector_register_handlers(NULL);
    MgmtSocketHandle mgmt_sock =
        mgmt_socket_create(MGMT_SOCKET_PATH_COLLECTOR);

    // (既存) ipc_msqid, shm_handle, udp_fd の初期化

    run_event_loop(udp_fd, ipc_msqid, shm_handle, mgmt_sock);  // 引数追加

    // クリーンアップ（既存処理の後に追加）
    mgmt_socket_destroy(mgmt_sock);
    handler_registry_destroy();

    log_close();
    return 0;
}
```

---

### Phase 3: Router 統合（スレッド追加）

**変更ファイル**: `Router/src/main.c`、新規 `Router/src/mgmt_handlers.c`

Router はすでに `atomic_bool g_keep_running` を持つため型変更不要。

対応コマンド: PING, GET_STATUS, SET_LOG_LEVEL, SHUTDOWN, GET_METRICS, RESET_METRICS, GET_CONFIG

**スレッド組み込み位置**:

```c
// pthread_create の順序（main.c）
// t2: signal_worker   ← 既存
// t3: router_worker   ← 既存
// t_mgmt: mgmt_worker ← 追加（t3 の後）

static MgmtWorkerArg mgmt_arg = {
    .socket_path  = MGMT_SOCKET_PATH_ROUTER,
    .keep_running = &g_keep_running,
};
pthread_create(&t_mgmt, NULL, mgmt_worker, &mgmt_arg);
```

`pthread_join` / クリーンアップは `cleanup_t3` の前に `cleanup_t_mgmt` ラベルを追加して対応。

---

### Phase 4: Viewer 統合（スレッド追加）

**変更ファイル**: `Viewer/src/main.c`、新規 `Viewer/src/mgmt_handlers.c`

Viewer は `shutdown_requested`（int）で終了制御しており `atomic_bool` を持たない。
`MgmtWorkerArg` が要求する `atomic_bool*` のために `g_keep_running` を追加する。

```c
// Viewer/src/main.c に追加
atomic_bool g_keep_running = true;

// 既存の shutdown_requested をセットする箇所に追記
atomic_store_explicit(&g_keep_running, false, memory_order_release);
```

対応コマンド: PING, GET_STATUS, SET_LOG_LEVEL, SHUTDOWN

---

### Phase 5: MgmtCtl 実装

`systemd` に対する `systemctl` に相当するモジュール。
`libmgmt.a` をリンクしてプロトコル部品を共有し、
コマンド送受信のみを直接実装する。

**ファイル構成**

```
MgmtCtl/
  Makefile
  src/
    main.c       引数パース・コマンド実行
    mgmt_send.c  sendto / recvfrom の実装
  include/
    mgmt_send.h
```

**CLI インターフェース**

```
mgmtctl <module> <command> [payload]

  mgmtctl collector ping
  mgmtctl collector get_metrics
  mgmtctl router set_log_level debug
  mgmtctl viewer get_status
  mgmtctl router shutdown
```

`main.c` は `<module>` からソケットパス（`mgmt_paths.h` を参照）を解決し、
`mgmt_send.c` の送受信関数を呼び出してレスポンスを標準出力に表示する。

**Makefile**

```makefile
TARGET  := mgmtctl
LDFLAGS := -L$(LIBDIR) -lmgmt -lcommon
# libmgmt.a を先にビルドすること
```

---

### Phase 6: 結合テスト

**6-1. 確認項目**

- [ ] 各モジュール起動時にソケットファイルが作成される
- [ ] `mgmtctl collector ping` に対して `{"status":"pong"}` が返る
- [ ] `mgmtctl router set_log_level debug` 後、ログレベルが変わることをログで確認
- [ ] `mgmtctl router shutdown` 後、プロセスがクリーンに終了する
- [ ] プロセス終了後にソケットファイルが削除される
- [ ] Collector の UDP 受信が mgmt コマンド処理中に途切れないことを確認

---

## 実装順序まとめ

```
Phase 0 → Phase 1 → Phase 5 → Phase 2 → Phase 3 → Phase 4 → Phase 6
 (Mgmt修正) (Makefile) (MgmtCtl)  (Collector) (Router)  (Viewer)  (結合テスト)
```

Phase 5（MgmtCtl）を Phase 2 より前に実施する。
各モジュールへのハンドラ組み込みと並行して動作確認できるようにするため。
Phase 0 が最もリスクが高い（既存 `libmgmt.a` を壊さない確認が必要）。

---

## 積み残し候補（今回スコープ外）

- `metrics_collector.c` の実装確認・完成（現在スタブの可能性あり）
- Mgmt コマンドの認証（`MGMT_RESULT_UNAUTHORIZED` は定義済みだが未使用）
- Viewer の `shutdown_pipe` 経由での `mgmt_worker` 停止統一
