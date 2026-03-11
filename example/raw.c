/* Example: Raw shared memory from pure C
 *
 * Uses the lsm_c wrapper to create a shared segment, write bytes,
 * open it from a second handle, and read them back.
 */

#include "lsm_c.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* message = "Hello from C!";
    size_t len = strlen(message) + 1; /* include null terminator */

    /* Writer: create segment and copy bytes in */
    lsm_memory* writer = lsm_create("cExample", 256, 1);
    if (!writer) {
        fprintf(stderr, "FAIL: lsm_create\n");
        return 1;
    }
    memcpy(lsm_data(writer), message, len);

    /* Reader: open the same segment and read bytes out */
    lsm_memory* reader = lsm_open("cExample", 256, 1);
    if (!reader) {
        fprintf(stderr, "FAIL: lsm_open\n");
        lsm_close(writer);
        lsm_destroy(writer);
        lsm_free(writer);
        return 1;
    }

    const char* received = (const char*)lsm_data(reader);
    printf("Received: %s\n", received);

    if (strcmp(received, message) != 0) {
        fprintf(stderr, "FAIL: mismatch\n");
        lsm_close(reader);
        lsm_free(reader);
        lsm_close(writer);
        lsm_destroy(writer);
        lsm_free(writer);
        return 1;
    }

    lsm_close(reader);
    lsm_free(reader);
    lsm_close(writer);
    lsm_destroy(writer);
    lsm_free(writer);

    printf("OK\n");
    return 0;
}
