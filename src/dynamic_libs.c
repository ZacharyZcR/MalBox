// src/dynamic_libs.c
#include "sandbox.h"
#include <sys/stat.h>
#include <libgen.h>

// 递归创建目录
int mkdir_p(const char *path, mode_t mode) {
    char *tmp = strdup(path);
    char *p = tmp;
    int ret = 0;

    // 跳过开头的斜杠
    if (*p == '/') p++;

    while (*p != '\0') {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
                ret = -1;
                break;
            }
            *p = '/';
        }
        p++;
    }

    if (ret == 0) {
        if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
            ret = -1;
        }
    }

    free(tmp);
    return ret;
}

// 复制文件
int copy_file(const char *src, const char *dest) {
    int fd_src = open(src, O_RDONLY);
    if (fd_src == -1) {
        printf("无法打开源文件 %s: %s\n", src, strerror(errno));
        return -1;
    }

    // 创建目标文件的目录结构
    char *dest_dir = strdup(dest);
    char *dir = dirname(dest_dir);
    mkdir_p(dir, 0755);
    free(dest_dir);

    int fd_dest = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (fd_dest == -1) {
        printf("无法创建目标文件 %s: %s\n", dest, strerror(errno));
        close(fd_src);
        return -1;
    }

    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(fd_src, buffer, sizeof(buffer))) > 0) {
        if (write(fd_dest, buffer, bytes_read) != bytes_read) {
            printf("写入文件失败: %s\n", strerror(errno));
            close(fd_src);
            close(fd_dest);
            return -1;
        }
    }

    close(fd_src);
    close(fd_dest);
    return 0;
}

// 检查文件是否是静态链接的ELF
int is_static_elf(const char *path) {
    FILE *fp;
    char cmd[PATH_MAX + 32];
    snprintf(cmd, sizeof(cmd), "file %s | grep -q 'statically linked'", path);

    fp = popen(cmd, "r");
    if (fp == NULL) return 0;

    int status = pclose(fp);
    return WEXITSTATUS(status) == 0;
}

// 解析动态库依赖并复制到沙箱
int prepare_dynamic_libs(const char *binary_path, const char *sandbox_root) {
    // 如果是静态链接的程序，无需处理
    if (is_static_elf(binary_path)) {
        printf("检测到静态链接程序，跳过库依赖处理\n");
        return 0;
    }

    printf("检测动态库依赖...\n");
    char temp_file[] = "/tmp/sandbox_libs_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1) {
        perror("创建临时文件失败");
        return -1;
    }
    close(fd);

    // 使用ldd查找依赖库
    char cmd[PATH_MAX * 2];
    snprintf(cmd, sizeof(cmd), "ldd %s | grep -o '/[^ ]*' > %s", binary_path, temp_file);
    int ret = system(cmd);
    if (ret != 0) {
        printf("无法获取程序依赖: %s\n", strerror(errno));
        unlink(temp_file);
        return -1;
    }

    // 读取依赖库列表并复制到沙箱
    FILE *fp = fopen(temp_file, "r");
    if (!fp) {
        perror("打开依赖列表失败");
        unlink(temp_file);
        return -1;
    }

    int lib_count = 0;
    char lib_path[PATH_MAX];
    while (fgets(lib_path, sizeof(lib_path), fp)) {
        // 去除换行符
        lib_path[strcspn(lib_path, "\n")] = 0;
        if (strlen(lib_path) == 0) continue;

        // 复制库文件到沙箱，保持路径结构
        char dest[PATH_MAX];
        snprintf(dest, sizeof(dest), "%s%s", sandbox_root, lib_path);

        printf("复制依赖库: %s -> %s\n", lib_path, dest);
        if (copy_file(lib_path, dest) == 0) {
            lib_count++;
        }

        // 如果是动态链接器，可能需要特殊处理
        if (strstr(lib_path, "/ld-linux") || strstr(lib_path, "/ld-musl") || strstr(lib_path, "/ld.so")) {
            printf("检测到动态链接器: %s\n", lib_path);

            // 某些系统可能使用动态链接器的特定路径
            // 例如，一些标准位置为/lib64/ld-linux-x86-64.so.2或/lib/ld-linux.so.2
            const char *standard_paths[] = {
                "/lib64/ld-linux-x86-64.so.2",
                "/lib/ld-linux.so.2",
                "/lib/ld-musl-x86_64.so.1",
                // 可能的其他位置...
                NULL
            };

            for (int i = 0; standard_paths[i] != NULL; i++) {
                if (access(standard_paths[i], F_OK) == 0 && strcmp(lib_path, standard_paths[i]) != 0) {
                    char std_dest[PATH_MAX];
                    snprintf(std_dest, sizeof(std_dest), "%s%s", sandbox_root, standard_paths[i]);

                    printf("创建动态链接器符号链接: %s -> %s\n", lib_path, standard_paths[i]);

                    // 确保目标目录存在
                    char *dir_path = strdup(std_dest);
                    mkdir_p(dirname(dir_path), 0755);
                    free(dir_path);

                    // 创建符号链接
                    symlink(lib_path, std_dest);
                }
            }
        }
    }

    fclose(fp);
    unlink(temp_file);

    printf("成功复制 %d 个依赖库到沙箱环境\n", lib_count);
    return lib_count;
}