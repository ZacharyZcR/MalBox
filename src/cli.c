// src/cli.c
#include "sandbox.h"

void print_usage(const char *program_name) {
    printf("用法: %s [ELF文件路径]\n", program_name);
    printf("如果不指定ELF文件，将运行默认的Hello World程序\n");
}

void print_file_info(const char *filepath) {
    char file_cmd[PATH_MAX + 16];
    snprintf(file_cmd, sizeof(file_cmd), "file %s", filepath);
    printf("文件信息: ");
    fflush(stdout);
    system(file_cmd);
}

int parse_arguments(int argc, char *argv[], sandbox_config *config) {
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

        config->binary_path = realpath(argv[1], NULL);
        if (!config->binary_path) {
            perror("获取文件完整路径失败");
            return EXIT_FAILURE;
        }

        config->binary_name = strdup(basename(argv[1]));
        if (!config->binary_name) {
            perror("内存分配失败");
            free(config->binary_path);
            config->binary_path = NULL;
            return EXIT_FAILURE;
        }

        printf("将在沙箱中运行: %s (文件名: %s)\n", config->binary_path, config->binary_name);

        // 检查是否是静态链接的ELF文件
        if (!is_static_elf(config->binary_path)) {
            printf("检测到动态链接程序，将自动处理库依赖\n");
        }
    } else {
        // 使用默认的Hello World程序
        config->using_default = 1;
        config->binary_path = compile_hello_world();
        if (!config->binary_path) {
            fprintf(stderr, "编译Hello World程序失败\n");
            return EXIT_FAILURE;
        }
        config->binary_name = strdup("hello");
        if (!config->binary_name) {
            perror("内存分配失败");
            free(config->binary_path);
            config->binary_path = NULL;
            return EXIT_FAILURE;
        }
        printf("编译默认的Hello World程序: %s\n", config->binary_path);
    }

    return 0;
}

void cleanup_config(sandbox_config *config) {
    if (config->binary_path) {
        free(config->binary_path);
        config->binary_path = NULL;
    }

    if (config->binary_name) {
        free(config->binary_name);
        config->binary_name = NULL;
    }
}