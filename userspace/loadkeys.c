/* Copyright © 2018-2019 N. Van Bossuyt.                                      */
/* This code is licensed under the MIT License.                               */
/* See: LICENSE.md                                                            */

#include <libsystem/cmdline.h>
#include <libsystem/cstring.h>
#include <libsystem/error.h>
#include <libsystem/io/Stream.h>
#include <libsystem/logger.h>
#include <libsystem/process.h>

#include <libdevice/keyboard.h>
#include <libdevice/keymap.h>

bool option_get;
bool option_list;

static const char *usages[] = {
    "",
    "OPTION...",
    NULL,
};

static CommandLineOption options[] = {
    COMMANDLINE_OPT_HELP,

    COMMANDLINE_OPT_BOOL("get", 'g', option_get, "Get the current keyboard keymap.", COMMANDLINE_NO_CALLBACK),
    COMMANDLINE_OPT_BOOL("list", 'l', option_list, "List all installed keymap on this system", COMMANDLINE_NO_CALLBACK),

    COMMANDLINE_OPT_END,
};

static CommandLine cmdline = CMDLINE(
    usages,
    options,
    "Get or set the current keyboard keymap",
    "Options can be combined.");

int loadkey_list_keymap()
{
    Stream *dir = stream_open("/res/keyboard", OPEN_READ);

    DirectoryEntry entry = {0};
    while (stream_read(dir, &entry, sizeof(entry)) > 0)
    {
        // FIXME: maybe show some info about the kmap file
        printf("- %s\n", entry.name);
    }

    stream_close(dir);

    return 0;
}

int loadkey_set_keymap(Stream *keyboard_device, const char *keymap_path)
{
    logger_info("Loading keymap file from %s", keymap_path);
    Stream *keymap_file = stream_open(keymap_path, OPEN_READ);

    if (keymap_file != NULL)
    {
        FileState stat;
        stream_stat(keymap_file, &stat);

        logger_info("Allocating keymap of size %dkio", stat.size / 1024);

        if (stat.size < sizeof(keymap_t))
        {
            stream_printf(err_stream, "Invalid keymap file format!\n");
            return -1;
        }

        keymap_t *new_keymap = malloc(stat.size);

        stream_read(keymap_file, new_keymap, stat.size);

        stream_close(keymap_file);

        if (new_keymap->magic[0] == 'k' &&
            new_keymap->magic[1] == 'm' &&
            new_keymap->magic[2] == 'a' &&
            new_keymap->magic[3] == 'p')
        {
            keyboard_set_keymap_args_t args = {.size = stat.size, .keymap = new_keymap};
            stream_call(keyboard_device, KEYBOARD_CALL_SET_KEYMAP, &args);

            printf("Keymap set to %s(%s)\n", new_keymap->language, new_keymap->region);

            free(new_keymap);

            return 0;
        }
        else
        {
            free(new_keymap);

            stream_printf(err_stream, "Invalid keymap file format!\n");

            return -1;
        }
    }
    else
    {
        error_print("Failled to open the keymap file");

        return -1;
    }
}

int loadkey_get_keymap(Stream *keyboard_device)
{
    keymap_t keymap;

    if (stream_call(keyboard_device, KEYBOARD_CALL_GET_KEYMAP, &keymap) == 0)
    {
        printf("Current keymap is %s(%s)\n", keymap.language, keymap.region);

        return 0;
    }
    else
    {
        stream_printf(err_stream, "Failled to retrived the current keymap!\n");

        return -1;
    }
}

int main(int argc, char **argv)
{
    argc = cmdline_parse(&cmdline, argc, argv);

    Stream *keyboard_device = stream_open(KEYBOARD_DEVICE, OPEN_READ);

    if (keyboard_device == NULL)
    {
        error_print("Failled to open the keyboard device");

        return -1;
    }

    if (!option_list && argc == 1)
    {
        option_get = true;
    }

    if (option_get)
    {
        return loadkey_get_keymap(keyboard_device);
    }

    if (option_list)
    {
        return loadkey_list_keymap();
    }

    if (!option_get && !option_list && argc == 2)
    {
        char font_path[PATH_LENGHT] = {0};
        snprintf(font_path, PATH_LENGHT, "/res/keyboard/%s.kmap", argv[1]);
        return loadkey_set_keymap(keyboard_device, font_path);
    }

    stream_close(keyboard_device);

    return 0;
}
