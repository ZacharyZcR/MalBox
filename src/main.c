// src/main.c
#include "sandbox.h"

int main(int argc, char *argv[]) {
    // 检查是否有root权限
    if (geteuid() != 0) {
        fprintf(stderr, "此程序需要root权限运行\n");
        return EXIT_FAILURE;
    }

    // 解析命令行参数并准备执行环境
    sandbox_config config;
    memset(&config, 0, sizeof(config));

    // 处理命令行参数
    int ret = parse_arguments(argc, argv, &config);
    if (ret != 0) {
        return ret; // 参数处理中已经输出了错误或帮助信息
    }

    // 打印文件类型信息
    print_file_info(config.binary_path);

    // 分配子进程栈
    char *stack = malloc(STACK_SIZE);
    if (!stack) {
        perror("栈内存分配失败");
        cleanup_config(&config);
        return EXIT_FAILURE;
    }

    // 创建带有命名空间的子进程
    int flags = CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | CLONE_NEWNET | SIGCHLD;
    printf("创建带有命名空间的沙箱...\n");
    pid_t pid = clone(child_func, stack + STACK_SIZE, flags, &config);

    if (pid == -1) {
        perror("创建子进程失败");
        free(stack);
        cleanup_config(&config);
        return EXIT_FAILURE;
    }

    printf("沙箱进程已启动，PID: %d\n", pid);

    // 设置用户命名空间映射
    if (setup_user_namespace(pid) != 0) {
        printf("警告: 用户命名空间设置不完整\n");
    }

    // 启动系统调用监控
    printf("启动系统调用监控...\n");
    setup_monitoring(pid);

    // 等待子进程
    int status;
    printf("等待沙箱进程完成...\n");
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        printf("沙箱进程退出，状态码: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("沙箱进程被信号终止: %d\n", WTERMSIG(status));
    }

    // 清理资源
    free(stack);
    cleanup_config(&config);

    return 0;
}