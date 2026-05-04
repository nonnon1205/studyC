# mgmtctl コマンド仕様書

## 概要

`mgmtctl` は studyC の各プロセス（Collector / Router / Viewer）に対して、  
実行中にコマンドを送信するための管理 CLI です。

内部では UNIX ドメインソケット（データグラム方式）を用いてプロセス間通信を行います。

---

## 使い方

```
mgmtctl <module> <command> [arg]
```

| 引数 | 説明 |
|---|---|
| `module` | 対象プロセス。`collector` / `router` / `viewer` のいずれか |
| `command` | コマンド名（下記参照） |
| `arg` | コマンドによっては必要な引数 |

### 出力形式

```
[<module>] <command> -> <result>
<payload>
```

例：
```
[collector] PING -> OK
pong
```

---

## ソケットパス

| プロセス | ソケットパス |
|---|---|
| Collector | `/tmp/studyc_collector.sock` |
| Router | `/tmp/studyc_router.sock` |
| Viewer | `/tmp/studyc_viewer.sock` |

---

## コマンド一覧

### 実装済み（全モジュール共通）

#### `ping` — 死活確認

```
mgmtctl <module> ping
```

- **引数**: なし
- **レスポンス**: `pong`
- **用途**: プロセスが起動・応答可能な状態かを確認する

例：
```
$ mgmtctl collector ping
[collector] PING -> OK
pong
```

---

#### `status` — ステータス取得

```
mgmtctl <module> status
```

- **引数**: なし
- **レスポンス**: JSON 形式

```json
{"status":"running","uptime_s":42}
```

| フィールド | 型 | 説明 |
|---|---|---|
| `status` | string | 常に `"running"`（起動中のみ応答するため） |
| `uptime_s` | number | プロセス起動からの経過秒数 |

例：
```
$ mgmtctl router status
[router] GET_STATUS -> OK
{"status":"running","uptime_s":127}
```

---

#### `shutdown` — グレースフルシャットダウン

```
mgmtctl <module> shutdown
```

- **引数**: なし
- **レスポンス**: `shutdown initiated`
- **動作**: 対象プロセスが正常終了シーケンスに入る（リソース解放 → プロセス終了）

例：
```
$ mgmtctl viewer shutdown
[viewer] SHUTDOWN -> OK
shutdown initiated
```

> **注意**: このコマンドを送信するとプロセスが終了します。  
> テストコードから呼ぶ場合は、プロセスの再起動まで考慮してください。

---

### 未実装・設計中

#### `all shutdown` — 全プロセス一括停止（未実装）

```
mgmtctl all shutdown
```

- **引数**: なし
- **動作**: Collector → Router → Viewer の順に `shutdown` コマンドを送信し、全プロセスを停止する
- **実装場所**: `MgmtCtl/src/main.c` の `module_to_path()` に `"all"` を追加し、  
  各ソケットパスへ順次コマンドを発行するロジックを追加する
- **考慮事項**: 停止順序は依存関係の逆順（Collector を先に止めてデータ流入を遮断してから Router・Viewer を止める）

---

### プロトコル定義済み・ハンドラ未実装

以下のコマンドは `mgmtctl` の引数として認識されますが、  
各プロセス側にハンドラが実装されていないため、現在は `HANDLER_FAILED` を返します。  
Phase 7 で実装予定です。

| コマンド | 引数 | 説明 |
|---|---|---|
| `metrics` | なし | パフォーマンスメトリクスのスナップショット取得 |
| `reset` | なし | メトリクスカウンターのリセット |
| `loglevel` | `trace\|debug\|info\|warn\|error\|fatal` | ログレベルの動的変更 |
| `bufsize` | `<整数>` | バッファ容量の変更 |
| `profiling` | なし | パフォーマンスプロファイリングの有効化 |
| `tracing` | なし | 詳細イベントトレースの有効化 |
| `config` | なし | 現在の設定ダンプ |

---

## エラーケース

| 状況 | 終了コード | stderr 出力例 |
|---|---|---|
| モジュール名が不正 | 1 | `Unknown module: foo` |
| コマンド名が不正 | 1 | `Unknown command: bar` |
| プロセスが応答しない（タイムアウト） | 1 | `No response from collector (is it running?)` |
| `loglevel` で引数なし | 1 | `loglevel requires a level argument` |
| `bufsize` で引数なし | 1 | `bufsize requires a size argument` |

タイムアウトは **5000 ms**（5秒）。

---

## プロトコル詳細

内部実装の参考情報です。CLI を使う上では意識不要。

| 項目 | 値 |
|---|---|
| 通信方式 | UNIX ドメインソケット（`SOCK_DGRAM`） |
| リクエストサイズ | `sizeof(MgmtCommandRequest)` （固定長） |
| レスポンスサイズ | `sizeof(MgmtCommandResponse)` （固定長） |
| ペイロード最大（リクエスト） | 512 バイト |
| ペイロード最大（レスポンス） | 2048 バイト（JSON 格納域） |
| プロトコルバージョン | 1 |
