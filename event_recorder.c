#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <sys/limits.h>
#include <sys/poll.h>
#include <linux/input.h>
#include <errno.h>

struct device_event {
    int fd;
    struct input_event event;
};

struct device_fd {
    int fd;
    char path[PATH_MAX];
};

struct device_fd_table {
    struct device_fd *fds;
    int fd_num;
};

struct device_events {
    struct device_event *events;
    int events_num;
    struct device_fd_table fd_table;
};

static int device_open(struct device_fd_table *fd_table, const char *path)
{
    int version;
    struct input_id id;
    int fd = 0;
    int i = 0;

    for (i = 0; i < fd_table->fd_num; i++) {
        if (strncmp(path, fd_table->fds[i].path, PATH_MAX) == 0)
            return fd_table->fds[i].fd;
    }

    if (fd_table->fds == NULL) {
        fd_table->fds = (struct device_fd*)malloc(sizeof(struct device_fd));
        if (fd_table->fds == NULL)
            return -1;
    } else {
        fd_table->fds = (struct device_fd*)realloc(fd_table->fds,
                (fd_table->fd_num + 1) * sizeof(struct device_fd));
        if (fd_table->fds == NULL)
            return -1;
    }

    fd = open(path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "could not open %s, %s\n", path, strerror(errno));
        return -1;
    }

    if(ioctl(fd, EVIOCGVERSION, &version))
        return -1;

    if(ioctl(fd, EVIOCGID, &id))
        return -1;

    fd_table->fds[fd_table->fd_num].fd = fd;
    strncpy(fd_table->fds[fd_table->fd_num].path, path, PATH_MAX);
    fd_table->fd_num++;

    return fd;
}

static int scan_dir(struct device_fd_table *fd_table, const char *dirname)
{
    char devname[PATH_MAX];
    char *filename;
    DIR *dir;
    struct dirent *de;
    dir = opendir(dirname);
    if(dir == NULL) {
        fprintf(stderr, "could not open %s, %s\n", dirname, strerror(errno));
        return -1;
    }

    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    while((de = readdir(dir))) {
        if(de->d_name[0] == '.' &&
           (de->d_name[1] == '\0' ||
            (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;
        strcpy(filename, de->d_name);
        device_open(fd_table, devname);
    }
    closedir(dir);
    return 0;
}

static int device_close(struct device_fd_table *fd_table)
{
    int i = 0;

    for (i = 0; i < fd_table->fd_num; i++) {
        close(fd_table->fds[i].fd);
    }
    free(fd_table->fds);
    fd_table->fds = NULL;

    return 0;
}

int events_create(const char *filename, struct device_events *dev_events)
{
    FILE *fp = fopen(filename, "r");
    int events_total_num = 0;

    if (fp == NULL) {
        fprintf(stderr, "could not open %s\n", filename);
        return 0;
    }

    dev_events->events = NULL;
    dev_events->events_num = 0;

    dev_events->fd_table.fds = NULL;
    dev_events->fd_table.fd_num = 0;

    do {
        struct input_event event;
        char path[PATH_MAX];
        unsigned int type, code;

        int ret = fscanf(fp, "[%8ld.%06ld] %s %04x %04x %08x\n",
                &event.time.tv_sec, &event.time.tv_usec,
                path, &type, &code, &event.value);
        if (ret != 6)
            break;

        event.type = type;
        event.code = code;
        path[strlen(path) - 1] = '\0'; //remove :
        if (dev_events->events == NULL) {
            dev_events->events = (struct device_event*)malloc(1000 * sizeof(struct device_event));
            if (dev_events->events == NULL)
                goto err;
            events_total_num = 100;
        } else if (events_total_num == dev_events->events_num) {
            dev_events->events = (struct device_event*)realloc(dev_events->events,
                    (events_total_num + 100) * sizeof(struct device_event));
            if (dev_events->events == NULL)
                goto err;
            events_total_num += 100;
        }
        dev_events->events[dev_events->events_num].event = event;

        dev_events->events[dev_events->events_num].fd = device_open(&dev_events->fd_table, path);
        dev_events->events_num++;
    } while (1);

err:
    fclose(fp);
    return 0;
}

int events_sort(struct device_events *dev_events)
{
    int i, j;

    for (i = 1; i < dev_events->events_num; i++) {
        struct input_event * event = &dev_events->events[i].event;
        for (j = i; j > 0; j--) {
            struct input_event * pre_event = &dev_events->events[j].event;
            int64_t delay_us = (event->time.tv_sec - pre_event->time.tv_sec) * 1000000LL +
                event->time.tv_usec - pre_event->time.tv_usec;
            if (delay_us < 0) {
                struct input_event tmp_event = *pre_event;
                *pre_event = *event;
                *event = tmp_event;
                continue;
            }
            break;
        }
    }

    return 0;
}

int events_replay(const struct device_events *dev_events)
{
    int i = 0;

    for (i = 0; i < dev_events->events_num; i++) {
        int fd = dev_events->events[i].fd;
        struct input_event * event = &dev_events->events[i].event;
        int64_t delay_us = 0;
        int ret;
        ret = write(fd, event, sizeof(*event));
        if(ret < (ssize_t) sizeof(*event)) {
            fprintf(stderr, "write fd:%d event failed, %s\n", fd, strerror(errno));
        }
        if (i + 1 < dev_events->events_num) {
            struct input_event * next_event = &dev_events->events[i+1].event;
            delay_us = (next_event->time.tv_sec - event->time.tv_sec) * 1000000LL +
                next_event->time.tv_usec - event->time.tv_usec;
            if (delay_us < 0)
                delay_us = 0;
        }
        /*printf("[%8ld.%06ld][%lld] fd[%d] %04x %04x %08x\n",
                event->time.tv_sec, event->time.tv_usec, delay_us,
                fd, event->type, event->code, event->value);*/
        usleep(delay_us);
    }

    return 0;
}

int events_free(struct device_events *dev_events)
{
    device_close(&dev_events->fd_table);

    free(dev_events->events);
    dev_events->events = NULL;
    dev_events->events_num = 0;

    return 0;
}

int events_recorder(struct device_fd_table *fd_table, const char *filename)
{
    FILE *fp = fopen(filename, "w+");
    struct pollfd *ufds;
    int i = 0;

    if (fp == NULL) {
        fprintf(stderr, "could not open %s\n", filename);
        return 0;
    }

    setbuf(fp, NULL);

    ufds = (struct pollfd *)calloc(fd_table->fd_num, sizeof(*ufds));
    for (i = 0; i < fd_table->fd_num; i++) {
        int clkid = CLOCK_MONOTONIC;
        ufds[i].fd = fd_table->fds[i].fd;
        ufds[i].events = POLLIN;
        if (ioctl(ufds[i].fd, EVIOCSCLOCKID, &clkid) != 0) {
            fprintf(stderr, "Can't enable monotonic clock reporting: %s\n", strerror(errno));
        }
    }

    while (1) {
        poll(ufds, fd_table->fd_num, -1);

        for (i = 0; i < fd_table->fd_num; i++) {
            if(ufds[i].revents & POLLIN) {
                struct input_event event;
                int ret = read(ufds[i].fd, &event, sizeof(event));
                if (ret < (int)sizeof(event)) {
                    fprintf(stderr, "could not get event ret:%d sizeof(%d)\n", ret, sizeof(event));
                    return 1;
                }
                fprintf(fp, "[%8ld.%06ld] ", event.time.tv_sec, event.time.tv_usec);
                fprintf(fp, "%s: ", fd_table->fds[i].path);
                fprintf(fp, "%04x %04x %08x\n", event.type, event.code, event.value);
            }
        }
    }

    free(ufds);
    fclose(fp);
    return 0;
}

static void usage (char *name)
{
    fprintf(stderr,
"Android event record/palyback utility - $Revision: 1.0 $\n\n"
"Usage：%s -r|p [-c count] [-d second] <event_record.txt>\n\n"
"  -r|p        Record or replay events  (default record)\n\n"
"  -c count    Repeat count for replay\n\n"
"  -d secound  Delay for everytime replay start\n\n"
"Example of event_record.txt: \n"
"[   20897.702414] /dev/input/event1: 0003 0035 000000b1\n"
"[   20897.702414] /dev/input/event1: 0000 0000 00000000\n" ,
name);
    exit(1);
}

int main(int argc, char *argv[])
{
    char *filename = NULL;
    int is_replay = 0;
    int replay_count = 1;
    int64_t replay_delay_us = 0;

    int opt;

    for (opt = 1; opt < argc; opt++) {
        if (argv[opt][0] == '-') {
            switch (argv[opt][1]) {
            case 'p':
                is_replay = 1;
                break;
            case 'c':
                if (opt + 1 >= argc)
                    usage(argv[0]);

                replay_count = atoi(argv[opt+1]);
                opt++;
                break;
            case 'd':
                if (opt + 1 >= argc)
                    usage(argv[0]);

                replay_delay_us = atof(argv[opt+1]) * 1000000;
                if (replay_delay_us < 0)
                    replay_delay_us = 0;
                opt++;
                break;
            default:
                break;
            }
        } else {
            filename = argv[opt];
        }
    }

    if (filename == NULL)
        usage(argv[0]);

    if (is_replay) {
        struct device_events dev_events;

        events_create(filename, &dev_events);

        events_sort(&dev_events);

        while (replay_count != 0) {
            events_replay(&dev_events);
            if (replay_count > 0)
                replay_count--;
            usleep(replay_delay_us);
        }

        events_free(&dev_events);
    } else {
        struct device_fd_table fd_table;
        const char *device_path = "/dev/input";

        fd_table.fds = NULL;
        fd_table.fd_num = 0;

        scan_dir(&fd_table, device_path);

        events_recorder(&fd_table, filename);

        device_close(&fd_table);
    }

    return 0;
}
