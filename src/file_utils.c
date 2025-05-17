#include "sandbox.h"

// 判断文件是否存在且可执行
int is_executable(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return (st.st_mode & S_IXUSR) != 0;
}