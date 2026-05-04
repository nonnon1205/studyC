import subprocess
import json
import os
import time

MGMTCTL = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "../../MgmtCtl/mgmtctl")
)


def _run(args):
    return subprocess.run(
        [MGMTCTL] + args,
        capture_output=True,
        text=True,
        timeout=10,
    )


# 5-1 / 5-2 / 5-3: ping — 死活確認
def test_ping_collector(studyc_processes):
    r = _run(["collector", "ping"])
    assert r.returncode == 0
    assert "PING -> OK" in r.stdout
    assert "pong" in r.stdout


def test_ping_router(studyc_processes):
    r = _run(["router", "ping"])
    assert r.returncode == 0
    assert "PING -> OK" in r.stdout
    assert "pong" in r.stdout


def test_ping_viewer(studyc_processes):
    r = _run(["viewer", "ping"])
    assert r.returncode == 0
    assert "PING -> OK" in r.stdout
    assert "pong" in r.stdout


# 5-4 / 5-5 / 5-6: status — JSON ステータス取得
def _assert_status_ok(r):
    assert r.returncode == 0
    assert "GET_STATUS -> OK" in r.stdout
    # 2行目がJSONペイロード
    payload_line = r.stdout.splitlines()[1]
    data = json.loads(payload_line)
    assert data["status"] == "running"
    assert isinstance(data["uptime_s"], int) and data["uptime_s"] >= 0


def test_status_collector(studyc_processes):
    _assert_status_ok(_run(["collector", "status"]))


def test_status_router(studyc_processes):
    _assert_status_ok(_run(["router", "status"]))


def test_status_viewer(studyc_processes):
    _assert_status_ok(_run(["viewer", "status"]))


# 5-7: 不正なモジュール名 — ソケット通信前にエラー終了
def test_unknown_module(studyc_processes):
    r = _run(["unknown_module", "ping"])
    assert r.returncode != 0
    assert "Unknown module:" in r.stderr


# 5-11: 正しいモジュール名 + 不正なコマンド名 — ソケット通信前にエラー終了
def test_unknown_command(studyc_processes):
    r = _run(["collector", "unknown_cmd"])
    assert r.returncode != 0
    assert "Unknown command:" in r.stderr


# 5-8 / 5-9 / 5-10: shutdown — グレースフルシャットダウン
def _wait_for_exit(proc, timeout=3.0):
    """プロセスが終了するまでポーリングする。タイムアウトしたら False を返す。"""
    deadline = time.time() + timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            return True
        time.sleep(0.05)
    return False


def _assert_shutdown_ok(r, proc):
    assert r.returncode == 0
    assert "SHUTDOWN -> OK" in r.stdout
    assert "shutdown initiated" in r.stdout
    assert _wait_for_exit(proc), "process did not exit after shutdown command"


def test_shutdown_collector(studyc_processes):
    _assert_shutdown_ok(_run(["collector", "shutdown"]), studyc_processes["collector"])


def test_shutdown_router(studyc_processes):
    _assert_shutdown_ok(_run(["router", "shutdown"]), studyc_processes["router"])


def test_shutdown_viewer(studyc_processes):
    _assert_shutdown_ok(_run(["viewer", "shutdown"]), studyc_processes["viewer"])
