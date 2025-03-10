// SPDX-License-Identifier: MIT
// Shell command executor.
// Copyright (C) 2025 Artem Senichev <artemsen@gmail.com>

#include "shellcmd.h"

#include "memdata.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * Read stdout from child process.
 * @param fd stdout file descriptor
 * @param out pointer to stdout buffer, caller should free the buffer
 * @param sz size of output buffer
 * @return 0 on sucess or error code
 */
static int read_stdout(int fd, uint8_t** out, size_t* sz)
{
    int rc = 0;
    ssize_t read_sz;
    uint8_t buf[4096];
    uint8_t* tmp;

    while (!rc) {
        read_sz = read(fd, buf, sizeof(buf));
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

        tmp = realloc(*out, *sz + read_sz);
        if (!tmp) {
            rc = ENOMEM;
            break;
        }
        memcpy(tmp + *sz, buf, read_sz);

        *sz += read_sz;
        *out = tmp;
    }

    if (rc && *sz) {
        free(*out);
        *out = NULL;
        *sz = 0;
    }

    return rc;
}

/**
 * Execute command in child process.
 * @param cmd command to execute
 * @param in_fd stdin file descriptor
 * @param out_fd stdout file descriptor
 * @return 0 on sucess or error code
 */
static int execute(const char* cmd, int in_fd, int out_fd)
{
    int null_fd;
    const char* shell;

    shell = getenv("SHELL");
    if (!shell || !*shell) {
        shell = "/bin/sh";
    }

    dup2(out_fd, STDOUT_FILENO);
    dup2(out_fd, STDERR_FILENO);
    close(out_fd);
    close(in_fd);

    // disable stdin to prevent interactive requests
    null_fd = open("/dev/null", O_WRONLY);
    if (null_fd != -1) {
        dup2(null_fd, STDIN_FILENO);
        close(null_fd);
    }

    execlp(shell, shell, "-c", cmd, NULL);

    return errno;
}

int shellcmd_exec(const char* cmd, uint8_t** out, size_t* sz)
{
    int rc;
    int pfd[2];
    pid_t pid;

    if (pipe(pfd) == -1) {
        return errno;
    }

    pid = fork();
    if (pid == -1) {
        rc = errno;
        close(pfd[0]);
        close(pfd[1]);
        return rc;
    }

    if (pid == 0) {
        // child process
        rc = execute(cmd, pfd[0], pfd[1]);
        exit(rc);
    }

    // parent process handling
    close(pfd[1]);
    read_stdout(pfd[0], out, sz);
    close(pfd[0]);

    if (waitpid(pid, &rc, 0) == -1) {
        return errno;
    }
    if (WIFEXITED(rc)) {
        return WEXITSTATUS(rc);
    }

    return ECHILD;
}

int shellcmd_expr(const char* expr, const char* path, char** out)
{
    int rc;
    char* cmd;
    size_t out_sz = 0;

    // reserve buffer for command
    cmd = malloc(strlen(expr) + strlen(path) + 1);
    if (!cmd) {
        return ENOMEM;
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
        return EINVAL;
    }

    rc = shellcmd_exec(cmd, (uint8_t**)out, &out_sz);

    if (out_sz) {
        // add last null
        char* tmp = realloc(*out, out_sz + 1);
        if (tmp) {
            tmp[out_sz] = 0;
            *out = tmp;
        } else {
            rc = ENOMEM;
            free(*out);
            *out = NULL;
        }
    }

    free(cmd);

    return rc;
}
