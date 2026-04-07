#define _GNU_SOURCE
#include <unistd.h>        
#include <sys/syscall.h>   
#include <sys/types.h>
#include <errno.h>

ssize_t read(int fd, void *buf, size_t count) {
   
    if (count == 0) { // 避免不必要的處理
        return 0;
    }
   
    return (ssize_t) syscall(SYS_read, fd, buf, count); // 呼叫system call, SYS_read是macro, 這樣寫可以提高擴展性 在不同架構下 SYS_read會變不同數字
}

ssize_t write(int fd, const void *buf, size_t count) {
    if (count == 0) {
        return 0;
    }
    return (ssize_t) syscall(SYS_write, fd, buf, count);
}
