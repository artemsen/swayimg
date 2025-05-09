// SPDX-License-Identifier: MIT
// Shell command executor.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "shellcmd.h"

#include "array.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * Read stream from child process.
 * @param fd stdout file descriptor
 * @param out pointer to output array
 * @return 0 on success or error code
 */
static int read_stream(int fd, struct array** out)
{
    int rc = 0;

    *out = arr_create(0, sizeof(uint8_t));
    if (!*out) {
        return ENOMEM;
    }

    while (!rc) {
        struct array* arr;
        uint8_t buf[4096];
        const ssize_t read_sz = read(fd, buf, sizeof(buf));

        if (read_sz == 0) {
            break;
        }
        if (read_sz == -1) {
            if (errno == EAGAIN) {
                continue;
            } else {
                rc = errno;
                break;
            }
        }

        arr = arr_append(*out, buf, read_sz);
        if (!arr) {
            rc = ENOMEM;
            break;
        }
        *out = arr;
    }

    if (rc || (*out)->size == 0) {
        arr_free(*out);
        *out = NULL;
    }

    return rc;
}

/**
 * Execute command in child process.
 * @param cmd command to execute
 * @param fd stdout file descriptor
 * @return 0 on success or error code
 */
static int execute(const char* cmd, int fd)
{
    int in_fd;
    const char* shell;

    // redirect stdout
    dup2(fd, STDOUT_FILENO);
    close(fd);

    // disable stdin to prevent interactive requests
    in_fd = open("/dev/null", O_RDONLY);
    if (in_fd != -1) {
        dup2(in_fd, STDIN_FILENO);
        close(in_fd);
    }

    // execute in shell
    shell = getenv("SHELL");
    if (!shell || !*shell) {
        shell = "/bin/sh";
    }
    execlp(shell, shell, "-c", cmd, NULL);

    return errno;
}

int shellcmd_exec(const char* cmd, struct array** out)
{
    int rc;
    int fds[2];
    pid_t pid;

    if (!cmd || !*cmd) {
        return EINVAL;
    }

    if (pipe(fds) == -1) {
        return errno;
    }

    pid = fork();
    switch (pid) {
        case -1:
            rc = errno;
            close(fds[0]);
            close(fds[1]);
            break;
        case 0: // child process
            close(fds[0]);
            rc = execute(cmd, fds[1]);
            exit(rc);
            break;
        default: // parent process
            close(fds[1]);
            read_stream(fds[0], out);
            close(fds[0]);
            if (waitpid(pid, &rc, 0) != -1) {
                rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : ECHILD;
            } else {
                rc = errno;
            }
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
