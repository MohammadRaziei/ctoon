#include <stdio.h>
#include "ctoon.h"

int main(void) {
    /* header version == version compiled into the installed library */
    if (ctoon_version() != (uint32_t)CTOON_VERSION_HEX) {
        printf("version mismatch: lib=%u header=%u\n",
               (unsigned)ctoon_version(), (unsigned)CTOON_VERSION_HEX);
        return 1;
    }
    printf("ctoon %s (C)\n", CTOON_VERSION_STRING);
    return 0;
}
