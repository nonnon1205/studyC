# E2E テスト 判断ロジック仕様書

各テストスクリプトが「何を入力として、何をどう確認して、何をもって PASS / FAIL とするか」を記述する。

---

## conftest.py — テスト共通基盤

### check_for_leaks() が具体的にチェックするもの

テストセッション終了後、以下の3種類のリソースが OS 上に残存していないかを確認する。

| 確認対象 | 確認方法 | 残存 = 意味するもの |
|---|---|---|
| UNIX ソケットファイル 3本 | `os.path.exists("/tmp/studyc_*.sock")` | プロセスが正常終了せず、ソケットを削除できなかった |
| POSIX 共有メモリ | `os.path.exists("/dev/shm/studyc_shm")` | SHM の `shm_unlink` が呼ばれなかった |
| SystemV MQ | `ipcs -q -m` の出力に `0x54321` または `345889`（同値の10進）が含まれるか | MQ の `mq_unlink` が呼ばれなかった |

FAIL 条件: いずれか1つでも残存していればテストセッションを FAIL にする。

### studyc_processes フィクスチャの起動確認方法

| プロセス | 確認方法 | 根拠 |
|---|---|---|
| Viewer | `wait_for_tcp_port(7777)` — 実際に TCP 接続が成功するまでリトライ | TCP listen が確認できれば起動完了と言える |
| Router | `os.path.exists("/tmp/studyc_router.sock")` が真になるまでポーリング | 管理ソケットのバインド完了 = 初期化完了の指標。systemd 不要で CI 環境でも動作する |
| Collector | `os.path.exists("/tmp/studyc_collector.sock")` が真になるまでポーリング | 同上 |

---

## test_data_pipeline.py — 正常系パイプライン

### 入力

`"Hello_CI_Pipeline_Test"` という固定文字列を UDP（ポート 9999）で Collector へ送信する。

### 判断ロジック

1. Viewer プロセスに SIGINT を送り、stdout を回収する
2. `assert "Hello_CI_Pipeline_Test" in viewer_out` — 送った文字列がそのまま Viewer の stdout に現れるか

PASS 条件: Viewer の stdout に送信文字列が含まれる  
FAIL 条件: 含まれない（パイプラインのどこかで詰まった、またはプロセスがクラッシュした）

### 現時点での限界

- 「そのまま届く」しか確認しない。到達までの遅延・経路・フォーマットは未定義
- 0.5秒の待機時間に根拠なし

---

## test_edge_cases.py — UDP 境界値・異常系

### 入力と判断

各パケットを送信後、3プロセス全員に対して `proc.poll() is None`（= プロセスが動き続けているか）を確認する。

| ケース | 送信内容 | 何を確認しているか |
|---|---|---|
| 1-1 | 0バイト（空） | 空パケットでクラッシュしないか |
| 1-2 | `b"A" * 1023` | バッファ上限ギリギリでクラッシュしないか |
| 1-3 | `b"B" * 4000` | バッファ超過時に切り詰め処理がクラッシュしないか |
| 1-4 | `b"\x00\x01\x02\x03\xff\xfe"` 混入 | バイナリ・制御文字でクラッシュしないか |

PASS 条件: `proc.poll() is None` — 全プロセスが終了していない  
FAIL 条件: `proc.poll()` が整数を返す — プロセスが終了した（ASan 有効ビルドならバッファ超過時に abort する）

### 現時点での限界

- 1-2: 1023バイトが「SHM に正しく書き込まれた」かは確認していない。クラッシュしないことしか見ていない
- 1-3: 「1023バイトに切り詰められた」かは確認していない。切り詰め処理が壊れていてもこのテストは PASS する

---

## test_tcp_edge_cases.py — TCP 異常系

### 入力と判断

| ケース | 操作 | 何を確認しているか |
|---|---|---|
| 即切断 × 10回 | TCP 接続後、何も送らずに即 `close()` | accept/recv のループが即切断で壊れないか |
| 放置切断 × 1回 | TCP 接続後、0.3秒待ってから `close()` | アイドル接続の切断で壊れないか |

各ケース後、`proc.poll() is None` で全プロセスの生死を確認する。

PASS 条件: 全プロセスが生存していること  
FAIL 条件: いずれかのプロセスが終了していること

### 現時点での限界

- FD（ファイルディスクリプタ）リークは検出できない。プロセスが生きていれば PASS になる
- 接続回数 10 という数値に根拠なし

---

## test_mgmt_cmd_scenarios.py — mgmtctl コマンドシナリオ

`subprocess.run(["mgmtctl", module, command])` を実行し、終了コードと出力文字列で判断する。

### ping（5-1 / 5-2 / 5-3）

| 確認項目 | 判断方法 |
|---|---|
| 終了コード | `r.returncode == 0` |
| ヘッダ行 | `"PING -> OK" in r.stdout` |
| ペイロード | `"pong" in r.stdout` |

### status（5-4 / 5-5 / 5-6）

| 確認項目 | 判断方法 |
|---|---|
| 終了コード | `r.returncode == 0` |
| ヘッダ行 | `"GET_STATUS -> OK" in r.stdout` |
| ペイロード形式 | `json.loads(r.stdout.splitlines()[1])` — 2行目が JSON としてパースできるか |
| status フィールド | `data["status"] == "running"` |
| uptime_s フィールド | `isinstance(data["uptime_s"], int) and data["uptime_s"] >= 0` — 非負の整数であること |

### shutdown（5-8 / 5-9 / 5-10）

| 確認項目 | 判断方法 |
|---|---|
| 終了コード | `r.returncode == 0` |
| ヘッダ行 | `"SHUTDOWN -> OK" in r.stdout` |
| ペイロード | `"shutdown initiated" in r.stdout` |
| プロセス終了 | `_wait_for_exit(proc)` — 3秒以内に `proc.poll() is not None` になるか |

### 不正モジュール（5-7）

| 確認項目 | 判断方法 |
|---|---|
| 終了コード | `r.returncode != 0` |
| エラーメッセージ | `"Unknown module:" in r.stderr` |

### 不正コマンド名（5-11）

| 確認項目 | 判断方法 |
|---|---|
| 終了コード | `r.returncode != 0` |
| エラーメッセージ | `"Unknown command:" in r.stderr` |

---

## 現時点での全体的な限界（パラメーター未定義問題）

現在のテストは「動作するか・クラッシュしないか」の確認にとどまっており、以下のパラメーターは定義・検証されていない。

| 未定義パラメーター | 本来確認すべきこと |
|---|---|
| バッファサイズ（1024バイト） | 切り詰め後の実データ長を TCP 出力側で確認する |
| 再試行回数（Router の TCP 接続リトライ） | 何回リトライして諦めるか、実際に観測する |
| タイムアウト値（mgmtctl の 5000ms） | 応答なし時に 5秒で諦めるか確認する |
| shutdown 完了時間（テストの上限 3秒） | グレースフル終了シーケンスの最大所要時間を仕様として定義する |
| TCP ウィンドウ・フロー制御 | 受信側が詰まったとき送信側がブロックするか確認する |

これらは Phase 7（新機能実装）以降で仕様として定義し、テストに落とし込む対象となる。


#個人向けメモ
pytest E2E テストの全体フロー

make test
  └─ pytest e2e/ -v
       │
       ├─ [セッション開始] conftest.py: manage_resources (autouse)
       │       └─ force_cleanup()  ← 前回の残骸を掃除してからスタート
       │
       ├─ [テスト1] test_data_pipeline.py::test_udp_to_tcp_pipeline
       │       │
       │       ├─ [フィクスチャ起動] conftest.py: studyc_processes
       │       │       ├─ Viewer 起動
       │       │       ├─ wait_for_tcp_port(7777) ← TCP Listen を確認してから次へ
       │       │       ├─ Router 起動 → 0.3秒待つ
       │       │       └─ Collector 起動 → 0.3秒待つ
       │       │
       │       ├─ [テスト本体]
       │       │       ├─ UDPパケット送信 → Collector へ
       │       │       ├─ 0.5秒待つ（パイプライン通過を待機）
       │       │       ├─ Viewer に SIGINT → stdout を回収
       │       │       └─ assert "Hello_CI_Pipeline_Test" in viewer_out
       │       │
       │       └─ [フィクスチャ後処理] 3プロセスに SIGINT → 終了待ち
       │
       ├─ [テスト2] test_edge_cases.py::test_udp_edge_cases
       │       ├─ [フィクスチャ起動] studyc_processes (同上)
       │       ├─ [テスト本体] 0バイト/1023バイト/4000バイト/バイナリ を UDP送信
       │       ├─ assert proc.poll() is None  ← 3プロセスが生存しているか確認
       │       └─ [フィクスチャ後処理]
       │
       ├─ [テスト3] test_mgmt_edge_cases.py::test_mgmt_edge_cases  (同様)
       ├─ [テスト4] test_tcp_edge_cases.py::test_tcp_edge_cases    (同様)
       │
       └─ [セッション終了] conftest.py: manage_resources
               ├─ check_for_leaks()  ← IPC/SHM/Unixソケットが残っていないか確認
               ├─ force_cleanup()
               └─ リークがあれば pytest.fail()


2つの重要な概念
フィクスチャ（fixture）
pytest の仕組みで、テスト関数の「前準備と後片付け」をセットにしたものです。


@pytest.fixture
def studyc_processes():
    # === ここが「前準備」 ===
    viewer_proc = subprocess.Popen(...)
    router_proc = subprocess.Popen(...)
    collector_proc = subprocess.Popen(...)

    yield {"collector": ..., "router": ..., "viewer": ...}
    # yield の戻り値がテスト関数の引数に渡される

    # === ここが「後片付け」（テスト終了後に必ず実行） ===
    for proc in [...]:
        _terminate_proc(proc)
テスト関数が引数に studyc_processes と書くだけで pytest が自動的に呼び出します。


def test_udp_edge_cases(studyc_processes):
    #                   ↑ これだけで3プロセスが起動済みの状態でテストが始まる
scope（スコープ）
フィクスチャがいつ作られていつ壊されるかの単位です。

scope	生存期間
scope="function"（デフォルト）	テスト1本ごとに起動・停止
scope="session"	pytest 全体で1回だけ
studyc_processes は function スコープなので、テストごとにプロセスが新しく起動します。manage_resources は session スコープなので、全テストの前後に1回だけ動きます。
