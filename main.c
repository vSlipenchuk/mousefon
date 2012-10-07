/*
    HERE REAL SOURCE of libholt :: http://code.google.com/p/libholt/source/checkout

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "holt.h"

int aborted = 0;
char *onkey="./on_mousefon_key";
#define LIDKEY 777 /* fake keycode :: when lid opens or close */
//123456789066

void on_key(int key, int isdown /* 1=down, 0 = up */) {
char buf[1024];
memset(buf,0,sizeof(buf));
snprintf(buf,sizeof(buf),"%s %d %d",onkey,key,isdown);
system(buf);
}


int main (int npar,char **par) {
    int fd,i;
    struct input_event ev;

    for(i=1;i<npar;i++) { // checkparams
        char *c = par[i];
        if (*c=='-') {
            c++;
            if (*c == 'c') { // cfg starts here
                onkey=c+1;
               }
           }
       }

    fd = holt_init(NULL);
    if (fd < 0) {
        printf("Error %d initializing holt library: %s\n", -fd, strerror(-fd));
        return fd;
    }

    printf("on_key will call %s script\n",onkey);

    while (!aborted) {
        holt_read_event(&ev);

        printf("Hevent type %u code %u value %d\n",
               ev.type, ev.code, ev.value);

        if (ev.type == 1) on_key(ev.code,ev.value);
        if (ev.type == 5 && ev.code == 0 ) on_key(LIDKEY, ev.value);

        //if (ev.type == EV_PWR) break; //unplugged

    } //;while ( !(ev.type == EV_KEY && ev.code == KEY_PHONE && ev.value == 0) );

    holt_deinit();
    return 0;
}
