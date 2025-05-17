#ifndef SANDBOX_H
#define SANDBOX_H

#define _GNU_SOURCE
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

// 文件工具函数
int is_executable(const char *path);

// hello world程序相关
extern const char* hello_world_c;
char* compile_hello_world(void);

// 沙箱环境函数
int child_func(void *arg);

// 辅助函数
void print_usage(const char *program_name);

// 添加到sandbox.h
int mkdir_p(const char *path, mode_t mode);
int copy_file(const char *src, const char *dest);
int is_static_elf(const char *path);
int prepare_dynamic_libs(const char *binary_path, const char *sandbox_root);

#endif // SANDBOX_H