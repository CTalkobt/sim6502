#include "keyboard_io.h"
#include "cia_io.h"
#include <string.h>

KeyboardMatrix::KeyboardMatrix() : m_host_shift_down(false) {
    reset();
    m_keymap = std::make_unique<KeymapTable>();
    m_keymap->load_builtin(MACHINE_C64);
}

void KeyboardMatrix::set_keymap(std::unique_ptr<KeymapTable> map) {
    if (map)
        m_keymap = std::move(map);
    else {
        m_keymap = std::make_unique<KeymapTable>();
        m_keymap->load_builtin(MACHINE_C64);
    }
}

void KeyboardMatrix::reset() {
    for (int i = 0; i < 8; i++) m_matrix[i] = 0xFF;  /* all released */
    m_host_shift_down = false;
}

void KeyboardMatrix::set_key(uint8_t row, uint8_t col, bool pressed) {
    if (row >= 8 || col >= 8) return;
    if (pressed)
        m_matrix[row] &= (uint8_t)~(1u << col);  /* bit clear = pressed */
    else
        m_matrix[row] |= (uint8_t)(1u << col);    /* bit set   = released */
}

void KeyboardMatrix::key_down(int keycode, bool shifted) {
    /* Track host shift so we can undo injected shift on key-up. */
    if (keycode == KBKEY_SHIFT)
        m_host_shift_down = true;

    if (!m_keymap) return;
    KeymapTable::MatrixAction action;
    if (!m_keymap->translate(keycode, shifted, action) || !action.valid)
        return;

    set_key(action.row, action.col, true);

    if (action.press_shift) {
        /* Force LSHIFT into matrix regardless of host shift state. */
        set_key(m_keymap->lshift_row(), m_keymap->lshift_col(), true);
    } else if (action.release_shift) {
        /* Strip shift from matrix even if host shift is held. */
        set_key(m_keymap->lshift_row(), m_keymap->lshift_col(), false);
    }
}

void KeyboardMatrix::key_up(int keycode, bool shifted) {
    if (keycode == KBKEY_SHIFT)
        m_host_shift_down = false;

    if (!m_keymap) return;
    KeymapTable::MatrixAction action;
    if (!m_keymap->translate(keycode, shifted, action) || !action.valid)
        return;

    set_key(action.row, action.col, false);

    if (action.press_shift && !m_host_shift_down) {
        /* Undo injected shift only when host shift is not still held. */
        set_key(m_keymap->lshift_row(), m_keymap->lshift_col(), false);
    } else if (action.release_shift && m_host_shift_down) {
        /* Restore shift now that the deshifting key was released. */
        set_key(m_keymap->lshift_row(), m_keymap->lshift_col(), true);
    }
}

void KeyboardMatrix::push_to_cia(CIAHandler* cia) const {
    if (!cia) return;
    for (int i = 0; i < 8; i++)
        cia->set_keyboard_row(i, m_matrix[i]);
}
