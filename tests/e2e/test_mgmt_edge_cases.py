import socket
import struct
import time
import os

def test_mgmt_edge_cases():
    """
    Mgmtソケット（UNIXドメインソケット）に対する不正なリクエスト送信テスト。
    ASanでクラッシュ（バッファオーバーフローやパース時のセグフォ）が起きないか確認する。
    """
    # 今回は Collector の Mgmt ソケットを標的とする
    sock_path = "/tmp/studyc_collector.sock"
    
    # プロセスの起動完了を少し待つ
    time.sleep(0.5)
    
    if not os.path.exists(sock_path):
        # ソケットファイルがまだなければスキップ (E2E環境に依存)
        return

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

    try:
        # 2-1: 構造体サイズ未満の短すぎるデータ
        sock.sendto(b"SHORT_GARBAGE", sock_path)
        time.sleep(0.1)

        # 2-2: 未定義のコマンドタイプ (cmd_type = 255)
        # MgmtCommandRequest のヘッダ: request_id(I), cmd_type(B), flags(B), payload_len(H)
        bad_cmd_header = struct.pack("<IBBH", 1, 255, 0, 0)
        # 残りのパディングを埋めて送信
        pad = b"\x00" * (600 - len(bad_cmd_header))
        sock.sendto(bad_cmd_header + pad, sock_path)
        time.sleep(0.1)

        # 2-3: ペイロードに不正な文字列 (JSONではないゴミ)
        # SET_LOG_LEVEL (cmd_type=1) を指定しつつ、ペイロードを破壊
        invalid_json_header = struct.pack("<IBBH", 2, 1, 0, 10)
        timestamp = b"\x00" * 16
        target = b"collector".ljust(64, b"\x00")
        payload = b"{not_json}".ljust(512, b"\x00")
        sock.sendto(invalid_json_header + timestamp + target + payload, sock_path)
        time.sleep(0.1)

        # 2-4: 超巨大なデータ (バッファサイズ越え)
        sock.sendto(b"X" * 4096, sock_path)
        time.sleep(0.1)

    finally:
        sock.close()