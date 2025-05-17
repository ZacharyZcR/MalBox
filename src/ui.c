#include "sandbox.h"

// 打印使用帮助
void print_usage(const char *program_name) {
    printf("用法: %s [ELF文件路径]\n", program_name);
    printf("如果不指定ELF文件，将运行默认的Hello World程序\n");
}