// SPDX-License-Identifier: MIT
// Shell command executor.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "shellcmd.h"

#include "array.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Timeouts in milliseconds
#define POLLING_TIMEOUT 10
#define PROCESS_TIMEOUT 10000

/**
 * Read stdout/stderr from child process.
 * @param fd file descriptor to read
 * @param out pointer to output array
 * @return number of bytes read or negative number for errors
 */
static int read_stream(int fd, struct array** data)
{
    uint8_t buf[4096];
    const ssize_t read_sz = read(fd, buf, sizeof(buf));

    if (read_sz < 0) {
        return -errno;
    }
    if (read_sz != 0) {
        if (*data) {
            struct array* arr = arr_append(*data, buf, read_sz);
            if (!arr) {
                return ENOMEM;
            }
            *data = arr;
        } else {
            *data = arr_create(read_sz, sizeof(uint8_t));
            if (!*data) {
                return ENOMEM;
            }
            memcpy(arr_nth(*data, 0), buf, read_sz);
        }
    }

    return read_sz;
}

/**
 * Read stdout/stderr from child process.
 * @param pid child process pid
 * @param fd_out stdout file descriptor
 * @param out pointer to output array for stdout
 * @param fd_err stderr file descriptor
 * @param err pointer to output array for stderr
 * @return child process exit code or negative number on errors
 */
static int read_output(pid_t pid, int fd_out, struct array** out, int fd_err,
                       struct array** err)
{
    struct pollfd poll_streams[] = {
        { .fd = fd_out, .events = POLLIN },
        { .fd = fd_err, .events = POLLIN },
    };
    nfds_t fds_num = ARRAY_SIZE(poll_streams);
    struct pollfd* fds = poll_streams;
    size_t timeout = 0;
    int rc = 0;

    while (rc == 0) {
        // poll events
        const int poll_rc = poll(fds, fds_num, POLLING_TIMEOUT);
        if (poll_rc < 0) {
            rc = -errno;
            break;
        }
        if (poll_rc == 0) {
            // poll timed out, check if child process still alive
            int status;
            const int wait_rc = waitpid(pid, &status, WNOHANG);
            if (wait_rc == -1) {
                rc = -errno;
                break;
            } else if (wait_rc == pid) {
                rc = WIFEXITED(status) ? WEXITSTATUS(status) : -ECHILD;
                break;
            } else if (wait_rc == 0) {
                timeout += POLLING_TIMEOUT;
                if (timeout > PROCESS_TIMEOUT) {
                    rc = SHELLCMD_TIMEOUT;
                    break;
                }
            }
            continue;
        }
        timeout = 0;

        for (size_t i = 0; i < fds_num; ++i) {
            if (fds[i].revents & POLLIN) {
                struct array** arr = (fds[i].fd == fd_out ? out : err);
                const int rsz = read_stream(fds[i].fd, arr);
                if (rsz < 0) {
                    rc = rsz;
                    break;
                }
            }
            if (fds[i].revents & POLLHUP) {
                if (fds_num > 1 && i == 0) {
                    fds = &poll_streams[1];
                }
                --fds_num;
                break;
            }
        }
    }

    return rc;
}

/**
 * Execute command in child process.
 * @param cmd command to execute
 * @param fd_in,fd_out,fd_err IO file descriptor
 * @return 0 on success or error code
 */
static int execute(const char* cmd, int fd_in, int fd_out, int fd_err)
{
    const char* shell;

    // redirect std io
    dup2(fd_in, STDIN_FILENO);
    close(fd_in);
    dup2(fd_out, STDOUT_FILENO);
    close(fd_out);
    dup2(fd_err, STDERR_FILENO);
    close(fd_err);

    // execute in shell
    shell = getenv("SHELL");
    if (!shell || !*shell) {
        shell = "/bin/sh";
    }
    execlp(shell, shell, "-c", cmd, NULL);

    return errno;
}

int shellcmd_exec(const char* cmd, struct array** out, struct array** err)
{
    int rc;
    pid_t pid;
    int fds_in[2], fds_out[2], fds_err[2];

    if (!cmd || !*cmd) {
        return EINVAL;
    }

    if (pipe(fds_in) == -1) {
        return errno;
    }
    if (pipe(fds_out) == -1) {
        close(fds_in[0]);
        close(fds_in[1]);
        return errno;
    }
    if (pipe(fds_err) == -1) {
        close(fds_in[0]);
        close(fds_in[1]);
        close(fds_out[0]);
        close(fds_out[1]);
        return errno;
    }

    pid = fork();
    switch (pid) {
        case -1:
            rc = errno;
            close(fds_in[0]);
            close(fds_in[1]);
            close(fds_out[0]);
            close(fds_out[1]);
            close(fds_err[0]);
            close(fds_err[1]);
            break;
        case 0: // child process
            close(fds_in[1]);
            close(fds_out[0]);
            close(fds_err[0]);
            rc = execute(cmd, fds_in[0], fds_out[1], fds_err[1]);
            exit(rc);
            break;
        default: // parent process
            close(fds_in[0]);
            close(fds_in[1]);
            close(fds_out[1]);
            close(fds_err[1]);
            rc = read_output(pid, fds_out[0], out, fds_err[0], err);
            close(fds_out[0]);
            close(fds_err[0]);
            break;
    }

    return rc;
}

char* shellcmd_expr(const char* expr, const char* path)
{
    char* cmd;

    // reserve buffer for command
    cmd = malloc(strlen(expr) + strlen(path) + 1);
    if (!cmd) {
        return NULL;
    }
    *cmd = 0;

    // construct command from template
    while (expr && *expr) {
        if (*expr == '%') {
            ++expr;
            if (*expr != '%') {
                str_append(path, 0, &cmd); // replace % with path
                continue;
            }
        }
        str_append(expr, 1, &cmd);
        ++expr;
    }

    if (!*cmd) {
        free(cmd);
        cmd = NULL;
    }

    return cmd;
}
