#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <paths.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <android/log.h>
#include <pwd.h>
#include <sys/system_properties.h>

#define TAG "JsDroidLoader"
#define LOGD(...) (__android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__))
#define LOGE(...) (__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))


sig_atomic_t update;
sig_atomic_t quited;
sig_atomic_t looped;
sig_atomic_t initko;
char start[11];

#if defined(__aarch64__)
#define ABI "arm64"
#elif defined(__x86_64__)
#define ABI "x86_64"
#elif defined(__arm__)
#define ABI "arm"
#elif defined(__i386__)
#define ABI "x86"
#endif

#define KEY "log.tag.jsdroid.event"

static int ko() {
    char value[PROP_VALUE_MAX] = {'\0'};
    __system_property_get(KEY, value);
    return !strcmp(value, "ko");
}

static void ok() {
    if (ko()) {
        __system_property_set(KEY, "");
    }
}


#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

static int worker() {
    char *arg[] = {"app_process", "/system/bin", "--nice-name=jsdroid_server",
                   "com.jsdroid.runner.ShellMain",
                   STR(SIGUSR1), start, NULL};
    pid_t pid = fork();
    switch (pid) {
        case -1:
            LOGE("cannot fork");
            return -1;
        case 0:
            break;
        default:
            return pid;
    }
    return execvp(arg[0], arg);
}

static void update_proc_title(size_t length, char *arg) {
    memset(arg, 0, length);
    strlcpy(arg, "jsdroid_daemon", length);
}

static int server(size_t length, char *arg) {
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGUSR1);

    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        LOGE("cannot sigprocmask");
    }
    sigemptyset(&set);

    update_proc_title(length, arg);

    if (worker() <= 0) {
        return -EPERM;
    }

    initko = 0;
    for (;;) {
        sigsuspend(&set);
        if (quited) {
            LOGD("signal arrived, update: %d", update);
            if (!update) {
                break;
            }
            update = 0;
            quited = 0;
            looped = 0;
            if (worker() <= 0) {
                break;
            }
        }
    }

    return 0;
}

static void report(time_t now) {

    char command[BUFSIZ];
    char time[BUFSIZ];
    struct tm *tm = localtime(&now);
    strftime(time, sizeof(time), "%m-%d %H:%M:%S.000", tm);
#ifdef LOGBELOW
    printf(LOGBELOW);
#endif
    printf("--- crash start ---\n");
    sprintf(command, "logcat -b crash -t '%s' -d", time);
    printf("[command] %s\n", command);
    fflush(stdout);
    system(command);
    fflush(stdout);
    printf("--- crash end ---\n");
    fflush(stdout);
    printf("--- jsdroid start ---\n");
    sprintf(command, "logcat -b main -t '%s' -d -s JsDroidLoader JsDroidServer", time);
    printf("[command] %s\n", command);
    fflush(stdout);
    system(command);
    fflush(stdout);
    printf("--- jsdroid end ---\n");
#ifdef LOGABOVE
    printf(LOGABOVE);
#endif
    fflush(stdout);
}

static int get_pid() {
    int pid = 0;
    DIR *proc;
    struct dirent *entry;

    if (!(proc = opendir("/proc"))) {
        return pid;
    };

    while ((entry = readdir(proc))) {
        int id;
        FILE *fp;
        char buf[PATH_MAX];

        if (!(id = atoi(entry->d_name))) {
            continue;
        }
        sprintf(buf, "/proc/%u/cmdline", id);
        fp = fopen(buf, "r");
        if (fp != NULL) {
            fgets(buf, PATH_MAX - 1, fp);
            fclose(fp);
            if (!strcasecmp(buf, "jsdroid_server")) {
                pid = id;
                break;
            }
        }
    }
    closedir(proc);
    return pid;
}

static void signal_check(int signo) {
    LOGD("check received signal: %d, ppid: %d", signo, getppid());
    if (signo == SIGUSR1) {
        looped = 1;
    }
}

static int check(time_t now) {
    int pid = 0;
    static int initko = 0;
    signal(SIGUSR1, signal_check);
    if (initko == 0) {
        printf("checking for server.");
    }
    for (int i = 0; i < 6; ++i) {
        int id = get_pid();
        if (pid == 0 && id > 0) {
            printf("%s, pid: %d\nchecking for stable.",
                   initko == 0 ? "started" : "reborn", id);
            i = 0;
            pid = id;
        } else if (pid > 0 && id == 0) {
            if (!initko && ko()) {
                initko = 1;
                return check(now);
            } else {
                printf("quited\n\n");
                fflush(stdout);
                report(now);
                return EXIT_FAILURE;
            }
        } else if (quited || looped) {
            break;
        } else if (pid != id) {
            initko = 1;
            return check(now);
        }
        printf(".");
        fflush(stdout);
        sleep(1);
    }
    if (pid > 0) {
        if (looped) {
            printf("success\n\n");
        } else {
            printf("timeout\n\n");
        }
#ifdef FEEDBACK
        printf(FEEDBACK);
#endif
        fflush(stdout);
        return EXIT_SUCCESS;
    } else {
        printf("fail\n");
        fflush(stdout);
        report(now);
        return EXIT_FAILURE;
    }
}

static void check_original() {
    int pid;
    if ((pid = get_pid()) > 0) {
        printf("found old jsdroid_server, pid: %d, killing\n", pid);
        kill(pid, SIGTERM);
        for (int i = 0; i < 3; ++i) {
            if (get_pid() > 0) {
                sleep(1);
                kill(pid, SIGKILL);
            } else {
                return;
            }
        }
        printf("cannot kill original jsdroid_server, pid: %d\n", pid);
        exit(EPERM);
    }
}

static void signal_handler(int signo) {
    if (signo == SIGCHLD) {
        pid_t pid;
        int status;
        for (;;) {
            pid = waitpid(-1, &status, WNOHANG);
            if (pid == 0 || pid == -1) {
                return;
            }
            quited = 1;
            update = 0;
            if (WIFEXITED(status)) {
                int exitstatus = WEXITSTATUS(status);
                if (exitstatus == 0) {
                    ok();
                    initko = 0;
                    update = 1;
                    LOGD("worker %d exited with status %d", pid, exitstatus);
                } else {
                    LOGE("worker %d exited with status %d", pid, exitstatus);
                }
            } else if (WIFSIGNALED(status)) {
                int termsig = WTERMSIG(status);
                if (!initko && ko()) {
                    initko = 1;
                    update = 1;
                    LOGD("worker %d exited on signal %d", pid, termsig);
                } else {
                    LOGE("worker %d exited on signal %d", pid, termsig);
                }
            }
        }
    } else if (signo == SIGUSR1) {
        pid_t ppid = getppid();
        LOGD("received signal: %d, ppid: %d", signo, ppid);
        if (ppid > 1) {
            looped = 1;
            kill(getppid(), SIGUSR1);
        }
    }
}

static size_t compute(int argc, char **argv) {
    char *s = argv[0];
    char *e = argv[argc - 1];
    return (e - s) + strlen(argv[argc - 1]) + 1;
}

static int sdk() {
    char sdk[PROP_VALUE_MAX] = {0};
    __system_property_get("ro.build.version.sdk", sdk);
    return atoi(sdk);
}

int main(int argc, char **argv) {
    int fd;
    uid_t uid;
    struct passwd *pw;
    struct timespec ts;
    int version;
    ok();
    uid = getuid();
    version = sdk();
    if (version) {
        printf("android sdk: %d.\n",version);
    }

    if (uid == 0) {
        printf("WARNING: run as root is experimental!!!\n");
    } else if (uid != 2000) {
        pw = getpwuid(uid);
        if (pw != NULL) {
            printf("ERROR: cannot be run as %s(%d).\n", pw->pw_name, uid);
        } else {
            printf("ERROR: cannot be run as non-shell(%d).\n", uid);
        }
        exit(EPERM);
    }

    check_original();

    signal(SIGCHLD, signal_handler);
    signal(SIGUSR1, signal_handler);

    clock_gettime(CLOCK_REALTIME, &ts);
    snprintf(start, sizeof(start), "%ld", ts.tv_sec);
    switch (fork()) {
        case -1:
            perror("cannot fork");
            return -EPERM;
        case 0:
            break;
        default:
            _exit(check(ts.tv_sec));
    }

    if (setsid() == -1) {
        perror("cannot setsid");
        return -EPERM;
    }

    chdir("/");

    if ((fd = open(_PATH_DEVNULL, O_RDWR)) == -1) {
        perror("cannot open " _PATH_DEVNULL);
        return -EPERM;
    }

    if (dup2(fd, STDIN_FILENO) == -1) {
        perror("cannot dup2(STDIN)");
        return -EPERM;
    }

    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("cannot dup2(STDOUT)");
        return -EPERM;
    }

    if (dup2(fd, STDERR_FILENO) == -1) {
        perror("cannot dup2(STDERR)");
        return -EPERM;
    }

    if (fd > STDERR_FILENO && close(fd) == -1) {
        perror("cannot close");
        return -EPERM;
    }

    return server(compute(argc, argv), argv[0]);
}
