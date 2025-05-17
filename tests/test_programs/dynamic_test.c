// tests/test_programs/dynamic_test.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    time_t now = time(NULL);
    printf("在沙箱中运行的动态链接程序\n");
    printf("当前时间: %s", ctime(&now));

    // 测试文件IO
    FILE *fp = fopen("/tmp/test.txt", "w");
    if (fp) {
        fprintf(fp, "测试写入文件\n");
        fclose(fp);
        printf("成功写入文件: /tmp/test.txt\n");
    } else {
        printf("无法写入文件: /tmp/test.txt\n");
    }

    return 0;
}