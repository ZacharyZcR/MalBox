// src/namespace.c
#include "sandbox.h"

int setup_user_namespace(pid_t pid) {
    char path[PATH_MAX];
    int success = 1;

    // 设置UID映射
    snprintf(path, sizeof(path), "/proc/%d/uid_map", pid);
    FILE *fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "0 %d 1\n", getuid());
        fclose(fp);
    } else {
        perror("写入uid_map失败");
        success = 0;
    }

    // 禁用setgroups
    snprintf(path, sizeof(path), "/proc/%d/setgroups", pid);
    fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "deny\n");
        fclose(fp);
    } else {
        perror("写入setgroups失败");
        success = 0;
    }

    // 设置GID映射
    snprintf(path, sizeof(path), "/proc/%d/gid_map", pid);
    fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "0 %d 1\n", getgid());
        fclose(fp);
    } else {
        perror("写入gid_map失败");
        success = 0;
    }

    return success ? 0 : -1;
}