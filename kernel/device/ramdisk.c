/* Copyright © 2018-2019 N. Van Bossuyt.                                      */
/* This code is licensed under the MIT License.                               */
/* See: LICENSE.md                                                            */

#include <libfile/tar.h>
#include <libmath/math.h>
#include <libsystem/cstring.h>
#include <libsystem/error.h>
#include <libsystem/logger.h>

#include "filesystem/Filesystem.h"
#include "multiboot.h"

void ramdisk_load(multiboot_module_t *module)
{
    // Extract the ramdisk tar archive.

    logger_info("Loading ramdisk at 0x%x...", module->mod_start);

    void *ramdisk = (void *)module->mod_start;

    tar_block_t block;
    for (size_t i = 0; tar_read(ramdisk, &block, i); i++)
    {
        Path *file_path = path(block.name);

        if (block.name[strlen(block.name) - 1] == '/')
        {
            int result = filesystem_mkdir(file_path);

            if (result < 0)
            {
                logger_warn("Failed to create directory %s: %s", block.name, error_to_string(-result));
            }
        }
        else
        {
            FsHandle *handle = filesystem_open(file_path, OPEN_WRITE | OPEN_CREATE);

            if (handle != NULL)
            {
                size_t writen = 0;
                error_t result = fshandle_write(handle, block.data, block.size, &writen);

                if (result != ERR_SUCCESS)
                {
                    logger_error("Failled to write file: %s", error_to_string(-result));
                }

                fshandle_destroy(handle);
            }
            else
            {
                logger_warn("Failed to open file %s!", block.name);
            }
        }

        path_delete(file_path);
    }

    logger_info("Loading ramdisk succeeded.");
}
