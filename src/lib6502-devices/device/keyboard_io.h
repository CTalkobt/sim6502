#ifndef KEYBOARD_IO_H
#define KEYBOARD_IO_H

#include "keymap.h"
#include <stdint.h>
#include <memory>

class CIAHandler;  /* forward declaration -- avoid circular includes */

/*
 * KeyboardMatrix — tracks which host keys are currently pressed and
 * maintains a C64-compatible 8x8 matrix (bit clear = key pressed).
 *
 * Consumers call key_down/key_up when host key events arrive, then
 * call push_to_cia() to write the current state into CIA1.
 */
class KeyboardMatrix {
public:
    KeyboardMatrix();

    /* Attach a keymap.  If nullptr is passed the built-in C64 map is loaded. */
    void set_keymap(std::unique_ptr<KeymapTable> map);

    /* Call these from key event handlers (keycodes are KBKEY_* / ASCII). */
    void key_down(int keycode, bool shifted);
    void key_up  (int keycode, bool shifted);

    /* Push all 8 matrix rows into CIA1. */
    void push_to_cia(CIAHandler* cia) const;

    /* Read a single matrix row (for testing / scripted inspection). */
    uint8_t get_row(int row) const {
        return (row >= 0 && row < 8) ? m_matrix[row] : 0xFF;
    }

    /* Release all keys. */
    void reset();

private:
    std::unique_ptr<KeymapTable> m_keymap;
    uint8_t m_matrix[8];        /* bit clear = key pressed */
    bool    m_host_shift_down;  /* SHIFT key currently held by host */

    void set_key(uint8_t row, uint8_t col, bool pressed);
};

#endif
