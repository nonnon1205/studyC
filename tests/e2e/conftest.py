import pytest
import subprocess
import socket
import os
import time
import signal

UDP_PORT = int(os.environ.get("TEST_UDP_PORT", 9999))
TCP_PORT = int(os.environ.get("TEST_TCP_PORT", 7777))
IPC_KEY_HEX = os.environ.get("TEST_IPC_KEY", "0x54321")
IPC_KEY_DEC = str(int(IPC_KEY_HEX, 16))
SHM_KEY_HEX = "0x67890"   # SHM/src/shm_common.h の SHM_KEY

SHM_NAME = "/studyc_shm"
UNIX_SOCKETS = [
    "/tmp/studyc_collector.sock",
    "/tmp/studyc_router.sock",
    "/tmp/studyc_viewer.sock"
]

def wait_for_tcp_port(port, timeout=5.0):
    """TCP ポートが listen 状態になるまで待つ。タイムアウトしたら False を返す。"""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                return True
        except (ConnectionRefusedError, socket.timeout, OSError):
            time.sleep(0.05)
    return False

def force_cleanup():
    """OSコマンドを使って強制的にリソースを掃除する"""
    for sock in UNIX_SOCKETS:
        if os.path.exists(sock):
            os.remove(sock)

    posix_shm_path = f"/dev/shm{SHM_NAME}"
    if os.path.exists(posix_shm_path):
        os.remove(posix_shm_path)

    try:
        subprocess.run(f"ipcrm -Q {IPC_KEY_HEX}", shell=True, stderr=subprocess.DEVNULL)
        # SHM のキーは MQ と別 (0x67890)
        subprocess.run(f"ipcrm -M {SHM_KEY_HEX}", shell=True, stderr=subprocess.DEVNULL)
    except Exception:
        pass

def check_for_leaks():
    """リソースがリークしていないか検査する。リークがあれば詳細文字列を返す"""
    leaks = []

    for sock in UNIX_SOCKETS:
        if os.path.exists(sock):
            leaks.append(f"UNIX Socket leaked: {sock}")

    if os.path.exists(f"/dev/shm{SHM_NAME}"):
        leaks.append(f"POSIX SHM leaked: {SHM_NAME}")

    try:
        out = subprocess.check_output("ipcs -q -m", shell=True, universal_newlines=True)
        if IPC_KEY_HEX in out or IPC_KEY_DEC in out:
            leaks.append(f"SystemV IPC (MQ/SHM) leaked for key {IPC_KEY_HEX}")
    except subprocess.CalledProcessError:
        pass

    return leaks

def _terminate_proc(proc):
    """プロセスに SIGINT → wait → タイムアウトなら SIGKILL の順に終了させる"""
    if proc.poll() is not None:
        return
    proc.send_signal(signal.SIGINT)
    try:
        proc.wait(timeout=3.0)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()

@pytest.fixture(scope="session", autouse=True)
def manage_resources():
    """テストセッション全体の開始・終了処理"""
    force_cleanup()

    yield

    leaks = check_for_leaks()
    force_cleanup()

    if leaks:
        pytest.fail("Resource Leak Detected!\n" + "\n".join(leaks))

@pytest.fixture(scope="function")
def studyc_processes():
    """各テストケースごとに Collector, Router, Viewer を起動・停止するフィクスチャ"""
    base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))

    env = os.environ.copy()
    env["STUDYC_UDP_PORT"] = str(UDP_PORT)
    env["STUDYC_TCP_PORT"] = str(TCP_PORT)

    # stdin=subprocess.PIPE: 子プロセスに EOF stdin が渡るのを防ぐ
    viewer_proc = subprocess.Popen(
        [f"{base_dir}/Viewer/Viewer"], env=env,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )

    # Viewer の TCP サーバーが listen 状態になってから Router を起動する
    if not wait_for_tcp_port(TCP_PORT, timeout=5.0):
        _terminate_proc(viewer_proc)
        pytest.fail(f"Viewer did not start listening on TCP port {TCP_PORT} within 5s")

    router_proc = subprocess.Popen(
        [f"{base_dir}/Router/Router"], env=env,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    time.sleep(0.3)  # Router が SHM/MQ を初期化するのを待つ

    collector_proc = subprocess.Popen(
        [f"{base_dir}/Collector/Collector"], env=env,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    time.sleep(0.3)  # Collector が UDP ソケットをバインドするのを待つ

    yield {
        "collector": collector_proc,
        "router": router_proc,
        "viewer": viewer_proc
    }

    for proc in [collector_proc, router_proc, viewer_proc]:
        _terminate_proc(proc)
