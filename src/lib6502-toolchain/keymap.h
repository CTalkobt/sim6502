#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdint.h>
#include <string>
#include <vector>
#include "machine.h"

/*
 * Key code constants that match wxKeyCode values so the GUI can pass
 * wx key events through without translation.  Non-printable keys use
 * the wx WXK_* values; printable chars use their ASCII values.
 */
enum {
    KBKEY_BACK      =   8,   /* WXK_BACK      */
    KBKEY_TAB       =   9,   /* WXK_TAB       */
    KBKEY_RETURN    =  13,   /* WXK_RETURN    */
    KBKEY_ESCAPE    =  27,   /* WXK_ESCAPE    */
    KBKEY_SPACE     =  32,   /* WXK_SPACE     */
    KBKEY_DELETE    = 127,   /* WXK_DELETE    */
    KBKEY_SHIFT     = 306,   /* WXK_SHIFT     */
    KBKEY_ALT       = 307,   /* WXK_ALT       */
    KBKEY_CTRL      = 308,   /* WXK_CONTROL   */
    KBKEY_END       = 312,   /* WXK_END       */
    KBKEY_HOME      = 313,   /* WXK_HOME      */
    KBKEY_LEFT      = 314,   /* WXK_LEFT      */
    KBKEY_UP        = 315,   /* WXK_UP        */
    KBKEY_RIGHT     = 316,   /* WXK_RIGHT     */
    KBKEY_DOWN      = 317,   /* WXK_DOWN      */
    KBKEY_F1        = 340,   /* WXK_F1        */
    KBKEY_F2        = 341,
    KBKEY_F3        = 342,
    KBKEY_F4        = 343,
    KBKEY_F5        = 344,
    KBKEY_F6        = 345,
    KBKEY_F7        = 346,
    KBKEY_F8        = 347,
    KBKEY_SCROLL    = 366,   /* WXK_SCROLL (Scroll Lock) -- release shortcut */
};

/* Shiftflag bits used in keymap entries (subset of VICE .vkm semantics). */
enum {
    KMF_DESHIFT      = 0x01,  /* force-remove host shift from C64 matrix    */
    KMF_INJECT_SHIFT = 0x02,  /* force-add C64 LSHIFT to matrix             */
    KMF_IS_SHIFT     = 0x20,  /* this entry is a shift-modifier key itself  */
};

struct KeymapEntry {
    int     host_keycode;
    bool    host_shifted;   /* preferred host shift state for this entry    */
    uint8_t row, col;       /* C64 matrix position                          */
    uint8_t shiftflag;
};

class KeymapTable {
public:
    KeymapTable() { reset_shift_positions(); }

    struct MatrixAction {
        uint8_t row, col;
        bool    press_shift;    /* force C64 LSHIFT into matrix              */
        bool    release_shift;  /* force C64 LSHIFT out of matrix            */
        bool    valid;
    };

    void load_builtin(machine_type_t machine);
    bool load_vice_vkm(const std::string& path);

    bool translate(int host_keycode, bool shifted, MatrixAction& out) const;

    /* Find the best VICE .vkm file for the given machine under vice_data_dir.
     * Returns empty string if nothing suitable was found. */
    static std::string find_vice_vkm(const std::string& vice_data_dir,
                                     machine_type_t machine,
                                     bool prefer_symbolic = true);

    void clear() { m_entries.clear(); reset_shift_positions(); }

    uint8_t lshift_row() const { return m_lshift_row; }
    uint8_t lshift_col() const { return m_lshift_col; }

private:
    std::vector<KeymapEntry> m_entries;
    uint8_t m_lshift_row, m_lshift_col;
    uint8_t m_rshift_row, m_rshift_col;

    void reset_shift_positions() {
        m_lshift_row = 1; m_lshift_col = 7;  /* C64 defaults */
        m_rshift_row = 6; m_rshift_col = 4;
    }
    void parse_vkm_line(const std::string& line);
    static int vice_keysym_to_kbkey(const std::string& name);
};

#endif
