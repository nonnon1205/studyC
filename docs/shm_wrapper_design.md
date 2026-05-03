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

## クリーンアップ責任

`shm_api_close()` / `shm_api_destroy()` 内部で `is_creator` を見て削除を判断する。
呼び出し側はオーナーを意識せず API を呼ぶだけ。

クラッシュ時の残骸は OS（`/dev/shm` は tmpfs、再起動で消える）に委譲する。

## モジュール間のヘッダ共有

`SHM/` と `PosixSHM/` は独立したモジュールであり、共通ヘッダは不要。
各モジュールが独自に `SharedData` を定義する。

ただし両モジュールは同一の共有メモリ領域を読み書きするため、
**`SharedData` のメモリレイアウトは両実装で一致させることが実装上の規約となる**。

## 残作業

- Makefile に `SHM_IMPL` 切り替え対応を追加する
- PosixSHM の `ShmContext` に `is_creator` フラグを追加し `shm_api_destroy` を修正する

## スコープ外

- クラッシュ時のクリーンアップ: OS（tmpfs）に委譲
- 実行時の動的切り替え: 対象外
- MQ のラッパー化: POSIX MQ を学ぶタイミングで別途検討
