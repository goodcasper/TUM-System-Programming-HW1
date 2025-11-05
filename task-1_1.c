// rw1.c
#define _GNU_SOURCE
#include <unistd.h>        // read, write prototypes (for signatures)
#include <sys/syscall.h>   // SYS_read, SYS_write
#include <sys/types.h>
#include <errno.h>

// 注意：我們不呼叫 read()/write()，而是呼叫 syscall()，避免遞迴到自己

ssize_t read(int fd, void *buf, size_t count) {
    // 最佳化：count 為 0 -> 不呼叫系統呼叫，直接回 0，且不動 errno
    if (count == 0) {
        return 0;
    }
    // 透過 syscall 直接進入內核，錯誤時會以 -1 回傳並設置 errno（glibc 幫忙）
    return (ssize_t) syscall(SYS_read, fd, buf, count);
}

ssize_t write(int fd, const void *buf, size_t count) {
    if (count == 0) {
        return 0;
    }
    return (ssize_t) syscall(SYS_write, fd, buf, count);
}
