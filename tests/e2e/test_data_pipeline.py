import socket
import subprocess
import time
from conftest import UDP_PORT, TCP_PORT

def _drain(proc, timeout=2.0):
    """プロセスに SIGINT を送って stdout/stderr を回収する"""
    proc.send_signal(2)  # SIGINT
    try:
        out, err = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, err = proc.communicate()
    return (
        out.decode('utf-8', errors='replace'),
        err.decode('utf-8', errors='replace'),
    )

def test_udp_to_tcp_pipeline(studyc_processes):
    """
    [正常系] CollectorにUDPパケットを送信し、Router経由でViewer(TCP)に到達するかをテストする。
    """
    procs = studyc_processes
    test_message = "Hello_CI_Pipeline_Test"

    # 1. UDPパケットをCollectorへ送信
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.sendto(test_message.encode('utf-8'), ('127.0.0.1', UDP_PORT))
    udp_sock.close()

    # 2. パイプラインを通過する時間を待つ
    time.sleep(0.5)

    # 3. Viewer の出力を回収
    viewer_out, viewer_err = _drain(procs["viewer"], timeout=3.0)

    print(f"--- Viewer STDOUT ---\n{viewer_out}")
    print(f"--- Viewer STDERR ---\n{viewer_err}")

    # 4. 診断のために Collector / Router の出力も回収
    collector_out, collector_err = _drain(procs["collector"], timeout=2.0)
    router_out, router_err = _drain(procs["router"], timeout=2.0)

    print(f"--- Collector STDOUT ---\n{collector_out}")
    print(f"--- Collector STDERR ---\n{collector_err}")
    print(f"--- Router STDOUT ---\n{router_out}")
    print(f"--- Router STDERR ---\n{router_err}")

    # 5. アサーション
    assert test_message in viewer_out, (
        f"Expected '{test_message}' in Viewer stdout.\n"
        f"Viewer out: {viewer_out!r}\n"
        f"Collector err: {collector_err!r}\n"
        f"Router err: {router_err!r}"
    )
