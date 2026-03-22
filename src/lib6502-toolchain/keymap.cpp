#include "keymap.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fstream>
#include <sstream>
#include <algorithm>

/* --------------------------------------------------------------------------
 * Built-in C64 keyboard matrix layout (row, col):
 *
 * Row 0: DEL(0) RET(1) CurDn(2) F7(3)  F1(4)  F3(5)  F5(6)  CurRt(7)
 * Row 1: '3'(0) 'W'(1) 'A'(2)   '4'(3) 'Z'(4) 'S'(5) 'E'(6) LSHIFT(7)
 * Row 2: '5'(0) 'R'(1) 'D'(2)   '6'(3) 'C'(4) 'F'(5) 'T'(6) 'X'(7)
 * Row 3: '7'(0) 'Y'(1) 'G'(2)   '8'(3) 'B'(4) 'H'(5) 'U'(6) 'V'(7)
 * Row 4: '9'(0) 'I'(1) 'J'(2)   '0'(3) 'M'(4) 'K'(5) 'O'(6) 'N'(7)
 * Row 5: '+'(0) 'P'(1) 'L'(2)   '-'(3) '.'(4) ':'(5) '@'(6) ','(7)
 * Row 6: GBP(0) '*'(1) ';'(2)   CLR(3) RSH(4) '='(5) '^'(6) '/'(7)
 * Row 7: '1'(0) '<-'(1) CTL(2)  '2'(3) SPC(4) CBM(5) 'Q'(6) STOP(7)
 * -------------------------------------------------------------------------- */

static const struct {
    int     keycode;
    bool    shifted;
    uint8_t row, col;
    uint8_t shiftflag;
} c64_builtin[] = {
    /* Letters — wx returns uppercase 'A'-'Z', shift propagates naturally */
    {'A', false, 1, 2, 0}, {'B', false, 3, 4, 0}, {'C', false, 2, 4, 0},
    {'D', false, 2, 2, 0}, {'E', false, 1, 6, 0}, {'F', false, 2, 5, 0},
    {'G', false, 3, 2, 0}, {'H', false, 3, 5, 0}, {'I', false, 4, 1, 0},
    {'J', false, 4, 2, 0}, {'K', false, 4, 5, 0}, {'L', false, 5, 2, 0},
    {'M', false, 4, 4, 0}, {'N', false, 4, 7, 0}, {'O', false, 4, 6, 0},
    {'P', false, 5, 1, 0}, {'Q', false, 7, 6, 0}, {'R', false, 2, 1, 0},
    {'S', false, 1, 5, 0}, {'T', false, 2, 6, 0}, {'U', false, 3, 6, 0},
    {'V', false, 3, 7, 0}, {'W', false, 1, 1, 0}, {'X', false, 2, 7, 0},
    {'Y', false, 3, 1, 0}, {'Z', false, 1, 4, 0},
    /* Digits */
    {'0', false, 4, 3, 0}, {'1', false, 7, 0, 0}, {'2', false, 7, 3, 0},
    {'3', false, 1, 0, 0}, {'4', false, 1, 3, 0}, {'5', false, 2, 0, 0},
    {'6', false, 2, 3, 0}, {'7', false, 3, 0, 0}, {'8', false, 3, 3, 0},
    {'9', false, 4, 0, 0},
    /* Special keys */
    {KBKEY_RETURN,  false, 0, 1, 0},
    {KBKEY_BACK,    false, 0, 0, 0},   /* DEL/INS key on C64 */
    {KBKEY_DELETE,  false, 0, 0, 0},
    {KBKEY_SPACE,   false, 7, 4, 0},
    {KBKEY_ESCAPE,  false, 7, 7, 0},   /* maps to RUN/STOP */
    {KBKEY_SHIFT,   false, 1, 7, KMF_IS_SHIFT},
    {KBKEY_CTRL,    false, 7, 2, 0},
    {KBKEY_HOME,    false, 6, 3, 0},   /* CLR/HOME */
    /* Cursor keys: Down/Right direct; Up/Left need shift injection */
    {KBKEY_DOWN,    false, 0, 2, 0},
    {KBKEY_RIGHT,   false, 0, 7, 0},
    {KBKEY_UP,      false, 0, 2, KMF_INJECT_SHIFT},
    {KBKEY_LEFT,    false, 0, 7, KMF_INJECT_SHIFT},
    /* Function keys: F2/F4/F6/F8 are Shift + F1/F3/F5/F7 on C64 */
    {KBKEY_F1, false, 0, 4, 0},
    {KBKEY_F2, false, 0, 4, KMF_INJECT_SHIFT},
    {KBKEY_F3, false, 0, 5, 0},
    {KBKEY_F4, false, 0, 5, KMF_INJECT_SHIFT},
    {KBKEY_F5, false, 0, 6, 0},
    {KBKEY_F6, false, 0, 6, KMF_INJECT_SHIFT},
    {KBKEY_F7, false, 0, 3, 0},
    {KBKEY_F8, false, 0, 3, KMF_INJECT_SHIFT},
    /* Common symbols accessible on US layout */
    {'-',  false, 5, 3, 0},   /* minus  */
    {'=',  false, 6, 5, 0},   /* equals */
    {',',  false, 5, 7, 0},   /* comma  */
    {'.',  false, 5, 4, 0},   /* period */
    {'/',  false, 6, 7, 0},   /* slash  */
    {';',  false, 5, 5, 0},   /* maps to C64 ':' key (unshifted) */
    {'\'', false, 6, 2, 0},   /* maps to C64 ';' key */
};

void KeymapTable::load_builtin(machine_type_t machine) {
    clear();
    if (machine == MACHINE_C64 || machine == MACHINE_C128 ||
        machine == MACHINE_X16 || machine == MACHINE_MEGA65) {
        int n = (int)(sizeof(c64_builtin) / sizeof(c64_builtin[0]));
        m_entries.reserve(n);
        for (int i = 0; i < n; i++) {
            KeymapEntry e;
            e.host_keycode  = c64_builtin[i].keycode;
            e.host_shifted  = c64_builtin[i].shifted;
            e.row           = c64_builtin[i].row;
            e.col           = c64_builtin[i].col;
            e.shiftflag     = c64_builtin[i].shiftflag;
            m_entries.push_back(e);
        }
        m_lshift_row = 1; m_lshift_col = 7;
        m_rshift_row = 6; m_rshift_col = 4;
    }
}

bool KeymapTable::translate(int host_keycode, bool shifted, MatrixAction& out) const {
    const KeymapEntry *best = nullptr;
    for (const auto& e : m_entries) {
        if (e.host_keycode != host_keycode) continue;
        if (e.host_shifted == shifted) {
            best = &e;
            break;  /* exact match */
        }
        if (!best) best = &e;  /* fallback */
    }
    if (!best) {
        out.valid = false;
        return false;
    }
    out.valid         = true;
    out.row           = best->row;
    out.col           = best->col;
    out.press_shift   = (best->shiftflag & KMF_INJECT_SHIFT) != 0;
    out.release_shift = (best->shiftflag & KMF_DESHIFT)      != 0;
    return true;
}

/* --------------------------------------------------------------------------
 * VICE keysym-name to KBKEY constant lookup
 * -------------------------------------------------------------------------- */

static const struct { const char *name; int kbkey; } s_keysym_map[] = {
    /* Named special keys */
    { "Return",       KBKEY_RETURN  },
    { "BackSpace",    KBKEY_BACK    },
    { "Escape",       KBKEY_ESCAPE  },
    { "Tab",          KBKEY_TAB     },
    { "space",        KBKEY_SPACE   },
    { "Delete",       KBKEY_DELETE  },
    { "Insert",       KBKEY_DELETE  },  /* map Insert to DEL too */
    { "Home",         KBKEY_HOME    },
    { "End",          KBKEY_END     },
    { "Up",           KBKEY_UP      },
    { "Down",         KBKEY_DOWN    },
    { "Left",         KBKEY_LEFT    },
    { "Right",        KBKEY_RIGHT   },
    { "F1",           KBKEY_F1      },
    { "F2",           KBKEY_F2      },
    { "F3",           KBKEY_F3      },
    { "F4",           KBKEY_F4      },
    { "F5",           KBKEY_F5      },
    { "F6",           KBKEY_F6      },
    { "F7",           KBKEY_F7      },
    { "F8",           KBKEY_F8      },
    { "F9",           349           },
    { "F10",          350           },
    { "F11",          351           },
    { "F12",          352           },
    { "Scroll_Lock",  KBKEY_SCROLL  },
    /* Modifier keys */
    { "Shift_L",      KBKEY_SHIFT   },
    { "Shift_R",      KBKEY_SHIFT   },  /* both map to same C64 shift */
    { "Control_L",    KBKEY_CTRL    },
    { "Control_R",    KBKEY_CTRL    },
    { "Alt_L",        KBKEY_ALT     },
    { "Alt_R",        KBKEY_ALT     },
    /* Symbol keysyms */
    { "minus",        '-'  },
    { "plus",         '+'  },
    { "equal",        '='  },
    { "slash",        '/'  },
    { "backslash",    '\\' },
    { "period",       '.'  },
    { "comma",        ','  },
    { "colon",        ':'  },
    { "semicolon",    ';'  },
    { "apostrophe",   '\'' },
    { "quotedbl",     '"'  },
    { "at",           '@'  },
    { "numbersign",   '#'  },
    { "dollar",       '$'  },
    { "percent",      '%'  },
    { "ampersand",    '&'  },
    { "asterisk",     '*'  },
    { "parenleft",    '('  },
    { "parenright",   ')'  },
    { "underscore",   '_'  },
    { "exclam",       '!'  },
    { "question",     '?'  },
    { "greater",      '>'  },
    { "less",         '<'  },
    { "bracketleft",  '['  },
    { "bracketright", ']'  },
    { "braceleft",    '{'  },
    { "braceright",   '}'  },
    { "bar",          '|'  },
    { "grave",        '`'  },
    { "asciitilde",   '~'  },
    { "asciicircum",  '^'  },
    { "sterling",     '#'  },   /* GBP sign -- nearest accessible key */
    { "leftarrow",    KBKEY_LEFT   },   /* C64 left-arrow key (backtick pos) */
    { "pi",           'P'  },
};

int KeymapTable::vice_keysym_to_kbkey(const std::string& name) {
    /* Single lowercase letter a-z */
    if (name.size() == 1 && islower((unsigned char)name[0]))
        return toupper((unsigned char)name[0]);

    /* Single digit */
    if (name.size() == 1 && isdigit((unsigned char)name[0]))
        return (unsigned char)name[0];

    /* Single printable symbol */
    if (name.size() == 1 && ispunct((unsigned char)name[0]))
        return (unsigned char)name[0];

    /* Named keys */
    int n = (int)(sizeof(s_keysym_map) / sizeof(s_keysym_map[0]));
    for (int i = 0; i < n; i++) {
        if (name == s_keysym_map[i].name)
            return s_keysym_map[i].kbkey;
    }
    return -1;
}

/* --------------------------------------------------------------------------
 * VICE .vkm file parser
 * -------------------------------------------------------------------------- */

void KeymapTable::parse_vkm_line(const std::string& line) {
    /* Skip blank lines and comments */
    size_t p = 0;
    while (p < line.size() && isspace((unsigned char)line[p])) p++;
    if (p >= line.size() || line[p] == '#') return;

    /* Handle directives */
    if (line[p] == '!') {
        std::string directive = line.substr(p);
        std::istringstream ss(directive);
        std::string token;
        ss >> token;
        if (token == "!CLEAR") {
            m_entries.clear();
        } else if (token == "!LSHIFT") {
            int row, col;
            if (ss >> row >> col) {
                m_lshift_row = (uint8_t)row;
                m_lshift_col = (uint8_t)col;
            }
        } else if (token == "!RSHIFT") {
            int row, col;
            if (ss >> row >> col) {
                m_rshift_row = (uint8_t)row;
                m_rshift_col = (uint8_t)col;
            }
        }
        /* !INCLUDE, !VSHIFT, !SHIFTL etc. ignored for now */
        return;
    }

    /* Key mapping line: KeysymName row col shiftflag [comment...] */
    /* Strip inline comment first */
    std::string data = line.substr(p);
    size_t comment = data.find('#');
    if (comment != std::string::npos) data = data.substr(0, comment);

    std::istringstream ss(data);
    std::string keysym;
    int row, col, shiftflag;
    if (!(ss >> keysym >> row >> col >> shiftflag)) return;

    int kbkey = vice_keysym_to_kbkey(keysym);
    if (kbkey < 0) return;  /* unknown keysym */
    if (row < 0 || row > 7 || col < 0 || col > 7) return;

    KeymapEntry e;
    e.host_keycode = kbkey;
    e.host_shifted = false;
    e.row          = (uint8_t)row;
    e.col          = (uint8_t)col;
    e.shiftflag    = (uint8_t)(shiftflag & 0xFF);
    m_entries.push_back(e);
}

bool KeymapTable::load_vice_vkm(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    clear();
    std::string line;
    while (std::getline(f, line)) {
        parse_vkm_line(line);
    }
    return !m_entries.empty();
}

/* --------------------------------------------------------------------------
 * VICE .vkm file discovery
 * -------------------------------------------------------------------------- */

static const char *machine_dirname(machine_type_t m) {
    switch (m) {
        case MACHINE_C64:   return "C64";
        case MACHINE_C128:  return "C128";
        case MACHINE_X16:   return "C64";  /* best approximation */
        case MACHINE_MEGA65:return "C64";  /* no VICE mega65 keymap */
        default:            return "C64";
    }
}

std::string KeymapTable::find_vice_vkm(const std::string& vice_data_dir,
                                        machine_type_t machine,
                                        bool prefer_symbolic) {
    if (vice_data_dir.empty()) return {};

    const char *mdir = machine_dirname(machine);

    /* Search order: gtk3_sym > gtk3_pos > sdl_sym > sdl_pos */
    const char *candidates[] = {
        "gtk3_sym.vkm", "gtk3_pos.vkm", "sdl_sym.vkm", "sdl_pos.vkm",
        "gtk3_sym_us.vkm", "sdl_sym_us.vkm",
        nullptr
    };
    if (!prefer_symbolic) {
        candidates[0] = "gtk3_pos.vkm";
        candidates[1] = "gtk3_sym.vkm";
        candidates[2] = "sdl_pos.vkm";
        candidates[3] = "sdl_sym.vkm";
    }

    for (int i = 0; candidates[i]; i++) {
        std::string path = vice_data_dir + "/" + mdir + "/" + candidates[i];
        FILE *f = fopen(path.c_str(), "r");
        if (f) { fclose(f); return path; }
    }
    return {};
}
