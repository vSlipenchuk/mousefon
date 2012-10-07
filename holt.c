#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/hiddev.h>

#include "holt.h"

static int fd_user[2]; /* event pipe from thread to user */

static pthread_t holt_thread;
static void *holt_thread_run (void *);

/* Library global data */
int holt_fd_hiddev = 0;
unsigned int holt_status = 0;

/** Initialize the library by opening the HOLT device
 * @param filename path of hiddev device, NULL for default
 * @return: negative: error (no need to call holt_deinit);
 * @return: positive: file descriptor for user input
 *          (call holt_deinit when done)
 */
int holt_init (char *filename) {
    int fd, fd_null, s, version, flags;
    struct hiddev_devinfo devinfo;
    struct input_event ev;

    if (holt_fd_hiddev != 0) return 0; //already initialized

    if (filename == NULL) filename = HOLT_DEFAULT_DEVICE;

    fd = open(filename, O_RDONLY);
    if (fd < 0) return -errno;

    s = ioctl(fd, HIDIOCGVERSION, &version);
    if (s < 0 || version != HOLT_HID_VERSION) {
        close(fd);
        return -EPROTO;
    }

    s = ioctl(fd, HIDIOCGDEVINFO, &devinfo);
    if (s < 0 ||
        devinfo.vendor != HOLT_VENDOR_ID ||
        devinfo.product != HOLT_DEVICE_ID ||
        devinfo.version != HOLT_DEVICE_VERSION ||
        devinfo.num_applications != HOLT_NUM_APPLICATIONS) {
        close(fd);
        return -ENODEV;
    }

    //set flags
    flags = HIDDEV_FLAG_UREF | HIDDEV_FLAG_REPORT;
    s = ioctl(fd, HIDIOCSFLAG, &flags);
    if (s < 0) {
        close(fd);
        return -EPROTO;
    }

    holt_fd_hiddev = fd;

    s = pipe(fd_user);
    if (s != 0) {
        close(fd);
        return -EPIPE;
    }

    //request status, write any 'updates' to /dev/null
    fd_null = open("/dev/null", O_WRONLY);
    if (fd_null < 0) {
        close(fd); close(fd_user[0]); close(fd_user[1]);
        return -ECOMM;
    }
    holt_request_status();
    holt_read_report(fd_null);
    close(fd_null);

    //send current status to user, with time 0
    ev.time.tv_sec = ev.time.tv_usec = 0;
    ev.type = EV_SW;
    ev.code = SW_LID;
    ev.value = (holt_status & HOLT_STATUS_OPEN) ? 0 : 1;
    s = holt_write_event(fd_user[1], &ev);
    if (s < 0) {
        close(fd); close(fd_user[0]); close(fd_user[1]);
        return -ECOMM;
    }

    ev.code = SW_HEADPHONE_INSERT;
    ev.value = (holt_status & HOLT_STATUS_HEADPHONE) ? 1 : 0;
    s = holt_write_event(fd_user[1], &ev);
    if (s < 0) {
        close(fd); close(fd_user[0]); close(fd_user[1]);
        return -ECOMM;
    }

    //create thread that listens for input and writes events to the pipe
    s = pthread_create(&holt_thread, NULL, holt_thread_run, NULL);
    if (s != 0) {
        close(fd); close(fd_user[0]); close(fd_user[1]);
        return -EAGAIN;
    }

    //Display tux on the screen
    holt_lcd_tux();

    return holt_fd();
}

/** @return The file descriptor for reading events */
int holt_fd (void) {
    return fd_user[0];
}

void holt_deinit (void) {
    void *ret;

    if (holt_fd_hiddev == 0) return; //not initialized

    close(holt_fd_hiddev);
    close(fd_user[0]);
    close(fd_user[1]);

    pthread_join(holt_thread, &ret);

    holt_fd_hiddev = 0;
}

/** Read an event from the holt device. This call is blocking. */
void holt_read_event (struct input_event *ev) {
    int s;

    s = holt_read_all(fd_user[0], ev, sizeof(*ev));
    if (s != 0) { //error
        ev->type = EV_SYN;
        ev->code = SYN_REPORT;
        ev->value = errno;
    }
}

void *holt_thread_run (__attribute__ ((unused)) void *arg) {
    int s;
    struct input_event ev;

    do {
        //select() does not work on hiddev, so we do (blocking) read()s,
        //until holt_deinit closes holt_fd_hiddev, which causes the read to fail

        s = holt_read_report(fd_user[1]);
    } while (s == 0);

    switch(errno) {
    case EIO: //mouse unplugged: send (final) event to user
        gettimeofday(&ev.time, NULL);
        ev.type = EV_PWR;
        ev.code = ev.value = 0; //no special PWR events in input.h...
        holt_write_event(fd_user[1], &ev);
        break;
    case EBADF: //holt_deinit has closed holt_fd_hiddev
        break;
    default: //also quit on other errors
        perror("Holt thread ignoring");
        break;
    }

    return NULL;
}
