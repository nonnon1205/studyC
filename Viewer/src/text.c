#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define BUFFER_SIZE 100
#define MSG_TYPE_TEXT 0

typedef struct{
	int type;
	int body_len;
    char buffer[BUFFER_SIZE];
} MsgData;

int main() {
    int fd[2]; // fd[0] = 読み取り用, fd[1] = 書き込み用
    pid_t pid;
    MsgData msg;

    // パイプ作成
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // プロセス生成
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        // 親プロセス
        close(fd[0]); // 読み取り側は閉じる

        // ★ テストケース1: 通常メッセージ送信
        const char* msg1 ="Hello from parent process!";
        strncpy(msg.buffer, msg1, BUFFER_SIZE - 1);
        msg.buffer[BUFFER_SIZE - 1] = '\0';
        
        msg.type = MSG_TYPE_TEXT;
        msg.body_len = strlen(msg1) + 1;

        if (write(fd[1], &msg, sizeof(msg)) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }

        // ★ テストケース2: 別メッセージ送信（確認用）
        const char* msg2 = "Second message test";

        strncpy(msg.buffer, msg2, BUFFER_SIZE - 1);
        msg.buffer[BUFFER_SIZE - 1] = '\0';
        msg.type = MSG_TYPE_TEXT;
        msg.body_len = strlen(msg.buffer);

        if (write(fd[1], &msg, sizeof(msg)) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }

        close(fd[1]); // 書き込み終了
        wait(NULL);   // 子プロセス終了待ち
    } else {

        int msg_len;
        
        // 子プロセス
        close(fd[1]); // 書き込み側は閉じる

        // ★ 複数メッセージを順に受信
        ssize_t bytesRead;
        while ((bytesRead = read(fd[0], &msg, sizeof(msg))) > 0) {
            printf("Child received: %s\n", msg.buffer);
        }
        if (bytesRead == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        close(fd[0]);
    }

    return 0;
}

