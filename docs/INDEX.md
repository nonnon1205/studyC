# ドキュメント目次

studyC プロジェクトのドキュメント一覧。用途に応じて4カテゴリに分類する。

---

## 製造用設計書 (`design/`)

実装にあたっての構造・方針を定義する文書。コードを書く前・書きながら参照する。

| ファイル | 概要 |
|---|---|
| [BasicSpecification.text](design/BasicSpecification.text) | **基本設計書**。システム全体の目的・プロセス構成・データフローの起点。最初に読む文書。 |
| [architecture_design.md](design/architecture_design.md) | **アーキテクチャ設計書**。モジュール間の依存関係・責務境界の詳細。（作成中） |
| [logger_detailed_design.md](design/logger_detailed_design.md) | **統合ロガー詳細設計書**。`unified_logger` の内部構造・スレッドセーフ設計・syslog連携の実装方針。 |
| [shm_wrapper_design.md](design/shm_wrapper_design.md) | **SHM ラッパー設計方針**。POSIX SHM と System V SHM の両実装の学習・設計比較メモ。 |

---

## API仕様書 (`api/`)

モジュール間・外部インターフェースの入出力を定義する文書。実装者と利用者の両方が参照する。

| ファイル | 概要 |
|---|---|
| [APISpecification.text](api/APISpecification.text) | **プロセス間通信 API 仕様書**。UDP・MQ・TCP の通信インターフェース定義。**注意: 旧モジュール名が残っており更新未済。** |
| [SHM_API_Specification.md](api/SHM_API_Specification.md) | **SHM API 仕様書**。`shm_api_write` / `shm_api_read` の引数・戻り値・エラー仕様。`SHM/` を使う全モジュールが参照する。 |
| [MgmtCtl_CommandSpec.md](api/MgmtCtl_CommandSpec.md) | **mgmtctl コマンド仕様書**。`mgmtctl <module> <command>` の入出力フォーマット・終了コード・プロトコル詳細。 |
| [unified_logger_api.md](api/unified_logger_api.md) | **統合ロガー API 仕様書**。`GLOG_*` マクロ・`log_init` / `log_close` の使い方と引数仕様。 |

---

## 検査用仕様書 (`test/`)

何をどう確認してPASS/FAILとするかを定義する文書。テストコードの設計根拠になる。

| ファイル | 概要 |
|---|---|
| [Test_Strategy.md](test/Test_Strategy.md) | **テスト戦略書**。テストピラミッドの採用方針・E2E→単体の段階的構築計画・各フェーズの目的。 |
| [EdgeCase_TestCases.md](test/EdgeCase_TestCases.md) | **エッジケース・異常系 試験項目書**。UDP境界値・SHM API・並行処理・mgmtctl の全試験項目と期待挙動。実装済み/未着手を明記。 |
| [TestSpec_E2E.md](test/TestSpec_E2E.md) | **E2E テスト 判断ロジック仕様書**。各 pytest スクリプトが「何を入力に、何をどう確認して、PASS/FAILとするか」を具体的に記述。 |

---

## プロジェクト管理 (`project/`)

作業計画・既知問題・意思決定の記録。開発の進捗管理に使う。

| ファイル | 概要 |
|---|---|
| [CI_Refactoring_Plan.md](project/CI_Refactoring_Plan.md) | **CI構築・リファクタリング計画**。フェーズ別のロードマップ・各タスクの完了/未着手状態。 |
| [mgmt_integration_plan.md](project/mgmt_integration_plan.md) | **Mgmt 統合実装計画**。Collector/Router/Viewer への管理インターフェース組み込みの設計方針と実装手順。（実施済み） |
| [known_issues.md](project/known_issues.md) | **既知問題一覧**。CI 整備完了まで修正を保留している問題の記録。着手時はここから削除する。 |

---

## ドキュメントの読み方ガイド

```
新機能を実装するとき
  1. design/BasicSpecification.text  ← システム全体の中での位置づけを確認
  2. api/<対象モジュール>             ← インターフェース仕様を確認
  3. test/EdgeCase_TestCases.md      ← テスト項目を先に確認（TDD）

問題が発生したとき
  project/known_issues.md            ← 既知か確認、なければ追記

テストを追加するとき
  test/Test_Strategy.md              ← どの層（E2E/単体）に属するか判断
  test/EdgeCase_TestCases.md         ← 試験項目を先に定義
  test/TestSpec_E2E.md               ← E2E の場合は判断ロジックも記述
```
