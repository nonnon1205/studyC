# CLAUDE.md — studyC プロジェクト規約

## モジュール構成

| ディレクトリ | 役割 |
|---|---|
| `Collector` | UDP受信 → SHM書込 + MQ通知 |
| `Router` | MQ受信 → SHM読出 → TCP中継 |
| `Viewer` | TCP受信 → コンソール表示 |
| `SHM` | 共有メモリ API ライブラリ (`libshm.a`) |
| `Common` | 共通ライブラリ：統合ロガー等 (`libcommon.a`) |
| `Mgmt` | 管理基盤ライブラリ：ソケット・ハンドラ登録・プロトコル (`libmgmt.a`) |
| `MgmtCtl` | 管理 CLI：`mgmtctl` バイナリ（`libmgmt.a` をリンク） |

**旧名称（使用禁止）**: PollIO → Collector, TestMsgRcv → Router, TestUDPKill → Viewer

## ビルド

```bash
# 全モジュール（依存順序は Makefile が管理）
make

# デバッグビルド（-DDEBUG フラグ付き）
make debug

# 個別ビルド（依存ライブラリを先にビルドすること）
make -C SHM && make -C Common   # ライブラリ先行
make -C Collector
make -C Router

# クリーン
make clean
```

**依存関係**: ビルド順序は `SHM` → `Common` → `Mgmt` → `Collector` / `Router` / `Viewer` / `MgmtCtl`。
ライブラリは `lib/` 以下に出力される（`libshm.a`, `libcommon.a`, `libmgmt.a`）。

## ロギング規約

統合ロガー (`Common/include/unified_logger.h`) を使用する。

### エントリポイント（main.c）
```c
log_init("ModuleName");   // 起動時（sigaction の後、pthread_create の前）
// ...
log_close();              // 終了時
```

### ログ出力マクロ
```c
// グローバルロガー用（推奨・最もシンプル）
GLOG_TRACE(fmt, ...)
GLOG_DEBUG(fmt, ...)
GLOG_INFO(fmt, ...)
GLOG_WARN(fmt, ...)
GLOG_ERR(fmt, ...)
GLOG_FATAL(fmt, ...)

// ハンドルベース（マルチインスタンス用）
ULOG_DEBUG_AT(handle, fmt, ...)   // __FILE__/__LINE__/__func__ 自動付与
```

**禁止**: `LOG_INFO` / `LOG_ERR` 等のマクロ名は `<syslog.h>` の定数と衝突するため定義・使用しない。

### printf を残してよい箇所
対話型ターミナルUIのプロンプト（`printf("> "); fflush(stdout);`）はそのままでよい。アプリケーションログではないため。

## コーディング規約

- **スレッドセーフ**: `inet_ntoa` は MT-Unsafe なので `inet_ntop` を使用すること
- **標準**: `-std=c11 -D_POSIX_C_SOURCE=200809L`
- **警告**: `-Wall -Wextra` でゼロ警告を維持する

## 積み残し（既知の未対応事項）

- `docs/APISpecification.text` に旧モジュール名が残っている（未更新）
- Router / Viewer に `GLOG_*` マクロ未適用（`printf`/`perror` のまま）
- ファイル命名規則の整理（例: `udp_worker.c` → `router_udp_worker.c`）
- Mgmt モジュールの各プロセスへの統合
