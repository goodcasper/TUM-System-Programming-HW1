// tracer.c  — Linux x86_64 only
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>     // struct user_regs_struct
#include <sys/wait.h>
#include <sys/syscall.h>  // SYS_read, SYS_write, ...
#include <unistd.h>

static inline int is_rw(long nr) {
    return nr == SYS_read || nr == SYS_write;
}

static inline const char *nr2name(long nr) {
    return nr == SYS_read ? "read" :
           nr == SYS_write ? "write" : "?";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <prog> [args...]\n", argv[0]);
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // 子行程：宣告要被追蹤並先停一下等父行程附加
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
            perror("PTRACE_TRACEME");
            _exit(1);
        }
        // 讓父行程有機會設置 options
        raise(SIGSTOP);
        // 以被追蹤身分執行目標程式
        execvp(argv[1], &argv[1]);
        perror("execvp");
        _exit(1);
    }

    // 父行程：tracer
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }

    // 清楚標示 syscall stop（SIGTRAP|0x80）
    if (ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD) == -1) {
        perror("PTRACE_SETOPTIONS");
        return 1;
    }

    // 暫存上一次「進入」syscall 時的資訊，好在「離開」時一起印出
    struct {
        long nr;
        long fd;
        unsigned long long buf;
        unsigned long long count;
        int valid;
    } last = {0};

    // 先讓 tracee 繼續並在 syscall 進出都停下
    if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1) {
        perror("PTRACE_SYSCALL");
        return 1;
    }

    for (;;) {
        if (waitpid(pid, &status, 0) == -1) {
            if (errno == ECHILD) break;  // 子行程已結束
            perror("waitpid");
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            break;  // 行程已結束或被訊號終止
        }

        if (!(WIFSTOPPED(status) && (WSTOPSIG(status) & 0x80))) {
            int sig = 0;
            if (WIFSTOPPED(status)) {
                int st = WSTOPSIG(status);
                // 對於由 ptrace 產生的 SIGTRAP（不含 0x80 的），不要轉回 tracee
                // 其他例如 SIGCHLD、SIGSTOP、SIGALRM…才原樣轉回去
                if (st != SIGTRAP) sig = st;
            }
            if (ptrace(PTRACE_SYSCALL, pid, 0, (void *)(long)sig) == -1) {
                perror("PTRACE_SYSCALL");
                break;
            }
            continue;
        }

        // 這裡是「syscall 進入」與「syscall 離開」都會到
        struct user_regs_struct regs;
        if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
            perror("PTRACE_GETREGS");
            break;
        }

        // x86_64 System V ABI:
        //  - orig_rax = syscall number（進入與離開都是同一個）
        //  - rax      = return value（只有在離開時才是最終回傳值）
        // 參數順序：rdi, rsi, rdx, r10, r8, r9
        long nr = regs.orig_rax;

        if (!last.valid) {
            // 視為「進入」點：若是 read/write，先把參數存起來
            if (is_rw(nr)) {
                last.nr    = nr;
                last.fd    = (long)regs.rdi;
                last.buf   = (unsigned long long)regs.rsi;
                last.count = (unsigned long long)regs.rdx;
                last.valid = 1;
            }
        } else {
            // 視為「離開」點：若同一個 syscall，就輸出
            if (nr == last.nr && is_rw(nr)) {
                long retval = (long)regs.rax;
                // 格式：write(1, 0x7fff..., 12) = 12
                fprintf(stderr, "%s(%ld, 0x%llx, %llu) = %ld\n",
                        nr2name(last.nr),
                        last.fd,
                        (unsigned long long)last.buf,
                        (unsigned long long)last.count,
                        retval);
            }
            last.valid = 0;
        }

        // 繼續跑到下一個 syscall 進出或訊號
        if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1) {
            perror("PTRACE_SYSCALL");
            break;
        }
    }

    return 0;
}
