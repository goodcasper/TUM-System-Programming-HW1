#define _GNU_SOURCE
#include <unistd.h>        
#include <sys/syscall.h>   
#include <sys/types.h>
#include <errno.h>



ssize_t read(int fd, void *buf, size_t count) {
   
    if (count == 0) {
        return 0;
    }
   
    return (ssize_t) syscall(SYS_read, fd, buf, count);
}

ssize_t write(int fd, const void *buf, size_t count) {
    if (count == 0) {
        return 0;
    }
    return (ssize_t) syscall(SYS_write, fd, buf, count);
}
