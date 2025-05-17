#include "sandbox.h"

int main(int argc, char *argv[]) {
    // 检查是否有root权限
    if (geteuid() != 0) {
        fprintf(stderr, "此程序需要root权限运行\n");
        return EXIT_FAILURE;
    }

    char *binary_path;
    char *binary_name;
    int using_default = 0;

    if (!is_static_elf(binary_path)) {
        printf("检测到动态链接程序，将自动处理库依赖\n");
    }

    // 处理命令行参数
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }

        // 检查指定的文件是否存在且可执行
        if (!is_executable(argv[1])) {
            fprintf(stderr, "错误: '%s' 不存在或不可执行\n", argv[1]);
            return EXIT_FAILURE;
        }

        binary_path = realpath(argv[1], NULL);
        if (!binary_path) {
            perror("获取文件完整路径失败");
            return EXIT_FAILURE;
        }

        binary_name = strdup(basename(argv[1]));
        if (!binary_name) {
            perror("内存分配失败");
            free(binary_path);
            return EXIT_FAILURE;
        }

        printf("将在沙箱中运行: %s (文件名: %s)\n", binary_path, binary_name);

        // 检查是否是静态链接的ELF文件
        char check_cmd[PATH_MAX + 32];
        snprintf(check_cmd, sizeof(check_cmd), "file %s | grep -q 'statically linked'", binary_path);
        if (system(check_cmd) != 0) {
            printf("警告: 文件可能不是静态链接的，可能在沙箱中无法正常运行\n");
            printf("建议使用 'gcc -static ...' 重新编译程序\n");
        }
    } else {
        // 使用默认的Hello World程序
        using_default = 1;
        binary_path = compile_hello_world();
        if (!binary_path) {
            fprintf(stderr, "编译Hello World程序失败\n");
            return EXIT_FAILURE;
        }
        binary_name = strdup("hello");
        if (!binary_name) {
            perror("内存分配失败");
            free(binary_path);
            return EXIT_FAILURE;
        }
        printf("编译默认的Hello World程序: %s\n", binary_path);
    }

    // 打印文件类型信息
    char file_cmd[PATH_MAX + 16];
    snprintf(file_cmd, sizeof(file_cmd), "file %s", binary_path);
    printf("文件信息: ");
    fflush(stdout);
    system(file_cmd);

    // 准备参数并启动沙箱
    char *arg = malloc(strlen(binary_path) + strlen(binary_name) + 2);
    if (!arg) {
        perror("内存分配失败");
        free(binary_path);
        free(binary_name);
        return EXIT_FAILURE;
    }

    strcpy(arg, binary_path);
    strcpy(arg + strlen(binary_path) + 1, binary_name);

    // 分配子进程栈
    char *stack = malloc(STACK_SIZE);
    if (!stack) {
        perror("栈内存分配失败");
        free(binary_path);
        free(binary_name);
        free(arg);
        return EXIT_FAILURE;
    }

    // 创建带有命名空间的子进程
    int flags = CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | CLONE_NEWNET | SIGCHLD;
    printf("创建带有命名空间的沙箱...\n");
    pid_t pid = clone(child_func, stack + STACK_SIZE, flags, arg);

    if (pid == -1) {
        perror("创建子进程失败");
        free(stack);
        free(binary_path);
        free(binary_name);
        free(arg);
        return EXIT_FAILURE;
    }

    printf("沙箱进程已启动，PID: %d\n", pid);

    // 设置用户空间映射
    char path[PATH_MAX];
    
    // 设置UID映射
    snprintf(path, sizeof(path), "/proc/%d/uid_map", pid);
    FILE *fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "0 %d 1\n", getuid());
        fclose(fp);
    } else {
        perror("写入uid_map失败");
    }

    // 禁用setgroups
    snprintf(path, sizeof(path), "/proc/%d/setgroups", pid);
    fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "deny\n");
        fclose(fp);
    } else {
        perror("写入setgroups失败");
    }

    // 设置GID映射
    snprintf(path, sizeof(path), "/proc/%d/gid_map", pid);
    fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "0 %d 1\n", getgid());
        fclose(fp);
    } else {
        perror("写入gid_map失败");
    }

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
    free(binary_path);
    free(binary_name);
    free(arg);

    return 0;
}