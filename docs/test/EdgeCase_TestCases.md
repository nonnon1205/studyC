# エッジケース・異常系 試験項目書

本ドキュメントは、システムが境界値や異常な状態に直面した際の期待される挙動を定義するものです。

## テスト層の分類方針

| 層 | 対象 | テスト手段 |
|:---|:---|:---|
| **E2E** | システムの外部インターフェース（UDP入口・mgmtctl CLI） | Python pytest (`tests/e2e/`) |
| **単体テスト** | 内部モジュールの関数・API（SHM API・内部ロジック等） | C テストフレームワーク（Phase 5 で導入予定） |

**外部インターフェースの定義:**
- UDP ポート（Collector への唯一のデータ入口）
- `mgmtctl` バイナリ（運用操作の入口）
- TCP ポート（Viewer のデータ出口）

SHM、MQ、UNIX ドメインソケット（`/tmp/studyc_*.sock`）はプロセス間の内部インターフェースであり、単体テストの対象とする。

---

## 1. UDP受信機能 (Collector) — **E2E** / 実装済み

| No | テスト項目（入力・状態） | 期待される挙動 | 備考 |
|:---|:---|:---|:---|
| 1-1 | 0バイトのUDPペイロードを受信 | 処理をスキップし無視する | DoS攻撃耐性の確認 |
| 1-2 | `MAX_PAYLOAD_SIZE - 1`（1023）バイトの文字列を受信 | 全て正常にSHMへ書き込まれ、Routerへ伝搬する | バッファ境界（最大） |
| 1-3 | `MAX_PAYLOAD_SIZE`（1024）バイト以上の文字列を受信 | 1023バイトで切り詰められ、バッファオーバーフローを起こさない | バッファ超過 |
| 1-4 | バイナリデータ・制御文字を含むUDPを受信 | システムがクラッシュせず処理される | 文字列以外の混入 |

---

## 2. 共有メモリ・API (SHM) — **単体テスト** / 未着手

| No | テスト項目（入力・状態） | 期待される挙動 | 備考 |
|:---|:---|:---|:---|
| 2-1 | `shm_api_write` の引数 `msg` に `NULL` を渡す | `assert` によりプロセスが即座にアボートする | API規約違反の早期検知 |
| 2-2 | `shm_api_write` に空文字列 `""` を渡す | SHMに空文字が書き込まれ、以降のシステムに正常伝搬する | 正常系エッジケース |
| 2-3 | 共有メモリの `message` 領域に `\0` が無い状態での `read` | `snprintf` の制限により、最大長まで安全に読み取られクラッシュしない | 共有メモリ破損時の保護 |

---

## 3. 並行処理・排他制御 — **単体テスト** / 未着手

| No | テスト項目（入力・状態） | 期待される挙動 | 備考 |
|:---|:---|:---|:---|
| 3-1 | SHMのMutexをロックした状態でプロセスを `SIGKILL` | 残されたプロセスがロバスト属性によりMutexを復旧し、デッドロックしない | EOWNERDEADの復旧 |
| 3-2 | RouterのMQ通知受信からSHM読取の間に、SHMが別値で上書きされる | MQの通知回数とSHMの値がズレても、システムはクラッシュせず最新の値を読む | レースコンディション許容 |
| 3-3 | Collector(作成側)起動前に、Routerを起動する | 初期化でSHMが存在しなくても、所定の待機・リトライまたは安全なエラー終了となる | 起動順序の非依存 |

---

## 4. ネットワーク連携 (Router -> Viewer) — **E2E** / 実装済み

| No | テスト項目（入力・状態） | 期待される挙動 | 備考 |
|:---|:---|:---|:---|
| 4-1 | Viewer (TCP Server) が起動していない状態で Router 起動 | 設定された回数・間隔で接続をリトライし、失敗した場合は安全にプロセス終了 | 起動時の異常系 |
| 4-2 | 運用中に Viewer 側のプロセスが強制終了 (TCP切断) | 次のデータ送信時に `send` エラーを検知し、SIGPIPEでクラッシュしない | `MSG_NOSIGNAL`等の確認 |
| 4-3 | SHMから読み取ったペイロードが 0バイト (`""`) | `[TCP-RELAY] \n` 等として正常にTCP送信される | 伝搬のエッジケース |

---

## 5. 管理操作 (mgmtctl CLI) — **E2E** / 実装済み

外部インターフェースである `mgmtctl` バイナリを直接呼び出してテストする。
UNIX ドメインソケットの直叩きは行わない（実装詳細への依存を避けるため）。

出力形式: `[<module>] <COMMAND> -> OK\n<payload>\n`

| No | テスト項目（コマンド） | 期待される挙動 | 備考 |
|:---|:---|:---|:---|
| 5-1 | `mgmtctl collector ping` | 終了コード0、stdout に `PING -> OK` と `pong` を含む | 死活確認の正常系 |
| 5-2 | `mgmtctl router ping` | 終了コード0、stdout に `PING -> OK` と `pong` を含む | 死活確認の正常系 |
| 5-3 | `mgmtctl viewer ping` | 終了コード0、stdout に `PING -> OK` と `pong` を含む | 死活確認の正常系 |
| 5-4 | `mgmtctl collector status` | 終了コード0、ペイロードが `{"status":"running","uptime_s":N}` の JSON | CLI引数は `status`、内部コマンドは `GET_STATUS` |
| 5-5 | `mgmtctl router status` | 終了コード0、ペイロードが `{"status":"running","uptime_s":N}` の JSON | CLI引数は `status`、内部コマンドは `GET_STATUS` |
| 5-6 | `mgmtctl viewer status` | 終了コード0、ペイロードが `{"status":"running","uptime_s":N}` の JSON | CLI引数は `status`、内部コマンドは `GET_STATUS` |
| 5-7 | `mgmtctl unknown_module ping` | 終了コード非0、stderr に `Unknown module:` を含む | 不正入力の異常系（ソケット通信前に失敗） |
| 5-8 | `mgmtctl collector shutdown` | 終了コード0、`SHUTDOWN -> OK` と `shutdown initiated` を含む、かつ Collector プロセスが終了する | shutdown 正常系 |
| 5-9 | `mgmtctl router shutdown` | 同上、Router プロセスが終了する | shutdown 正常系 |
| 5-10 | `mgmtctl viewer shutdown` | 同上、Viewer プロセスが終了する | shutdown 正常系 |
| 5-11 | `mgmtctl collector unknown_cmd` | 終了コード非0、stderr に `Unknown command:` を含む | 不正コマンド名の異常系（ソケット通信前に失敗） |