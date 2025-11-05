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

static int is_rw(long nr) {
    return nr == SYS_read || nr == SYS_write;
}

static const char *nr2name(long nr) {
    return nr == SYS_read ? "read" :
           nr == SYS_write ? "write" : "?";
}

int main(int argc, char *argv[]) {

    pid_t pid = fork();

    if (pid < 0) { // fork fail
        perror("fork");
        return 1;
    }

    if (pid == 0) { // child process
    
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) { // 讓父process可以來追蹤子process
            perror("PTRACE_TRACEME"); // 失敗時要進行的處理
            _exit(1);
        }

        // 先暫停直到父process呼叫ptrace(syscall)才繼續執行 讓父行程有機會設置 options
        raise(SIGSTOP); // 使process進入stop狀態

        // 以被追蹤身分執行目標程式
        execvp(argv[1], &argv[1]); // 跳去執行目標程式 如果有跳成功就不會執行下面
        perror("execvp"); // 跑到下面代表沒跳成功 輸出錯誤訊息
        _exit(1);
    }

    // 父行程：tracer
    int status;
    if (waitpid(pid, &status, 0) == -1) { // 確認子process進入到暫停狀態 再繼續執行 避免先執行到後面的ptrace(syscall)
        perror("waitpid");
        return 1;
    }

    // 當tracee使用系統呼叫時 系統會發送一個特別的signal給tracer 此signal為SIGTRAP | 0x80（也就是 signal number 第 7 bit 被設為 1）
    if (ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD) == -1) {
        perror("PTRACE_SETOPTIONS");
        return 1;
    }

    // 暫存上一次「進入」syscall 時的資訊，好在「離開」時一起印出
    struct {
        long nr; // sys call number
        long fd; // 檔案描述符
        unsigned long long buf; // buffer 位址
        unsigned long long count; // buffer 長度
        int valid; // 標注是否已經有 進入時的資料
    } last = {0}; // 全部初始化為0

    // 先讓tracee繼續並在進入sys call之前停下
    if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1) {
        perror("PTRACE_SYSCALL");
        return 1;
    }

    for (;;) {
        if (waitpid(pid, &status, 0) == -1) { // 這裡是在等子行程遇到事件被停下來 被停下來就可以繼續執行了
            if (errno == ECHILD) break;  // 子行程已結束
            perror("waitpid");
            break;
        }

        // 如果子process停下來不是因為sys call 就進來這個if
        // WIFSTOPPED判斷子行程是否停下來 
        // WSTOPSIG判斷讓子行程停下來的signal是什麼 如果跟0x80 ＆ 是true代表是 sys call
        if (!(WIFSTOPPED(status) && (WSTOPSIG(status) & 0x80))) {
            int sig = 0;
            if (WIFSTOPPED(status)) {
                int st = WSTOPSIG(status); // 取得停下來的原因
               
                if (st != SIGTRAP) sig = st; 
                // SIGTRAP是由ptrace產生的 所以不該把原訊號傳回給child
                // 如果是其他一般訊號就要傳給child
            }
            // 讓子行程繼續執行 且接受signal (因為前面子行程有被停下來 上面是在檢查停下來的原因)
            if (ptrace(PTRACE_SYSCALL, pid, 0, (void *)(long)sig) == -1) {
                perror("PTRACE_SYSCALL");
                break;
            }
            continue;
        }

        // 這裡是「syscall 進入」與「syscall 離開」都會到
        struct user_regs_struct regs; // 這個結構存放所有x86 cpu暫存器的值
        // 把 pid 這個 child process 的寄存器 (register) 狀態讀出來，存進 regs
        if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
            perror("PTRACE_GETREGS");
            break;
        }

        
        //  - orig_rax = syscall number（進入與離開都是同一個）
        //  - rax      = return value（只有在離開時才是最終回傳值）
        // 參數順序：rdi, rsi, rdx, r10, r8, r9
        long nr = regs.orig_rax;

        if (!last.valid) { // 一開始last所有數都被初始為0
            // 視為「進入」點：若是 read/write，先把參數存起來
            if (is_rw(nr)) {
                last.nr    = nr;
                last.fd    = (long)regs.rdi; // 第一個參數
                last.buf   = (unsigned long long)regs.rsi; // 第二個參數
                last.count = (unsigned long long)regs.rdx;
                last.valid = 1;
            }
        } else {
            // 視為「離開」點：若同一個 syscall，就輸出
            if (nr == last.nr) {
                long retval = (long)regs.rax; // return value
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

        // 子process 繼續執行 但會在下一個sys call呼叫或離開前停下
        if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1) {
            perror("PTRACE_SYSCALL");
            break;
        }
    }

    return 0;
}
