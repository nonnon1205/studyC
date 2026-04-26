# TestMsgRcv を Memcheck で起動するコマンド
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$1
