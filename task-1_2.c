// task-1_2.c — only for x86-64 (64-bit Intel/AMD)

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <errno.h>

/* glibc-style error handling: if retval < 0 → set errno = -retval, return -1 */
static inline ssize_t ret_errno(long r) {
    if (r < 0) {
        errno = -r;
        return -1;
    }
    return (ssize_t)r;
}

ssize_t read(int fd, void *buf, size_t count) {
    if (count == 0) return 0;  // optimization: no syscall if size is zero
    long r;
    asm volatile (
        "syscall"
        : "=a"(r)                                // rax ← return value
        : "0"(SYS_read), "D"(fd), "S"(buf), "d"(count)  // rax=syscall#, rdi, rsi, rdx
        : "rcx", "r11", "memory"                // rcx & r11 are clobbered by syscall
    );
    return ret_errno(r);
}

ssize_t write(int fd, const void *buf, size_t count) {
    if (count == 0) return 0;
    long r;
    asm volatile (
        "syscall"
        : "=a"(r)
        : "0"(SYS_write), "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret_errno(r);
}

/* optional weak aliases for compatibility */
ssize_t __read(int fd, void *buf, size_t count)  __attribute__((weak, alias("read")));
ssize_t __write(int fd, const void *buf, size_t count) __attribute__((weak, alias("write")));
