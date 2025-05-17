#include "sandbox.h"

// Hello world程序的源代码（当未指定ELF文件时使用）
const char* hello_world_c =
"#include <stdio.h>\n"
"int main() {\n"
"    printf(\"在沙箱中运行: 你好，世界！\\n\");\n"
"    return 0;\n"
"}\n";

// 预编译hello world程序并返回其路径
char* compile_hello_world() {
    char template[] = "/tmp/hello-XXXXXX";
    char *dir = mkdtemp(template);
    if (!dir) {
        perror("创建临时目录失败");
        return NULL;
    }

    char src_path[PATH_MAX];
    snprintf(src_path, sizeof(src_path), "%s/hello.c", dir);

    FILE *fp = fopen(src_path, "w");
    if (!fp) {
        perror("创建hello.c源文件失败");
        return NULL;
    }
    fputs(hello_world_c, fp);
    fclose(fp);

    char compile_cmd[PATH_MAX + 64];
    char *binary_path = malloc(PATH_MAX);
    snprintf(binary_path, PATH_MAX, "%s/hello", dir);
    // 确保使用完全静态链接
    snprintf(compile_cmd, sizeof(compile_cmd), "gcc -static -o %s %s", binary_path, src_path);

    if (system(compile_cmd) != 0) {
        perror("编译hello.c失败");
        free(binary_path);
        return NULL;
    }

    return binary_path;
}