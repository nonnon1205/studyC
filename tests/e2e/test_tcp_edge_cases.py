import socket
import time
import os

def test_tcp_edge_cases(studyc_processes):
    """
    Viewer（TCPサーバー）に対する異常系テスト。
    意図的な切断や連続接続を繰り返し、FD(ファイルディスクリプタ)リークやクラッシュが起きないか確認する。
    """
    tcp_ip = "127.0.0.1"
    tcp_port = int(os.environ.get("STUDYC_TCP_PORT", 7777))
    
    # 起動を少し待つ
    time.sleep(0.5)

    # 3-1: 接続して即切断 (Three-way handshake 直後の RST/FIN エミュレート)
    # これを繰り返すことで、Viewer側の accept() と recv() == 0 のハンドリングを叩く
    for _ in range(10):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(2.0)
        try:
            sock.connect((tcp_ip, tcp_port))
            # 何も送らずに即座に閉じる
        except (ConnectionRefusedError, TimeoutError, OSError):
            pass
        finally:
            sock.close()
        time.sleep(0.05)

    # 3-2: 接続後、データを受信せずに放置して切断
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(2.0)
    try:
        sock.connect((tcp_ip, tcp_port))
        # 繋いだまま少し待機（この間に他の処理がブロックしないかの確認）
        time.sleep(0.3)
    except (ConnectionRefusedError, TimeoutError, OSError):
        pass
    finally:
        sock.close()
    
    # 最後に少し待って、C言語側でASanやメモリリークのエラーが出ないか猶予を与える
    time.sleep(0.2)

    procs = studyc_processes
    for name, proc in procs.items():
        assert proc.poll() is None, f"{name} crashed during TCP edge case tests (exit code: {proc.poll()})"