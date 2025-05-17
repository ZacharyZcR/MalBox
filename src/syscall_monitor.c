#include "sandbox.h"
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/reg.h>
#include <asm/unistd.h>

// 保存常见系统调用名称
static const char *syscall_names[] = {
    #ifdef __x86_64__
    [0] = "read",
    [1] = "write",
    [2] = "open",
    [3] = "close",
    [4] = "stat",
    [5] = "fstat",
    [6] = "lstat",
    [7] = "poll",
    [8] = "lseek",
    [9] = "mmap",
    [10] = "mprotect",
    [11] = "munmap",
    [12] = "brk",
    [13] = "rt_sigaction",
    [14] = "rt_sigprocmask",
    [15] = "rt_sigreturn",
    [16] = "ioctl",
    [17] = "pread64",
    [18] = "pwrite64",
    [19] = "readv",
    [20] = "writev",
    [21] = "access",
    [22] = "pipe",
    [23] = "select",
    [24] = "sched_yield",
    [25] = "mremap",
    [26] = "msync",
    [27] = "mincore",
    [28] = "madvise",
    [29] = "shmget",
    [30] = "shmat",
    // 只列出部分常见的系统调用，完整列表太长
    [56] = "clone",
    [57] = "fork",
    [58] = "vfork",
    [59] = "execve",
    [60] = "exit",
    [61] = "wait4",
    [62] = "kill",
    [63] = "uname",
    // 网络相关
    [41] = "socket",
    [42] = "connect",
    [43] = "accept",
    [44] = "sendto",
    [45] = "recvfrom",
    [46] = "sendmsg",
    [47] = "recvmsg",
    [48] = "shutdown",
    [49] = "bind",
    [50] = "listen",
    [51] = "getsockname",
    // 文件操作相关
    [257] = "openat",
    [80] = "mkdir",
    [82] = "rename",
    [83] = "rmdir",
    [84] = "creat",
    [86] = "link",
    [87] = "unlink",
    [88] = "symlink",
    [89] = "readlink",
    [90] = "chmod",
    [92] = "chown",
    #else
    // 32位系统调用表，如果需要支持32位系统
    // ...
    #endif
};

#ifdef __x86_64__
#define SYSCALL_MAX 335  // x86_64系统上大约有335个系统调用
#else
#define SYSCALL_MAX 385  // 32位系统可能有更多
#endif

// 系统调用监控结构
typedef struct {
    pid_t pid;                      // 被监控进程ID
    FILE *log_file;                 // 日志文件
    int syscall_count[SYSCALL_MAX]; // 系统调用计数器
    struct timeval last_entry;      // 上次系统调用进入时间
    long exec_time_us[SYSCALL_MAX]; // 每类系统调用执行时间(微秒)
    int in_syscall;                 // 是否在系统调用中
    int current_syscall;            // 当前系统调用号
    unsigned long args[6];          // 当前系统调用参数
} syscall_monitor_t;

// 返回当前微秒时间戳
static long get_current_time_us() __attribute__((used));
static long get_current_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

// 计算两个时间戳之间的差值（微秒）
static long time_diff_us(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) * 1000000 + (end->tv_usec - start->tv_usec);
}

// 获取系统调用名称
static const char *get_syscall_name(int syscall_nr) {
    if (syscall_nr >= 0 && syscall_nr < SYSCALL_MAX && syscall_names[syscall_nr] != NULL) {
        return syscall_names[syscall_nr];
    }
    return "unknown";
}

// 从进程内存中读取字符串
static void read_string_from_process(pid_t pid, unsigned long addr, char *str, size_t maxlen) {
    size_t i = 0;
    unsigned long tmp;

    while (i < maxlen - 1) {
        tmp = ptrace(PTRACE_PEEKDATA, pid, addr + i, NULL);
        if (errno != 0) {
            str[i] = '\0';
            return;
        }

        // 一次读取8个字节（一个long）
        for (size_t j = 0; j < sizeof(long) && i < maxlen - 1; j++, i++) {
            char c = (char)(tmp & 0xFF);
            str[i] = c;
            if (c == '\0') {
                return;
            }
            tmp >>= 8;
        }
    }
    str[maxlen - 1] = '\0';
}

// 处理系统调用入口
static void handle_syscall_entry(syscall_monitor_t *monitor, struct user_regs_struct *regs) {
    #ifdef __x86_64__
    monitor->current_syscall = regs->orig_rax;
    monitor->args[0] = regs->rdi;
    monitor->args[1] = regs->rsi;
    monitor->args[2] = regs->rdx;
    monitor->args[3] = regs->r10;
    monitor->args[4] = regs->r8;
    monitor->args[5] = regs->r9;
    #else
    // 32位系统的寄存器不同
    // ...
    #endif

    gettimeofday(&monitor->last_entry, NULL);

    // 简单记录系统调用信息
    fprintf(monitor->log_file, "[ENTRY] syscall %d (%s), args: %lx, %lx, %lx, %lx, %lx, %lx\n",
            monitor->current_syscall, get_syscall_name(monitor->current_syscall),
            monitor->args[0], monitor->args[1], monitor->args[2],
            monitor->args[3], monitor->args[4], monitor->args[5]);

    // 特殊处理某些系统调用
    if (monitor->current_syscall == __NR_open || monitor->current_syscall == __NR_openat) {
        char path[PATH_MAX] = {0};
        if (monitor->current_syscall == __NR_open) {
            read_string_from_process(monitor->pid, monitor->args[0], path, PATH_MAX);
        } else { // openat
            read_string_from_process(monitor->pid, monitor->args[1], path, PATH_MAX);
        }
        fprintf(monitor->log_file, "[FILE] Attempting to open: %s\n", path);
    } else if (monitor->current_syscall == __NR_execve) {
        char path[PATH_MAX] = {0};
        read_string_from_process(monitor->pid, monitor->args[0], path, PATH_MAX);
        fprintf(monitor->log_file, "[EXEC] Executing: %s\n", path);
    } else if (monitor->current_syscall == __NR_connect) {
        fprintf(monitor->log_file, "[NET] Attempting to connect, socket fd: %ld\n", monitor->args[0]);
    }
}

// 处理系统调用退出
static void handle_syscall_exit(syscall_monitor_t *monitor, struct user_regs_struct *regs) {
    struct timeval exit_time;
    gettimeofday(&exit_time, NULL);

    long ret = 0;
    #ifdef __x86_64__
    ret = regs->rax;
    #else
    // 32位系统的寄存器不同
    // ...
    #endif

    // 计算执行时间
    long exec_time = time_diff_us(&monitor->last_entry, &exit_time);
    monitor->exec_time_us[monitor->current_syscall] += exec_time;

    // 增加系统调用计数
    monitor->syscall_count[monitor->current_syscall]++;

    // 记录返回值和执行时间
    fprintf(monitor->log_file, "[EXIT] syscall %d (%s), result: %ld, time: %ld us\n",
            monitor->current_syscall, get_syscall_name(monitor->current_syscall),
            ret, exec_time);

    // 特殊处理某些系统调用的返回值
    if ((monitor->current_syscall == __NR_open || monitor->current_syscall == __NR_openat) && ret >= 0) {
        fprintf(monitor->log_file, "[FILE] Successfully opened file, fd: %ld\n", ret);
    } else if (monitor->current_syscall == __NR_connect && ret == 0) {
        fprintf(monitor->log_file, "[NET] Successfully connected\n");
    }
}

// 子进程监控函数 - 在子进程内部调用
int setup_monitoring(pid_t child_pid) {
    // 创建日志文件
    char log_path[PATH_MAX];
    snprintf(log_path, sizeof(log_path), "/tmp/malbox_syscall_%d.log", child_pid);
    FILE *log_file = fopen(log_path, "w");
    if (!log_file) {
        perror("无法创建系统调用日志文件");
        return -1;
    }

    fprintf(log_file, "===== MalBox系统调用监控 =====\n");
    fprintf(log_file, "目标进程: %d\n\n", child_pid);

    // 初始化监控结构
    syscall_monitor_t monitor;
    memset(&monitor, 0, sizeof(monitor));
    monitor.pid = child_pid;
    monitor.log_file = log_file;
    monitor.in_syscall = 0;

    // 等待子进程停止（由于PTRACE_TRACEME）
    waitpid(child_pid, NULL, 0);

    // 设置ptrace选项
    if (ptrace(PTRACE_SETOPTIONS, child_pid, 0,
               PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
               PTRACE_O_TRACEEXEC | PTRACE_O_TRACESYSGOOD) == -1) {
        perror("设置ptrace选项失败");
        fclose(log_file);
        return -1;
    }

    printf("系统调用监控已启动，日志文件: %s\n", log_path);

    // 主监控循环
    int status;
    while (1) {
        // 继续执行直到下一个系统调用
        if (ptrace(PTRACE_SYSCALL, child_pid, 0, 0) == -1) {
            perror("ptrace失败");
            break;
        }

        if (waitpid(child_pid, &status, 0) == -1) {
            perror("waitpid失败");
            break;
        }

        // 检查进程是否退出
        if (WIFEXITED(status)) {
            fprintf(log_file, "\n[INFO] 进程正常退出，状态码: %d\n", WEXITSTATUS(status));
            break;
        }

        if (WIFSIGNALED(status)) {
            fprintf(log_file, "\n[INFO] 进程被信号终止: %d\n", WTERMSIG(status));
            break;
        }

        // 处理系统调用
        if (WIFSTOPPED(status) && WSTOPSIG(status) & 0x80) {
            struct user_regs_struct regs;
            if (ptrace(PTRACE_GETREGS, child_pid, 0, &regs) == -1) {
                perror("获取寄存器失败");
                continue;
            }

            if (monitor.in_syscall) {
                handle_syscall_exit(&monitor, &regs);
            } else {
                handle_syscall_entry(&monitor, &regs);
            }

            monitor.in_syscall = !monitor.in_syscall;
        }
    }

    // 输出系统调用统计信息
    fprintf(log_file, "\n===== 系统调用统计 =====\n");
    for (int i = 0; i < SYSCALL_MAX; i++) {
        if (monitor.syscall_count[i] > 0) {
            fprintf(log_file, "%-20s (#%d): %d 次调用, 总执行时间: %ld us, 平均: %.2f us\n",
                    get_syscall_name(i), i, monitor.syscall_count[i],
                    monitor.exec_time_us[i],
                    (float)monitor.exec_time_us[i] / monitor.syscall_count[i]);
        }
    }

    // 计算不同系统调用的数量
    int unique_syscalls = 0;
    for (int i = 0; i < SYSCALL_MAX; i++) {
        if (monitor.syscall_count[i] > 0) {
            unique_syscalls++;
        }
    }

    fprintf(log_file, "\n系统调用监控完成，共记录 %d 种不同的系统调用\n", unique_syscalls);
    printf("系统调用监控完成，共记录 %d 种不同的系统调用\n", unique_syscalls);

    fclose(log_file);
    return 0;
}

// 准备被追踪的子进程
int prepare_traced_child() {
    // 通知父进程我们准备好被追踪
    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
        perror("ptrace(TRACEME) 失败");
        return -1;
    }

    // 向自己发送SIGSTOP信号，暂停直到父进程准备好监控
    kill(getpid(), SIGSTOP);
    return 0;
}