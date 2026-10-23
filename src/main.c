#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void on_signal(int sig)
{
    (void)sig;
    running = 0;
}

static void usage(FILE *out, const char *prog)
{
    fprintf(out,
            "Usage: %s -a FILE\n"
            "       %s --batch-file FILE\n"
            "\n"
            "Options:\n"
            "  -a, --batch-file FILE  File to read URLs from (required);\n"
            "                         created if it does not exist\n"
            "  -h, --help             Show this help and exit\n"
            "  -v, --version          Show version and exit\n",
            prog, prog);
}

/* Open the file in append mode so it is created if missing and never truncated. */
static int ensure_file(const char *path)
{
    FILE *fp = fopen(path, "a");
    if (fp == NULL) {
        fprintf(stderr, "[yt-dlpd] cannot open '%s': %s\n", path, strerror(errno));
        return -1;
    }
    fclose(fp);
    return 0;
}

/* Run `yt-dlp -a path` and block until it exits. */
static int run_ytdlp(const char *path)
{
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "[%s] fork failed: %s\n", PROGRAM_NAME, strerror(errno));
        return -1;
    }

    if (pid == 0) {
        execlp("yt-dlp", "yt-dlp", "-a", path, (char *)NULL);
        fprintf(stderr, "[%s] cannot execute yt-dlp: %s\n", PROGRAM_NAME, strerror(errno));
        _exit(127);
    }

    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            fprintf(stderr, "[%s] waitpid failed: %s\n", PROGRAM_NAME, strerror(errno));
            return -1;
        }
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
        printf("[%s] Download finished\n", PROGRAM_NAME);
        fflush(stdout);
    }
    else if (WIFEXITED(status))
        fprintf(stderr, "[%s] yt-dlp exited with status %d\n", PROGRAM_NAME, WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        fprintf(stderr, "[%s] yt-dlp killed by signal %d\n", PROGRAM_NAME, WTERMSIG(status));
    return 0;
}

#define WATCH_MASK (IN_CLOSE_WRITE | IN_MOVE_SELF | IN_DELETE_SELF)

/* Block and report changes to path until interrupted. */
static int monitor_file(const char *path)
{
    int fd = inotify_init1(IN_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "[%s] inotify_init failed: %s\n", PROGRAM_NAME, strerror(errno));
        return -1;
    }

    int wd = inotify_add_watch(fd, path, WATCH_MASK);
    if (wd < 0) {
        fprintf(stderr, "[%s] cannot watch '%s': %s\n", PROGRAM_NAME, path, strerror(errno));
        close(fd);
        return -1;
    }

    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));

    while (running) {
        ssize_t len = read(fd, buf, sizeof buf);
        if (len < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "[%s] read failed: %s\n", PROGRAM_NAME, strerror(errno));
            close(fd);
            return -1;
        }

        int changed = 0;
        int rewatch = 0;
        int ignored = 0;
        for (char *p = buf; p < buf + len;) {
            const struct inotify_event *ev = (const struct inotify_event *)p;
            p += sizeof(struct inotify_event) + ev->len;
            if (ev->wd != wd)
                continue; /* stale event from a watch we already replaced */
            if (ev->mask & (IN_CLOSE_WRITE | IN_MOVE_SELF | IN_DELETE_SELF))
                changed = 1;
            if (ev->mask & (IN_MOVE_SELF | IN_DELETE_SELF | IN_IGNORED))
                rewatch = 1;
            if (ev->mask & IN_IGNORED)
                ignored = 1;
        }

        /* The file was replaced or removed (e.g. by an editor); watch the new one. */
        if (rewatch) {
            if (!ignored)
                inotify_rm_watch(fd, wd);
            if (ensure_file(path) != 0) {
                close(fd);
                return -1;
            }
            wd = inotify_add_watch(fd, path, WATCH_MASK);
            if (wd < 0) {
                fprintf(stderr, "[%s] lost '%s': %s\n", PROGRAM_NAME, path, strerror(errno));
                close(fd);
                return -1;
            }
        }

        if (changed) {
            printf("[%s] %s changed\n", PROGRAM_NAME, path);
            printf("[%s] Invoking yt-dlp to download...\n", PROGRAM_NAME);
            fflush(stdout);
            run_ytdlp(path);
        }
    }

    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    static const struct option long_opts[] = {
        {"batch-file", required_argument, NULL, 'a'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0},
    };
    const char *batch_file = NULL;
    int opt;

    while ((opt = getopt_long(argc, argv, "a:hv", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'a':
            batch_file = optarg;
            break;
        case 'v':
            printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
            return 0;
        case 'h':
            usage(stdout, argv[0]);
            return 0;
        default:
            usage(stderr, argv[0]);
            return 2;
        }
    }

    if (batch_file == NULL) {
        fprintf(stderr, "[yt-dlpd] option -a/--batch-file is required\n");
        usage(stderr, argv[0]);
        return 2;
    }

    if (ensure_file(batch_file) != 0)
        return 1;

    struct sigaction sa = {.sa_handler = on_signal};
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("[%s] Monitoring %s\n", PROGRAM_NAME, batch_file);
    fflush(stdout);

    return monitor_file(batch_file);
}
