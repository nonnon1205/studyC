# 統合ロガー (Unified Logger) API仕様書

## 1. 概要
本ドキュメントは、`Common`モジュールが提供する統合ロガー（Unified Logger）のAPI仕様を定義します。
このロガーは、syslogおよび標準ファイルへの出力をサポートし、マルチスレッド環境で安全に利用できる（スレッドセーフな）設計となっています。

## 2. データ型

### UlogHandle
```c
typedef struct UlogContext* UlogHandle;
```
ロガーの内部状態を隠蔽する不透明ポインタ（Opaque Handle）です。初期化時に取得し、APIの各関数に渡して使用します。

### UlogLevel
```c
typedef enum {
    ULOG_LEVEL_TRACE   = 0,  /* 詳細なトレース（関数の入出など） */
    ULOG_LEVEL_DEBUG   = 1,  /* デバッグ情報（状態遷移など） */
    ULOG_LEVEL_INFO    = 2,  /* 一般的な情報（正常な操作記録など） */
    ULOG_LEVEL_WARN    = 3,  /* 警告（エラーではないが注意が必要な状態） */
    ULOG_LEVEL_ERROR   = 4,  /* エラー（介入が必要な異常状態） */
    ULOG_LEVEL_FATAL   = 5   /* 致命的エラー（プログラム継続不可能） */
} UlogLevel;
```
ログの出力レベルを定義します。syslogの重要度レベルに準拠しています。

## 3. ライフサイクル・初期化API

### ulog_init
```c
UlogHandle ulog_init(const char* ident, UlogLevel level, uint32_t flags);
```
ロガーを初期化し、ハンドルを返します。プログラム（またはスレッド）の起動時に呼び出します。
- **ident**: syslog等に出力される識別子（例: `"TestMsgRcv"`, `"router_worker"`）
- **level**: 出力する最小ログレベル（このレベル以上のログが出力されます）
- **flags**: ロガーの動作フラグ（ビットORで複数指定可能）
  - `0x01`: ファイル出力（`/tmp/sutdyc.log` 等）を有効化
  - `0x02`: スレッドID（TID）をログに付与
  - `0x04`: タイムスタンプを付与

### ulog_close
```c
void ulog_close(UlogHandle logger);
```
ロガーのリソースを安全に解放します。プログラムの終了時に呼び出します。

## 4. ロギング用マクロ (推奨)
開発者が直接 `ulog_log` などを呼ぶ代わりに、ソースコード上の位置情報（ファイル名、行番号、関数名）を自動付与できるマクロ群の使用を推奨します。

### 標準マクロ
純粋にメッセージのみを出力します。
- `ULOG_TRACE(logger, fmt, ...)`
- `ULOG_DEBUG(logger, fmt, ...)`
- `ULOG_INFO(logger, fmt, ...)`
- `ULOG_WARN(logger, fmt, ...)`
- `ULOG_ERROR(logger, fmt, ...)`
- `ULOG_FATAL(logger, fmt, ...)`

### ATマクロ (位置情報付き)
`__FILE__`, `__LINE__`, `__func__` を自動で展開して出力します。障害調査・デバッグに極めて有用です。
- `ULOG_TRACE_AT(logger, fmt, ...)`
- `ULOG_DEBUG_AT(logger, fmt, ...)`
- `ULOG_INFO_AT(logger, fmt, ...)`
- `ULOG_WARN_AT(logger, fmt, ...)`
- `ULOG_ERROR_AT(logger, fmt, ...)`
- `ULOG_FATAL_AT(logger, fmt, ...)`

## 5. 制御・設定API

### ulog_set_level / ulog_get_level
```c
void ulog_set_level(UlogHandle logger, UlogLevel level);
UlogLevel ulog_get_level(UlogHandle logger);
```
稼働中に動的にログレベルを変更・取得します。Mgmtモジュールなどからの動的制御に利用します。

### ulog_set_context_tag
```c
int ulog_set_context_tag(UlogHandle logger, const char* tag);
```
特定のリクエストIDやセッションIDなどのコンテキストタグ（最大32文字）を設定します。設定後、このロガーから出力されるすべてのログにタグが付与されます。

## 6. 統計情報API

### ulog_stats
```c
int ulog_stats(UlogHandle logger, uint64_t* total_logs, uint64_t* dropped_logs);
```
ロガーの統計情報を取得します。
- **total_logs**: APIが呼び出された総ログ件数
- **dropped_logs**: ログレベルの閾値により出力がスキップ（ドロップ）された件数

### ulog_stats_reset
```c
int ulog_stats_reset(UlogHandle logger);
```
統計情報（カウンタ）をゼロにリセットします。

## 7. 後方互換（レガシー）API
既存のコードベースからスムーズに移行するため、デフォルトのグローバルロガーを使用する互換APIも提供しています。
これらの関数は引数に `UlogHandle` を取りません。

- `void log_init(const char* ident);`
- `void log_close(void);`
- `void log_info(const char* fmt, ...);`
- `void log_warn(const char* fmt, ...);`
- `void log_err(const char* fmt, ...);`