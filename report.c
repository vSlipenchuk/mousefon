#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/hiddev.h>

#include "holt.h"

#include <stdio.h>


/* Mutex for avoiding concurrent writes */
pthread_mutex_t write_mutex = PTHREAD_MUTEX_INITIALIZER;

static int
process_report (int fd_user, struct hiddev_usage_ref *uref)
{
    struct input_event ev;
    int s;

    gettimeofday(&ev.time, NULL);

    if ((holt_status & HOLT_STATUS_OPEN) && uref[1].value == 0x1f) {
        holt_status &= ~HOLT_STATUS_OPEN;
        holt_set_backlight(0);

        ev.type = EV_SW;
        ev.code = SW_LID;
        ev.value = 1;
        s = holt_write_event(fd_user, &ev);
        if (s < 0) return s;
    } else if (!(holt_status & HOLT_STATUS_OPEN) && uref[1].value == 0xff) {
        holt_status |= HOLT_STATUS_OPEN;
        holt_set_backlight(1);

        ev.type = EV_SW;
        ev.code = SW_LID;
        ev.value = 0;
        s = holt_write_event(fd_user, &ev);
        if (s < 0) return s;
    }

    if (uref[1].value != 0x1f && uref[1].value != 0xff) {
        fprintf(stderr, "Holt: Ignoring unknown LID value 0x%2x\n",
                uref[1].value);
    }

    if ((holt_status & HOLT_STATUS_HEADPHONE) && uref[3].value == 0xff) {
        holt_status &= ~HOLT_STATUS_HEADPHONE;
        holt_set_sound(1, 0);

        ev.type = EV_SW;
        ev.code = SW_HEADPHONE_INSERT;
        ev.value = 0;
        s = holt_write_event(fd_user, &ev);
        if (s < 0) return s;
    } else if (!(holt_status & HOLT_STATUS_HEADPHONE) &&
               uref[3].value == 0x1f) {
        holt_status |= HOLT_STATUS_HEADPHONE;
        holt_set_sound(0, 1);

        ev.type = EV_SW;
        ev.code = SW_HEADPHONE_INSERT;
        ev.value = 1;
        s = holt_write_event(fd_user, &ev);
        if (s < 0) return s;
   }

    if (uref[3].value != 0x1f && uref[3].value != 0xff) {
        fprintf(stderr, "Holt: Ignoring unknown Headphone value 0x%2x\n",
                uref[3].value);
    }

    //key/button processing.
    switch(uref[0].value) {
    case 0x00: ev.code = KEY_RESERVED; break;
    case 0x03: ev.code = KEY_NUMERIC_POUND; break;
    case 0x04: ev.code = KEY_NUMERIC_9; break;
    case 0x05: ev.code = KEY_NUMERIC_6; break;
    case 0x06: ev.code = KEY_NUMERIC_3; break;
    case 0x07: ev.code = KEY_CANCEL; break; //Hangup
    case 0x08: ev.code = KEY_PHONE; break; //Side key pickup/hangup
    case 0x0b: ev.code = KEY_NUMERIC_0; break;
    case 0x0c: ev.code = KEY_NUMERIC_8; break;
    case 0x0d: ev.code = KEY_NUMERIC_5; break;
    case 0x0e: ev.code = KEY_NUMERIC_2; break;
    case 0x0f: ev.code = KEY_AGAIN; break; //Re/HF : Redial
    case 0x10: ev.code = KEY_VOLUMEDOWN; break;
    case 0x13: ev.code = KEY_NUMERIC_STAR; break;
    case 0x14: ev.code = KEY_NUMERIC_7; break;
    case 0x15: ev.code = KEY_NUMERIC_4; break;
    case 0x16: ev.code = KEY_NUMERIC_1; break;
    case 0x17: ev.code = KEY_SELECT; break; //Pickup
    case 0x18: ev.code = KEY_VOLUMEUP; break;
    default:
        fprintf(stderr, "Holt: Ignoring unknown key code 0x%02x\n",
                uref[0].value);
        ev.code = KEY_RESERVED; break;
    }

    if (ev.code != KEY_RESERVED) {
        ev.type = EV_KEY;
        ev.value = 1; //keypress
        holt_write_event(fd_user, &ev);

        //Currently we do not properly handle multiple
        //simultaneous keypresses, and send the release event immediately.

        ev.value = 0; //keyrelease
        s = holt_write_event(fd_user, &ev);
        if (s < 0) return s;
    }

    //scroll wheel processing
    switch(uref[2].value) {
    case 0x01: ev.value = -1; break; //Up becomes Down when lid is open
    case 0x00: ev.value = 0; break;
    case 0xff: ev.value = 1; break;
    default:
        fprintf(stderr, "Holt: Ignoring unknown wheel value 0x%2x\n",
                uref[2].value);
        ev.value = 0; break;
    }

    if (ev.value != 0) {
        ev.type = EV_REL;
        ev.code = REL_DIAL;
        s = holt_write_event(fd_user, &ev);
        if (s < 0) return s;
    }

    //check other bytes for weird values
    if (uref[4].value != 0xff || uref[5].value != 0xff ||
        uref[6].value != 0x00) {
        fprintf(stderr,
                "Holt: Ignoring unknown other values 0x%02x 0x%02x 0x%02x\n",
                uref[4].value, uref[5].value, uref[6].value);
    }

    return 0;
}

/** Read a report using fd_hiddev and process it
 * @param fd_user file descriptor for output (pipe to user)
 */
int holt_read_report (int fd_user) {
    struct hiddev_usage_ref uref[HOLT_INPUT_USAGE_SIZE_MAX];
    int s;

    //read uref with report id (1 or 2)
    s = holt_read_all(holt_fd_hiddev, &uref[0], sizeof(uref[0]));
    if (s < 0) return -1;

    // printf("."); fflush(stdout);

    // printf("HOLT id %u fi %u ui %u value %u\n",
    //        uref->report_id, uref->field_index, uref->usage_index,
    //        uref->value);

    switch(uref->report_id) {
    case 1: //device-specific event
        s = holt_read_all(holt_fd_hiddev, uref,
                          sizeof(uref[0]) * HOLT_INPUT_USAGE_SIZE_1);
        if (s < 0) return -1;

        return process_report(fd_user, uref);
    case 2: //normal mouse event: ignore
        return holt_read_all(holt_fd_hiddev, uref,
                             sizeof(uref[0]) * HOLT_INPUT_USAGE_SIZE_2);
    default:
        errno = EINVAL;
        return -1;
    }
}

/** Write a holt report to the HID device */
void holt_write_report (unsigned char *report) {
    struct hiddev_usage_ref_multi urefm;
    struct hiddev_report_info rinfo;
    int s;
    unsigned int u;

    urefm.uref.report_type = HID_REPORT_TYPE_OUTPUT;
    urefm.uref.report_id = 1;
    urefm.uref.field_index = 0;
    urefm.uref.usage_index = 0;
    urefm.uref.usage_code = 1;
    urefm.num_values = HOLT_OUTPUT_USAGE_SIZE;

    for (u = 0; u < HOLT_OUTPUT_USAGE_SIZE; u++) {
        urefm.values[u] = report[u];
    }

    pthread_mutex_lock(&write_mutex);

    s = ioctl(holt_fd_hiddev, HIDIOCSUSAGES, &urefm.uref);
    if (__builtin_expect(s == 0, 1)) {
        rinfo.report_type = HID_REPORT_TYPE_OUTPUT;
        rinfo.report_id = 1;
        rinfo.num_fields = 1;

        ioctl(holt_fd_hiddev, HIDIOCSREPORT, &rinfo);
    }

    pthread_mutex_unlock(&write_mutex);
}

void holt_set_sound (int speaker, int headphone) {
    unsigned char report[HOLT_OUTPUT_USAGE_SIZE];

    memset(report, 0, sizeof(report));
    report[0] = HOLT_COMMAND_SOUND;
    if (speaker) report[1] |= HOLT_BIT_SPEAKER;
    if (headphone) report[1] |= HOLT_BIT_HEADPHONE;

    holt_write_report(report);
}

void holt_set_backlight (int on) {
    unsigned char report[HOLT_OUTPUT_USAGE_SIZE];

    memset(report, 0, sizeof(report));
    report[0] = HOLT_COMMAND_BACKLIGHT;
    if (on) report[1] |= HOLT_BIT_BACKLIGHT;

    return holt_write_report(report);
}

void holt_request_status (void) {
    unsigned char report[HOLT_OUTPUT_USAGE_SIZE];

    memset(report, 0, sizeof(report));
    report[0] = HOLT_COMMAND_STATUS;

    return holt_write_report(report);
}

