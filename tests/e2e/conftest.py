import pytest
import subprocess
import os
import time
import signal

# 提案2: 環境変数でオーバーライド可能な設計
UDP_PORT = int(os.environ.get("TEST_UDP_PORT", 9999))
TCP_PORT = int(os.environ.get("TEST_TCP_PORT", 7777))
IPC_KEY_HEX = os.environ.get("TEST_IPC_KEY", "0x54321")
IPC_KEY_DEC = str(int(IPC_KEY_HEX, 16)) # ipcs コマンドは10進数で出力されることがあるため

SHM_NAME = "/studyc_shm" # POSIX SHM用
UNIX_SOCKETS = [
    "/tmp/studyc_collector.sock",
    "/tmp/studyc_router.sock",
    "/tmp/studyc_viewer.sock"
]

def force_cleanup():
    """OSコマンドを使って強制的にリソースを掃除する"""
    # UNIXドメインソケットの削除
    for sock in UNIX_SOCKETS:
        if os.path.exists(sock):
            os.remove(sock)
            
    # POSIX SHMの削除
    posix_shm_path = f"/dev/shm{SHM_NAME}"
    if os.path.exists(posix_shm_path):
        os.remove(posix_shm_path)
        
    # SystemV IPC (MQ / SHM) の削除
    try:
        # MQ
        subprocess.run(f"ipcrm -Q {IPC_KEY_HEX}", shell=True, stderr=subprocess.DEVNULL)
        # SHM
        subprocess.run(f"ipcrm -M {IPC_KEY_HEX}", shell=True, stderr=subprocess.DEVNULL)
    except Exception:
        pass

def check_for_leaks():
    """リソースがリークしていないか検査する。リークがあれば詳細文字列を返す"""
    leaks = []
    
    # 1. UNIXソケット
    for sock in UNIX_SOCKETS:
        if os.path.exists(sock):
            leaks.append(f"UNIX Socket leaked: {sock}")
            
    # 2. POSIX SHM
    if os.path.exists(f"/dev/shm{SHM_NAME}"):
        leaks.append(f"POSIX SHM leaked: {SHM_NAME}")
        
    # 3. SystemV IPC (ipcs コマンドで確認)
    try:
        out = subprocess.check_output("ipcs -q -m", shell=True, universal_newlines=True)
        if IPC_KEY_HEX in out or IPC_KEY_DEC in out:
            leaks.append(f"SystemV IPC (MQ/SHM) leaked for key {IPC_KEY_HEX}")
    except subprocess.CalledProcessError:
        pass
        
    return leaks

@pytest.fixture(scope="session", autouse=True)
def manage_resources():
    """テストセッション全体の開始・終了処理"""
    # 1. テスト開始前の準備（前回のクラッシュ残骸を消す）
    force_cleanup()
    
    yield # ここでテスト群が実行される
    
    # 2. テスト終了後のリーク検出（提案1の解決策）
    leaks = check_for_leaks()
    
    # 3. 次回のために掃除しておく
    force_cleanup()
    
    # 4. リークがあればアサーションエラーとしてテスト全体を失敗させる
    if leaks:
        pytest.fail("Resource Leak Detected!\n" + "\n".join(leaks))

@pytest.fixture(scope="function")
def studyc_processes():
    """各テストケースごとに Collector, Router, Viewer を起動・停止するフィクスチャ"""
    base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
    
    # プロセス起動 (環境変数を渡す)
    env = os.environ.copy()
    
    # ※現状はハードコードされているが、将来Phase 2でC言語側が環境変数を読むようになる
    env["UDP_PORT"] = str(UDP_PORT) 
    
    viewer_proc = subprocess.Popen([f"{base_dir}/Viewer/Viewer"], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.1) # TCPサーバーが立ち上がるのを少し待つ
    
    router_proc = subprocess.Popen([f"{base_dir}/Router/Router"], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.1) # SHM/MQの起動を少し待つ
    
    collector_proc = subprocess.Popen([f"{base_dir}/Collector/Collector"], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.2) # 全体が繋がるのを待つ
    
    yield {
        "collector": collector_proc,
        "router": router_proc,
        "viewer": viewer_proc
    }
    
    # 終了処理 (SIGINTを送って正常終了させ、クリーンアップロジックが働くか確認する)
    for proc in [collector_proc, router_proc, viewer_proc]:
        if proc.poll() is None: # まだ動いていれば
            proc.send_signal(signal.SIGINT)
            proc.wait(timeout=2.0)