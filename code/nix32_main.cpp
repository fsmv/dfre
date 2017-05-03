// Copyright (c) 2016-2017 Andrew Kallmeyer <fsmv@sapium.net>
// Provided under the MIT License: https://mit-license.org

#include <stdint.h>
#include <sys/syscall.h>

extern "C"
uint32_t syscall3(uint32_t call, void *arg1, void *arg2, void *arg3);
uint32_t write(int fd, const void *buf, uint32_t count) {
    return syscall3(SYS_write, (void*)fd, (void*)buf, (void*)count);
}

int main(int argc, char *argv[]) {
    char Message[] = "Hello, World!\n";
    write(0, Message, sizeof(Message) - 1);
    return 0;
}
