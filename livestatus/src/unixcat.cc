// +------------------------------------------------------------------+
// |             ____ _               _        __  __ _  __           |
// |            / ___| |__   ___  ___| | __   |  \/  | |/ /           |
// |           | |   | '_ \ / _ \/ __| |/ /   | |\/| | ' /            |
// |           | |___| | | |  __/ (__|   <    | |  | | . \            |
// |            \____|_| |_|\___|\___|_|\_\___|_|  |_|_|\_\           |
// |                                                                  |
// | Copyright Mathias Kettner 2014             mk@mathias-kettner.de |
// +------------------------------------------------------------------+
//
// This file is part of Check_MK.
// The official homepage is at http://mathias-kettner.de/check_mk.
//
// check_mk is free software;  you can redistribute it and/or modify it
// under the  terms of the  GNU General Public License  as published by
// the Free Software Foundation in version 2.  check_mk is  distributed
// in the hope that it will be useful, but WITHOUT ANY WARRANTY;  with-
// out even the implied warranty of  MERCHANTABILITY  or  FITNESS FOR A
// PARTICULAR PURPOSE. See the  GNU General Public License for more de-
// tails. You should have  received  a copy of the  GNU  General Public
// License along with GNU Make; see the file  COPYING.  If  not,  write
// to the Free Software Foundation, Inc., 51 Franklin St,  Fifth Floor,
// Boston, MA 02110-1301 USA.

#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>  // IWYU pragma: keep
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cinttypes>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int copy_data(int from, int to);
void *voidp;

struct thread_info {
    int from;
    int to;
    int should_shutdown;
    int terminate_on_read_eof;
};

ssize_t read_with_timeout(int from, char *buffer, int size, int us) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(from, &fds);
    struct timeval tv;
    tv.tv_sec = us / 1000000;
    tv.tv_usec = us % 1000000;
    int retval = select(from + 1, &fds, nullptr, nullptr, &tv);
    if (retval > 0) {
        return read(from, buffer, size);
    }
    return -2;
}

void *copy_thread(void *info) {
    // https://llvm.org/bugs/show_bug.cgi?id=29089
    signal(SIGWINCH, SIG_IGN);  // NOLINT

    struct thread_info *ti = reinterpret_cast<struct thread_info *>(info);
    int from = ti->from;
    int to = ti->to;

    char read_buffer[65536];
    while (true) {
        ssize_t r =
            read_with_timeout(from, read_buffer, sizeof(read_buffer), 1000000);
        if (r == -1) {
            fprintf(stderr, "Error reading from %d: %s\n", from,
                    strerror(errno));
            break;
        } else if (r == 0) {
            if (ti->should_shutdown != 0) {
                shutdown(to, SHUT_WR);
            }
            if (ti->terminate_on_read_eof != 0) {
                exit(0);
                return voidp;
            }
            break;
        } else if (r == -2) {
            r = 0;
        }

        const char *buffer = read_buffer;
        size_t bytes_to_write = r;
        while (bytes_to_write > 0) {
            ssize_t bytes_written = write(to, buffer, bytes_to_write);
            if (bytes_written == -1) {
                uintmax_t b = bytes_to_write;
                fprintf(stderr,
                        "Error: Cannot write %" PRIuMAX " bytes to %d: %s\n", b,
                        to, strerror(errno));
                break;
            }
            buffer += bytes_written;
            bytes_to_write -= bytes_written;
        }
    }
    return voidp;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s UNIX-socket\n", argv[0]);
        exit(1);
    }

    // https://llvm.org/bugs/show_bug.cgi?id=29089
    signal(SIGWINCH, SIG_IGN);  // NOLINT

    const char *unixpath = argv[1];
    struct stat st;

    if (0 != stat(unixpath, &st)) {
        fprintf(stderr, "No UNIX socket %s existing\n", unixpath);
        exit(2);
    }

    int sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "Cannot create client socket: %s\n", strerror(errno));
        exit(3);
    }

    /* Connect */
    struct sockaddr_un sockaddr;
    sockaddr.sun_family = AF_UNIX;
    strncpy(sockaddr.sun_path, unixpath, sizeof(sockaddr.sun_path));
    if (connect(sock, reinterpret_cast<struct sockaddr *>(&sockaddr),
                sizeof(sockaddr)) != 0) {
        fprintf(stderr, "Couldn't connect to UNIX-socket at %s: %s.\n",
                unixpath, strerror(errno));
        close(sock);
        exit(4);
    }

    struct thread_info toleft_info = {sock, 1, 0, 1};
    struct thread_info toright_info = {0, sock, 1, 0};
    pthread_t toright_thread, toleft_thread;
    if (pthread_create(&toright_thread, nullptr, copy_thread, &toright_info) !=
            0 ||
        pthread_create(&toleft_thread, nullptr, copy_thread, &toleft_info) !=
            0) {
        fprintf(stderr, "Couldn't create threads: %s.\n", strerror(errno));
        close(sock);
        exit(5);
    }
    if (pthread_join(toleft_thread, nullptr) != 0 ||
        pthread_join(toright_thread, nullptr) != 0) {
        fprintf(stderr, "Couldn't join threads: %s.\n", strerror(errno));
        close(sock);
        exit(6);
    }

    close(sock);
    return 0;
}
