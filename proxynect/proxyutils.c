#include "proxynect.h"

#include <stdio.h>

#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>

static struct proxynect_device *map_device(int index, int flags) {
    char fn[80];
    sprintf(fn, "/proxynect_%d", index);

    int fd = shm_open(fn, flags, 0777);

    if(fd < 0) {
        goto fail;
    }

    if(flags & O_CREAT) {
        if(ftruncate(fd, sizeof(struct proxynect_device)) < 0) {
            goto fail;
        }
    }

    struct proxynect_device *device = (struct proxynect_device *)mmap(
        NULL, sizeof(struct proxynect_device), PROT_READ | PROT_WRITE,
        MAP_SHARED, fd, 0);

    if(device == MAP_FAILED) {
        goto fail;
    }

    close(fd);

    return device;

fail:
    if(fd >= 0) {
        close(fd);
        if(flags & O_CREAT)
            shm_unlink(fn);
    }
    return NULL;
}

struct proxynect_device *create_device(int index, int force) {
    if(force) {
        return map_device(index, O_RDWR | O_CREAT);
    } else {
        return map_device(index, O_RDWR | O_CREAT | O_EXCL);
    }
}

struct proxynect_device *open_device(int index) {
    return map_device(index, O_RDWR);
}

void close_device(struct proxynect_device *device) {
    munmap(device, sizeof(struct proxynect_device));
}

void destroy_device(struct proxynect_device *device) {
    char fn[80];
    sprintf(fn, "/proxynect_%d", device->index);
    shm_unlink(fn);

    munmap(device, sizeof(struct proxynect_device));
}
