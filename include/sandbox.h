#ifndef SANDBOX_H
#define SANDBOX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <limits.h>
#include <linux/limits.h>
#include <libgen.h>

#define STACK_SIZE (1024 * 1024)  // 子进程栈大小

// 沙箱配置结构体
typedef struct {
    char *binary_path;         // 可执行文件路径
    char *binary_name;         // 可执行文件名
    int using_default;         // 是否使用默认程序
    // 可以添加更多配置选项，如网络模式、资源限制等
} sandbox_config;

// ---- 文件工具函数 ----
int is_executable(const char *path);
int is_static_elf(const char *path);
int mkdir_p(const char *path, mode_t mode);
int copy_file(const char *src, const char *dest);

// ---- 动态库处理 ----
int prepare_dynamic_libs(const char *binary_path, const char *sandbox_root);

// ---- Hello World程序相关 ----
extern const char* hello_world_c;
char* compile_hello_world(void);

// ---- 沙箱环境函数 ----
int child_func(void *arg);
int create_sandbox_directories(const char *sandbox_root);
int copy_executable_to_sandbox(const char *binary_path, const char *binary_name,
                              const char *sandbox_root);
int enter_sandbox(const char *sandbox_root);

// ---- 命令行界面函数 ----
void print_usage(const char *program_name);
int parse_arguments(int argc, char *argv[], sandbox_config *config);
void cleanup_config(sandbox_config *config);
void print_file_info(const char *filepath);

// ---- 命名空间函数 ----
int setup_user_namespace(pid_t pid);

// ---- 系统调用监控函数 ----
int setup_monitoring(pid_t child_pid);
int prepare_traced_child(void);

#endif // SANDBOX_H