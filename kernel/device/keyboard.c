/* Copyright © 2018-2019 N. Van Bossuyt.                                      */
/* This code is licensed under the MIT License.                               */
/* See: LICENSE.md                                                            */

/* keyboard.c: keyboard driver                                                */

#include <libsystem/assert.h>
#include <libsystem/atomic.h>
#include <libsystem/error.h>
#include <libsystem/logger.h>

#include <libdevice/keyboard.h>
#include <libdevice/keymap.c>
#include <libdevice/keymap.h>
#include <libdevice/keys.c>
#include <libdevice/keys.h>
#include <libkernel/message.h>

#include "filesystem/Filesystem.h"
#include "tasking.h"
#include "x86/irq.h"

/* --- Private functions ---------------------------------------------------- */

#define PS2KBD_ESCAPE 0xE0

typedef enum
{
    PS2KBD_STATE_NORMAL,
    PS2KBD_STATE_NORMAL_ESCAPED,
} PS2KeyboardState;

static PS2KeyboardState keyboard_state = PS2KBD_STATE_NORMAL;
static key_motion_t keyboard_keystate[__KEY_COUNT] = {KEY_MOTION_UP};
static keymap_t *keyboard_keymap = NULL;

int keyboad_get_codepoint(key_t key)
{
    keymap_keybing_t *binding = keymap_lookup(keyboard_keymap, key);

    if (binding == NULL)
    {
        return 0;
    }
    else
    {
        if (keyboard_keystate[KEY_LSHIFT] == KEY_MOTION_DOWN ||
            keyboard_keystate[KEY_RSHIFT] == KEY_MOTION_DOWN)
        {
            return binding->shift_codepoint;
        }
        else if (keyboard_keystate[KEY_RALT] == KEY_MOTION_DOWN)
        {
            return binding->alt_codepoint;
        }
        else
        {
            return binding->regular_codepoint;
        }
    }
}

void keyboard_handle_key(key_t key, key_motion_t motion)
{
    if (key_is_valid(key))
    {
        if (motion == KEY_MOTION_DOWN)
        {
            if (keyboard_keystate[key] == KEY_MOTION_UP)
            {
                keyboard_event_t keyevent = {key, keyboad_get_codepoint(key)};
                message_t keypressed_event = message(KEYBOARD_KEYPRESSED, -1);
                message_set_payload(keypressed_event, keyevent);

                task_messaging_broadcast(task_kernel(), KEYBOARD_CHANNEL, &keypressed_event);
            }

            keyboard_event_t keyevent = {key, keyboad_get_codepoint(key)};
            message_t keypressed_event = message(KEYBOARD_KEYTYPED, -1);
            message_set_payload(keypressed_event, keyevent);

            task_messaging_broadcast(task_kernel(), KEYBOARD_CHANNEL, &keypressed_event);
        }
        else if (motion == KEY_MOTION_UP)
        {
            keyboard_event_t keyevent = {key, keyboad_get_codepoint(key)};
            message_t keypressed_event = message(KEYBOARD_KEYRELEASED, -1);
            message_set_payload(keypressed_event, keyevent);

            task_messaging_broadcast(task_kernel(), KEYBOARD_CHANNEL, &keypressed_event);
        }

        keyboard_keystate[key] = motion;
    }
    else
    {
        logger_warn("Invalide scancode %d", key);
    }
}

reg32_t keyboard_irq(reg32_t esp, processor_context_t *context)
{
    __unused(context);

    int byte = in8(0x60);

    if (keyboard_state == PS2KBD_STATE_NORMAL)
    {
        if (byte == PS2KBD_ESCAPE)
        {
            keyboard_state = PS2KBD_STATE_NORMAL_ESCAPED;
        }
        else
        {
            key_t key = byte & 0x7F;
            keyboard_handle_key(key, byte & 0x80 ? KEY_MOTION_UP : KEY_MOTION_DOWN);
        }
    }
    else if (keyboard_state == PS2KBD_STATE_NORMAL_ESCAPED)
    {
        keyboard_state = PS2KBD_STATE_NORMAL;

        key_t key = (byte & 0x7F) + 0x80;
        keyboard_handle_key(key, byte & 0x80 ? KEY_MOTION_UP : KEY_MOTION_DOWN);
    }

    return esp;
}

/* --- Public functions ----------------------------------------------------- */

keymap_t *keyboard_load_keymap(const char *path)
{
    Stream *kmfile = stream_open(path, OPEN_READ);

    if (kmfile == NULL)
    {
        return NULL;
    }

    FileState stat;
    stream_stat(kmfile, &stat);

    assert(stat.type == FILE_TYPE_REGULAR);

    logger_info("Allocating keymap of size %dkio", stat.size / 1024);
    keymap_t *keymap = malloc(stat.size);

    stream_read(kmfile, keymap, stat.size);

    stream_close(kmfile);

    return keymap;
}

error_t keyboard_FsOperationCall(FsNode *node, FsHandle *handle, int request, void *args)
{
    __unused(node);
    __unused(handle);

    if (request == KEYBOARD_CALL_SET_KEYMAP)
    {
        keyboard_set_keymap_args_t *size_and_keymap = args;
        keymap_t *new_keymap = size_and_keymap->keymap;

        atomic_begin();

        if (keyboard_keymap != NULL)
        {
            free(keyboard_keymap);
        }

        keyboard_keymap = malloc(size_and_keymap->size);
        memcpy(keyboard_keymap, new_keymap, size_and_keymap->size);

        atomic_end();

        return ERR_SUCCESS;
    }
    else if (request == KEYBOARD_CALL_GET_KEYMAP)
    {
        if (keyboard_keymap != NULL)
        {
            memcpy(args, keyboard_keymap, sizeof(keymap_t));

            return ERR_SUCCESS;
        }
        else
        {
            // FIXME: Maybe add another ERR_* for this error...
            return ERR_INPUTOUTPUT_ERROR;
        }
    }
    else
    {
        return ERR_INAPPROPRIATE_CALL_FOR_DEVICE;
    }
}

void keyboard_initialize()
{
    logger_info("Initializing keyboad...");

    keyboard_keymap = keyboard_load_keymap("/res/keyboard/en_us.kmap");

    irq_register(1, keyboard_irq);

    FsNode *keyboard_device = __create(FsNode);

    fsnode_init(keyboard_device, FSNODE_DEVICE);

    FSNODE(keyboard_device)->call = (FsOperationCall)keyboard_FsOperationCall;

    Path *keyboard_device_path = path(KEYBOARD_DEVICE);
    filesystem_link_and_take_ref(keyboard_device_path, keyboard_device);
    path_delete(keyboard_device_path);
}