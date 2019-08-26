#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>


namespace util {  namespace serial {

int     uart_open(const char* port);
void    uart_close(int fd);
int     uart_set(int fd, int speed, int databits, int stopbits, char parity, int flow_ctrl, unsigned int tmout_ms=500);
int     uart_send(int fd, char* buf, int len, unsigned int tmout_ms);
int     uart_recv(int fd, char* buf, int len, unsigned int tmout_ms);
void    uart_flush(int fd);

}}
