/*
 * sim6502-gui — Phase 4
 *
 * Main entry point for the GUI binary.  Integrates with the simulator core
 * via the sim_api.h library interface (sim_session_t *).
 *
 * Phase 1 features:
 *   - Toolbar: Load, Step, Run/Pause, Reset, Processor selector
 *   - Status bar: state / PC+registers / filename
 *   - Default docking layout: Registers | Disassembly / Memory | Console
 *   - Keyboard shortcuts: F5 run, F6/Esc pause, F7 step, Ctrl+R reset, Ctrl+L load
 *   - Persistent font size, window position and size
 *
 * Phase 2 additions:
 *   - Register pane: compact/expanded modes, diff highlighting, click-to-edit
 *   - Disassembly pane: table layout, clickable BP gutter, cycle counts, symbols
 *   - Memory pane: last-write byte highlighting, follow-PC/SP buttons
 *   - Console pane: scrollable log, command history, full CLI interpreter
 *
 * Build: make gui
 * Dependencies: SDL2, OpenGL 3.3, Dear ImGui (docking branch)
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

extern "C" {
#include "cpu.h"
#include "sim_api.h"
}

/* --------------------------------------------------------------------------
 * Core globals
 * -------------------------------------------------------------------------- */
static sim_session_t *g_sim     = NULL;
static bool  g_running          = false;  /* true = run mode each frame */
static int   g_speed            = 5000;   /* instructions per frame */

/* Font / DPI state */
static float g_ui_scale         = 1.0f;
static int   g_base_font_size   = 13;
static bool  g_font_rebuild     = false;

/* SDL window handle — used by INI write callback */
static SDL_Window *g_window = nullptr;

/* Toolbar state */
static char g_filename_buf[512] = "";
static bool g_focus_filename    = false;

static const char *g_proc_labels[] = { "6502", "6502-undoc", "65C02", "65CE02", "45GS02" };
static const char *g_proc_ids[]    = { "6502", "6502-undoc", "65c02", "65ce02", "45gs02" };
static int  g_proc_idx             = 0;

/* Pane visibility */
static bool show_registers   = true;
static bool show_disassembly = true;
static bool show_memory      = true;
static bool show_console     = true;

/* --------------------------------------------------------------------------
 * Phase 2 state
 * -------------------------------------------------------------------------- */

/* Register diff — snapshot CPU before each sim_step call */
static cpu_t g_prev_cpu       = {};
static bool  g_prev_cpu_valid = false;

/* Registers pane */
static bool g_regs_expanded = false;

/* Disassembly view */
static uint16_t g_disasm_addr   = 0x0200;
static bool     g_disasm_follow = true;

/* Memory view — last-write log (populated after each sim_step) */
static uint16_t g_last_writes[256];
static int      g_last_write_count = 0;

/* ---- Phase 3 pane visibility ---- */
static bool show_breakpoints = false;
static bool show_trace       = false;
static bool show_stack       = false;
static bool show_watches     = false;

/* ---- Watch list (GUI-side only) ---- */
#define WATCH_MAX 16
struct WatchEntry {
    uint16_t addr;
    char     name[32];
    uint8_t  cur_val;
    uint8_t  prev_val;
    bool     changed;
    bool     valid;   /* false = not yet read */
};
static WatchEntry g_watches[WATCH_MAX];
static int        g_watch_count = 0;

/* ---- Phase 4 pane visibility ---- */
static bool show_iref     = false;
static bool show_symbols  = false;
static bool show_source   = false;
static bool show_profiler = false;

/* ---- Phase 5: keyboard shortcut state ---- */
static bool     g_focus_console     = false;  /* `: focus console input              */
static bool     g_focus_disasm_addr = false;  /* Ctrl+Shift+F: focus disasm addr bar */
static bool     g_focus_src_search  = false;  /* Ctrl+F (source): focus search bar   */
static bool     g_goto_open         = false;  /* Ctrl+G: go-to-address popup         */
static char     g_goto_buf[8]       = "";

/* F8 step-over: temp breakpoint tracking */
static bool     g_step_over_active  = false;
static uint16_t g_step_over_bp      = 0;

/* ---- Binary load dialog (toolbar: .bin / .prg) ---- */
static bool     g_binload_open     = false;
static char     g_binload_path[512] = "";
static char     g_binload_addr[8]   = "0200";
static bool     g_binload_is_prg   = false;
static bool     g_binload_override = false;   /* PRG: use g_binload_addr instead of header */
static uint16_t g_binload_header   = 0;       /* PRG: address read from file header */

/* ---- Binary save dialog ---- */
static bool     g_binsave_open      = false;
static char     g_binsave_path[512] = "";
static char     g_binsave_start[8]  = "0200";
static char     g_binsave_count[8]  = "0100";

/* ---- Instruction Reference ---- */
static char g_iref_filter[32] = "";
static int  g_iref_sel_idx    = -1;

/* ---- Symbol Browser ---- */
static char g_sym_filter[64] = "";

/* ---- Source Viewer ---- */
#define SRC_MAX_LINES    4096
#define SRC_MAX_LINE_LEN  256
static char g_src_lines[SRC_MAX_LINES][SRC_MAX_LINE_LEN];
static int  g_src_line_count    = 0;
static char g_src_loaded_file[512] = "";
static char g_src_search[64]    = "";
static int  g_src_search_hit    = -1;

/* ---- Heatmap texture (profiler) ---- */
static GLuint g_heatmap_tex   = 0;
static bool   g_heatmap_dirty = true;

/* Console */
#define CON_MAX_LINES   1024
#define CON_MAX_HISTORY   64

struct ConLine { char text[256]; ImVec4 color; };

static ConLine g_con_lines[CON_MAX_LINES];
static int     g_con_line_count    = 0;
static char    g_con_input[256]    = "";
static bool    g_con_scroll_bottom = true;
static int     g_con_history_pos   = -1;
static char    g_con_history_buf[CON_MAX_HISTORY][256];
static int     g_con_history_count = 0;

/* Console text colours (non-const; updated by apply_theme) */
static ImVec4 CON_COL_NORMAL = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
static ImVec4 CON_COL_CMD    = ImVec4(0.6f,  0.8f,  1.0f,  1.0f);
static ImVec4 CON_COL_OK     = ImVec4(0.4f,  1.0f,  0.4f,  1.0f);
static ImVec4 CON_COL_ERR    = ImVec4(1.0f,  0.4f,  0.4f,  1.0f);
static ImVec4 CON_COL_WARN   = ImVec4(1.0f,  0.85f, 0.3f,  1.0f);

/* --------------------------------------------------------------------------
 * Theme  (0 = Dark, 1 = Light, 2 = Auto / follow OS)
 * -------------------------------------------------------------------------- */
static int  g_theme         = 2;    /* default: auto-detect */
static bool g_theme_rebuild = false;

struct AppColors {
    ImVec4 text_normal;        /* console default / general text           */
    ImVec4 text_cmd;           /* console command echo (cyan-blue)         */
    ImVec4 text_ok;            /* success, running, flag set (green)       */
    ImVec4 text_err;           /* error, active breakpoint dot (red)       */
    ImVec4 text_warn;          /* warning (yellow/gold)                    */
    ImVec4 text_warn_orange;   /* EOM prefix, FINISHED state (orange)      */
    ImVec4 text_changed;       /* changed register / PC cursor (bright Y)  */
    ImVec4 flag_dim;           /* cleared CPU flag (dim gray)              */
    ImVec4 bp_empty;           /* empty BP gutter dot (very dim)           */
    ImVec4 bp_text;            /* disasm text on a breakpoint line         */
    ImVec4 mnemonic;           /* instruction mnemonic (blue)              */
    ImVec4 symbol_label;       /* symbol in disasm gutter (green)          */
    ImVec4 source_comment;     /* source viewer comment text (green)       */
    ImVec4 source_label;       /* source viewer label / mnemonic (gold)    */
    ImVec4 highlight_blue_bg;  /* current-PC row background (blue tint)    */
    ImVec4 highlight_green_bg; /* current-SP row background (green tint)   */
    ImVec4 gl_clear;           /* OpenGL window clear colour               */
};
static AppColors g_col;

/* --------------------------------------------------------------------------
 * Font rebuild
 * -------------------------------------------------------------------------- */
static void rebuild_font(void)
{
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig font_cfg;
    font_cfg.SizePixels = floorf((float)g_base_font_size * g_ui_scale);
    io.Fonts->AddFontDefault(&font_cfg);
    io.Fonts->Build();
}

/* --------------------------------------------------------------------------
 * UI scale detection
 * -------------------------------------------------------------------------- */
static float detect_ui_scale(int display_index)
{
    const char *env = SDL_getenv("SIM6502_SCALE");
    if (env) { float v = (float)SDL_atof(env); if (v >= 0.5f && v <= 8.0f) return v; }

    const char *gdk = SDL_getenv("GDK_SCALE");
    if (gdk) { float v = (float)SDL_atof(gdk); if (v >= 1.0f && v <= 8.0f) return v; }

    const char *qt = SDL_getenv("QT_SCALE_FACTOR");
    if (qt)  { float v = (float)SDL_atof(qt);  if (v >= 1.0f && v <= 8.0f) return v; }

    float ddpi = 0.0f, hdpi = 0.0f, vdpi = 0.0f;
    if (SDL_GetDisplayDPI(display_index, &ddpi, &hdpi, &vdpi) == 0
            && hdpi > 0.0f && hdpi != 96.0f)
        return floorf(hdpi / 96.0f * 4.0f + 0.5f) / 4.0f;

    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(display_index, &mode) == 0) {
        if (mode.w >= 3840) return 2.00f;
        if (mode.w >= 2560) return 1.50f;
    }
    return 1.0f;
}

/* --------------------------------------------------------------------------
 * OS dark-mode detection
 * Returns true if the OS/desktop is using a dark colour scheme.
 * -------------------------------------------------------------------------- */
static bool detect_os_is_dark(void)
{
    /* 1. GTK_THEME env var (e.g. "Adwaita:dark") */
    const char *gtk_theme = SDL_getenv("GTK_THEME");
    if (gtk_theme) {
        if (strstr(gtk_theme, "dark") || strstr(gtk_theme, "Dark")) return true;
        return false;
    }

    /* 3. GNOME: gsettings color-scheme */
    {
        FILE *f = popen("gsettings get org.gnome.desktop.interface color-scheme"
                        " 2>/dev/null", "r");
        if (f) {
            char buf[64] = "";
            if (fgets(buf, sizeof(buf), f)) { /* consume */ }
            pclose(f);
            if (buf[0] != '\0') {
                if (strstr(buf, "dark") || strstr(buf, "Dark")) return true;
                return false;
            }
        }
    }

    /* 4. KDE: kdeglobals BackgroundNormal luminance */
    {
        const char *home = SDL_getenv("HOME");
        if (home) {
            char path[512];
            snprintf(path, sizeof(path), "%s/.config/kdeglobals", home);
            FILE *f = fopen(path, "r");
            if (f) {
                char ln[256];
                bool in_colors = false;
                while (fgets(ln, sizeof(ln), f)) {
                    if (strncmp(ln, "[Colors:Window]", 15) == 0) {
                        in_colors = true; continue;
                    }
                    if (ln[0] == '[') { in_colors = false; continue; }
                    if (in_colors) {
                        int r = 0, g = 0, b = 0;
                        if (sscanf(ln, "BackgroundNormal=%d,%d,%d", &r, &g, &b) == 3) {
                            fclose(f);
                            return (r * 299 + g * 587 + b * 114) / 1000 < 128;
                        }
                    }
                }
                fclose(f);
            }
        }
    }

    /* Default: dark */
    return true;
}

/* --------------------------------------------------------------------------
 * apply_theme: reset ImGui style, apply dark/light colours, platform tweaks
 * -------------------------------------------------------------------------- */
static void apply_theme(void)
{
    bool dark;
    if      (g_theme == 0) dark = true;
    else if (g_theme == 1) dark = false;
    else                   dark = detect_os_is_dark();

    /* Reset style to ImGui defaults, then apply colour scheme */
    ImGui::GetStyle() = ImGuiStyle();

    if (dark) {
        ImGui::StyleColorsDark();

        g_col.text_normal       = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        g_col.text_cmd          = ImVec4(0.6f,  0.8f,  1.0f,  1.0f);
        g_col.text_ok           = ImVec4(0.4f,  1.0f,  0.4f,  1.0f);
        g_col.text_err          = ImVec4(1.0f,  0.4f,  0.4f,  1.0f);
        g_col.text_warn         = ImVec4(1.0f,  0.85f, 0.3f,  1.0f);
        g_col.text_warn_orange  = ImVec4(1.0f,  0.6f,  0.2f,  1.0f);
        g_col.text_changed      = ImVec4(1.0f,  1.0f,  0.2f,  1.0f);
        g_col.flag_dim          = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
        g_col.bp_empty          = ImVec4(0.35f, 0.35f, 0.35f, 0.6f);
        g_col.bp_text           = ImVec4(1.0f,  0.6f,  0.6f,  1.0f);
        g_col.mnemonic          = ImVec4(0.4f,  0.8f,  1.0f,  1.0f);
        g_col.symbol_label      = ImVec4(0.5f,  0.9f,  0.5f,  1.0f);
        g_col.source_comment    = ImVec4(0.5f,  0.8f,  0.5f,  1.0f);
        g_col.source_label      = ImVec4(1.0f,  0.85f, 0.3f,  1.0f);
        g_col.highlight_blue_bg = ImVec4(0.3f,  0.4f,  0.8f,  0.35f);
        g_col.highlight_green_bg= ImVec4(0.3f,  0.6f,  0.3f,  0.25f);
        g_col.gl_clear          = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
    } else {
        ImGui::StyleColorsLight();

        g_col.text_normal       = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        g_col.text_cmd          = ImVec4(0.1f,  0.4f,  0.8f,  1.0f);
        g_col.text_ok           = ImVec4(0.1f,  0.55f, 0.1f,  1.0f);
        g_col.text_err          = ImVec4(0.8f,  0.1f,  0.1f,  1.0f);
        g_col.text_warn         = ImVec4(0.65f, 0.45f, 0.0f,  1.0f);
        g_col.text_warn_orange  = ImVec4(0.75f, 0.35f, 0.0f,  1.0f);
        g_col.text_changed      = ImVec4(0.5f,  0.35f, 0.0f,  1.0f);
        g_col.flag_dim          = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
        g_col.bp_empty          = ImVec4(0.65f, 0.65f, 0.65f, 0.8f);
        g_col.bp_text           = ImVec4(0.65f, 0.1f,  0.1f,  1.0f);
        g_col.mnemonic          = ImVec4(0.05f, 0.35f, 0.7f,  1.0f);
        g_col.symbol_label      = ImVec4(0.1f,  0.45f, 0.1f,  1.0f);
        g_col.source_comment    = ImVec4(0.1f,  0.45f, 0.1f,  1.0f);
        g_col.source_label      = ImVec4(0.5f,  0.3f,  0.0f,  1.0f);
        g_col.highlight_blue_bg = ImVec4(0.3f,  0.4f,  0.8f,  0.2f);
        g_col.highlight_green_bg= ImVec4(0.2f,  0.5f,  0.2f,  0.2f);
        g_col.gl_clear          = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
    }

    /* Update console colour variables to match theme */
    CON_COL_NORMAL = g_col.text_normal;
    CON_COL_CMD    = g_col.text_cmd;
    CON_COL_OK     = g_col.text_ok;
    CON_COL_ERR    = g_col.text_err;
    CON_COL_WARN   = g_col.text_warn;

    /* Per-platform style geometry tweaks */
    ImGuiStyle &style = ImGui::GetStyle();
#if defined(__APPLE__)
    style.WindowRounding   = 10.0f;
    style.FrameRounding    =  6.0f;
    style.PopupRounding    =  8.0f;
    style.ScrollbarRounding=  8.0f;
    style.GrabRounding     =  5.0f;
    style.TabRounding      =  6.0f;
    style.ScrollbarSize    = 14.0f;
    style.GrabMinSize      = 12.0f;
    style.WindowBorderSize =  1.0f;
    style.FrameBorderSize  =  0.0f;
#elif defined(_WIN32)
    style.WindowRounding   =  4.0f;
    style.FrameRounding    =  3.0f;
    style.PopupRounding    =  3.0f;
    style.ScrollbarRounding=  3.0f;
    style.GrabRounding     =  3.0f;
    style.TabRounding      =  4.0f;
    style.ScrollbarSize    = 14.0f;
    style.WindowBorderSize =  1.0f;
    style.FrameBorderSize  =  0.0f;
#else   /* Linux / GTK / KDE */
    style.WindowRounding   =  6.0f;
    style.FrameRounding    =  4.0f;
    style.PopupRounding    =  5.0f;
    style.ScrollbarRounding=  5.0f;
    style.GrabRounding     =  4.0f;
    style.TabRounding      =  5.0f;
    style.ScrollbarSize    = 12.0f;
    style.WindowBorderSize =  1.0f;
    style.FrameBorderSize  =  0.0f;
#endif

    style.ScaleAllSizes(g_ui_scale);
}

/* --------------------------------------------------------------------------
 * Default docking layout (applied once on first run)
 *
 *   ┌──────────┬──────────────────────┐
 *   │ Registers│                      │
 *   ├──────────┤    Disassembly       │
 *   │          │                      │
 *   │  Memory  ├──────────────────────┤
 *   │          │  Console             │
 *   └──────────┴──────────────────────┘
 * -------------------------------------------------------------------------- */
static void setup_default_layout(ImGuiID dockspace_id, ImVec2 size)
{
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, size);

    ImGuiID left_col, right_col;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.28f, &left_col, &right_col);

    ImGuiID left_top, left_bot;
    ImGui::DockBuilderSplitNode(left_col, ImGuiDir_Up, 0.38f, &left_top, &left_bot);

    ImGuiID right_top, right_bot;
    ImGui::DockBuilderSplitNode(right_col, ImGuiDir_Up, 0.72f, &right_top, &right_bot);

    ImGui::DockBuilderDockWindow("Registers",   left_top);
    ImGui::DockBuilderDockWindow("Memory",      left_bot);
    ImGui::DockBuilderDockWindow("Disassembly", right_top);
    ImGui::DockBuilderDockWindow("Console",     right_bot);

    ImGui::DockBuilderFinish(dockspace_id);
}

/* --------------------------------------------------------------------------
 * do_load: assemble and load the file named in g_filename_buf
 * -------------------------------------------------------------------------- */
static void update_watches(void); /* forward declarations — defined before draw_toolbar */
static void src_load(const char *path);

/* Case-insensitive extension check: ext must include the leading '.'. */
static bool ext_is(const char *path, const char *ext_lower)
{
    const char *e = strrchr(path, '.');
    if (!e) return false;
    for (const char *a = e, *b = ext_lower; *b; a++, b++)
        if (tolower((unsigned char)*a) != (unsigned char)*b) return false;
    return true;
}

/* Open the binary-load dialog for a .bin or .prg path. */
static void open_binload_dialog(const char *path)
{
    strncpy(g_binload_path, path, sizeof(g_binload_path) - 1);
    g_binload_is_prg   = ext_is(path, ".prg");
    g_binload_override = false;
    g_binload_header   = 0;
    if (g_binload_is_prg) {
        FILE *f = fopen(path, "rb");
        if (f) {
            int lo = fgetc(f), hi = fgetc(f);
            fclose(f);
            if (lo != EOF && hi != EOF)
                g_binload_header = (uint16_t)((unsigned)lo | ((unsigned)hi << 8));
        }
        snprintf(g_binload_addr, sizeof(g_binload_addr), "%04X", (unsigned)g_binload_header);
    } else {
        snprintf(g_binload_addr, sizeof(g_binload_addr), "0200");
    }
    g_binload_open = true;
}
static void do_load(void)
{
    if (g_filename_buf[0] == '\0') return;
    g_running = false;
    if (sim_load_asm(g_sim, g_filename_buf) != 0) {
        fprintf(stderr, "sim6502-gui: failed to load '%s'\n", g_filename_buf);
    } else {
        fprintf(stdout, "sim6502-gui: loaded '%s'\n", g_filename_buf);
        g_prev_cpu_valid   = false;
        g_last_write_count = 0;
        /* Re-seed watch values from the newly loaded program */
        for (int i = 0; i < g_watch_count; i++) g_watches[i].valid = false;
        update_watches();
        /* Phase 4: load source text + reset profiler view */
        src_load(g_filename_buf);
        g_heatmap_dirty = true;
    }
}

/* --------------------------------------------------------------------------
 * do_step_over: step past the current instruction.
 * For JSR abs ($20): sets a temp breakpoint at the return address and runs.
 * For all other instructions: single-steps like F7.
 * -------------------------------------------------------------------------- */
static void do_step_over(void)
{
    g_running = false;
    sim_state_t st = sim_get_state(g_sim);
    if (st != SIM_READY && st != SIM_PAUSED) return;
    cpu_t *cpu = sim_get_cpu(g_sim);
    if (!cpu) return;
    g_prev_cpu = *cpu; g_prev_cpu_valid = true;
    uint8_t op = sim_mem_read_byte(g_sim, cpu->pc);
    if (op == 0x20) { /* JSR abs: run to return address (PC+3) */
        uint16_t ret       = (uint16_t)(cpu->pc + 3);
        g_step_over_bp     = ret;
        g_step_over_active = true;
        sim_break_set(g_sim, ret, NULL);
        g_running = true;
    } else {
        sim_step(g_sim, 1);
        g_last_write_count = sim_get_last_writes(g_sim, g_last_writes, 256);
        update_watches();
    }
}

/* --------------------------------------------------------------------------
 * Console helpers
 * -------------------------------------------------------------------------- */
static void con_add(ImVec4 color, const char *fmt, ...)
{
    if (g_con_line_count >= CON_MAX_LINES) {
        memmove(g_con_lines, g_con_lines + 1,
                sizeof(g_con_lines[0]) * (CON_MAX_LINES - 1));
        g_con_line_count = CON_MAX_LINES - 1;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_con_lines[g_con_line_count].text,
              sizeof(g_con_lines[0].text), fmt, ap);
    va_end(ap);
    g_con_lines[g_con_line_count].color = color;
    g_con_line_count++;
    g_con_scroll_bottom = true;
}

/* InputText history callback */
static int con_input_cb(ImGuiInputTextCallbackData *data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        const int prev = g_con_history_pos;
        if (data->EventKey == ImGuiKey_UpArrow) {
            if (g_con_history_pos == -1)
                g_con_history_pos = g_con_history_count - 1;
            else if (g_con_history_pos > 0)
                g_con_history_pos--;
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (g_con_history_pos != -1)
                if (++g_con_history_pos >= g_con_history_count)
                    g_con_history_pos = -1;
        }
        if (prev != g_con_history_pos) {
            const char *h = (g_con_history_pos >= 0)
                            ? g_con_history_buf[g_con_history_pos] : "";
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, h);
        }
    }
    return 0;
}

/* Command interpreter */
static void con_exec(const char *cmd)
{
    con_add(CON_COL_CMD, "> %s", cmd);

    /* Add to history (deduplicate consecutive identical) */
    if (cmd[0] != '\0') {
        if (g_con_history_count == 0 ||
                strcmp(g_con_history_buf[g_con_history_count - 1], cmd) != 0) {
            if (g_con_history_count >= CON_MAX_HISTORY) {
                memmove(g_con_history_buf, g_con_history_buf + 1,
                        sizeof(g_con_history_buf[0]) * (CON_MAX_HISTORY - 1));
                g_con_history_count = CON_MAX_HISTORY - 1;
            }
            snprintf(g_con_history_buf[g_con_history_count],
                     sizeof(g_con_history_buf[0]), "%s", cmd);
            g_con_history_count++;
        }
    }
    g_con_history_pos = -1;

    /* Parse verb (first word) and args_rest (remainder of line) */
    char buf[256];
    strncpy(buf, cmd, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *verb = buf;
    while (*verb == ' ' || *verb == '\t') verb++;
    char *verb_end = verb;
    while (*verb_end && *verb_end != ' ' && *verb_end != '\t') verb_end++;
    char *args_rest = verb_end;
    while (*args_rest == ' ' || *args_rest == '\t') args_rest++;
    if (*verb_end) *verb_end = '\0';
    if (verb[0] == '\0') return;

    /* ---- Commands ---- */

    if (strcmp(verb, "help") == 0) {
        con_add(CON_COL_NORMAL, "Commands:");
        con_add(CON_COL_NORMAL, "  step [n]                     step n instructions (default 1)");
        con_add(CON_COL_NORMAL, "  run                          run continuously");
        con_add(CON_COL_NORMAL, "  pause                        pause execution");
        con_add(CON_COL_NORMAL, "  reset                        reset CPU to start address");
        con_add(CON_COL_NORMAL, "  regs                         print CPU registers");
        con_add(CON_COL_NORMAL, "  mem [addr] [n]               hex dump n bytes at addr");
        con_add(CON_COL_NORMAL, "  disasm [addr] [n]            disassemble n instructions");
        con_add(CON_COL_NORMAL, "  break <addr> [cond]          set breakpoint");
        con_add(CON_COL_NORMAL, "  del <addr>                   remove breakpoint");
        con_add(CON_COL_NORMAL, "  breaks                       list all breakpoints");
        con_add(CON_COL_NORMAL, "  load <path>                  assemble and load .asm file");
        con_add(CON_COL_NORMAL, "  bload <path> [addr]          load .bin (addr req) or .prg");
        con_add(CON_COL_NORMAL, "  bsave <path> <start> <end>   save memory range as .bin/.prg");
        con_add(CON_COL_NORMAL, "  cls                          clear console output");
        return;
    }

    if (strcmp(verb, "cls") == 0) {
        g_con_line_count = 0;
        return;
    }

    if (strcmp(verb, "step") == 0) {
        int n = args_rest[0] ? atoi(args_rest) : 1;
        if (n < 1) n = 1;
        sim_state_t st = sim_get_state(g_sim);
        if (st != SIM_READY && st != SIM_PAUSED) {
            con_add(CON_COL_ERR, "Cannot step (state: %s)", sim_state_name(st));
            return;
        }
        cpu_t *cpu = sim_get_cpu(g_sim);
        if (cpu) { g_prev_cpu = *cpu; g_prev_cpu_valid = true; }
        sim_step(g_sim, n);
        g_last_write_count = sim_get_last_writes(g_sim, g_last_writes, 256);
        cpu = sim_get_cpu(g_sim);
        if (cpu) {
            bool is45 = (sim_get_cpu_type(g_sim) == CPU_45GS02);
            if (is45)
                con_add(CON_COL_NORMAL,
                    "A=%02X X=%02X Y=%02X Z=%02X B=%02X  S=%04X PC=%04X  P=%02X  Cycles=%lu",
                    cpu->a, cpu->x, cpu->y, cpu->z, cpu->b,
                    cpu->s, cpu->pc, cpu->p, cpu->cycles);
            else
                con_add(CON_COL_NORMAL,
                    "A=%02X X=%02X Y=%02X  S=%04X PC=%04X  P=%02X  Cycles=%lu",
                    cpu->a, cpu->x, cpu->y, cpu->s, cpu->pc, cpu->p, cpu->cycles);
        }
        return;
    }

    if (strcmp(verb, "run") == 0) {
        sim_state_t st = sim_get_state(g_sim);
        if (st == SIM_READY || st == SIM_PAUSED) {
            g_running = true;
            con_add(CON_COL_OK, "Running...");
        } else {
            con_add(CON_COL_ERR, "Cannot run (state: %s)", sim_state_name(st));
        }
        return;
    }

    if (strcmp(verb, "pause") == 0) {
        g_running = false;
        con_add(CON_COL_WARN, "Paused.");
        return;
    }

    if (strcmp(verb, "reset") == 0) {
        g_running = false;
        sim_reset(g_sim);
        g_prev_cpu_valid   = false;
        g_last_write_count = 0;
        for (int i = 0; i < g_watch_count; i++) g_watches[i].valid = false;
        update_watches();
        con_add(CON_COL_OK, "CPU reset. State: %s", sim_state_name(sim_get_state(g_sim)));
        return;
    }

    if (strcmp(verb, "regs") == 0) {
        if (sim_get_state(g_sim) == SIM_IDLE) {
            con_add(CON_COL_ERR, "No program loaded.");
            return;
        }
        cpu_t *cpu = sim_get_cpu(g_sim);
        if (!cpu) return;
        bool is45 = (sim_get_cpu_type(g_sim) == CPU_45GS02);
        if (is45)
            con_add(CON_COL_NORMAL,
                "A=%02X X=%02X Y=%02X Z=%02X B=%02X  S=%04X PC=%04X  P=%02X  Cycles=%lu",
                cpu->a, cpu->x, cpu->y, cpu->z, cpu->b,
                cpu->s, cpu->pc, cpu->p, cpu->cycles);
        else
            con_add(CON_COL_NORMAL,
                "A=%02X X=%02X Y=%02X  S=%04X PC=%04X  P=%02X  Cycles=%lu",
                cpu->a, cpu->x, cpu->y, cpu->s, cpu->pc, cpu->p, cpu->cycles);
        return;
    }

    if (strcmp(verb, "mem") == 0) {
        if (sim_get_state(g_sim) == SIM_IDLE) {
            con_add(CON_COL_ERR, "No program loaded.");
            return;
        }
        cpu_t *cpu = sim_get_cpu(g_sim);
        uint16_t addr = cpu ? cpu->pc : 0x0200;
        int n = 128;
        if (args_rest[0]) {
            char *endp;
            addr = (uint16_t)strtol(args_rest, &endp, 16);
            while (*endp == ' ' || *endp == '\t') endp++;
            if (*endp) n = atoi(endp);
        }
        if (n < 1) n = 1;
        if (n > 256) n = 256;
        for (int row = 0; row * 16 < n; row++) {
            uint16_t ra = (uint16_t)(addr + row * 16);
            int bytes = ((row + 1) * 16 <= n) ? 16 : n - row * 16;
            char ln[80];
            int pos = snprintf(ln, sizeof(ln), "%04X:", ra);
            for (int c = 0; c < bytes; c++) {
                uint8_t v = sim_mem_read_byte(g_sim, (uint16_t)(ra + c));
                pos += snprintf(ln + pos, (int)sizeof(ln) - pos, " %02X", v);
            }
            con_add(CON_COL_NORMAL, "%s", ln);
        }
        return;
    }

    if (strcmp(verb, "disasm") == 0) {
        if (sim_get_state(g_sim) == SIM_IDLE) {
            con_add(CON_COL_ERR, "No program loaded.");
            return;
        }
        cpu_t *cpu = sim_get_cpu(g_sim);
        uint16_t addr = cpu ? cpu->pc : 0x0200;
        int n = 8;
        if (args_rest[0]) {
            char *endp;
            addr = (uint16_t)strtol(args_rest, &endp, 16);
            while (*endp == ' ' || *endp == '\t') endp++;
            if (*endp) n = atoi(endp);
        }
        if (n < 1) n = 1;
        if (n > 64) n = 64;
        char dbuf[128];
        for (int i = 0; i < n; i++) {
            int consumed = sim_disassemble_one(g_sim, addr, dbuf, sizeof(dbuf));
            con_add(CON_COL_NORMAL, "%s", dbuf);
            if (consumed < 1) consumed = 1;
            addr = (uint16_t)(addr + consumed);
        }
        return;
    }

    if (strcmp(verb, "break") == 0) {
        if (!args_rest[0]) { con_add(CON_COL_ERR, "Usage: break <addr> [cond]"); return; }
        char *endp;
        uint16_t addr = (uint16_t)strtol(args_rest, &endp, 16);
        while (*endp == ' ' || *endp == '\t') endp++;
        const char *cond = *endp ? endp : NULL;
        if (sim_break_set(g_sim, addr, cond))
            con_add(CON_COL_OK, "Breakpoint set at $%04X%s%s",
                    addr, cond ? " if " : "", cond ? cond : "");
        else
            con_add(CON_COL_ERR, "Breakpoint table full (max 16).");
        return;
    }

    if (strcmp(verb, "del") == 0) {
        if (!args_rest[0]) { con_add(CON_COL_ERR, "Usage: del <addr>"); return; }
        uint16_t addr = (uint16_t)strtol(args_rest, NULL, 16);
        sim_break_clear(g_sim, addr);
        con_add(CON_COL_OK, "Breakpoint at $%04X removed.", addr);
        return;
    }

    if (strcmp(verb, "breaks") == 0) {
        int cnt = sim_break_count(g_sim);
        if (cnt == 0) { con_add(CON_COL_NORMAL, "No breakpoints set."); return; }
        con_add(CON_COL_NORMAL, "%d breakpoint(s):", cnt);
        for (int i = 0; i < cnt; i++) {
            uint16_t addr = 0;
            char cond[128] = "";
            sim_break_get(g_sim, i, &addr, cond, sizeof(cond));
            if (cond[0])
                con_add(CON_COL_NORMAL, "  %d: $%04X  if %s", i, addr, cond);
            else
                con_add(CON_COL_NORMAL, "  %d: $%04X", i, addr);
        }
        return;
    }

    if (strcmp(verb, "load") == 0) {
        if (!args_rest[0]) { con_add(CON_COL_ERR, "Usage: load <path>"); return; }
        char path[512];
        strncpy(path, args_rest, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        int len = (int)strlen(path);
        while (len > 0 && (path[len-1] == '\n' || path[len-1] == '\r' ||
                           path[len-1] == ' '  || path[len-1] == '\t'))
            path[--len] = '\0';
        g_running = false;
        snprintf(g_filename_buf, sizeof(g_filename_buf), "%s", path);
        do_load();
        if (sim_get_state(g_sim) != SIM_IDLE)
            con_add(CON_COL_OK, "Loaded: %s", path);
        else
            con_add(CON_COL_ERR, "Failed to load: %s", path);
        return;
    }

    if (strcmp(verb, "bload") == 0) {
        /* bload <path> [addr]  -- .bin requires addr; .prg uses header unless addr given */
        const char *p = args_rest;
        char path[512]; int pi = 0;
        if (*p == '"') {
            p++;
            while (*p && *p != '"' && pi < 511) path[pi++] = *p++;
            if (*p == '"') p++;
        } else {
            while (*p && !isspace((unsigned char)*p) && pi < 511) path[pi++] = *p++;
        }
        path[pi] = '\0';
        while (*p == ' ' || *p == '\t') p++;
        if (!path[0]) { con_add(CON_COL_ERR, "Usage: bload <path> [addr]"); return; }
        bool is_prg = ext_is(path, ".prg");
        g_running = false;
        int ok;
        if (is_prg) {
            uint16_t override_addr = *p ? (uint16_t)strtol(p, nullptr, 16) : 0;
            ok = sim_load_prg(g_sim, path, override_addr);
        } else {
            if (!*p) { con_add(CON_COL_ERR, "bload: address required for .bin"); return; }
            uint16_t addr = (uint16_t)strtol(p, nullptr, 16);
            ok = sim_load_bin(g_sim, path, addr);
        }
        if (ok == 0) {
            g_prev_cpu_valid   = false;
            g_last_write_count = 0;
            for (int i = 0; i < g_watch_count; i++) g_watches[i].valid = false;
            update_watches();
            uint16_t laddr = 0, lsize = 0;
            sim_get_load_info(g_sim, &laddr, &lsize);
            con_add(CON_COL_OK, "bload: %u bytes at $%04X%s",
                    (unsigned)lsize, (unsigned)laddr, is_prg ? " (PRG)" : "");
            snprintf(g_filename_buf, sizeof(g_filename_buf), "%s", path);
        } else {
            con_add(CON_COL_ERR, "bload: failed to open '%s'", path);
        }
        return;
    }

    if (strcmp(verb, "bsave") == 0) {
        /* bsave <path> <start> <end>  -- saves [start, end) as .bin or .prg */
        const char *p = args_rest;
        char path[512]; int pi = 0;
        if (*p == '"') {
            p++;
            while (*p && *p != '"' && pi < 511) path[pi++] = *p++;
            if (*p == '"') p++;
        } else {
            while (*p && !isspace((unsigned char)*p) && pi < 511) path[pi++] = *p++;
        }
        path[pi] = '\0';
        while (*p == ' ' || *p == '\t') p++;
        if (!path[0]) { con_add(CON_COL_ERR, "Usage: bsave <path> <start> <end>"); return; }
        char *endp;
        uint16_t start = (uint16_t)strtol(p, &endp, 16);
        p = endp;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) { con_add(CON_COL_ERR, "Usage: bsave <path> <start> <end>"); return; }
        uint16_t end_addr = (uint16_t)strtol(p, nullptr, 16);
        if (end_addr <= start) { con_add(CON_COL_ERR, "bsave: end must be > start"); return; }
        uint16_t count = (uint16_t)(end_addr - start);
        bool is_prg = ext_is(path, ".prg");
        int ok = is_prg
            ? sim_save_prg(g_sim, path, start, count)
            : sim_save_bin(g_sim, path, start, count);
        if (ok == 0)
            con_add(CON_COL_OK, "bsave: %u bytes at $%04X → '%s'%s",
                    (unsigned)count, (unsigned)start, path, is_prg ? " (PRG)" : "");
        else
            con_add(CON_COL_ERR, "bsave: failed to write '%s'", path);
        return;
    }

    con_add(CON_COL_ERR, "Unknown command '%s'. Type 'help' for list.", verb);
}

/* --------------------------------------------------------------------------
 * Helper: refresh all watch values from memory
 * -------------------------------------------------------------------------- */
static void update_watches(void)
{
    if (sim_get_state(g_sim) == SIM_IDLE) return;
    for (int i = 0; i < g_watch_count; i++) {
        uint8_t v = sim_mem_read_byte(g_sim, g_watches[i].addr);
        if (!g_watches[i].valid) {
            g_watches[i].cur_val  = v;
            g_watches[i].prev_val = v;
            g_watches[i].valid    = true;
            g_watches[i].changed  = false;
        } else {
            g_watches[i].changed  = (v != g_watches[i].cur_val);
            g_watches[i].prev_val = g_watches[i].cur_val;
            g_watches[i].cur_val  = v;
        }
    }
}

/* --------------------------------------------------------------------------
 * Phase 4 helpers: source loader and heatmap texture update
 * -------------------------------------------------------------------------- */
static void src_load(const char *path)
{
    g_src_line_count = 0;
    g_src_loaded_file[0] = '\0';
    g_src_search_hit = -1;
    if (!path || !path[0]) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[SRC_MAX_LINE_LEN];
    while (g_src_line_count < SRC_MAX_LINES) {
        if (!fgets(line, sizeof(line), f)) break;
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        snprintf(g_src_lines[g_src_line_count++], SRC_MAX_LINE_LEN, "%s", line);
    }
    fclose(f);
    snprintf(g_src_loaded_file, sizeof(g_src_loaded_file), "%s", path);
}

static void update_heatmap_tex(void)
{
    if (!g_sim || !g_heatmap_tex) return;
    static uint8_t pixels[256 * 256 * 3];
    uint32_t max_val = 1;
    for (int i = 0; i < 65536; i++) {
        uint32_t v = sim_profiler_get_exec(g_sim, (uint16_t)i);
        if (v > max_val) max_val = v;
    }
    for (int i = 0; i < 65536; i++) {
        uint32_t v = sim_profiler_get_exec(g_sim, (uint16_t)i);
        float t = (float)v / (float)max_val;
        if (t > 0.0f) t = logf(1.0f + t * 255.0f) / logf(256.0f);
        uint8_t val = (uint8_t)(t * 255.0f);
        pixels[i * 3 + 0] = val;
        pixels[i * 3 + 1] = (uint8_t)(val / 3);
        pixels[i * 3 + 2] = 0;
    }
    glBindTexture(GL_TEXTURE_2D, g_heatmap_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_heatmap_dirty = false;
}

/* --------------------------------------------------------------------------
 * Pane: Breakpoint Manager (Phase 3)
 *
 * Lists all breakpoints with enable toggle and delete button.
 * Inline add-row at the top for fast entry.
 * -------------------------------------------------------------------------- */
static void draw_pane_breakpoints(void)
{
    if (!show_breakpoints) return;
    ImGui::Begin("Breakpoints", &show_breakpoints);

    /* Add row */
    static char bp_addr_buf[8] = "";
    static char bp_cond_buf[64] = "";
    ImGui::SetNextItemWidth(60);
    ImGui::InputText("Addr##bpa", bp_addr_buf, sizeof(bp_addr_buf),
                     ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130);
    ImGui::InputText("if##bpc", bp_cond_buf, sizeof(bp_cond_buf));
    ImGui::SameLine();
    if (ImGui::SmallButton("Add") && bp_addr_buf[0]) {
        uint16_t addr = (uint16_t)strtol(bp_addr_buf, NULL, 16);
        const char *cond = bp_cond_buf[0] ? bp_cond_buf : NULL;
        if (sim_break_set(g_sim, addr, cond)) {
            bp_addr_buf[0] = '\0';
            bp_cond_buf[0] = '\0';
        }
    }
    ImGui::Separator();

    int cnt = sim_break_count(g_sim);
    if (cnt == 0) {
        ImGui::TextDisabled("No breakpoints set.");
        ImGui::End();
        return;
    }

    const ImGuiTableFlags tf =
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_BordersInnerV  |
        ImGuiTableFlags_RowBg;

    if (ImGui::BeginTable("##bptab", 4, tf)) {
        ImGui::TableSetupColumn("En",        ImGuiTableColumnFlags_WidthFixed,   22.0f);
        ImGui::TableSetupColumn("Address",   ImGuiTableColumnFlags_WidthFixed,   52.0f);
        ImGui::TableSetupColumn("Condition", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("",          ImGuiTableColumnFlags_WidthFixed,   30.0f);
        ImGui::TableHeadersRow();

        int to_delete = -1;
        for (int i = 0; i < cnt; i++) {
            uint16_t addr = 0;
            char cond[128] = "";
            sim_break_get(g_sim, i, &addr, cond, sizeof(cond));
            bool enabled = (sim_break_is_enabled(g_sim, i) != 0);

            ImGui::TableNextRow();

            /* Enable checkbox */
            ImGui::TableSetColumnIndex(0);
            char cb_id[16]; snprintf(cb_id, sizeof(cb_id), "##en%d", i);
            if (ImGui::Checkbox(cb_id, &enabled))
                sim_break_toggle(g_sim, i);

            /* Address */
            ImGui::TableSetColumnIndex(1);
            if (!enabled) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::Text("$%04X", (unsigned)addr);
            if (!enabled) ImGui::PopStyleColor();

            /* Condition */
            ImGui::TableSetColumnIndex(2);
            if (cond[0]) {
                if (!enabled) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::TextUnformatted(cond);
                if (!enabled) ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("(always)");
            }

            /* Delete */
            ImGui::TableSetColumnIndex(3);
            char del_id[16]; snprintf(del_id, sizeof(del_id), "Del##%d", i);
            if (ImGui::SmallButton(del_id))
                to_delete = i;
        }
        ImGui::EndTable();

        if (to_delete >= 0) {
            uint16_t addr = 0;
            sim_break_get(g_sim, to_delete, &addr, NULL, 0);
            sim_break_clear(g_sim, addr);
        }
    }

    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Pane: Execution Trace Log (Phase 3)
 *
 * Ring buffer of the last SIM_TRACE_DEPTH instructions executed.
 * Slot 0 = most recent.  Virtualised with ImGuiListClipper.
 * -------------------------------------------------------------------------- */
static void draw_pane_trace(void)
{
    if (!show_trace) return;
    ImGui::Begin("Trace Log", &show_trace);

    /* Controls */
    bool rec = (sim_trace_is_enabled(g_sim) != 0);
    if (ImGui::Checkbox("Record", &rec))
        sim_trace_enable(g_sim, rec ? 1 : 0);
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear"))
        sim_trace_clear(g_sim);
    int cnt = sim_trace_count(g_sim);
    ImGui::SameLine();
    ImGui::TextDisabled("(%d / %d)", cnt, SIM_TRACE_DEPTH);
    ImGui::Separator();

    if (cnt == 0) {
        ImGui::TextDisabled("No entries. Enable 'Record' then step or run.");
        ImGui::End();
        return;
    }

    const ImGuiTableFlags tf =
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_BordersInnerV  |
        ImGuiTableFlags_ScrollY        |
        ImGuiTableFlags_RowBg;

    float avail_h = ImGui::GetContentRegionAvail().y;
    if (ImGui::BeginTable("##trace", 5, tf, ImVec2(0, avail_h))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#",     ImGuiTableColumnFlags_WidthFixed,   38.0f);
        ImGui::TableSetupColumn("PC",    ImGuiTableColumnFlags_WidthFixed,   44.0f);
        ImGui::TableSetupColumn("Instr", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Cyc",   ImGuiTableColumnFlags_WidthFixed,   26.0f);
        ImGui::TableSetupColumn("A  X  Y", ImGuiTableColumnFlags_WidthFixed, 66.0f);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(cnt);
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                sim_trace_entry_t e;
                if (!sim_trace_get(g_sim, i, &e)) continue;

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%d", i);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%04X", (unsigned)e.pc);

                ImGui::TableSetColumnIndex(2);
                /* disasm = "$XXXX: %-18s %-6s %s"
                 * Skip the 7-char "$XXXX: " address prefix. */
                const char *ins = (strlen(e.disasm) > 7) ? e.disasm + 7 : e.disasm;
                ImGui::TextUnformatted(ins);

                ImGui::TableSetColumnIndex(3);
                ImGui::TextDisabled("%d", e.cycles_delta);

                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%02X %02X %02X",
                            (unsigned)e.cpu.a,
                            (unsigned)e.cpu.x,
                            (unsigned)e.cpu.y);
            }
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Pane: Stack Inspector (Phase 3)
 *
 * Shows the 6502 stack page ($0100–$01FF).
 * The current SP row is highlighted; values above SP are active stack data.
 * -------------------------------------------------------------------------- */
static void draw_pane_stack(void)
{
    if (!show_stack) return;
    ImGui::Begin("Stack", &show_stack);

    sim_state_t state = sim_get_state(g_sim);
    if (state == SIM_IDLE) {
        ImGui::TextDisabled("(no program loaded)");
        ImGui::End();
        return;
    }

    cpu_t *cpu = sim_get_cpu(g_sim);
    if (!cpu) { ImGui::End(); return; }

    uint8_t sp = (uint8_t)(cpu->s & 0xFF);   /* low byte = page offset */
    uint16_t sp_addr = (uint16_t)(0x0100 | sp);

    ImGui::Text("SP = $%04X  (depth %d)", (unsigned)sp_addr, (int)(0xFF - sp));
    ImGui::Separator();

    const ImGuiTableFlags tf =
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_BordersInnerV  |
        ImGuiTableFlags_ScrollY        |
        ImGuiTableFlags_RowBg;

    float avail_h = ImGui::GetContentRegionAvail().y;
    if (ImGui::BeginTable("##stk", 3, tf, ImVec2(0, avail_h))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Addr",  ImGuiTableColumnFlags_WidthFixed,   52.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed,   38.0f);
        ImGui::TableSetupColumn("Note",  ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableHeadersRow();

        /* Walk from $01FF down to $0101 (skip $0100 = SP=0xFF = empty) */
        bool need_scroll = false;
        for (int offset = 0xFF; offset >= 0x01; offset--) {
            uint16_t addr = (uint16_t)(0x0100 | offset);
            uint8_t  val  = sim_mem_read_byte(g_sim, addr);
            bool is_sp    = (offset == (sp + 1) % 0x100); /* top of stack */
            bool active   = (offset > sp);                 /* pushed data */

            ImGui::TableNextRow();

            /* Highlight the top-of-stack row */
            if (is_sp) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    ImGui::GetColorU32(g_col.highlight_green_bg));
                need_scroll = true;
            }

            ImGui::TableSetColumnIndex(0);
            if (active)
                ImGui::Text("$%04X", (unsigned)addr);
            else
                ImGui::TextDisabled("$%04X", (unsigned)addr);

            ImGui::TableSetColumnIndex(1);
            if (active)
                ImGui::Text("%02X", (unsigned)val);
            else
                ImGui::TextDisabled("%02X", (unsigned)val);

            ImGui::TableSetColumnIndex(2);
            if (is_sp)
                ImGui::TextColored(g_col.text_ok, "<-- SP+1");
            else if (offset == sp)
                ImGui::TextColored(g_col.text_changed, "<-- SP");
            else if (active && offset < 0xFF && offset > sp) {
                /* Pair check: every two pushed bytes may be a return address */
                if ((offset & 1) == 1 && offset + 1 <= 0xFF) {
                    uint8_t lo = sim_mem_read_byte(g_sim, addr);
                    uint8_t hi = sim_mem_read_byte(g_sim, (uint16_t)(addr + 1));
                    uint16_t ret_addr = (uint16_t)((hi << 8) | lo);
                    /* RTS pops (stored_val+1) */
                    ImGui::TextDisabled("->$%04X ?", (unsigned)((ret_addr + 1) & 0xFFFF));
                }
            }
        }

        /* Scroll the table so SP row is visible */
        if (need_scroll) {
            float row_h = ImGui::GetTextLineHeightWithSpacing();
            int row_idx = 0xFF - (sp + 1);
            ImGui::SetScrollY((float)row_idx * row_h);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Pane: I/O Watch (Phase 3)
 *
 * User-defined address watch list.  Values are refreshed after each step.
 * Changed values are highlighted yellow.
 * -------------------------------------------------------------------------- */
static void draw_pane_watches(void)
{
    if (!show_watches) return;
    ImGui::Begin("Watch", &show_watches);

    /* Add row */
    static char w_addr_buf[8] = "";
    static char w_name_buf[32] = "";
    ImGui::SetNextItemWidth(60);
    ImGui::InputText("Addr##wa", w_addr_buf, sizeof(w_addr_buf),
                     ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputText("Name##wn", w_name_buf, sizeof(w_name_buf));
    ImGui::SameLine();
    bool can_add = (g_watch_count < WATCH_MAX && w_addr_buf[0]);
    if (!can_add) ImGui::BeginDisabled();
    if (ImGui::SmallButton("Add")) {
        WatchEntry &w = g_watches[g_watch_count++];
        w.addr    = (uint16_t)strtol(w_addr_buf, NULL, 16);
        snprintf(w.name, sizeof(w.name), "%s", w_name_buf[0] ? w_name_buf : "?");
        w.cur_val = w.prev_val = 0;
        w.changed = false;
        w.valid   = false;
        w_addr_buf[0] = '\0';
        w_name_buf[0] = '\0';
        update_watches();  /* seed the initial value */
    }
    if (!can_add) ImGui::EndDisabled();
    ImGui::Separator();

    if (g_watch_count == 0) {
        ImGui::TextDisabled("No watches. Enter address and click Add.");
        ImGui::End();
        return;
    }

    const ImGuiTableFlags tf =
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_BordersInnerV  |
        ImGuiTableFlags_RowBg;

    if (ImGui::BeginTable("##wtab", 5, tf)) {
        ImGui::TableSetupColumn("Addr",  ImGuiTableColumnFlags_WidthFixed,   52.0f);
        ImGui::TableSetupColumn("Name",  ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Hex",   ImGuiTableColumnFlags_WidthFixed,   30.0f);
        ImGui::TableSetupColumn("Dec",   ImGuiTableColumnFlags_WidthFixed,   28.0f);
        ImGui::TableSetupColumn("",      ImGuiTableColumnFlags_WidthFixed,   30.0f);
        ImGui::TableHeadersRow();

        int to_del = -1;
        for (int i = 0; i < g_watch_count; i++) {
            WatchEntry &w = g_watches[i];

            /* Refresh if not yet read */
            if (!w.valid && sim_get_state(g_sim) != SIM_IDLE) {
                w.cur_val = sim_mem_read_byte(g_sim, w.addr);
                w.prev_val = w.cur_val;
                w.valid    = true;
            }

            ImGui::TableNextRow();

            ImVec4 val_col = w.changed
                ? g_col.text_changed
                : ImGui::GetStyleColorVec4(ImGuiCol_Text);

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("$%04X", (unsigned)w.addr);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(w.name);

            ImGui::TableSetColumnIndex(2);
            ImGui::PushStyleColor(ImGuiCol_Text, val_col);
            ImGui::Text("%02X", (unsigned)w.cur_val);
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(3);
            ImGui::PushStyleColor(ImGuiCol_Text, val_col);
            ImGui::Text("%3d", (int)w.cur_val);
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(4);
            char del_id[16]; snprintf(del_id, sizeof(del_id), "Del##w%d", i);
            if (ImGui::SmallButton(del_id))
                to_del = i;
        }
        ImGui::EndTable();

        if (to_del >= 0) {
            /* Remove by shifting */
            for (int i = to_del; i < g_watch_count - 1; i++)
                g_watches[i] = g_watches[i + 1];
            g_watch_count--;
        }
    }

    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Pane: Instruction Reference (Phase 4)
 *
 * Two tabs: By Mnemonic (filterable table) and By Opcode (16×16 grid).
 * Selecting a row/cell shows a detail strip below.
 * -------------------------------------------------------------------------- */
static void draw_pane_iref(void)
{
    if (!show_iref) return;
    ImGui::Begin("Instruction Ref", &show_iref);

    int n = sim_opcode_count(g_sim);
    if (n == 0) {
        ImGui::TextDisabled("(no processor loaded)");
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth(150.0f);
    ImGui::InputText("Filter##ir", g_iref_filter, sizeof(g_iref_filter));
    ImGui::SameLine();
    ImGui::TextDisabled("%d opcodes  %s", n, sim_processor_name(g_sim));

    if (ImGui::BeginTabBar("##irtabs")) {
        /* ---- By Mnemonic tab ---- */
        if (ImGui::BeginTabItem("By Mnemonic")) {
            const ImGuiTableFlags tf =
                ImGuiTableFlags_SizingFixedFit |
                ImGuiTableFlags_BordersOuter   |
                ImGuiTableFlags_BordersInnerV  |
                ImGuiTableFlags_ScrollY        |
                ImGuiTableFlags_RowBg;

            float detail_h = (g_iref_sel_idx >= 0) ? ImGui::GetTextLineHeightWithSpacing() * 3.0f : 0.0f;
            float table_h  = ImGui::GetContentRegionAvail().y - detail_h - 4.0f;
            if (table_h < 40.0f) table_h = 40.0f;

            if (ImGui::BeginTable("##irtab", 5, tf, ImVec2(0.0f, table_h))) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Opcode", ImGuiTableColumnFlags_WidthFixed,   68.0f);
                ImGui::TableSetupColumn("Mnem",   ImGuiTableColumnFlags_WidthFixed,   48.0f);
                ImGui::TableSetupColumn("Mode",   ImGuiTableColumnFlags_WidthFixed,   50.0f);
                ImGui::TableSetupColumn("Bytes",  ImGuiTableColumnFlags_WidthFixed,   40.0f);
                ImGui::TableSetupColumn("Cycles", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableHeadersRow();

                for (int i = 0; i < n; i++) {
                    sim_opcode_info_t info;
                    if (!sim_opcode_get(g_sim, i, &info)) continue;

                    if (g_iref_filter[0]) {
                        bool match = false;
                        const char *f = g_iref_filter;
                        size_t flen   = strlen(f);
                        const char *m = info.mnemonic;
                        size_t mlen   = strlen(m);
                        for (size_t j = 0; j + flen <= mlen && !match; j++) {
                            bool ok = true;
                            for (size_t k = 0; k < flen; k++)
                                if (toupper((unsigned char)m[j+k]) != toupper((unsigned char)f[k]))
                                    { ok = false; break; }
                            if (ok) match = true;
                        }
                        if (!match) continue;
                    }

                    ImGui::TableNextRow();
                    bool selected = (g_iref_sel_idx == i);

                    /* Format opcode byte sequence */
                    char opbuf[16] = "";
                    int  op_pos    = 0;
                    for (int b = 0; b < (int)info.opcode_len && b < 4; b++)
                        op_pos += snprintf(opbuf + op_pos, sizeof(opbuf) - op_pos,
                                           b ? " %02X" : "%02X", info.opcode_bytes[b]);

                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable(opbuf, selected,
                            ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 0)))
                        g_iref_sel_idx = selected ? -1 : i;

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextColored(g_col.mnemonic, "%s", info.mnemonic);

                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextDisabled("%s", sim_mode_name(info.mode));

                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%d", info.instr_bytes);

                    ImGui::TableSetColumnIndex(4);
                    if (info.cycles > 0) ImGui::Text("%d", info.cycles);
                    else ImGui::TextDisabled("?");
                }
                ImGui::EndTable();
            }

            /* Detail strip */
            if (g_iref_sel_idx >= 0) {
                ImGui::Separator();
                sim_opcode_info_t info;
                if (sim_opcode_get(g_sim, g_iref_sel_idx, &info)) {
                    char opbuf[32] = "";
                    int  p = 0;
                    for (int b = 0; b < (int)info.opcode_len && b < 4; b++)
                        p += snprintf(opbuf + p, sizeof(opbuf) - p,
                                      b ? " $%02X" : "$%02X", info.opcode_bytes[b]);
                    ImGui::TextColored(g_col.mnemonic, "%-6s", info.mnemonic);
                    ImGui::SameLine();
                    ImGui::Text("mode=%-5s  bytes=%d  cycles=%d  opcode=%s",
                        sim_mode_name(info.mode), info.instr_bytes, info.cycles, opbuf);
                }
            }
            ImGui::EndTabItem();
        }

        /* ---- By Opcode tab: one 16×16 grid per opcode prefix group ---- */
        if (ImGui::BeginTabItem("By Opcode")) {

            /* Build per-prefix-group lookup: by_idx[base] = opcode-table index */
            struct PrefixGroup {
                char    label[20];
                uint8_t prefix[3];
                int     plen;         /* number of prefix bytes (0–3)         */
                int     by_idx[256];  /* opcode table index, -1 = absent      */
                bool    any;          /* true if at least one entry exists     */
                bool    row_used[16]; /* high-nibble rows that have >=1 entry  */
                bool    col_used[16]; /* low-nibble cols that have >=1 entry   */
            };

            PrefixGroup grp[4];
            for (int g = 0; g < 4; g++) {
                for (int b = 0; b < 256; b++) grp[g].by_idx[b] = -1;
                memset(grp[g].row_used, 0, sizeof(grp[g].row_used));
                memset(grp[g].col_used, 0, sizeof(grp[g].col_used));
                grp[g].any  = false;
                grp[g].plen = 0;
                memset(grp[g].prefix, 0, sizeof(grp[g].prefix));
            }
            /* Group 0: no prefix  ($__)                */
            snprintf(grp[0].label, sizeof(grp[0].label), "$__");
            /* Group 1: EOM prefix ($EA __)             */
            snprintf(grp[1].label, sizeof(grp[1].label), "$EA __");
            grp[1].prefix[0] = 0xEA; grp[1].plen = 1;
            /* Group 2: NEG NEG prefix ($42 $42 __)     */
            snprintf(grp[2].label, sizeof(grp[2].label), "$42 $42 __");
            grp[2].prefix[0] = 0x42; grp[2].prefix[1] = 0x42; grp[2].plen = 2;
            /* Group 3: NEG NEG EOM ($42 $42 $EA __)    */
            snprintf(grp[3].label, sizeof(grp[3].label), "$42 $42 $EA __");
            grp[3].prefix[0] = 0x42; grp[3].prefix[1] = 0x42;
            grp[3].prefix[2] = 0xEA; grp[3].plen = 3;

            int total_ops = sim_opcode_count(g_sim);
            for (int i = 0; i < total_ops; i++) {
                sim_opcode_info_t info;
                if (!sim_opcode_get(g_sim, i, &info)) continue;
                int plen = (int)info.opcode_len - 1;
                if (plen < 0 || plen > 3) continue;
                uint8_t base = info.opcode_bytes[info.opcode_len - 1];
                for (int g = 0; g < 4; g++) {
                    if (grp[g].plen != plen) continue;
                    bool ok = true;
                    for (int p = 0; p < plen; p++)
                        if (info.opcode_bytes[p] != grp[g].prefix[p]) { ok = false; break; }
                    if (ok) {
                        grp[g].by_idx[base]        = i;
                        grp[g].any                 = true;
                        grp[g].row_used[base >> 4] = true;
                        grp[g].col_used[base & 0xF] = true;
                        break;
                    }
                }
            }

            /* Selection encoded as (group_idx << 8) | base_byte, -1 = none */
            static int         by_opcode_sel = -1;
            static const char *sel_proc      = nullptr;
            const  char       *cur_proc      = sim_processor_name(g_sim);
            if (cur_proc != sel_proc) { sel_proc = cur_proc; by_opcode_sel = -1; }

            const float cell_w  = 44.0f;
            const float cell_h  = ImGui::GetTextLineHeight()
                                 + ImGui::GetStyle().CellPadding.y * 2.0f;
            const float label_h = ImGui::GetTextLineHeightWithSpacing();
            const float detail_h = (by_opcode_sel >= 0)
                                  ? ImGui::GetTextLineHeightWithSpacing() + 4.0f : 0.0f;
            float avail_h = ImGui::GetContentRegionAvail().y - detail_h;
            if (avail_h < 60.0f) avail_h = 60.0f;
            float table_h = avail_h - label_h - ImGui::GetStyle().ItemSpacing.y;
            if (table_h < cell_h * 4.0f) table_h = cell_h * 4.0f;

            const float cp  = ImGui::GetStyle().CellPadding.x;
            const float sbw = ImGui::GetStyle().ScrollbarSize;

            /* Horizontal scroll container holds all grids side by side */
            ImGui::BeginChild("##opgrids_h", ImVec2(0.0f, avail_h), false,
                              ImGuiWindowFlags_HorizontalScrollbar);

            bool first_grp = true;
            for (int g = 0; g < 4; g++) {
                if (!grp[g].any) continue;
                if (!first_grp) ImGui::SameLine(0.0f, 14.0f);
                first_grp = false;

                /* Prefix groups: only render rows/cols that contain at least
                   one opcode.  Standard group keeps the full 16×16 layout.  */
                const bool filter = (g > 0);
                uint8_t active_cols[16], active_rows[16];
                int     nc = 0, nr = 0;
                for (int i = 0; i < 16; i++) {
                    if (!filter || grp[g].col_used[i]) active_cols[nc++] = (uint8_t)i;
                    if (!filter || grp[g].row_used[i]) active_rows[nr++] = (uint8_t)i;
                }

                /* Grid width sized to actual column count so BeginTable has
                   a non-zero outer_size.x when placed after SameLine         */
                float this_grid_w = 26.0f + (float)nc * cell_w
                                  + (float)(nc + 1) * cp * 2.0f + sbw + 4.0f;

                ImGui::BeginGroup();
                ImGui::TextDisabled("%s", grp[g].label);

                char tbl_id[16]; snprintf(tbl_id, sizeof(tbl_id), "##opg%d", g);
                if (ImGui::BeginTable(tbl_id, nc + 1,
                        ImGuiTableFlags_BordersOuter   |
                        ImGuiTableFlags_SizingFixedFit |
                        ImGuiTableFlags_ScrollY,
                        ImVec2(this_grid_w, table_h))) {

                    ImGui::TableSetupScrollFreeze(1, 1);
                    ImGui::TableSetupColumn("##rh", ImGuiTableColumnFlags_WidthFixed, 26.0f);
                    for (int ci = 0; ci < nc; ci++) {
                        char hdr[4]; snprintf(hdr, sizeof(hdr), "_%X", (int)active_cols[ci]);
                        ImGui::TableSetupColumn(hdr, ImGuiTableColumnFlags_WidthFixed, cell_w);
                    }
                    ImGui::TableHeadersRow();

                    for (int ri = 0; ri < nr; ri++) {
                        uint8_t r = active_rows[ri];
                        ImGui::TableNextRow(ImGuiTableRowFlags_None, cell_h);
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextDisabled("%X_", (int)r);
                        for (int ci = 0; ci < nc; ci++) {
                            ImGui::TableSetColumnIndex(ci + 1);
                            uint8_t bv  = (uint8_t)((r << 4) | active_cols[ci]);
                            int     idx = grp[g].by_idx[bv];
                            if (idx >= 0) {
                                sim_opcode_info_t info;
                                sim_opcode_get(g_sim, idx, &info);
                                int  encoded = (g << 8) | (int)bv;
                                bool sel = (by_opcode_sel == encoded);
                                char lbl[16];
                                snprintf(lbl, sizeof(lbl), "%-3s##%d%02X",
                                         info.mnemonic, g, bv);
                                ImGui::PushStyleColor(ImGuiCol_Text,
                                    sel ? g_col.text_changed : g_col.mnemonic);
                                if (ImGui::Selectable(lbl, sel, 0,
                                        ImVec2(cell_w - ImGui::GetStyle().CellPadding.x * 2, 0)))
                                    by_opcode_sel = sel ? -1 : encoded;
                                ImGui::PopStyleColor();
                            } else {
                                ImGui::TextDisabled("---");
                            }
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::EndGroup();
            }
            ImGui::EndChild();

            /* ---- Detail strip for the selected cell ---- */
            if (by_opcode_sel >= 0) {
                ImGui::Separator();
                int sel_g  = (by_opcode_sel >> 8) & 0xFF;
                int sel_bv = (by_opcode_sel)       & 0xFF;
                if (sel_g < 4 && grp[sel_g].by_idx[sel_bv] >= 0) {
                    sim_opcode_info_t info;
                    sim_opcode_get(g_sim, grp[sel_g].by_idx[sel_bv], &info);
                    char opbuf[16] = "";
                    int  p = 0;
                    for (int b = 0; b < (int)info.opcode_len && b < 4; b++)
                        p += snprintf(opbuf + p, sizeof(opbuf) - p,
                                      b ? " $%02X" : "$%02X", info.opcode_bytes[b]);
                    ImGui::Text("%s  ", opbuf);
                    ImGui::SameLine();
                    ImGui::TextColored(g_col.mnemonic, "%-6s", info.mnemonic);
                    ImGui::SameLine();
                    ImGui::Text("mode=%-5s  bytes=%d  cycles=%d",
                        sim_mode_name(info.mode), info.instr_bytes, info.cycles);
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Pane: Symbol Table Browser (Phase 4)
 *
 * Shows all symbols in the session's symbol table.
 * Filter by name prefix.  Click an address to navigate disassembly.
 * Inline add-row at top; per-row delete button.
 * -------------------------------------------------------------------------- */
static void draw_pane_symbols(void)
{
    if (!show_symbols) return;
    ImGui::Begin("Symbols", &show_symbols);

    int count = sim_sym_count(g_sim);

    /* Filter bar */
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("Filter##sf", g_sym_filter, sizeof(g_sym_filter));
    ImGui::SameLine();
    ImGui::TextDisabled("%d symbols", count);

    /* Add row */
    ImGui::Separator();
    static char sym_new_addr[8]  = "";
    static char sym_new_name[32] = "";
    static int  sym_new_type_idx = 0;
    static const char *sym_types[] = { "LABEL","VAR","CONST","FUNC","IO","REGION","TRAP" };
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputText("Addr##sna", sym_new_addr, sizeof(sym_new_addr),
                     ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("Name##snn", sym_new_name, sizeof(sym_new_name));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(75.0f);
    ImGui::Combo("##snt", &sym_new_type_idx, sym_types, IM_ARRAYSIZE(sym_types));
    ImGui::SameLine();
    bool can_add_sym = (sym_new_addr[0] && sym_new_name[0]);
    if (!can_add_sym) ImGui::BeginDisabled();
    if (ImGui::SmallButton("Add##sna2")) {
        uint16_t addr = (uint16_t)strtol(sym_new_addr, NULL, 16);
        sim_sym_add(g_sim, addr, sym_new_name, sym_types[sym_new_type_idx]);
        sym_new_addr[0] = '\0';
        sym_new_name[0] = '\0';
    }
    if (!can_add_sym) ImGui::EndDisabled();
    ImGui::Separator();

    if (count == 0 && !can_add_sym) {
        ImGui::TextDisabled("No symbols loaded.");
        ImGui::End();
        return;
    }

    const ImGuiTableFlags tf =
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_BordersInnerV  |
        ImGuiTableFlags_ScrollY        |
        ImGuiTableFlags_RowBg;

    float avail_h = ImGui::GetContentRegionAvail().y;
    if (ImGui::BeginTable("##symtab", 5, tf, ImVec2(0.0f, avail_h))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Addr",    ImGuiTableColumnFlags_WidthFixed,   52.0f);
        ImGui::TableSetupColumn("Name",    ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Type",    ImGuiTableColumnFlags_WidthFixed,   62.0f);
        ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("",        ImGuiTableColumnFlags_WidthFixed,   22.0f);
        ImGui::TableHeadersRow();

        int to_del = -1;
        for (int i = 0; i < count; i++) {
            uint16_t addr;
            char     name[64], comment[128];
            int      type;
            if (!sim_sym_get_idx(g_sim, i, &addr, name, sizeof(name),
                                 &type, comment, sizeof(comment))) continue;

            if (g_sym_filter[0]) {
                bool match = false;
                const char *f = g_sym_filter;
                size_t flen   = strlen(f);
                const char *m = name;
                size_t mlen   = strlen(m);
                for (size_t j = 0; j + flen <= mlen && !match; j++) {
                    bool ok = true;
                    for (size_t k = 0; k < flen; k++)
                        if (tolower((unsigned char)m[j+k]) != tolower((unsigned char)f[k]))
                            { ok = false; break; }
                    if (ok) match = true;
                }
                if (!match) continue;
            }

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            /* Clicking navigates the disassembly pane */
            char addr_id[24];
            snprintf(addr_id, sizeof(addr_id), "$%04X##sy%d", (unsigned)addr, i);
            if (ImGui::Selectable(addr_id, false, ImGuiSelectableFlags_SpanAllColumns)) {
                g_disasm_addr   = addr;
                g_disasm_follow = false;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(name);

            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("%s", sim_sym_type_name(type));

            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("%s", comment);

            ImGui::TableSetColumnIndex(4);
            char del_id[16]; snprintf(del_id, sizeof(del_id), "X##sy%d", i);
            if (ImGui::SmallButton(del_id)) to_del = i;
        }
        ImGui::EndTable();
        if (to_del >= 0) sim_sym_remove_idx(g_sim, to_del);
    }
    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Pane: Source Viewer (Phase 4)
 *
 * View mode: shows the loaded .asm source with syntax colouring.
 * Comments = green, labels/defs = yellow, mnemonics = cyan.
 * Search-and-highlight with jump-to-line.
 * -------------------------------------------------------------------------- */
static void draw_pane_source(void)
{
    if (!show_source) return;
    ImGui::Begin("Source", &show_source);

    /* Reload if filename changed */
    const char *cur_file = sim_get_filename(g_sim);
    if (cur_file && cur_file[0] && strcmp(cur_file, g_src_loaded_file) != 0)
        src_load(cur_file);

    if (g_src_line_count == 0) {
        ImGui::TextDisabled("(no source loaded — load a .asm file first)");
        ImGui::End();
        return;
    }

    /* Search bar */
    bool do_search = false, do_goto = false;
    if (g_focus_src_search) {
        ImGui::SetKeyboardFocusHere();
        g_focus_src_search = false;
    }
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::InputText("##srcsearch", g_src_search, sizeof(g_src_search),
                         ImGuiInputTextFlags_EnterReturnsTrue))
        do_search = true;
    ImGui::SameLine();
    if (ImGui::SmallButton("Find")) do_search = true;
    ImGui::SameLine();
    static char goto_buf[8] = "";
    ImGui::SetNextItemWidth(56.0f);
    if (ImGui::InputText("Line##srgl", goto_buf, sizeof(goto_buf),
                         ImGuiInputTextFlags_CharsDecimal |
                         ImGuiInputTextFlags_EnterReturnsTrue))
        do_goto = true;
    ImGui::SameLine();
    if (ImGui::SmallButton("Go##srgg")) do_goto = true;
    ImGui::SameLine();
    ImGui::TextDisabled("%d lines", g_src_line_count);
    ImGui::Separator();

    /* Search logic */
    if (do_search && g_src_search[0]) {
        size_t nlen  = strlen(g_src_search);
        int    start = (g_src_search_hit >= 0) ? g_src_search_hit + 1 : 0;
        int    found = -1;
        for (int i = 0; i < g_src_line_count && found < 0; i++) {
            int         idx = (start + i) % g_src_line_count;
            const char *h   = g_src_lines[idx];
            size_t      hlen = strlen(h);
            for (size_t j = 0; j + nlen <= hlen && found < 0; j++) {
                bool ok = true;
                for (size_t k = 0; k < nlen; k++)
                    if (tolower((unsigned char)h[j+k]) != tolower((unsigned char)g_src_search[k]))
                        { ok = false; break; }
                if (ok) found = idx;
            }
        }
        g_src_search_hit = found;
    }
    if (do_goto && goto_buf[0]) {
        int ln = atoi(goto_buf) - 1;
        if (ln >= 0 && ln < g_src_line_count) g_src_search_hit = ln;
    }

    /* Source table */
    const ImGuiTableFlags tf =
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_ScrollY        |
        ImGuiTableFlags_RowBg;

    float avail_h = ImGui::GetContentRegionAvail().y;
    if (ImGui::BeginTable("##srctab", 2, tf, ImVec2(0.0f, avail_h))) {
        ImGui::TableSetupScrollFreeze(0, 0);
        ImGui::TableSetupColumn("##ln",  ImGuiTableColumnFlags_WidthFixed,   38.0f);
        ImGui::TableSetupColumn("##src", ImGuiTableColumnFlags_WidthStretch, 1.0f);

        /* Scroll to highlighted line */
        static int scroll_to = -1;
        if (g_src_search_hit >= 0 && (do_search || do_goto))
            scroll_to = g_src_search_hit;

        ImGuiListClipper clipper;
        clipper.Begin(g_src_line_count);
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                ImGui::TableNextRow();

                if (i == g_src_search_hit)
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                        ImGui::GetColorU32(g_col.highlight_blue_bg));

                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%4d", i + 1);

                ImGui::TableSetColumnIndex(1);
                const char *line = g_src_lines[i];
                const char *p    = line;
                while (*p == ' ' || *p == '\t') p++;

                if (!*p) {
                    ImGui::Text(" ");
                } else if (*p == ';') {
                    /* Full comment line */
                    ImGui::TextColored(g_col.source_comment, "%s", line);
                } else if (p == line) {
                    /* Label / definition at column 0 */
                    const char *semi = strchr(line, ';');
                    if (semi) {
                        char lbuf[SRC_MAX_LINE_LEN];
                        int ll = (int)(semi - line);
                        if (ll >= SRC_MAX_LINE_LEN) ll = SRC_MAX_LINE_LEN - 1;
                        memcpy(lbuf, line, ll); lbuf[ll] = '\0';
                        ImGui::TextColored(g_col.source_label, "%s", lbuf);
                        ImGui::SameLine(0, 0);
                        ImGui::TextColored(g_col.source_comment, "%s", semi);
                    } else {
                        ImGui::TextColored(g_col.source_label, "%s", line);
                    }
                } else {
                    /* Indented instruction: indent(dim) + mnemonic(cyan) + operand + comment(green) */
                    const char *mnem_s = p;
                    while (*p && *p != ' ' && *p != '\t' && *p != ';') p++;
                    const char *mnem_e = p;
                    const char *semi   = strchr(mnem_e, ';');
                    const char *tail_e = semi ? semi : (line + strlen(line));
                    int piece = 0;

                    /* Leading indent */
                    if (mnem_s > line) {
                        char ibuf[64];
                        int il = (int)(mnem_s - line);
                        if (il > 63) il = 63;
                        memcpy(ibuf, line, il); ibuf[il] = '\0';
                        ImGui::TextDisabled("%s", ibuf);
                        if (mnem_e > mnem_s || tail_e > mnem_e || semi)
                            ImGui::SameLine(0, 0);
                        piece++;
                    }
                    /* Mnemonic in cyan */
                    if (mnem_e > mnem_s) {
                        char mbuf[16];
                        int ml = (int)(mnem_e - mnem_s);
                        if (ml > 15) ml = 15;
                        memcpy(mbuf, mnem_s, ml); mbuf[ml] = '\0';
                        if (piece) ImGui::SameLine(0, 0);
                        ImGui::TextColored(g_col.mnemonic, "%s", mbuf);
                        piece++;
                    }
                    /* Operand */
                    if (tail_e > mnem_e) {
                        char obuf[SRC_MAX_LINE_LEN];
                        int ol = (int)(tail_e - mnem_e);
                        if (ol >= SRC_MAX_LINE_LEN) ol = SRC_MAX_LINE_LEN - 1;
                        memcpy(obuf, mnem_e, ol); obuf[ol] = '\0';
                        if (piece) ImGui::SameLine(0, 0);
                        ImGui::TextUnformatted(obuf);
                        piece++;
                    }
                    /* Inline comment */
                    if (semi) {
                        if (piece) ImGui::SameLine(0, 0);
                        ImGui::TextColored(g_col.source_comment, "%s", semi);
                        piece++;
                    }
                    if (!piece) ImGui::Text(" ");
                }
            }
        }

        /* Scroll to hit */
        if (scroll_to >= 0) {
            float lh = ImGui::GetTextLineHeightWithSpacing();
            ImGui::SetScrollY((float)scroll_to * lh - avail_h * 0.4f);
            scroll_to = -1;
        }

        ImGui::EndTable();
    }
    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Pane: Statistics / Profiler (Phase 4)
 *
 * Three tabs: Histogram (top-N by exec count), Heatmap (65536-byte grid),
 * Cycles (execution budget by instruction category).
 * -------------------------------------------------------------------------- */
static void draw_pane_profiler(void)
{
    if (!show_profiler) return;
    ImGui::Begin("Profiler", &show_profiler);

    bool enabled = (sim_profiler_is_enabled(g_sim) != 0);
    if (ImGui::Checkbox("Record##prec", &enabled))
        sim_profiler_enable(g_sim, enabled ? 1 : 0);
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##pclr")) {
        sim_profiler_clear(g_sim);
        g_heatmap_dirty = true;
    }
    ImGui::Separator();

    if (ImGui::BeginTabBar("##proftabs")) {
        /* ---- Histogram tab ---- */
        if (ImGui::BeginTabItem("Histogram")) {
            uint16_t top_addrs[32];
            uint32_t top_counts[32];
            int top_n = sim_profiler_top_exec(g_sim, top_addrs, top_counts, 32);

            if (top_n == 0) {
                ImGui::TextDisabled("No data — enable Record and run the program.");
            } else {
                uint32_t max_c = top_counts[0] > 0 ? top_counts[0] : 1;
                const ImGuiTableFlags tf =
                    ImGuiTableFlags_SizingFixedFit |
                    ImGuiTableFlags_BordersInnerV  |
                    ImGuiTableFlags_ScrollY        |
                    ImGuiTableFlags_RowBg;
                float avail_h = ImGui::GetContentRegionAvail().y;
                if (ImGui::BeginTable("##histtab", 5, tf, ImVec2(0.0f, avail_h))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Addr",   ImGuiTableColumnFlags_WidthFixed,   52.0f);
                    ImGui::TableSetupColumn("Instr",  ImGuiTableColumnFlags_WidthFixed,   90.0f);
                    ImGui::TableSetupColumn("Count",  ImGuiTableColumnFlags_WidthFixed,   65.0f);
                    ImGui::TableSetupColumn("Cycles", ImGuiTableColumnFlags_WidthFixed,   65.0f);
                    ImGui::TableSetupColumn("Bar",    ImGuiTableColumnFlags_WidthStretch, 1.0f);
                    ImGui::TableHeadersRow();

                    for (int i = 0; i < top_n; i++) {
                        uint16_t addr  = top_addrs[i];
                        uint32_t count = top_counts[i];
                        uint32_t cycs  = sim_profiler_get_cycles(g_sim, addr);

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("$%04X", (unsigned)addr);

                        ImGui::TableSetColumnIndex(1);
                        char dbuf[64];
                        sim_disassemble_one(g_sim, addr, dbuf, sizeof(dbuf));
                        /* dbuf = "$XXXX: 18-char-hexdump mnemonic operand"
                         * mnemonic starts at offset 7+19 = 26 from the string.
                         * Simpler: skip the 7-char address prefix and show ~20 chars */
                        const char *ins = (strlen(dbuf) > 7) ? dbuf + 7 : dbuf;
                        /* Skip the hex dump field (18 chars) + 1 space */
                        const char *mn  = (strlen(ins) > 19) ? ins + 19 : ins;
                        while (*mn == ' ') mn++;
                        ImGui::TextColored(g_col.mnemonic,
                                           "%-16.16s", mn);

                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%u", (unsigned)count);

                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%u", (unsigned)cycs);

                        ImGui::TableSetColumnIndex(4);
                        float frac = (float)count / (float)max_c;
                        char overlay[24];
                        snprintf(overlay, sizeof(overlay), "%.1f%%", frac * 100.0f);
                        ImGui::ProgressBar(frac, ImVec2(-1.0f, 0.0f), overlay);
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndTabItem();
        }

        /* ---- Heatmap tab ---- */
        if (ImGui::BeginTabItem("Heatmap")) {
            if (g_heatmap_dirty && g_heatmap_tex)
                update_heatmap_tex();

            ImGui::TextDisabled("Execution density — X=low byte, Y=page (high byte)");
            ImGui::Separator();

            if (g_heatmap_tex) {
                float avail_w = ImGui::GetContentRegionAvail().x;
                float sz = (avail_w < 512.0f) ? avail_w : 512.0f;
                sz -= sz * fmodf(sz, 1.0f); /* keep integer size */
                ImVec2 img_size(sz, sz);
                ImGui::Image((ImTextureID)(uintptr_t)g_heatmap_tex, img_size);

                if (ImGui::IsItemHovered()) {
                    ImVec2 mp   = ImGui::GetMousePos();
                    ImVec2 rmin = ImGui::GetItemRectMin();
                    ImVec2 rmax = ImGui::GetItemRectMax();
                    float  fx   = (mp.x - rmin.x) / (rmax.x - rmin.x);
                    float  fy   = (mp.y - rmin.y) / (rmax.y - rmin.y);
                    if (fx >= 0.0f && fx < 1.0f && fy >= 0.0f && fy < 1.0f) {
                        int      bx = (int)(fx * 256);
                        int      by = (int)(fy * 256);
                        uint16_t ha = (uint16_t)(by * 256 + bx);
                        uint32_t hc = sim_profiler_get_exec(g_sim, ha);
                        ImGui::BeginTooltip();
                        ImGui::Text("$%04X : %u exec", (unsigned)ha, (unsigned)hc);
                        ImGui::EndTooltip();
                    }
                }
            } else {
                ImGui::TextColored(g_col.text_err,
                                   "Heatmap texture not initialised.");
            }
            ImGui::EndTabItem();
        }

        /* ---- Cycle Budget tab ---- */
        if (ImGui::BeginTabItem("Cycles")) {
            struct Bucket { const char *name; uint64_t cycles; uint32_t count; };
            Bucket buckets[10] = {
                { "Load",      0, 0 }, { "Store",     0, 0 },
                { "Branch",    0, 0 }, { "Jump/Call", 0, 0 },
                { "Stack",     0, 0 }, { "Arithmetic",0, 0 },
                { "Logic",     0, 0 }, { "Compare",   0, 0 },
                { "Transfer",  0, 0 }, { "Other",     0, 0 },
            };
            uint64_t total_cycles = 0;
            uint32_t total_exec   = 0;

            for (int addr = 0; addr < 65536; addr++) {
                uint32_t ec = sim_profiler_get_exec(g_sim, (uint16_t)addr);
                if (!ec) continue;
                uint32_t cc = sim_profiler_get_cycles(g_sim, (uint16_t)addr);
                total_exec   += ec;
                total_cycles += cc;

                /* Classify by opcode mnemonic */
                sim_opcode_info_t info;
                char m0 = 0, m1 = 0;
                uint8_t opbyte = sim_mem_read_byte(g_sim, (uint16_t)addr);
                if (sim_opcode_by_byte(g_sim, opbyte, &info)) {
                    m0 = (char)toupper((unsigned char)info.mnemonic[0]);
                    m1 = (char)toupper((unsigned char)info.mnemonic[1]);
                }
                int b = 9; /* Other */
                if      (m0=='L' && m1=='D')                   b = 0;
                else if (m0=='S' && m1=='T')                   b = 1;
                else if (m0=='B' && m1!='I' && m1!='R')        b = 2;
                else if (m0=='J' || (m0=='R' && (m1=='T'||m1=='E'))) b = 3;
                else if (m0=='P' && (m1=='H' || m1=='L'))      b = 4;
                else if ((m0=='A'&&m1=='D') || (m0=='S'&&m1=='B')
                         || m0=='I' || m0=='D')                b = 5;
                else if ((m0=='A'&&m1=='N') || m0=='O' || (m0=='E'&&m1=='O')) b = 6;
                else if (m0=='C' && (m1=='M'||m1=='P'))        b = 7;
                else if (m0=='T')                              b = 8;

                buckets[b].cycles += cc;
                buckets[b].count  += ec;
            }

            if (total_exec == 0) {
                ImGui::TextDisabled("No data — enable Record and run the program.");
            } else {
                ImGui::Text("Total: %u instructions, %llu cycles",
                    (unsigned)total_exec, (unsigned long long)total_cycles);
                ImGui::Separator();

                const ImGuiTableFlags tf =
                    ImGuiTableFlags_SizingFixedFit |
                    ImGuiTableFlags_BordersInnerV  |
                    ImGuiTableFlags_RowBg;
                if (ImGui::BeginTable("##cycbud", 4, tf)) {
                    ImGui::TableSetupColumn("Category",  ImGuiTableColumnFlags_WidthFixed,   90.0f);
                    ImGui::TableSetupColumn("Count",     ImGuiTableColumnFlags_WidthFixed,   65.0f);
                    ImGui::TableSetupColumn("Cycles",    ImGuiTableColumnFlags_WidthFixed,   80.0f);
                    ImGui::TableSetupColumn("Share",     ImGuiTableColumnFlags_WidthStretch, 1.0f);
                    ImGui::TableHeadersRow();

                    for (int i = 0; i < 10; i++) {
                        if (!buckets[i].count) continue;
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(buckets[i].name);
                        ImGui::TableSetColumnIndex(1); ImGui::Text("%u", (unsigned)buckets[i].count);
                        ImGui::TableSetColumnIndex(2); ImGui::Text("%llu", (unsigned long long)buckets[i].cycles);
                        ImGui::TableSetColumnIndex(3);
                        float frac = total_cycles ? (float)buckets[i].cycles / (float)total_cycles : 0.0f;
                        char overlay[16]; snprintf(overlay, sizeof(overlay), "%.1f%%", frac * 100.0f);
                        ImGui::ProgressBar(frac, ImVec2(-1.0f, 0.0f), overlay);
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Toolbar
 * -------------------------------------------------------------------------- */
static void draw_toolbar(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));

    float btn_w   = 60.0f;
    float combo_w = 110.0f;
    float avail   = ImGui::GetContentRegionAvail().x;
    float input_w = avail - combo_w - btn_w * 5.0f
                  - ImGui::GetStyle().ItemSpacing.x * 8.0f - 12.0f;
    if (input_w < 80.0f) input_w = 80.0f;

    if (g_focus_filename) {
        ImGui::SetKeyboardFocusHere();
        g_focus_filename = false;
    }

    ImGui::SetNextItemWidth(input_w);
    bool enter_pressed = ImGui::InputText("##filename", g_filename_buf,
                                          sizeof(g_filename_buf),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Path to .asm / .bin / .prg file (Ctrl+L to focus)");

    ImGui::SameLine();
    if (ImGui::Button("Load") || enter_pressed) {
        if (ext_is(g_filename_buf, ".bin") || ext_is(g_filename_buf, ".prg")) {
            open_binload_dialog(g_filename_buf);
        } else {
            do_load();
            if (sim_get_state(g_sim) != SIM_IDLE)
                con_add(CON_COL_OK, "Loaded: %s", g_filename_buf);
            else
                con_add(CON_COL_ERR, "Failed to load: %s", g_filename_buf);
        }
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    /* Step (F7) */
    bool can_step = (sim_get_state(g_sim) == SIM_READY ||
                     sim_get_state(g_sim) == SIM_PAUSED);
    if (!can_step) ImGui::BeginDisabled();
    if (ImGui::Button("Step")) {
        cpu_t *cpu = sim_get_cpu(g_sim);
        if (cpu) { g_prev_cpu = *cpu; g_prev_cpu_valid = true; }
        sim_step(g_sim, 1);
        g_last_write_count = sim_get_last_writes(g_sim, g_last_writes, 256);
        update_watches();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Step into 1 instruction (F7) / Step over: F8");
    if (!can_step) ImGui::EndDisabled();

    ImGui::SameLine();

    /* Run / Pause toggle (F5 / F6) */
    bool can_run = (sim_get_state(g_sim) == SIM_READY ||
                    sim_get_state(g_sim) == SIM_PAUSED || g_running);
    if (!can_run) ImGui::BeginDisabled();
    if (g_running) {
        if (ImGui::Button("Pause")) g_running = false;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause execution (F6 / Esc)");
    } else {
        if (ImGui::Button(" Run ")) g_running = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run continuously (F5)");
    }
    if (!can_run) ImGui::EndDisabled();

    ImGui::SameLine();

    /* Reset (Ctrl+R) */
    bool can_reset = (sim_get_state(g_sim) != SIM_IDLE);
    if (!can_reset) ImGui::BeginDisabled();
    if (ImGui::Button("Reset")) {
        g_running = false;
        sim_reset(g_sim);
        g_prev_cpu_valid   = false;
        g_last_write_count = 0;
        for (int i = 0; i < g_watch_count; i++) g_watches[i].valid = false;
        update_watches();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset CPU (Ctrl+R)");
    if (!can_reset) ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    /* Processor dropdown */
    ImGui::SetNextItemWidth(combo_w);
    if (ImGui::BeginCombo("##proc", g_proc_labels[g_proc_idx])) {
        for (int i = 0; i < (int)(sizeof(g_proc_labels) / sizeof(g_proc_labels[0])); i++) {
            bool selected = (i == g_proc_idx);
            if (ImGui::Selectable(g_proc_labels[i], selected)) {
                g_proc_idx = i;
                sim_set_processor(g_sim, g_proc_ids[i]);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Active processor");

    ImGui::PopStyleVar();
    ImGui::Separator();
}

/* --------------------------------------------------------------------------
 * Status bar
 * -------------------------------------------------------------------------- */
static void draw_statusbar(void)
{
    ImGui::Separator();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));

    sim_state_t state = sim_get_state(g_sim);
    cpu_t *cpu        = sim_get_cpu(g_sim);

    if (g_running)
        ImGui::TextColored(g_col.text_ok, "RUNNING");
    else {
        const char *sname = sim_state_name(state);
        if      (state == SIM_IDLE)     ImGui::TextDisabled("%s", sname);
        else if (state == SIM_FINISHED) ImGui::TextColored(g_col.text_warn_orange, "%s", sname);
        else                            ImGui::Text("%s", sname);
    }

    ImGui::SameLine(0, 20);

    if (state != SIM_IDLE && cpu) {
        ImGui::Text("PC=%04X  A=%02X  X=%02X  Y=%02X  S=%04X  Cycles=%lu",
                    cpu->pc, cpu->a, cpu->x, cpu->y, cpu->s, cpu->cycles);
    } else {
        ImGui::TextDisabled("PC=----  A=--  X=--  Y=--  S=----  Cycles=0");
    }

    const char *fname = sim_get_filename(g_sim);
    float fname_w = ImGui::CalcTextSize(fname).x + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - fname_w);
    ImGui::TextDisabled("%s", fname);

    ImGui::PopStyleVar();
}

/* --------------------------------------------------------------------------
 * Pane: Registers (Phase 2)
 *
 * Compact mode: coloured diff highlighting per register, flags row, cycles.
 * Expanded mode: InputScalar fields for live editing of each register.
 * Yellow text = value changed since last sim_step.
 * -------------------------------------------------------------------------- */
static void draw_pane_registers(void)
{
    if (!show_registers) return;
    ImGui::Begin("Registers", &show_registers);

    cpu_t *cpu        = sim_get_cpu(g_sim);
    sim_state_t state = sim_get_state(g_sim);
    if (state == SIM_IDLE || !cpu) {
        ImGui::TextDisabled("(no program loaded)");
        ImGui::End();
        return;
    }

    bool is_45gs02 = (sim_get_cpu_type(g_sim) == CPU_45GS02);
    cpu_t *prev    = g_prev_cpu_valid ? &g_prev_cpu : NULL;

    /* Expand/Compact toggle button — right-aligned */
    {
        const char *btn = g_regs_expanded ? "Compact" : "Expand";
        float bw = ImGui::CalcTextSize(btn).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - bw);
        if (ImGui::SmallButton(btn)) g_regs_expanded = !g_regs_expanded;
    }
    ImGui::Separator();

    /* Highlight if changed, otherwise default */
    auto diff_col = [&](bool changed) -> ImVec4 {
        return changed ? g_col.text_changed
                       : ImGui::GetStyleColorVec4(ImGuiCol_Text);
    };

    if (!g_regs_expanded) {
        /* ---- Compact mode ---- */
        auto show_byte = [&](const char *name, uint8_t cur, uint8_t old_val) {
            bool ch = prev && (old_val != cur);
            ImGui::PushStyleColor(ImGuiCol_Text, diff_col(ch));
            ImGui::Text("%s %02X", name, (unsigned)cur);
            ImGui::PopStyleColor();
        };

        show_byte("A", cpu->a, prev ? prev->a : cpu->a);
        ImGui::SameLine(0, 10);
        show_byte("X", cpu->x, prev ? prev->x : cpu->x);
        ImGui::SameLine(0, 10);
        show_byte("Y", cpu->y, prev ? prev->y : cpu->y);

        if (is_45gs02) {
            show_byte("Z", cpu->z, prev ? prev->z : cpu->z);
            ImGui::SameLine(0, 10);
            show_byte("B", cpu->b, prev ? prev->b : cpu->b);
        }

        ImGui::Separator();

        {
            bool ch_s  = prev && prev->s  != cpu->s;
            bool ch_pc = prev && prev->pc != cpu->pc;
            ImGui::PushStyleColor(ImGuiCol_Text, diff_col(ch_s));
            if (is_45gs02)
                ImGui::Text("S  %04X", (unsigned)cpu->s);
            else
                ImGui::Text("S  %04X", (unsigned)(0x0100 | (cpu->s & 0xFF)));
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 10);
            ImGui::PushStyleColor(ImGuiCol_Text, diff_col(ch_pc));
            ImGui::Text("PC %04X", (unsigned)cpu->pc);
            ImGui::PopStyleColor();
        }

        ImGui::Separator();

        /* Flags — green=set, dim=clear, yellow=changed */
        struct { const char *lbl; uint8_t mask; } flags[] = {
            {"N", FLAG_N}, {"V", FLAG_V}, {"-", FLAG_U},
            {"B", FLAG_B}, {"D", FLAG_D}, {"I", FLAG_I},
            {"Z", FLAG_Z}, {"C", FLAG_C},
        };
        for (int i = 0; i < 8; i++) {
            bool set     = (cpu->p & flags[i].mask) != 0;
            bool changed = prev && ((prev->p & flags[i].mask) != (cpu->p & flags[i].mask));
            ImVec4 col;
            if      (changed) col = g_col.text_changed;
            else if (set)     col = g_col.text_ok;
            else              col = g_col.flag_dim;
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::Text("%s", flags[i].lbl);
            ImGui::PopStyleColor();
            if (i < 7) ImGui::SameLine();
        }

        ImGui::Separator();

        bool ch_cy = prev && prev->cycles != cpu->cycles;
        ImGui::PushStyleColor(ImGuiCol_Text, diff_col(ch_cy));
        ImGui::Text("Cycles  %lu", cpu->cycles);
        ImGui::PopStyleColor();

        if (is_45gs02 && cpu->eom_prefix)
            ImGui::TextColored(g_col.text_warn_orange, "[EOM]");

    } else {
        /* ---- Expanded / editable mode ---- */
        auto edit_byte = [&](const char *name, uint8_t cur, uint8_t prev_val) {
            bool ch = prev && (prev_val != cur);
            if (ch) ImGui::PushStyleColor(ImGuiCol_Text, g_col.text_changed);
            ImGui::Text("%-2s", name);
            if (ch) ImGui::PopStyleColor();
            ImGui::SameLine(45);
            ImGui::SetNextItemWidth(56);
            char id[8]; snprintf(id, sizeof(id), "##%s", name);
            uint8_t edited = cur;
            if (ImGui::InputScalar(id, ImGuiDataType_U8, &edited,
                                   NULL, NULL, "%02X",
                                   ImGuiInputTextFlags_CharsHexadecimal))
                sim_set_reg_byte(g_sim, name, edited);
        };

        edit_byte("A", cpu->a, prev ? prev->a : cpu->a);
        edit_byte("X", cpu->x, prev ? prev->x : cpu->x);
        edit_byte("Y", cpu->y, prev ? prev->y : cpu->y);
        if (is_45gs02) {
            edit_byte("Z", cpu->z, prev ? prev->z : cpu->z);
            edit_byte("B", cpu->b, prev ? prev->b : cpu->b);
        }

        ImGui::Separator();

        /* S — 16-bit on 45GS02, 8-bit page 1 on others */
        {
            bool ch_s = prev && prev->s != cpu->s;
            if (ch_s) ImGui::PushStyleColor(ImGuiCol_Text, g_col.text_changed);
            ImGui::Text("S ");
            if (ch_s) ImGui::PopStyleColor();
            ImGui::SameLine(45);
            ImGui::SetNextItemWidth(56);
            if (is_45gs02) {
                uint16_t s_edit = cpu->s;
                if (ImGui::InputScalar("##S", ImGuiDataType_U16, &s_edit,
                                       NULL, NULL, "%04X",
                                       ImGuiInputTextFlags_CharsHexadecimal))
                    sim_set_reg_byte(g_sim, "S", (uint8_t)(s_edit & 0xFF));
            } else {
                uint8_t s_edit = (uint8_t)(cpu->s & 0xFF);
                if (ImGui::InputScalar("##S", ImGuiDataType_U8, &s_edit,
                                       NULL, NULL, "%02X",
                                       ImGuiInputTextFlags_CharsHexadecimal))
                    sim_set_reg_byte(g_sim, "S", s_edit);
            }
        }

        /* PC — 16-bit */
        {
            bool ch_pc = prev && prev->pc != cpu->pc;
            if (ch_pc) ImGui::PushStyleColor(ImGuiCol_Text, g_col.text_changed);
            ImGui::Text("PC");
            if (ch_pc) ImGui::PopStyleColor();
            ImGui::SameLine(45);
            ImGui::SetNextItemWidth(56);
            uint16_t pc_edit = cpu->pc;
            if (ImGui::InputScalar("##PC", ImGuiDataType_U16, &pc_edit,
                                   NULL, NULL, "%04X",
                                   ImGuiInputTextFlags_CharsHexadecimal))
                sim_set_pc(g_sim, pc_edit);
        }

        edit_byte("P", cpu->p, prev ? prev->p : cpu->p);

        ImGui::Separator();
        ImGui::Text("Cycles  %lu", cpu->cycles);
    }

    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Pane: Disassembly (Phase 2)
 *
 * Table columns: BP gutter (clickable) | PC arrow | disasm text | cycles
 * Follow-PC mode: g_disasm_addr tracks cpu->pc each frame.
 * Mouse wheel scrolls the view when not following PC.
 * -------------------------------------------------------------------------- */
static void draw_pane_disassembly(void)
{
    if (!show_disassembly) return;
    ImGui::Begin("Disassembly", &show_disassembly);

    sim_state_t state = sim_get_state(g_sim);
    if (state == SIM_IDLE) {
        ImGui::TextDisabled("(no program loaded)");
        ImGui::End();
        return;
    }

    cpu_t *cpu  = sim_get_cpu(g_sim);
    uint16_t pc = cpu ? cpu->pc : 0;

    /* Controls bar */
    ImGui::Checkbox("Follow PC", &g_disasm_follow);
    ImGui::SameLine();
    {
        char addr_buf[8];
        snprintf(addr_buf, sizeof(addr_buf), "%04X", (unsigned)g_disasm_addr);
        ImGui::SetNextItemWidth(60);
        if (g_focus_disasm_addr) {
            ImGui::SetKeyboardFocusHere();
            g_focus_disasm_addr = false;
        }
        if (ImGui::InputText("##da", addr_buf, sizeof(addr_buf),
                ImGuiInputTextFlags_CharsHexadecimal |
                ImGuiInputTextFlags_EnterReturnsTrue)) {
            g_disasm_addr   = (uint16_t)strtol(addr_buf, NULL, 16);
            g_disasm_follow = false;
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("PC")) {
        g_disasm_addr   = pc;
        g_disasm_follow = true;
    }
    ImGui::Separator();

    /* Follow PC: keep view aligned to current instruction */
    if (g_disasm_follow)
        g_disasm_addr = pc;

    /* Mouse wheel scrolling (when not following PC) */
    if (ImGui::IsWindowHovered() && !g_disasm_follow) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel < 0.0f) {
            /* Scroll down: advance by instruction */
            int ticks = (int)(-wheel + 0.5f);
            char tmp[128];
            uint16_t a = g_disasm_addr;
            for (int i = 0; i < ticks; i++) {
                int c = sim_disassemble_one(g_sim, a, tmp, sizeof(tmp));
                if (c < 1) c = 1;
                a = (uint16_t)(a + c);
            }
            g_disasm_addr = a;
        } else if (wheel > 0.0f) {
            /* Scroll up: approximate 3 bytes per instruction */
            int ticks = (int)(wheel + 0.5f);
            g_disasm_addr = (uint16_t)(g_disasm_addr - ticks * 3);
        }
    }

    /* Estimate rows that fit */
    float row_h  = ImGui::GetTextLineHeightWithSpacing();
    float avail_h = ImGui::GetContentRegionAvail().y;
    int rows = (avail_h > 0) ? (int)(avail_h / row_h) + 1 : 24;
    if (rows < 4)  rows = 4;
    if (rows > 80) rows = 80;

    /* Table */
    const ImGuiTableFlags tflags =
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_BordersInnerV;

    if (ImGui::BeginTable("##dasm", 4, tflags)) {
        ImGui::TableSetupColumn("BP",    ImGuiTableColumnFlags_WidthFixed, 18.0f);
        ImGui::TableSetupColumn(" ",     ImGuiTableColumnFlags_WidthFixed, 14.0f);
        ImGui::TableSetupColumn("Instr", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Cyc",   ImGuiTableColumnFlags_WidthFixed, 24.0f);
        ImGui::TableHeadersRow();

        char buf[128];
        uint16_t addr = g_disasm_addr;

        for (int i = 0; i < rows; i++) {
            bool is_pc = (addr == pc);
            bool has_bp = (sim_has_breakpoint(g_sim, addr) != 0);
            int consumed = sim_disassemble_one(g_sim, addr, buf, sizeof(buf));

            ImGui::TableNextRow();

            /* BP gutter — clickable to toggle */
            ImGui::TableSetColumnIndex(0);
            {
                char btn_id[16];
                snprintf(btn_id, sizeof(btn_id), "%s##%04X",
                         has_bp ? "*" : ".", (unsigned)addr);
                if (has_bp)
                    ImGui::PushStyleColor(ImGuiCol_Text, g_col.text_err);
                else
                    ImGui::PushStyleColor(ImGuiCol_Text, g_col.bp_empty);
                if (ImGui::SmallButton(btn_id)) {
                    if (has_bp) sim_break_clear(g_sim, addr);
                    else        sim_break_set(g_sim, addr, NULL);
                }
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(has_bp ? "Remove breakpoint at $%04X"
                                             : "Set breakpoint at $%04X",
                                      (unsigned)addr);
            }

            /* PC arrow */
            ImGui::TableSetColumnIndex(1);
            if (is_pc)
                ImGui::TextColored(g_col.text_changed, ">");
            else
                ImGui::TextDisabled(" ");

            /* Disassembly text */
            ImGui::TableSetColumnIndex(2);
            {
                const char *sym = sim_sym_by_addr(g_sim, addr);
                if (sym) {
                    ImGui::PushStyleColor(ImGuiCol_Text, g_col.symbol_label);
                    ImGui::Text("%s:", sym);
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0, 6);
                }
                if (is_pc)
                    ImGui::PushStyleColor(ImGuiCol_Text, g_col.text_changed);
                else if (has_bp)
                    ImGui::PushStyleColor(ImGuiCol_Text, g_col.bp_text);
                ImGui::Text("%s", buf);
                if (is_pc || has_bp)
                    ImGui::PopStyleColor();
            }

            /* Cycles */
            ImGui::TableSetColumnIndex(3);
            int cyc = sim_get_opcode_cycles(g_sim, addr);
            if (cyc > 0)
                ImGui::TextDisabled("%d", cyc);

            if (consumed < 1) consumed = 1;
            addr = (uint16_t)(addr + consumed);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Pane: Memory View (Phase 2)
 *
 * 16-byte rows, hex + ASCII sidebar.
 * Bytes written in the most-recent sim_step are highlighted yellow.
 * -------------------------------------------------------------------------- */
static void draw_pane_memory(void)
{
    if (!show_memory) return;
    ImGui::Begin("Memory", &show_memory);

    sim_state_t state = sim_get_state(g_sim);
    if (state == SIM_IDLE) {
        ImGui::TextDisabled("(no program loaded)");
        ImGui::End();
        return;
    }

    static uint16_t mem_base  = 0x0200;
    static char     addr_buf[8] = "0200";

    /* Navigation bar */
    ImGui::SetNextItemWidth(60);
    if (ImGui::InputText("##ma", addr_buf, sizeof(addr_buf),
            ImGuiInputTextFlags_CharsHexadecimal |
            ImGuiInputTextFlags_EnterReturnsTrue)) {
        mem_base = (uint16_t)strtol(addr_buf, NULL, 16);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("PC")) {
        cpu_t *cpu = sim_get_cpu(g_sim);
        if (cpu) {
            mem_base = cpu->pc;
            snprintf(addr_buf, sizeof(addr_buf), "%04X", (unsigned)mem_base);
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("SP")) {
        cpu_t *cpu = sim_get_cpu(g_sim);
        if (cpu) {
            mem_base = (uint16_t)(0x0100 | (cpu->s & 0xFF));
            snprintf(addr_buf, sizeof(addr_buf), "%04X", (unsigned)mem_base);
        }
    }
    ImGui::Separator();

    /* Is this address in the last-write log? */
    auto was_written = [&](uint16_t a) -> bool {
        for (int i = 0; i < g_last_write_count; i++)
            if (g_last_writes[i] == a) return true;
        return false;
    };

    /* Hex dump — 16 bytes per row, 16 rows */
    for (int row = 0; row < 16; row++) {
        uint16_t raddr = (uint16_t)(mem_base + row * 16);
        ImGui::Text("%04X ", (unsigned)raddr);
        for (int col = 0; col < 16; col++) {
            uint16_t ba  = (uint16_t)(raddr + col);
            uint8_t  v   = sim_mem_read_byte(g_sim, ba);
            bool written = was_written(ba);
            ImGui::SameLine(0, col == 8 ? 8 : 4);
            if (written)
                ImGui::TextColored(g_col.text_changed, "%02X", (unsigned)v);
            else
                ImGui::Text("%02X", (unsigned)v);
        }
        /* ASCII sidebar */
        ImGui::SameLine(0, 8);
        ImGui::TextDisabled("|");
        for (int col = 0; col < 16; col++) {
            uint8_t v = sim_mem_read_byte(g_sim, (uint16_t)(raddr + col));
            char ch = (v >= 0x20 && v < 0x7F) ? (char)v : '.';
            ImGui::SameLine(0, 0);
            ImGui::TextDisabled("%c", ch);
        }
        ImGui::SameLine(0, 0);
        ImGui::TextDisabled("|");
    }

    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Pane: Console (Phase 2)
 *
 * Scrollable text log with coloured output, command-line input, and
 * Up/Down history navigation.
 * -------------------------------------------------------------------------- */
static void draw_pane_console(void)
{
    if (!show_console) return;
    ImGui::Begin("Console", &show_console);

    /* Output scroll region */
    float input_h = ImGui::GetFrameHeightWithSpacing() + 4.0f;
    ImGui::BeginChild("##con_out", ImVec2(0, -input_h), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    for (int i = 0; i < g_con_line_count; i++) {
        ImGui::PushStyleColor(ImGuiCol_Text, g_con_lines[i].color);
        ImGui::TextUnformatted(g_con_lines[i].text);
        ImGui::PopStyleColor();
    }

    if (g_con_scroll_bottom) {
        ImGui::SetScrollHereY(1.0f);
        g_con_scroll_bottom = false;
    }

    ImGui::EndChild();
    ImGui::Separator();

    /* Command input */
    ImGui::PushItemWidth(-1);
    bool reclaim = false;
    if (g_focus_console) {
        ImGui::SetKeyboardFocusHere();
        g_focus_console = false;
    }
    if (ImGui::InputText("##coninput", g_con_input, sizeof(g_con_input),
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_CallbackHistory,
            con_input_cb)) {
        if (g_con_input[0] != '\0') {
            con_exec(g_con_input);
            g_con_input[0] = '\0';
        }
        reclaim = true;
    }
    ImGui::PopItemWidth();

    /* Keep focus on the input field after Enter */
    if (reclaim)
        ImGui::SetKeyboardFocusHere(-1);

    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Binary load popup (.bin / .prg)
 * -------------------------------------------------------------------------- */
static void draw_binload_popup(void)
{
    if (g_binload_open) {
        ImGui::OpenPopup("Load Binary##binload");
        g_binload_open = false;
    }
    if (ImGui::BeginPopupModal("Load Binary##binload", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("File: %s", g_binload_path);
        ImGui::Separator();

        if (g_binload_is_prg) {
            ImGui::Text("PRG header address: $%04X", (unsigned)g_binload_header);
            ImGui::Checkbox("Override load address", &g_binload_override);
            if (!g_binload_override) ImGui::BeginDisabled();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            ImGui::InputText("##prg_addr", g_binload_addr, sizeof(g_binload_addr),
                ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll);
            if (!g_binload_override) ImGui::EndDisabled();
        } else {
            ImGui::Text("Load address (hex):");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            ImGui::InputText("##bin_addr", g_binload_addr, sizeof(g_binload_addr),
                ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll);
        }

        ImGui::Separator();
        bool do_load_bin = ImGui::Button("Load");
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();

        if (do_load_bin) {
            int ok;
            if (g_binload_is_prg) {
                uint16_t override_addr = g_binload_override
                    ? (uint16_t)strtol(g_binload_addr, nullptr, 16)
                    : 0;
                ok = sim_load_prg(g_sim, g_binload_path, override_addr);
            } else {
                uint16_t addr = (uint16_t)strtol(g_binload_addr, nullptr, 16);
                ok = sim_load_bin(g_sim, g_binload_path, addr);
            }
            if (ok == 0) {
                g_prev_cpu_valid   = false;
                g_last_write_count = 0;
                for (int i = 0; i < g_watch_count; i++) g_watches[i].valid = false;
                update_watches();
                uint16_t laddr = 0, lsize = 0;
                sim_get_load_info(g_sim, &laddr, &lsize);
                con_add(CON_COL_OK, "bload: %u bytes at $%04X%s",
                        (unsigned)lsize, (unsigned)laddr,
                        g_binload_is_prg ? " (PRG)" : "");
                snprintf(g_filename_buf, sizeof(g_filename_buf), "%s", g_binload_path);
            } else {
                con_add(CON_COL_ERR, "bload: failed to open '%s'", g_binload_path);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

/* --------------------------------------------------------------------------
 * Binary save popup (.bin / .prg)
 * -------------------------------------------------------------------------- */
static void draw_binsave_popup(void)
{
    if (g_binsave_open) {
        ImGui::OpenPopup("Save Binary##binsave");
        g_binsave_open = false;
    }
    if (ImGui::BeginPopupModal("Save Binary##binsave", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Output path (.prg = PRG header, .bin = raw):");
        ImGui::SetNextItemWidth(320.0f);
        ImGui::InputText("##savepath", g_binsave_path, sizeof(g_binsave_path));

        ImGui::Text("Start (hex):");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::InputText("##savestart", g_binsave_start, sizeof(g_binsave_start),
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll);
        ImGui::SameLine();
        ImGui::Text("  Count (hex):");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::InputText("##savecount", g_binsave_count, sizeof(g_binsave_count),
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll);

        uint16_t start = (uint16_t)strtol(g_binsave_start, nullptr, 16);
        uint16_t count = (uint16_t)strtol(g_binsave_count, nullptr, 16);
        bool is_prg = ext_is(g_binsave_path, ".prg");
        ImGui::TextDisabled("Format: %s  |  End: $%04X",
            is_prg ? "PRG (2-byte header + data)" : "raw binary",
            (unsigned)((start + count) & 0xFFFF));

        ImGui::Separator();
        bool valid = (count > 0 && g_binsave_path[0] != '\0');
        if (!valid) ImGui::BeginDisabled();
        bool do_save = ImGui::Button("Save");
        if (!valid) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();

        if (do_save && valid) {
            int ok = is_prg
                ? sim_save_prg(g_sim, g_binsave_path, start, count)
                : sim_save_bin(g_sim, g_binsave_path, start, count);
            if (ok == 0)
                con_add(CON_COL_OK, "bsave: %u bytes at $%04X → '%s'%s",
                        (unsigned)count, (unsigned)start, g_binsave_path,
                        is_prg ? " (PRG)" : "");
            else
                con_add(CON_COL_ERR, "bsave: failed to write '%s'", g_binsave_path);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

/* --------------------------------------------------------------------------
 * Go-to-address popup (Ctrl+G)
 * -------------------------------------------------------------------------- */
static void draw_goto_popup(void)
{
    if (g_goto_open) {
        ImGui::OpenPopup("Go to Address##goto");
        g_goto_open = false;
    }
    if (ImGui::BeginPopupModal("Go to Address##goto", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Navigate disassembly to hex address:");
        ImGui::SetNextItemWidth(120.0f);
        bool enter = ImGui::InputText("##gotoaddr", g_goto_buf, sizeof(g_goto_buf),
                        ImGuiInputTextFlags_CharsHexadecimal |
                        ImGuiInputTextFlags_EnterReturnsTrue  |
                        ImGuiInputTextFlags_AutoSelectAll);
        ImGui::SameLine();
        bool ok = ImGui::Button("Go") || enter;
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        if (ok && g_goto_buf[0]) {
            g_disasm_addr    = (uint16_t)strtol(g_goto_buf, nullptr, 16);
            g_disasm_follow  = false;
            show_disassembly = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

/* --------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------- */
int main(int /*argc*/, char ** /*argv*/)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    const int display_index = 0;
    const float ui_scale    = detect_ui_scale(display_index);
    g_ui_scale = ui_scale;

    SDL_Rect usable = { 0, 0, 1600, 900 };
    SDL_GetDisplayUsableBounds(display_index, &usable);

    /* Pre-parse INI before SDL/ImGui init so font size and window geometry
     * are correct from frame 1 — no rebuild flash.                        */
    bool first_run;
    int  pre_win_x = -32768, pre_win_y = -32768;
    int  pre_win_w = 0,      pre_win_h = 0;
    {
        FILE *ini = fopen("sim6502-gui.ini", "r");
        first_run = (ini == nullptr);
        if (ini) {
            char ln[256];
            bool in_sec = false;
            while (fgets(ln, sizeof(ln), ini)) {
                if (strncmp(ln, "[sim6502][Settings]", 19) == 0) { in_sec = true;  continue; }
                if (ln[0] == '[')                                 { in_sec = false; continue; }
                if (!in_sec) continue;
                int v;
                if (sscanf(ln, "FontSize=%d", &v) == 1 && v >= 8  && v <= 32)  g_base_font_size = v;
                if (sscanf(ln, "Theme=%d",    &v) == 1 && v >= 0  && v <= 2)   g_theme = v;
                if (sscanf(ln, "WindowW=%d",  &v) == 1 && v >= 320)             pre_win_w = v;
                if (sscanf(ln, "WindowH=%d",  &v) == 1 && v >= 240)             pre_win_h = v;
                if (sscanf(ln, "WindowX=%d",  &v) == 1)                         pre_win_x = v;
                if (sscanf(ln, "WindowY=%d",  &v) == 1)                         pre_win_y = v;
            }
            fclose(ini);
        }
    }

    const int win_w = (pre_win_w > 0) ? pre_win_w : ((int)(usable.w * 0.80f) / 2) * 2;
    const int win_h = (pre_win_h > 0) ? pre_win_h : ((int)(usable.h * 0.80f) / 2) * 2;
    const bool has_saved_pos = (pre_win_x != -32768 && pre_win_y != -32768);
    const int  win_x = has_saved_pos ? pre_win_x : SDL_WINDOWPOS_CENTERED;
    const int  win_y = has_saved_pos ? pre_win_y : SDL_WINDOWPOS_CENTERED;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    SDL_Window *window = SDL_CreateWindow(
        "sim6502-gui",
        win_x, win_y,
        win_w, win_h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit(); return 1;
    }
    g_window = window;

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    if (!gl_ctx) {
        fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    /* Custom INI handler: persists FontSize and window geometry */
    {
        ImGuiSettingsHandler h = {};
        h.TypeName   = "sim6502";
        h.TypeHash   = ImHashStr("sim6502");
        h.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char*) -> void* {
            return (void*)(intptr_t)1;
        };
        h.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line) {
            int v;
            if (sscanf(line, "FontSize=%d", &v) == 1 && v >= 8 && v <= 32)
                if (v != g_base_font_size) { g_base_font_size = v; g_font_rebuild = true; }
            if (sscanf(line, "Theme=%d", &v) == 1 && v >= 0 && v <= 2)
                if (v != g_theme) { g_theme = v; g_theme_rebuild = true; }
        };
        h.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
            int x = 0, y = 0, w = 800, wh = 600;
            if (g_window) {
                SDL_GetWindowPosition(g_window, &x, &y);
                SDL_GetWindowSize(g_window, &w, &wh);
            }
            buf->appendf("[%s][Settings]\n", handler->TypeName);
            buf->appendf("FontSize=%d\n", g_base_font_size);
            buf->appendf("Theme=%d\n",    g_theme);
            buf->appendf("WindowX=%d\n",  x);
            buf->appendf("WindowY=%d\n",  y);
            buf->appendf("WindowW=%d\n",  w);
            buf->appendf("WindowH=%d\n",  wh);
            buf->appendf("\n");
        };
        ImGui::AddSettingsHandler(&h);
    }

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename  = "sim6502-gui.ini";

    apply_theme();

    ImFontConfig font_cfg;
    font_cfg.SizePixels = floorf((float)g_base_font_size * ui_scale);
    io.Fonts->AddFontDefault(&font_cfg);

    ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 330");

    /* Create heatmap texture for profiler (256×256 RGB, nearest-filter) */
    glGenTextures(1, &g_heatmap_tex);
    glBindTexture(GL_TEXTURE_2D, g_heatmap_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    fprintf(stdout, "sim6502-gui: display %d  scale=%.2fx  font=%.0fpx  window=%dx%d%s\n",
            display_index, ui_scale, font_cfg.SizePixels,
            win_w, win_h, first_run ? "  (first run)" : "");

    /* Create simulator session */
    g_sim = sim_create("6502");
    if (!g_sim) {
        fprintf(stderr, "sim6502-gui: failed to create sim session\n");
        return 1;
    }

    /* Console welcome message */
    con_add(CON_COL_OK,     "sim6502-gui Phase 4 ready.  Type 'help' for commands.");
    con_add(CON_COL_NORMAL, "Processor: %s", sim_processor_name(g_sim));

    bool running        = true;
    bool layout_applied = false;

    while (running) {
        /* ---- Events ---- */
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT)
                running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_F4
                    && (ev.key.keysym.mod & KMOD_ALT))
                running = false;
        }

        /* ---- Keyboard shortcuts (when ImGui is not capturing keyboard) ---- */
        if (!io.WantCaptureKeyboard) {
            bool ctrl  = ImGui::IsKeyDown(ImGuiKey_LeftCtrl)  || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
            bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);

            /* F5: Run */
            if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
                if (sim_get_state(g_sim) == SIM_READY ||
                    sim_get_state(g_sim) == SIM_PAUSED)
                    g_running = true;
            }

            /* F6 / Esc: Pause */
            if (ImGui::IsKeyPressed(ImGuiKey_F6) ||
                ImGui::IsKeyPressed(ImGuiKey_Escape))
                g_running = false;

            /* Shift+F7: Step back (Phase 6) / F7: Step into */
            if (ImGui::IsKeyPressed(ImGuiKey_F7)) {
                if (shift) {
                    con_add(CON_COL_WARN, "Step-back not yet available (Phase 6).");
                } else {
                    g_running = false;
                    if (sim_get_state(g_sim) == SIM_READY ||
                        sim_get_state(g_sim) == SIM_PAUSED) {
                        cpu_t *cpu = sim_get_cpu(g_sim);
                        if (cpu) { g_prev_cpu = *cpu; g_prev_cpu_valid = true; }
                        sim_step(g_sim, 1);
                        g_last_write_count = sim_get_last_writes(g_sim, g_last_writes, 256);
                        update_watches();
                    }
                }
            }

            /* Shift+F8: Step forward (Phase 6) / F8: Step over */
            if (ImGui::IsKeyPressed(ImGuiKey_F8)) {
                if (shift)
                    con_add(CON_COL_WARN, "Step-forward not yet available (Phase 6).");
                else
                    do_step_over();
            }

            /* F9: Toggle breakpoint at current PC */
            if (ImGui::IsKeyPressed(ImGuiKey_F9)) {
                cpu_t *cpu = sim_get_cpu(g_sim);
                if (cpu && sim_get_state(g_sim) != SIM_IDLE) {
                    if (sim_has_breakpoint(g_sim, cpu->pc))
                        sim_break_clear(g_sim, cpu->pc);
                    else
                        sim_break_set(g_sim, cpu->pc, NULL);
                }
            }

            if (ctrl) {
                /* Ctrl+Shift+R: Reverse-continue (Phase 6) / Ctrl+R: Reset */
                if (ImGui::IsKeyPressed(ImGuiKey_R)) {
                    if (shift) {
                        con_add(CON_COL_WARN, "Reverse-continue not yet available (Phase 6).");
                    } else {
                        g_running = false;
                        sim_reset(g_sim);
                        g_prev_cpu_valid   = false;
                        g_last_write_count = 0;
                        for (int i = 0; i < g_watch_count; i++) g_watches[i].valid = false;
                        update_watches();
                    }
                }

                /* Ctrl+L: Focus load filename field */
                if (!shift && ImGui::IsKeyPressed(ImGuiKey_L))
                    g_focus_filename = true;

                /* Ctrl+G: Go to address popup */
                if (!shift && ImGui::IsKeyPressed(ImGuiKey_G)) {
                    snprintf(g_goto_buf, sizeof(g_goto_buf), "%04X",
                             (unsigned)g_disasm_addr);
                    g_goto_open = true;
                }

                /* Ctrl+Shift+F: Find in disassembly / Ctrl+F: Find in active pane */
                if (ImGui::IsKeyPressed(ImGuiKey_F)) {
                    if (shift) {
                        show_disassembly    = true;
                        g_focus_disasm_addr = true;
                    } else {
                        if (show_source)
                            g_focus_src_search = true;
                        else
                            g_focus_disasm_addr = true;
                    }
                }

                /* Ctrl+Shift+E: Export VIC frame (Phase 6) */
                if (shift && ImGui::IsKeyPressed(ImGuiKey_E))
                    con_add(CON_COL_WARN, "VIC frame export not yet available (Phase 6).");
            }

            /* ` (backtick): Focus CLI console */
            if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent)) {
                show_console    = true;
                g_focus_console = true;
            }
        }

        /* ---- Per-frame simulation step (run mode) ---- */
        if (g_running) {
            cpu_t *cpu = sim_get_cpu(g_sim);
            if (cpu) { g_prev_cpu = *cpu; g_prev_cpu_valid = true; }
            int ev_code = sim_step(g_sim, g_speed);
            g_last_write_count = sim_get_last_writes(g_sim, g_last_writes, 256);
            update_watches();
            if (sim_profiler_is_enabled(g_sim)) g_heatmap_dirty = true;
            if (ev_code > 0)
                g_running = false;
            else if (ev_code < 0)
                g_running = false;
            /* Clean up step-over temp breakpoint when execution stops */
            if (!g_running && g_step_over_active) {
                sim_break_clear(g_sim, g_step_over_bp);
                g_step_over_active = false;
            }
        }

        /* ---- Theme rebuild (before NewFrame) ---- */
        if (g_theme_rebuild) {
            apply_theme();
            g_theme_rebuild = false;
        }

        /* ---- Font rebuild (before NewFrame) ---- */
        if (g_font_rebuild) {
            rebuild_font();
            g_font_rebuild = false;
        }

        /* ---- New ImGui frame ---- */
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        /* ---- Full-viewport dockspace host window ---- */
        {
            const ImGuiViewport *vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(vp->WorkPos);
            ImGui::SetNextWindowSize(vp->WorkSize);
            ImGui::SetNextWindowViewport(vp->ID);

            ImGuiWindowFlags host_flags =
                ImGuiWindowFlags_MenuBar               |
                ImGuiWindowFlags_NoDocking             |
                ImGuiWindowFlags_NoTitleBar            |
                ImGuiWindowFlags_NoCollapse            |
                ImGuiWindowFlags_NoResize              |
                ImGuiWindowFlags_NoMove                |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoNavFocus;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(8, 4));
            ImGui::Begin("##dockspace_host", nullptr, host_flags);
            ImGui::PopStyleVar(3);

            /* Menu bar */
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Load...", "Ctrl+L")) g_focus_filename = true;
                    ImGui::Separator();
                    if (ImGui::MenuItem("Save Binary/PRG...")) {
                        /* Pre-populate from the currently loaded program */
                        uint16_t laddr = 0, lsize = 0;
                        sim_get_load_info(g_sim, &laddr, &lsize);
                        snprintf(g_binsave_start, sizeof(g_binsave_start), "%04X", (unsigned)laddr);
                        snprintf(g_binsave_count, sizeof(g_binsave_count), "%04X", (unsigned)(lsize ? lsize : 0x100));
                        /* Derive save path from loaded filename (swap extension to .prg) */
                        const char *fn = sim_get_filename(g_sim);
                        if (fn && fn[0] && strcmp(fn, "(none)") != 0) {
                            strncpy(g_binsave_path, fn, sizeof(g_binsave_path) - 5);
                            char *dot = strrchr(g_binsave_path, '.');
                            if (dot) strcpy(dot, ".prg");
                            else strncat(g_binsave_path, ".prg", sizeof(g_binsave_path) - strlen(g_binsave_path) - 1);
                        } else {
                            strncpy(g_binsave_path, "output.prg", sizeof(g_binsave_path) - 1);
                        }
                        g_binsave_open = true;
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Quit", "Alt+F4")) running = false;
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Run")) {
                    if (ImGui::MenuItem("Step Into",  "F7")) {
                        g_running = false;
                        cpu_t *cpu = sim_get_cpu(g_sim);
                        if (cpu) { g_prev_cpu = *cpu; g_prev_cpu_valid = true; }
                        sim_step(g_sim, 1);
                        g_last_write_count = sim_get_last_writes(g_sim, g_last_writes, 256);
                        update_watches();
                    }
                    if (ImGui::MenuItem("Step Over", "F8"))
                        do_step_over();
                    if (ImGui::MenuItem("Run",   "F5"))  g_running = true;
                    if (ImGui::MenuItem("Pause", "F6"))  g_running = false;
                    if (ImGui::MenuItem("Reset", "Ctrl+R")) {
                        g_running = false;
                        sim_reset(g_sim);
                        g_prev_cpu_valid   = false;
                        g_last_write_count = 0;
                        for (int i = 0; i < g_watch_count; i++) g_watches[i].valid = false;
                        update_watches();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Toggle Breakpoint", "F9")) {
                        cpu_t *cpu = sim_get_cpu(g_sim);
                        if (cpu && sim_get_state(g_sim) != SIM_IDLE) {
                            if (sim_has_breakpoint(g_sim, cpu->pc))
                                sim_break_clear(g_sim, cpu->pc);
                            else
                                sim_break_set(g_sim, cpu->pc, NULL);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Step Back",        "Shift+F7"))
                        con_add(CON_COL_WARN, "Step-back not yet available (Phase 6).");
                    if (ImGui::MenuItem("Step Forward",     "Shift+F8"))
                        con_add(CON_COL_WARN, "Step-forward not yet available (Phase 6).");
                    if (ImGui::MenuItem("Reverse Continue", "Ctrl+Shift+R"))
                        con_add(CON_COL_WARN, "Reverse-continue not yet available (Phase 6).");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("View")) {
                    ImGui::MenuItem("Registers",   nullptr, &show_registers);
                    ImGui::MenuItem("Disassembly", nullptr, &show_disassembly);
                    ImGui::MenuItem("Memory",      nullptr, &show_memory);
                    ImGui::MenuItem("Console",     nullptr, &show_console);
                    ImGui::Separator();
                    if (ImGui::MenuItem("Go to Address...", "Ctrl+G")) {
                        snprintf(g_goto_buf, sizeof(g_goto_buf), "%04X",
                                 (unsigned)g_disasm_addr);
                        g_goto_open = true;
                    }
                    ImGui::Separator();
                    ImGui::MenuItem("Breakpoints", nullptr, &show_breakpoints);
                    ImGui::MenuItem("Trace Log",   nullptr, &show_trace);
                    ImGui::MenuItem("Stack",       nullptr, &show_stack);
                    ImGui::MenuItem("Watch",       nullptr, &show_watches);
                    ImGui::Separator();
                    ImGui::MenuItem("Instr Ref",   nullptr, &show_iref);
                    ImGui::MenuItem("Symbols",     nullptr, &show_symbols);
                    ImGui::MenuItem("Source",      nullptr, &show_source);
                    ImGui::MenuItem("Profiler",    nullptr, &show_profiler);
                    ImGui::Separator();
                    if (ImGui::BeginMenu("Font Size")) {
                        static const int sizes[] = { 10, 11, 12, 13, 14, 15, 16, 18, 20, 24 };
                        for (int sz : sizes) {
                            char label[16];
                            snprintf(label, sizeof(label), "%d px", sz);
                            if (ImGui::MenuItem(label, nullptr, g_base_font_size == sz)) {
                                if (g_base_font_size != sz) {
                                    g_base_font_size = sz;
                                    g_font_rebuild   = true;
                                }
                            }
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Theme")) {
                        if (ImGui::MenuItem("Auto (OS)",  nullptr, g_theme == 2)) {
                            if (g_theme != 2) { g_theme = 2; g_theme_rebuild = true; }
                        }
                        if (ImGui::MenuItem("Dark",       nullptr, g_theme == 0)) {
                            if (g_theme != 0) { g_theme = 0; g_theme_rebuild = true; }
                        }
                        if (ImGui::MenuItem("Light",      nullptr, g_theme == 1)) {
                            if (g_theme != 1) { g_theme = 1; g_theme_rebuild = true; }
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            /* Toolbar */
            draw_toolbar();

            /* Reserve height for status bar */
            float sb_h = ImGui::GetFrameHeight()
                       + ImGui::GetStyle().ItemSpacing.y * 2.0f
                       + ImGui::GetStyle().SeparatorTextBorderSize + 2.0f;
            float ds_h = ImGui::GetContentRegionAvail().y - sb_h;
            if (ds_h < 0.0f) ds_h = 0.0f;

            /* Apply default layout on first run */
            const ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
            if (first_run && !layout_applied) {
                layout_applied = true;
                setup_default_layout(dockspace_id,
                    ImVec2(ImGui::GetContentRegionAvail().x, ds_h));
            }

            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, ds_h), ImGuiDockNodeFlags_None);

            /* Status bar */
            draw_statusbar();

            ImGui::End();
        }

        /* Phase 1+2 panes */
        draw_pane_registers();
        draw_pane_disassembly();
        draw_pane_memory();
        draw_pane_console();

        /* Phase 3 panes */
        draw_pane_breakpoints();
        draw_pane_trace();
        draw_pane_stack();
        draw_pane_watches();

        /* Phase 4 panes */
        draw_pane_iref();
        draw_pane_symbols();
        draw_pane_source();
        draw_pane_profiler();

        /* Popups */
        draw_binload_popup();
        draw_binsave_popup();
        draw_goto_popup();

        /* Render */
        ImGui::Render();
        int fb_w = 0, fb_h = 0;
        SDL_GL_GetDrawableSize(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(g_col.gl_clear.x, g_col.gl_clear.y, g_col.gl_clear.z, g_col.gl_clear.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    if (g_heatmap_tex) { glDeleteTextures(1, &g_heatmap_tex); g_heatmap_tex = 0; }
    sim_destroy(g_sim);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
