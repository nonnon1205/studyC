import socket
import time
from conftest import UDP_PORT, TCP_PORT

def test_udp_to_tcp_pipeline(studyc_processes):
    """
    [正常系] CollectorにUDPパケットを送信し、Router経由でViewer(TCP)に到達するかをテストする。
    ※Viewerはコンソール出力しかしない設計のため、ここではViewerプロセスの標準出力をフックして検証する。
    """
    procs = studyc_processes
    test_message = "Hello_CI_Pipeline_Test"
    
    # 1. UDPパケットをCollectorへ送信
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.sendto(test_message.encode('utf-8'), ('127.0.0.1', UDP_PORT))
    udp_sock.close()
    
    # 2. Viewerが標準出力（コンソール）に出力するのを待つ
    time.sleep(0.5) 
    
    # ViewerにSIGINTを送り、バッファをフラッシュさせて出力を取得
    procs["viewer"].send_signal(2) # SIGINT
    try:
        out, err = procs["viewer"].communicate(timeout=2.0)
    except subprocess.TimeoutExpired:
        procs["viewer"].kill()
        out, err = procs["viewer"].communicate()
        
    out_text = out.decode('utf-8', errors='replace')
    err_text = err.decode('utf-8', errors='replace')
    
    # 3. アサーション（期待される結果の検証）
    # Viewerは Router から受け取った "[TCP-RELAY] Hello_CI_Pipeline_Test" のような出力をするはず
    print(f"--- Viewer STDOUT ---\n{out_text}")
    print(f"--- Viewer STDERR ---\n{err_text}")
    
    # ※現状のViewerの出力フォーマットに合わせてアサートする。
    # もしViewerが単に受信データをprintfしているだけなら、以下のようにチェックする
    assert test_message in out_text, f"Expected message '{test_message}' not found in Viewer's output."
    
    # Router が SIGPIPE 等でクラッシュしていないかも確認
    assert procs["router"].poll() is None, "Router process crashed during pipeline execution!"