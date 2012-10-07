/* Miscellaneous helper functions */

#include <unistd.h>
#include "holt.h"

/** Write until all bytes are written */
int holt_write_all (int fd, void *buf, size_t size) {
    int s;
    while(size > 0) {
        s = write(fd, buf, size);
        if (s == 0) return -1;
        if (s < 0) return s;
        buf += s;
        size -= s;
    }
    return 0;
}

int holt_write_event (int fd, struct input_event *ev) {
    int s = holt_write_all(fd, ev, sizeof(*ev));
    fsync(fd);
    return s;
}

/** Read until all bytes are read */
int holt_read_all (int fd, void *buf, size_t size) {
    int s;
    while(size > 0) {
        s = read(fd, buf, size);
        if (s <= 0) return s;
        buf += s;
        size -= s;
    }
    return 0;
}

