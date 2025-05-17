#include "sandbox.h"

// 在子进程(沙箱)中运行的函数
int child_func(void *arg) {
    // 参数结构：文件路径 + \0 + 文件名
    char *binary_path = (char *)arg;
    char *binary_name = binary_path + strlen(binary_path) + 1;

    // 创建新的挂载命名空间
    if (unshare(CLONE_NEWNS) == -1) {
        perror("创建挂载命名空间失败");
        return EXIT_FAILURE;
    }

    // 使挂载传播为私有
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        perror("设置挂载传播属性失败");
        return EXIT_FAILURE;
    }

    // 为沙箱创建临时目录
    char sandbox_dir[] = "/tmp/sandbox-XXXXXX";
    char *dir = mkdtemp(sandbox_dir);
    if (!dir) {
        perror("创建沙箱临时目录失败");
        return EXIT_FAILURE;
    }

    printf("创建沙箱目录: %s\n", dir);

    // 挂载tmpfs作为沙箱根目录
    if (mount("none", dir, "tmpfs", 0, "size=50M") == -1) {
        perror("挂载tmpfs失败");
        return EXIT_FAILURE;
    }

    // 创建基本目录结构
    char path[PATH_MAX];

    // 创建必要的目录结构
    // 创建/bin目录
    snprintf(path, sizeof(path), "%s/bin", dir);
    if (mkdir(path, 0755) == -1) {
        printf("创建bin目录失败: %s (错误码: %d)\n", strerror(errno), errno);
        return EXIT_FAILURE;
    }

    // 创建/lib和/lib64目录
    snprintf(path, sizeof(path), "%s/lib", dir);
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        printf("创建lib目录失败: %s\n", strerror(errno));
    }

    snprintf(path, sizeof(path), "%s/lib64", dir);
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        printf("创建lib64目录失败: %s\n", strerror(errno));
    }

    // 创建/tmp目录
    snprintf(path, sizeof(path), "%s/tmp", dir);
    if (mkdir(path, 0777) == -1 && errno != EEXIST) {
        printf("创建tmp目录失败: %s\n", strerror(errno));
    }

    // 创建/dev目录
    snprintf(path, sizeof(path), "%s/dev", dir);
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        printf("创建dev目录失败: %s\n", strerror(errno));
    }

    // 复制可执行文件到沙箱
    char dest_path[PATH_MAX];
    snprintf(dest_path, sizeof(dest_path), "%s/bin/%s", dir, binary_name);
    printf("复制文件到: %s\n", dest_path);

    // 文件复制和权限设置
    int src_fd = open(binary_path, O_RDONLY);
    if (src_fd == -1) {
        perror("打开源文件失败");
        return EXIT_FAILURE;
    }

    int dest_fd = open(dest_path, O_WRONLY | O_CREAT, 0755);
    if (dest_fd == -1) {
        perror("创建目标文件失败");
        close(src_fd);
        return EXIT_FAILURE;
    }

    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        if (write(dest_fd, buffer, bytes_read) != bytes_read) {
            perror("写入文件失败");
            close(src_fd);
            close(dest_fd);
            return EXIT_FAILURE;
        }
    }

    close(src_fd);
    close(dest_fd);

    // 检查文件是否成功复制和可执行
    struct stat st;
    if (stat(dest_path, &st) != 0) {
        printf("复制后无法获取文件状态: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("文件复制成功 - 大小: %ld 字节, 权限: %o\n", st.st_size, st.st_mode & 0777);

    if (!(st.st_mode & S_IXUSR)) {
        printf("警告: 目标文件不可执行，设置执行权限\n");
        if (chmod(dest_path, 0755) != 0) {
            perror("设置执行权限失败");
            return EXIT_FAILURE;
        }
    }

    // *** 新增代码：处理动态库依赖 ***
    printf("检查程序类型并处理依赖...\n");
    prepare_dynamic_libs(binary_path, dir);

    // 切换根目录并执行程序
    printf("切换到目录: %s\n", dir);
    if (chdir(dir) == -1) {
        perror("切换目录失败");
        return EXIT_FAILURE;
    }

    // chroot到沙箱目录
    printf("执行chroot到: %s\n", dir);
    if (chroot(dir) == -1) {
        perror("chroot失败");
        return EXIT_FAILURE;
    }

    // 在chroot内部更改目录到根目录
    if (chdir("/") == -1) {
        perror("切换到根目录失败");
        return EXIT_FAILURE;
    }

    // 显示目录内容
    printf("沙箱环境中的/bin目录内容:\n");
    system("ls -la /bin");

    // 执行程序
    char exec_path[PATH_MAX];
    snprintf(exec_path, sizeof(exec_path), "/bin/%s", binary_name);

    if (access(exec_path, X_OK) == -1) {
        printf("无法访问执行文件: %s (%s)\n", exec_path, strerror(errno));
        printf("尝试列出/bin目录内容:\n");
        system("ls -la /bin || echo '无法列出目录'");
    } else {
        printf("执行文件存在且可执行: %s\n", exec_path);
    }

    printf("在沙箱中执行程序: %s\n", exec_path);

    // 开启ptrace跟踪
    if (prepare_traced_child() == -1) {
        printf("设置跟踪失败: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("程序已准备好被跟踪\n");

    // 执行程序
    if (execl(exec_path, binary_name, NULL) == -1) {
        printf("执行程序失败: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;  // 不应该到达这里
}