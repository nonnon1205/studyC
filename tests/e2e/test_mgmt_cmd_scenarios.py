import subprocess
import json
import os

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
    assert isinstance(data["uptime_s"], int)


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
