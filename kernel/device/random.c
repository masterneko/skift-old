/* Copyright © 2018-2019 N. Van Bossuyt.                                      */
/* This code is licensed under the MIT License.                               */
/* See: LICENSE.md                                                            */

#include "filesystem/Filesystem.h"

static int random_state = 1411743402;

static error_t random_FsOperationRead(FsNode *node, FsHandle *handle, void *buffer, size_t size, size_t *readed)
{
    __unused(node);
    __unused(handle);

    byte *b = buffer;

    for (uint i = 0; i < size; i++)
    {
        uint x = random_state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        random_state = x;

        b[i] = (byte)x;
    }

    *readed = size;

    return ERR_SUCCESS;
}

static error_t random_FsOperationWrite(FsNode *node, FsHandle *handle, const void *buffer, uint size, size_t *writen)
{
    __unused(node);
    __unused(handle);
    __unused(buffer);

    *writen = size;

    return ERR_SUCCESS;
}

void random_initialize(void)
{
    FsNode *random_device = __create(FsNode);

    fsnode_init(random_device, FSNODE_DEVICE);

    FSNODE(random_device)->read = (FsOperationRead)random_FsOperationRead;
    FSNODE(random_device)->write = (FsOperationWrite)random_FsOperationWrite;

    Path *random_device_path = path("/dev/random");
    filesystem_link_and_take_ref(random_device_path, random_device);
    path_delete(random_device_path);
}