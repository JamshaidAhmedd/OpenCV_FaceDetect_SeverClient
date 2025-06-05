// protocol.cpp
#include "protocol.h"
#include <sys/types.h>
#include <sys/socket.h>

bool send_all(int sockfd, const char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(sockfd, buf + total, len - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

bool recv_all(int sockfd, char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = recv(sockfd, buf + total, len - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}
