// tests/test_programs/syscall_test.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

int main() {
    printf("系统调用测试程序启动\n");

    // 文件操作
    int fd = open("/tmp/test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd != -1) {
        const char *data = "这是测试数据\n";
        write(fd, data, strlen(data));
        close(fd);
        printf("写入文件完成\n");
    }

    // 目录操作
    mkdir("/tmp/test_dir", 0755);

    // 获取进程信息
    pid_t pid = getpid();
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    printf("进程ID: %d\n", pid);
    printf("主机名: %s\n", hostname);

    // 内存分配
    void *mem = malloc(1024 * 1024);
    if (mem) {
        memset(mem, 0, 1024 * 1024);
        free(mem);
    }

    // 睡眠
    printf("睡眠1秒\n");
    sleep(1);

    printf("测试完成\n");
    return 0;
}