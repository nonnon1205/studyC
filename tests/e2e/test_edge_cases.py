import socket
import time
import os

def test_udp_edge_cases():
    """
    EdgeCase_TestCases.md に定義された UDP 境界値テスト
    ASanによるメモリ破壊検知を期待するため、C言語側のプロセスがクラッシュしなければPASSとする。
    """
    udp_ip = "127.0.0.1"
    udp_port = int(os.environ.get("STUDYC_UDP_PORT", 9999))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    try:
        # 1-1: 0バイトのUDPペイロードを受信
        sock.sendto(b"", (udp_ip, udp_port))
        time.sleep(0.1)

        # 1-2: 1023バイトの文字列を受信 (バッファ境界)
        # MAX_PAYLOAD_SIZE = 1024 のため、1023文字 + '\0' が SHMに収まる最大長
        payload_1023 = b"A" * 1023
        sock.sendto(payload_1023, (udp_ip, udp_port))
        time.sleep(0.1)

        # 1-3: 1024バイト以上の文字列を受信 (バッファ超過)
        # 4000バイトの巨大なデータを送りつけ、バッファオーバーフローが起きないか確認
        payload_4000 = b"B" * 4000
        sock.sendto(payload_4000, (udp_ip, udp_port))
        time.sleep(0.1)

        # 1-4: バイナリデータ・制御文字を含むUDPを受信
        payload_binary = b"C" * 100 + b"\x00\x01\x02\x03\xff\xfe" + b"D" * 100
        sock.sendto(payload_binary, (udp_ip, udp_port))
        time.sleep(0.1)

    finally:
        sock.close()