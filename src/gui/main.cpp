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
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imgui_filedlg.h"

extern "C" {
#include "cpu.h"
#include "sim_api.h"
#include "vic2.h"
#include "patterns.h"
}

/* --------------------------------------------------------------------------
 * Core globals
 * -------------------------------------------------------------------------- */
static sim_session_t *g_sim     = NULL;
static bool  g_running          = false;  /* true = run mode each frame */
static int   g_speed            = 5000;   /* instructions per frame (unthrottled) */

/* ---- Speed throttle ---- */
static bool   g_throttle        = false;  /* limit run speed to g_throttle_scale × C64 */
static float  g_throttle_scale  = 1.0f;   /* speed multiplier: 1.0 = C64 (~1 MHz) */
static Uint64 g_thr_last        = 0;      /* SDL perf counter at last throttled frame */
static double g_thr_debt        = 0.0;   /* fractional cycle carry-over between frames */
static bool   g_was_running     = false;  /* detects start-of-run for timer reset */
static const double C64_HZ      = 985248.0; /* PAL C64 clock at scale 1.0 */

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
static bool show_console     = true;

/* Multi-instance memory views */
#define MAX_MEM_VIEWS 4
struct MemViewState {
    bool     open;
    bool     initialized;
    uint16_t base;
    char     addr_buf[8];
};
static MemViewState g_mem_views[MAX_MEM_VIEWS];  /* zero-init; [0].open set at startup */

/* Layout preset state (#19) */
static bool g_layout_save_open       = false;
static char g_layout_save_name[64]   = "";
static bool g_reset_layout           = false;

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
static bool show_profiler    = false;
static bool show_snap_diff   = false;
static bool show_patterns    = false;

/* ---- Phase 6 pane visibility ---- */
static bool show_vic_screen   = false;
static bool show_vic_sprites  = false;
static bool show_vic_regs     = false;

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
static char g_sym_filter[64]    = "";
static bool g_symload_open      = false;
static char g_symload_path[512] = "";

/* ---- Trace Run buffer ---- */
#define TRACE_RUN_CAP 2000
static sim_trace_entry_t g_trace_run_buf[TRACE_RUN_CAP];
static int   g_trace_run_count       = 0;
static char  g_trace_run_reason[16]  = "";
static char  g_trace_run_addr[8]     = "";   /* empty = use PC */
static int   g_trace_run_max         = 100;
static bool  g_trace_run_stop_brk    = true;

/* ---- File-open / file-save dialog ---- */
static FileDlgState g_filedlg;

enum FileDlgMode {
    FILEDLG_NONE = 0,
    FILEDLG_OPEN_FILE,   /* toolbar Browse → auto-detect ASM / BIN / PRG */
    FILEDLG_OPEN_SYM,    /* Symbols pane → load .sym file                */
    FILEDLG_SAVE_BIN,    /* Save Binary popup → pick output path          */
};
static FileDlgMode g_filedlg_mode = FILEDLG_NONE;

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

/* ---- VIC-II screen texture (Phase 6 / 27a) ---- */
static GLuint g_vic_tex    = 0;
static bool   g_vic_dirty  = true;
static bool   g_vic_freeze = false;

/* ---- VIC-II sprite textures (Phase 6 / 27b) — 8 sprites, 24×21 RGBA ---- */
static GLuint g_spr_tex[8] = {};
static bool   g_spr_dirty  = true;
static bool   g_spr_freeze = false;

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
 * Consolidated execution helpers — called from toolbar, keyboard, menu
 * -------------------------------------------------------------------------- */

/* Step one instruction into (records history, updates write-log / watches). */
static void do_step_into(void)
{
    g_running = false;
    sim_state_t st = sim_get_state(g_sim);
    if (st != SIM_READY && st != SIM_PAUSED) return;
    cpu_t *cpu = sim_get_cpu(g_sim);
    if (cpu) { g_prev_cpu = *cpu; g_prev_cpu_valid = true; }
    sim_step(g_sim, 1);
    g_last_write_count = sim_get_last_writes(g_sim, g_last_writes, 256);
    g_vic_dirty = true;
    g_spr_dirty = true;
    update_watches();
}

/* Reset CPU to start address; clear write-log and watches. */
static void do_reset(void)
{
    g_running = false;
    sim_reset(g_sim);
    g_prev_cpu_valid   = false;
    g_last_write_count = 0;
    g_vic_dirty        = true;
    g_spr_dirty        = true;
    for (int i = 0; i < g_watch_count; i++) g_watches[i].valid = false;
    update_watches();
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

/* Step back one instruction in execution history. */
static void do_step_back(void)
{
    g_running = false;
    if (sim_get_state(g_sim) == SIM_IDLE) return;
    cpu_t *cpu = sim_get_cpu(g_sim);
    if (cpu) { g_prev_cpu = *cpu; g_prev_cpu_valid = true; }
    if (!sim_history_step_back(g_sim)) {
        con_add(CON_COL_WARN, "No history to step back into.");
        return;
    }
    g_last_write_count = 0;   /* writes were undone, not applied */
    update_watches();
}

/* Step forward one instruction in history (re-execute the undone instruction). */
static void do_step_fwd(void)
{
    g_running = false;
    if (sim_get_state(g_sim) == SIM_IDLE) return;
    cpu_t *cpu = sim_get_cpu(g_sim);
    if (cpu) { g_prev_cpu = *cpu; g_prev_cpu_valid = true; }
    if (!sim_history_step_fwd(g_sim)) {
        con_add(CON_COL_WARN, "Already at the present.");
        return;
    }
    g_last_write_count = sim_get_last_writes(g_sim, g_last_writes, 256);
    update_watches();
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
        con_add(CON_COL_NORMAL, "  stepback / sb                step back one instruction in history");
        con_add(CON_COL_NORMAL, "  stepfwd  / sf                step forward one instruction in history");
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

    if (strcmp(verb, "stepback") == 0 || strcmp(verb, "sb") == 0) {
        do_step_back();
        cpu_t *cpu = sim_get_cpu(g_sim);
        if (cpu) con_add(CON_COL_NORMAL, "BACK  PC=%04X", cpu->pc);
        return;
    }

    if (strcmp(verb, "stepfwd") == 0 || strcmp(verb, "sf") == 0) {
        do_step_fwd();
        cpu_t *cpu = sim_get_cpu(g_sim);
        if (cpu) con_add(CON_COL_NORMAL, "FWD   PC=%04X", cpu->pc);
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
        do_reset();
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
 * VIC-II Screen Renderer (Phase 6 — item 27a)
 *
 * Renders a 384×272 composite frame (320×200 active + border on all sides)
 * from current simulator memory state.  Supported video modes (decoded from
 * $D011 / $D016):
 *
 *   ECM=0 BMM=0 MCM=0  Standard Character
 *   ECM=0 BMM=0 MCM=1  Multicolour Character
 *   ECM=1 BMM=0 MCM=0  Extended Colour
 *   ECM=0 BMM=1 MCM=0  Standard Bitmap
 *   ECM=0 BMM=1 MCM=1  Multicolour Bitmap
 *
 * VIC bank: CIA2 Port A ($DD00) bits 1:0, inverted (standard C64 mapping).
 * Colour RAM: always $D800–$DBFF regardless of bank.
 * -------------------------------------------------------------------------- */

/* vic2.h provides VIC2_FRAME_W, VIC2_FRAME_H, VIC2_ACTIVE_X, VIC2_ACTIVE_Y,
 * vic2_palette[16][3], and vic2_render_rgb().  No local duplicates needed. */

static void update_vic_tex(void)
{
    if (!g_sim || !g_vic_tex) return;
    if (g_vic_freeze) { g_vic_dirty = false; return; }

    static uint8_t pixels[VIC2_FRAME_W * VIC2_FRAME_H * 3];
    vic2_render_rgb(sim_get_memory(g_sim), pixels);

    glBindTexture(GL_TEXTURE_2D, g_vic_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    VIC2_FRAME_W, VIC2_FRAME_H, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_vic_dirty = false;
}

/* --------------------------------------------------------------------------
 * Pane: VIC-II Screen (Phase 6 — item 27a)
 *
 * Displays the rendered VIC-II frame as a scaled image.  The render is
 * updated whenever memory changes (after step, run, or reset).  A "Freeze"
 * toggle locks the last rendered frame for comparison while execution
 * continues.  Register state is summarised below the image.
 * -------------------------------------------------------------------------- */
static void draw_pane_vic_screen(void)
{
    if (!show_vic_screen) return;
    ImGui::Begin("VIC-II Screen", &show_vic_screen,
                 ImGuiWindowFlags_HorizontalScrollbar);

    sim_state_t state = sim_get_state(g_sim);
    if (state == SIM_IDLE) {
        ImGui::TextDisabled("(no program loaded)");
        ImGui::End();
        return;
    }

    if (g_vic_dirty)
        update_vic_tex();

    /* ---- Scale controls ---- */
    static float vic_scale = 2.0f;
    ImGui::Text("Scale:");
    ImGui::SameLine();
    if (ImGui::RadioButton("1x", vic_scale == 1.0f)) vic_scale = 1.0f;
    ImGui::SameLine();
    if (ImGui::RadioButton("2x", vic_scale == 2.0f)) vic_scale = 2.0f;
    ImGui::SameLine();
    if (ImGui::RadioButton("3x", vic_scale == 3.0f)) vic_scale = 3.0f;
    ImGui::SameLine();
    ImGui::Checkbox("Freeze", &g_vic_freeze);

    /* ---- Rendered frame ---- */
    ImVec2 img_sz((float)VIC2_FRAME_W * vic_scale, (float)VIC2_FRAME_H * vic_scale);
    ImGui::Image((ImTextureID)(uintptr_t)g_vic_tex, img_sz);

    /* ---- Register summary ---- */
    ImGui::Separator();
    uint8_t ctrl1    = sim_mem_read_byte(g_sim, 0xD011);
    uint8_t ctrl2    = sim_mem_read_byte(g_sim, 0xD016);
    uint8_t memsetup = sim_mem_read_byte(g_sim, 0xD018);
    uint8_t cia2a    = sim_mem_read_byte(g_sim, 0xDD00);
    int     bank     = (~cia2a) & 3;

    bool ecm = (ctrl1 >> 6) & 1;
    bool bmm = (ctrl1 >> 5) & 1;
    bool den = (ctrl1 >> 4) & 1;
    bool mcm = (ctrl2 >> 4) & 1;

    const char *mode = "Unknown";
    if      (!den)               mode = "Display Off";
    else if (!bmm&&!ecm&&!mcm)  mode = "Standard Char";
    else if (!bmm&&!ecm&& mcm)  mode = "Multicolour Char";
    else if (!bmm&& ecm&&!mcm)  mode = "Extended Colour";
    else if ( bmm&&!ecm&&!mcm)  mode = "Standard Bitmap";
    else if ( bmm&&!ecm&& mcm)  mode = "Multicolour Bitmap";

    uint32_t vic_bank_u  = (uint32_t)bank * 0x4000u;
    uint32_t screen_addr = vic_bank_u + (uint32_t)((memsetup>>4)&0xF)*1024u;
    uint32_t cg_addr     = vic_bank_u + (uint32_t)((memsetup>>1)&0x7)*2048u;
    uint32_t bm_addr     = vic_bank_u + (uint32_t)(((memsetup>>3)&1)*0x2000u);

    /* Mode label: green=char, yellow=bitmap, grey=off */
    ImVec4 mode_col = !den ? ImVec4(0.5f,0.5f,0.5f,1.0f)
                    :  bmm ? ImVec4(1.0f,0.9f,0.2f,1.0f)
                           : ImVec4(0.4f,1.0f,0.4f,1.0f);
    ImGui::TextColored(mode_col, "Mode: %-20s", mode);
    ImGui::SameLine();
    ImGui::TextDisabled(" D011:%02X  D016:%02X  D018:%02X", ctrl1, ctrl2, memsetup);
    ImGui::Text("Bank: %d ($%04X)  Screen:$%04X  %s:$%04X",
                bank, (unsigned)vic_bank_u, (unsigned)screen_addr,
                bmm ? "Bitmap" : "CharGen",
                (unsigned)(bmm ? bm_addr : cg_addr));
    ImGui::Text("Border:%X  BG0:%X  CIA2PA:%02X",
                sim_mem_read_byte(g_sim, 0xD020) & 0xF,
                sim_mem_read_byte(g_sim, 0xD021) & 0xF,
                cia2a);

    ImGui::End();
}

/* --------------------------------------------------------------------------
 * VIC-II Sprite pane (Phase 6 / 27b)
 *
 * update_spr_textures(): reads all 8 sprites from VIC-II state and uploads
 *   each to a 24×21 RGBA GL texture (transparent pixels have alpha=0).
 * draw_pane_vic_sprites(): ImGui pane — 4-per-row grid, scale, freeze, info.
 * -------------------------------------------------------------------------- */
static void update_spr_textures(void)
{
    if (!g_sim || !g_spr_tex[0]) return;
    if (g_spr_freeze) { g_spr_dirty = false; return; }

    static uint8_t pixels[24 * 21 * 4];  /* RGBA */

    uint8_t d01c  = sim_mem_read_byte(g_sim, 0xD01C);  /* multicolour flags */
    uint8_t d018  = sim_mem_read_byte(g_sim, 0xD018);  /* memory setup      */
    uint8_t cia2a = sim_mem_read_byte(g_sim, 0xDD00);
    uint8_t d025  = sim_mem_read_byte(g_sim, 0xD025) & 0xF;  /* shared MC0 */
    uint8_t d026  = sim_mem_read_byte(g_sim, 0xD026) & 0xF;  /* shared MC1 */

    uint32_t vic_bank    = (uint32_t)((~cia2a) & 3) * 0x4000u;
    uint32_t screen_base = vic_bank + (uint32_t)((d018 >> 4) & 0xF) * 1024u;

    for (int sn = 0; sn < 8; sn++) {
        uint8_t  ptr   = sim_mem_read_byte(g_sim, (uint16_t)((screen_base + 0x3F8 + sn) & 0xFFFF));
        uint32_t saddr = vic_bank + (uint32_t)ptr * 64u;
        uint8_t  color = sim_mem_read_byte(g_sim, (uint16_t)(0xD027 + sn)) & 0xF;
        bool     is_mcm = ((d01c >> sn) & 1) != 0;

        memset(pixels, 0, sizeof(pixels));  /* all transparent */

        for (int row = 0; row < 21; row++) {
            uint8_t b[3];
            for (int bi = 0; bi < 3; bi++)
                b[bi] = sim_mem_read_byte(g_sim,
                            (uint16_t)((saddr + (uint32_t)(row*3 + bi)) & 0xFFFF));

            if (!is_mcm) {
                /* Standard sprite: 24px, 1bpp */
                for (int col = 0; col < 24; col++) {
                    if ((b[col/8] >> (7 - col%8)) & 1) {
                        int pi = (row * 24 + col) * 4;
                        pixels[pi+0] = vic2_palette[color][0];
                        pixels[pi+1] = vic2_palette[color][1];
                        pixels[pi+2] = vic2_palette[color][2];
                        pixels[pi+3] = 255;
                    }
                }
            } else {
                /* Multicolour sprite: 12 pixel pairs, 2bpp
                   00=transparent, 01=d025, 10=sprite color, 11=d026 */
                const uint8_t *mc[4] = {
                    NULL,                /* 00: transparent   */
                    vic2_palette[d025],  /* 01: shared color 0 */
                    vic2_palette[color], /* 10: sprite color  */
                    vic2_palette[d026]   /* 11: shared color 1 */
                };
                for (int pair = 0; pair < 12; pair++) {
                    int sel = (b[pair/4] >> (6 - (pair%4)*2)) & 0x3;
                    if (sel == 0) continue;
                    int pi0 = (row * 24 + pair*2) * 4;
                    int pi1 = pi0 + 4;
                    pixels[pi0+0] = pixels[pi1+0] = mc[sel][0];
                    pixels[pi0+1] = pixels[pi1+1] = mc[sel][1];
                    pixels[pi0+2] = pixels[pi1+2] = mc[sel][2];
                    pixels[pi0+3] = pixels[pi1+3] = 255;
                }
            }
        }

        glBindTexture(GL_TEXTURE_2D, g_spr_tex[sn]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        24, 21, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    g_spr_dirty = false;
}

static void draw_pane_vic_sprites(void)
{
    if (!show_vic_sprites) return;
    ImGui::Begin("VIC-II Sprites", &show_vic_sprites,
                 ImGuiWindowFlags_HorizontalScrollbar);

    sim_state_t state = sim_get_state(g_sim);
    if (state == SIM_IDLE) {
        ImGui::TextDisabled("(no program loaded)");
        ImGui::End();
        return;
    }

    if (g_spr_dirty)
        update_spr_textures();

    /* Scale controls */
    static float spr_scale = 3.0f;
    ImGui::Text("Scale:");
    ImGui::SameLine();
    if (ImGui::RadioButton("2x##spr", spr_scale == 2.0f)) spr_scale = 2.0f;
    ImGui::SameLine();
    if (ImGui::RadioButton("3x##spr", spr_scale == 3.0f)) spr_scale = 3.0f;
    ImGui::SameLine();
    if (ImGui::RadioButton("4x##spr", spr_scale == 4.0f)) spr_scale = 4.0f;
    ImGui::SameLine();
    ImGui::Checkbox("Freeze##spr", &g_spr_freeze);

    ImGui::Separator();

    /* Read sprite state registers */
    uint8_t d015  = sim_mem_read_byte(g_sim, 0xD015);
    uint8_t d01c  = sim_mem_read_byte(g_sim, 0xD01C);
    uint8_t d01d  = sim_mem_read_byte(g_sim, 0xD01D);  /* X-expand   */
    uint8_t d017  = sim_mem_read_byte(g_sim, 0xD017);  /* Y-expand   */
    uint8_t d01b  = sim_mem_read_byte(g_sim, 0xD01B);  /* behind BG  */
    uint8_t d010  = sim_mem_read_byte(g_sim, 0xD010);  /* X MSBs     */
    uint8_t d018  = sim_mem_read_byte(g_sim, 0xD018);
    uint8_t cia2a = sim_mem_read_byte(g_sim, 0xDD00);
    uint32_t vic_bank    = (uint32_t)((~cia2a) & 3) * 0x4000u;
    uint32_t screen_base = vic_bank + (uint32_t)((d018 >> 4) & 0xF) * 1024u;

    ImVec2 img_sz(24.0f * spr_scale, 21.0f * spr_scale);
    float  cell_w = img_sz.x + 14.0f;
    float  cell_h = img_sz.y + 78.0f;

    for (int sn = 0; sn < 8; sn++) {
        if (sn % 4 != 0) ImGui::SameLine(0.0f, 6.0f);

        bool     enabled = ((d015 >> sn) & 1) != 0;
        uint16_t sx      = (uint16_t)(sim_mem_read_byte(g_sim, (uint16_t)(0xD000 + sn*2))
                            | (((d010 >> sn) & 1) << 8));
        uint8_t  sy      = sim_mem_read_byte(g_sim, (uint16_t)(0xD001 + sn*2));
        uint8_t  color   = sim_mem_read_byte(g_sim, (uint16_t)(0xD027 + sn)) & 0xF;
        bool     is_mcm  = ((d01c >> sn) & 1) != 0;
        bool     x_exp   = ((d01d >> sn) & 1) != 0;
        bool     y_exp   = ((d017 >> sn) & 1) != 0;
        bool     behind  = ((d01b >> sn) & 1) != 0;
        uint8_t  ptr     = sim_mem_read_byte(g_sim,
                               (uint16_t)((screen_base + 0x3F8 + sn) & 0xFFFF));
        uint32_t saddr   = (vic_bank + (uint32_t)ptr * 64u) & 0xFFFF;

        ImGui::PushID(sn);

        ImVec4 border_col = enabled
            ? ImVec4(0.25f, 0.75f, 0.25f, 1.0f)
            : ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, border_col);

        if (ImGui::BeginChild("##sc", ImVec2(cell_w, cell_h), true)) {
            /* Header */
            if (enabled)
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Spr %d", sn);
            else
                ImGui::TextDisabled("Spr %d (off)", sn);

            /* Sprite bitmap with transparent background */
            ImGui::Image((ImTextureID)(uintptr_t)g_spr_tex[sn], img_sz);

            /* Info lines */
            ImGui::Text("X=%-3d Y=%d", sx, sy);
            char flags[16] = "";
            if (is_mcm) strcat(flags, "MC ");
            if (x_exp)  strcat(flags, "XE ");
            if (y_exp)  strcat(flags, "YE ");
            if (behind) strcat(flags, "BG");
            ImGui::Text("Col:%X  %s", color, flags);
            ImGui::TextDisabled("$%04X[%02X]", (unsigned)saddr, ptr);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
        ImGui::PopID();
    }

    /* Footer: shared MC colours and enable mask */
    ImGui::Separator();
    uint8_t d025 = sim_mem_read_byte(g_sim, 0xD025) & 0xF;
    uint8_t d026 = sim_mem_read_byte(g_sim, 0xD026) & 0xF;
    ImGui::Text("D025 MC0:%X  D026 MC1:%X  D015 En:%02X", d025, d026, d015);

    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Pane: VIC-II Registers (Phase 6 / 27e)
 *
 * Shows all key VIC-II control registers with decoded fields, computed video
 * addresses (screen RAM, char gen, bitmap base), colour swatches for border
 * and background registers, and the current raster position.
 * -------------------------------------------------------------------------- */
static void draw_pane_vic_regs(void)
{
    if (!show_vic_regs) return;
    ImGui::Begin("VIC-II Registers", &show_vic_regs);

    sim_state_t state = sim_get_state(g_sim);
    if (state == SIM_IDLE) {
        ImGui::TextDisabled("(no program loaded)");
        ImGui::End();
        return;
    }

    /* Read all relevant VIC-II registers */
    uint8_t ctrl1    = sim_mem_read_byte(g_sim, 0xD011);
    uint8_t ctrl2    = sim_mem_read_byte(g_sim, 0xD016);
    uint8_t memsetup = sim_mem_read_byte(g_sim, 0xD018);
    uint8_t raster   = sim_mem_read_byte(g_sim, 0xD012);
    uint8_t border   = sim_mem_read_byte(g_sim, 0xD020) & 0xF;
    uint8_t bg0      = sim_mem_read_byte(g_sim, 0xD021) & 0xF;
    uint8_t bg1      = sim_mem_read_byte(g_sim, 0xD022) & 0xF;
    uint8_t bg2      = sim_mem_read_byte(g_sim, 0xD023) & 0xF;
    uint8_t bg3      = sim_mem_read_byte(g_sim, 0xD024) & 0xF;
    uint8_t d019     = sim_mem_read_byte(g_sim, 0xD019);
    uint8_t d01a     = sim_mem_read_byte(g_sim, 0xD01A);
    uint8_t cia2a    = sim_mem_read_byte(g_sim, 0xDD00);

    int ecm     = (ctrl1 >> 6) & 1;
    int bmm     = (ctrl1 >> 5) & 1;
    int den     = (ctrl1 >> 4) & 1;
    int rsel    = (ctrl1 >> 3) & 1;
    int rst8    = (ctrl1 >> 7) & 1;
    int yscroll = ctrl1 & 7;
    int mcm     = (ctrl2 >> 4) & 1;
    int csel    = (ctrl2 >> 3) & 1;
    int xscroll = ctrl2 & 7;
    int bank    = (~cia2a) & 3;

    uint32_t vic_bank    = (uint32_t)bank * 0x4000u;
    uint32_t screen_addr = vic_bank + (uint32_t)((memsetup >> 4) & 0xF) * 1024u;
    uint32_t cg_addr     = vic_bank + (uint32_t)((memsetup >> 1) & 0x7) * 2048u;
    uint32_t bm_addr     = vic_bank + (uint32_t)(((memsetup >> 3) & 1) * 0x2000u);
    uint16_t raster_line = (uint16_t)(raster | (rst8 << 8));

    const char *mode;
    if      (!den)               mode = "Display Off";
    else if (!bmm&&!ecm&&!mcm)  mode = "Standard Char";
    else if (!bmm&&!ecm&& mcm)  mode = "Multicolour Char";
    else if (!bmm&& ecm&&!mcm)  mode = "Extended Colour";
    else if ( bmm&&!ecm&&!mcm)  mode = "Standard Bitmap";
    else if ( bmm&&!ecm&& mcm)  mode = "Multicolour Bitmap";
    else                         mode = "Invalid";

    /* Colour swatch helper */
    auto colour_swatch = [](const char *id, int ci) {
        ci &= 0xF;
        ImVec4 col(vic2_palette[ci][0] / 255.0f,
                   vic2_palette[ci][1] / 255.0f,
                   vic2_palette[ci][2] / 255.0f, 1.0f);
        ImGui::ColorButton(id, col,
                           ImGuiColorEditFlags_NoPicker |
                           ImGuiColorEditFlags_NoTooltip,
                           ImVec2(14, 14));
    };

    /* ---- Mode summary ---- */
    ImGui::TextColored(den ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
                           : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                       "Mode: %s", mode);
    ImGui::SameLine();
    ImGui::TextDisabled("  Raster: %d ($%03X)", raster_line, raster_line);

    ImGui::Separator();

    /* ---- Register table ---- */
    if (ImGui::BeginTable("vicreg", 3,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
            ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Reg",     ImGuiTableColumnFlags_WidthFixed,   55);
        ImGui::TableSetupColumn("Hex",     ImGuiTableColumnFlags_WidthFixed,   38);
        ImGui::TableSetupColumn("Decoded", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        char buf[96];

        snprintf(buf, sizeof(buf),
                 "ECM=%d BMM=%d DEN=%d RSEL=%d RST8=%d yscrl=%d",
                 ecm, bmm, den, rsel, rst8, yscroll);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("D011");
        ImGui::TableSetColumnIndex(1); ImGui::Text("$%02X", ctrl1);
        ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(buf);

        snprintf(buf, sizeof(buf), "MCM=%d CSEL=%d xscrl=%d", mcm, csel, xscroll);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("D016");
        ImGui::TableSetColumnIndex(1); ImGui::Text("$%02X", ctrl2);
        ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(buf);

        snprintf(buf, sizeof(buf), "scr=bits[7:4]  chr/bm=bits[3:1]");
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("D018");
        ImGui::TableSetColumnIndex(1); ImGui::Text("$%02X", memsetup);
        ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(buf);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("D012");
        ImGui::TableSetColumnIndex(1); ImGui::Text("$%02X", raster);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("Raster line low byte  (full: %d)", raster_line);

        snprintf(buf, sizeof(buf), "IRQ=%d  RST=%d MBC=%d MMC=%d LP=%d",
                 (d019 >> 7) & 1, d019 & 1,
                 (d019 >> 1) & 1, (d019 >> 2) & 1, (d019 >> 3) & 1);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("D019");
        ImGui::TableSetColumnIndex(1); ImGui::Text("$%02X", d019);
        ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(buf);

        snprintf(buf, sizeof(buf), "ERST=%d EMBC=%d EMMC=%d ELP=%d",
                 d01a & 1, (d01a >> 1) & 1, (d01a >> 2) & 1, (d01a >> 3) & 1);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("D01A");
        ImGui::TableSetColumnIndex(1); ImGui::Text("$%02X", d01a);
        ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(buf);

        ImGui::EndTable();
    }

    ImGui::Separator();

    /* ---- Video addresses ---- */
    if (ImGui::BeginTable("vicaddr", 2,
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 93);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

        char buf[40];

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("VIC Bank");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d  ($%04X-$%04X)", bank,
                    (unsigned)vic_bank, (unsigned)(vic_bank + 0x3FFF));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Screen RAM");
        ImGui::TableSetColumnIndex(1);
        snprintf(buf, sizeof(buf), "$%04X", (unsigned)screen_addr);
        ImGui::TextUnformatted(buf);

        ImGui::TableNextRow();
        if (bmm) {
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Bitmap Base");
            ImGui::TableSetColumnIndex(1);
            snprintf(buf, sizeof(buf), "$%04X", (unsigned)bm_addr);
        } else {
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Char Gen");
            ImGui::TableSetColumnIndex(1);
            snprintf(buf, sizeof(buf), "$%04X", (unsigned)cg_addr);
        }
        ImGui::TextUnformatted(buf);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Colour RAM");
        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("$D800");

        ImGui::EndTable();
    }

    ImGui::Separator();

    /* ---- Colour registers with swatches ---- */
    struct ColReg { const char *label; int ci; };
    ColReg col_regs[] = {
        { "Border", border }, { "BG0", bg0 },
        { "BG1", bg1 },       { "BG2", bg2 }, { "BG3", bg3 },
    };
    for (auto &c : col_regs) {
        colour_swatch(c.label, c.ci);
        ImGui::SameLine();
        ImGui::Text("%-7s  %X  %s", c.label, c.ci, vic2_color_names[c.ci]);
    }

    ImGui::End();
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
static void draw_trace_table(sim_trace_entry_t *entries, int count)
{
    const ImGuiTableFlags tf =
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_BordersInnerV  |
        ImGuiTableFlags_ScrollY        |
        ImGuiTableFlags_RowBg;

    float avail_h = ImGui::GetContentRegionAvail().y;
    if (ImGui::BeginTable("##trace_tbl", 5, tf, ImVec2(0, avail_h))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#",       ImGuiTableColumnFlags_WidthFixed,   38.0f);
        ImGui::TableSetupColumn("PC",      ImGuiTableColumnFlags_WidthFixed,   44.0f);
        ImGui::TableSetupColumn("Instr",   ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Cyc",     ImGuiTableColumnFlags_WidthFixed,   26.0f);
        ImGui::TableSetupColumn("A  X  Y", ImGuiTableColumnFlags_WidthFixed,   66.0f);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(count);
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                sim_trace_entry_t &e = entries[i];
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%d", i);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%04X", (unsigned)e.pc);

                ImGui::TableSetColumnIndex(2);
                /* disasm string: "$XXXX: bytes  MNEM OPER" — skip 7-char "$XXXX: " prefix */
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
}

static void draw_pane_trace(void)
{
    if (!show_trace) return;
    ImGui::Begin("Trace", &show_trace);

    if (ImGui::BeginTabBar("##trace_tabs")) {

        /* ── Tab 1: Live Log (ring buffer) ─────────────────────── */
        if (ImGui::BeginTabItem("Live Log")) {
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
            } else {
                /* Copy ring buffer into a flat array for the table */
                static sim_trace_entry_t live_flat[SIM_TRACE_DEPTH];
                for (int i = 0; i < cnt; i++)
                    sim_trace_get(g_sim, i, &live_flat[i]);
                draw_trace_table(live_flat, cnt);
            }
            ImGui::EndTabItem();
        }

        /* ── Tab 2: Trace Run ───────────────────────────────────── */
        if (ImGui::BeginTabItem("Run Trace")) {
            /* Controls row */
            ImGui::SetNextItemWidth(80.0f);
            ImGui::InputText("From $", g_trace_run_addr, sizeof(g_trace_run_addr),
                             ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            ImGui::InputInt("Max", &g_trace_run_max);
            if (g_trace_run_max < 1)   g_trace_run_max = 1;
            if (g_trace_run_max > TRACE_RUN_CAP) g_trace_run_max = TRACE_RUN_CAP;
            ImGui::SameLine();
            ImGui::Checkbox("Stop@BRK", &g_trace_run_stop_brk);
            ImGui::SameLine();

            bool can_run = (sim_get_state(g_sim) == SIM_READY ||
                            sim_get_state(g_sim) == SIM_PAUSED);
            if (!can_run) ImGui::BeginDisabled();
            if (ImGui::Button("Run Trace")) {
                /* Parse start address (empty = current PC = -1) */
                int start = -1;
                if (g_trace_run_addr[0]) {
                    unsigned int v = 0;
                    sscanf(g_trace_run_addr, "%x", &v);
                    start = (int)(v & 0xFFFF);
                }
                g_trace_run_count = sim_trace_run(
                    g_sim, start, g_trace_run_max,
                    g_trace_run_stop_brk ? 1 : 0,
                    g_trace_run_buf, TRACE_RUN_CAP,
                    g_trace_run_reason, (int)sizeof(g_trace_run_reason));
            }
            if (!can_run) ImGui::EndDisabled();

            if (g_trace_run_count > 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("Stop: %s  (%d instr)", g_trace_run_reason, g_trace_run_count);
                ImGui::Separator();
                draw_trace_table(g_trace_run_buf, g_trace_run_count);
            } else {
                ImGui::Separator();
                ImGui::TextDisabled("Press 'Run Trace' to execute and capture a trace.");
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
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
 * Resolve a path relative to the running executable's directory.
 * Used for preset symbol files bundled alongside the binary.
 * -------------------------------------------------------------------------- */
static void sym_resolve_path(const char *rel, char *out, size_t outsz)
{
    char exe[512]; exe[0] = '\0';
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) {
        exe[n] = '\0';
        char *slash = strrchr(exe, '/');
        if (slash) {
            *slash = '\0';
            snprintf(out, outsz, "%s/%s", exe, rel);
            return;
        }
    }
    snprintf(out, outsz, "%s", rel);
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
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputText("Filter##sf", g_sym_filter, sizeof(g_sym_filter));
    ImGui::SameLine();
    ImGui::TextDisabled("%d symbols", count);

    /* Load / preset / clear toolbar */
    if (ImGui::SmallButton("Load File...##slb")) {
        static const FileDlgFilter sym_filters[] = {
            { "Symbol Files (*.sym)", ".sym"   },
            { "All Files (*.*)",       nullptr },
        };
        filedlg_open(&g_filedlg, "Load Symbol File", false, sym_filters, 2);
        g_filedlg_mode = FILEDLG_OPEN_SYM;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    static const struct { const char *label; const char *file; } s_presets[] = {
        { "C64",    "symbols/c64.sym"    },
        { "C128",   "symbols/c128.sym"   },
        { "MEGA65", "symbols/mega65.sym" },
        { "X16",    "symbols/x16.sym"    },
    };
    for (int pi = 0; pi < 4; pi++) {
        ImGui::SameLine();
        char pid[32]; snprintf(pid, sizeof(pid), "%s##sp%d", s_presets[pi].label, pi);
        if (ImGui::SmallButton(pid)) {
            char path[512];
            sym_resolve_path(s_presets[pi].file, path, sizeof(path));
            int added = sim_sym_load_file(g_sim, path);
            if (added > 0)
                con_add(ImVec4(0.4f,1.0f,0.4f,1.0f),
                        "Loaded %d symbol(s) [%s]", added, s_presets[pi].label);
            else
                con_add(ImVec4(1.0f,0.4f,0.4f,1.0f),
                        "Could not load preset: %s", s_presets[pi].file);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Load %s preset symbols", s_presets[pi].label);
    }
    if (count > 0) {
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear All##sca")) {
            for (int i = count - 1; i >= 0; i--)
                sim_sym_remove_idx(g_sim, i);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Remove all %d symbols", count);
    }

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
 * Pane: Test Runner (Phase 4)
 *
 * Enter test vectors (input registers + expected outputs) for a subroutine
 * and run them all in one click.  Uses sim_validate_routine().
 * -------------------------------------------------------------------------- */

#define TEST_RUNNER_MAX_ROWS 32

struct TestRow {
    char  label[32];
    /* inputs  (-1 in the int means "not set") */
    char  in_a[6],  in_x[6],  in_y[6],  in_z[6],  in_p[6];
    /* expects (-1 in the int means "not checked") */
    char  ex_a[6],  ex_x[6],  ex_y[6],  ex_z[6],  ex_p[6];
    /* result */
    int   result;       /* -1 = not run, 0 = fail, 1 = pass */
    char  fail_msg[128];
    int   act_a, act_x, act_y, act_z, act_p;
};

static bool     show_test_runner = false;
static char     g_tr_routine[8]  = "0300";
static char     g_tr_scratch[8]  = "FFF8";
static TestRow  g_tr_rows[TEST_RUNNER_MAX_ROWS];
static int      g_tr_row_count   = 0;
static char     g_tr_summary[64] = "";

/* C++ wrapper for parse_mon_value since it takes const char ** */
extern "C" int parse_mon_value(const char **, unsigned long *);
static int parse_mon_value_cpp(const char *s, unsigned long *out) {
    const char *p = s;
    return parse_mon_value(&p, out);
}

/* Parse a hex/decimal input string to int; return -1 if blank */
static int tr_parse_field(const char *s) {
    if (!s || !s[0]) return -1;
    unsigned long v;
    if (!parse_mon_value_cpp(s, &v)) return -1;
    return (int)(v & 0xFF);
}

static void draw_pane_test_runner(void)
{
    if (!show_test_runner) return;
    ImGui::Begin("Test Runner", &show_test_runner);

    sim_state_t state = sim_get_state(g_sim);
    bool can_run = (state == SIM_READY || state == SIM_PAUSED);

    /* --- Top controls --- */
    ImGui::SetNextItemWidth(70.0f);
    ImGui::InputText("Routine $", g_tr_routine, sizeof(g_tr_routine),
                     ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    ImGui::InputText("Scratch $", g_tr_scratch, sizeof(g_tr_scratch),
                     ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    if (ImGui::SmallButton("Add Row") && g_tr_row_count < TEST_RUNNER_MAX_ROWS) {
        TestRow &r = g_tr_rows[g_tr_row_count++];
        memset(&r, 0, sizeof(r));
        r.result = -1;
        snprintf(r.label, sizeof(r.label), "test %d", g_tr_row_count);
    }
    ImGui::SameLine();
    if (!can_run || g_tr_row_count == 0) ImGui::BeginDisabled();
    if (ImGui::Button("Run All")) {
        uint16_t raddr = (uint16_t)strtol(g_tr_routine, NULL, 16);
        uint16_t saddr = (uint16_t)strtol(g_tr_scratch, NULL, 16);

        /* Build test vector arrays */
        sim_test_in_t     *ins = (sim_test_in_t     *)calloc((size_t)g_tr_row_count, sizeof(sim_test_in_t));
        sim_test_expect_t *exs = (sim_test_expect_t *)calloc((size_t)g_tr_row_count, sizeof(sim_test_expect_t));
        sim_test_result_t *res = (sim_test_result_t *)calloc((size_t)g_tr_row_count, sizeof(sim_test_result_t));

        if (ins && exs && res) {
            for (int i = 0; i < g_tr_row_count; i++) {
                TestRow &row = g_tr_rows[i];
                /* Initialise all to "don't set / don't check" */
                ins[i].a = ins[i].x = ins[i].y = ins[i].z = ins[i].b = ins[i].s = ins[i].p = -1;
                exs[i].a = exs[i].x = exs[i].y = exs[i].z = exs[i].b = exs[i].s = exs[i].p = -1;

                auto pf = [](const char *s) -> int {
                    return tr_parse_field(s);
                };
                ins[i].a = pf(row.in_a); ins[i].x = pf(row.in_x);
                ins[i].y = pf(row.in_y); ins[i].z = pf(row.in_z);
                ins[i].p = pf(row.in_p);
                exs[i].a = pf(row.ex_a); exs[i].x = pf(row.ex_x);
                exs[i].y = pf(row.ex_y); exs[i].z = pf(row.ex_z);
                exs[i].p = pf(row.ex_p);
                strncpy(ins[i].label, row.label, 47);
            }

            int passed = sim_validate_routine(g_sim, raddr, saddr, 0,
                                              ins, exs, res, g_tr_row_count);
            for (int i = 0; i < g_tr_row_count; i++) {
                g_tr_rows[i].result  = res[i].passed;
                g_tr_rows[i].act_a   = res[i].a;
                g_tr_rows[i].act_x   = res[i].x;
                g_tr_rows[i].act_y   = res[i].y;
                g_tr_rows[i].act_z   = res[i].z;
                g_tr_rows[i].act_p   = res[i].p;
                strncpy(g_tr_rows[i].fail_msg, res[i].fail_msg, 127);
            }
            snprintf(g_tr_summary, sizeof(g_tr_summary),
                     "%d / %d passed", passed, g_tr_row_count);
        }
        free(ins); free(exs); free(res);
    }
    if (!can_run || g_tr_row_count == 0) ImGui::EndDisabled();

    if (g_tr_summary[0]) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f,1.0f,0.6f,1.0f), "%s", g_tr_summary);
    }
    ImGui::Separator();

    /* --- Test vector table --- */
    const ImGuiTableFlags tf =
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_ScrollY        | ImGuiTableFlags_RowBg;
    float avail_h = ImGui::GetContentRegionAvail().y;
    if (ImGui::BeginTable("##tr_table", 12, tf, ImVec2(0.0f, avail_h))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("",      ImGuiTableColumnFlags_WidthFixed,  14.0f); /* status */
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,  90.0f);
        ImGui::TableSetupColumn("A in",  ImGuiTableColumnFlags_WidthFixed,  42.0f);
        ImGui::TableSetupColumn("X in",  ImGuiTableColumnFlags_WidthFixed,  42.0f);
        ImGui::TableSetupColumn("Y in",  ImGuiTableColumnFlags_WidthFixed,  42.0f);
        ImGui::TableSetupColumn("Z in",  ImGuiTableColumnFlags_WidthFixed,  42.0f);
        ImGui::TableSetupColumn("A exp", ImGuiTableColumnFlags_WidthFixed,  42.0f);
        ImGui::TableSetupColumn("X exp", ImGuiTableColumnFlags_WidthFixed,  42.0f);
        ImGui::TableSetupColumn("Y exp", ImGuiTableColumnFlags_WidthFixed,  42.0f);
        ImGui::TableSetupColumn("Z exp", ImGuiTableColumnFlags_WidthFixed,  42.0f);
        ImGui::TableSetupColumn("Result",ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Del",   ImGuiTableColumnFlags_WidthFixed,  20.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < g_tr_row_count; ) {
            TestRow &row = g_tr_rows[i];
            ImGui::TableNextRow();
            ImGui::PushID(i);

            /* Status icon */
            ImGui::TableSetColumnIndex(0);
            if      (row.result == 1)  ImGui::TextColored(ImVec4(0.3f,1.0f,0.3f,1.0f), "✓");
            else if (row.result == 0)  ImGui::TextColored(ImVec4(1.0f,0.3f,0.3f,1.0f), "✗");
            else                       ImGui::TextDisabled("·");

            /* Label */
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##lbl", row.label, sizeof(row.label));

            /* Input fields */
            auto inp_field = [&](int col, char *buf, size_t bufsz) {
                ImGui::TableSetColumnIndex(col);
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##f", buf, bufsz, ImGuiInputTextFlags_CharsHexadecimal);
            };
            inp_field(2, row.in_a, sizeof(row.in_a));
            inp_field(3, row.in_x, sizeof(row.in_x));
            inp_field(4, row.in_y, sizeof(row.in_y));
            inp_field(5, row.in_z, sizeof(row.in_z));
            inp_field(6, row.ex_a, sizeof(row.ex_a));
            inp_field(7, row.ex_x, sizeof(row.ex_x));
            inp_field(8, row.ex_y, sizeof(row.ex_y));
            inp_field(9, row.ex_z, sizeof(row.ex_z));

            /* Result / fail msg */
            ImGui::TableSetColumnIndex(10);
            if (row.result == 1) {
                ImGui::TextColored(ImVec4(0.3f,1.0f,0.3f,1.0f),
                    "A=$%02X X=$%02X Y=$%02X Z=$%02X",
                    (unsigned)row.act_a,(unsigned)row.act_x,
                    (unsigned)row.act_y,(unsigned)row.act_z);
            } else if (row.result == 0) {
                ImGui::TextColored(ImVec4(1.0f,0.5f,0.5f,1.0f), "%s", row.fail_msg);
            } else {
                ImGui::TextDisabled("(not run)");
            }

            /* Delete button */
            ImGui::TableSetColumnIndex(11);
            if (ImGui::SmallButton("x")) {
                for (int j = i; j < g_tr_row_count - 1; j++)
                    g_tr_rows[j] = g_tr_rows[j+1];
                g_tr_row_count--;
                ImGui::PopID();
                continue;
            }
            ImGui::PopID();
            i++;
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Pane: Pattern Library
 *
 * Browse and copy built-in assembly snippet templates.
 * -------------------------------------------------------------------------- */

static int  g_pat_selected = -1;       /* index into g_snippets[] */
static char g_pat_filter[32] = "";     /* name / category filter   */

static void draw_pane_patterns(void)
{
    if (!show_patterns) return;
    ImGui::Begin("Pattern Library", &show_patterns);

    /* Filter */
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("Filter", g_pat_filter, sizeof(g_pat_filter));
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) g_pat_filter[0] = '\0';

    ImGui::Separator();

    /* Left: scrollable list */
    float list_w = 230.0f;
    ImGui::BeginChild("##pat_list", ImVec2(list_w, 0.0f), true);

    const char *cur_cat = "";
    for (int i = 0; i < g_snippet_count; i++) {
        const snippet_t *s = &g_snippets[i];
        /* Filter */
        if (g_pat_filter[0]) {
            bool ok = (strstr(s->name,     g_pat_filter) != nullptr) ||
                      (strstr(s->category, g_pat_filter) != nullptr) ||
                      (strstr(s->processor,g_pat_filter) != nullptr);
            if (!ok) continue;
        }
        /* Category header */
        if (strcmp(s->category, cur_cat) != 0) {
            cur_cat = s->category;
            ImGui::TextDisabled("[%s]", cur_cat);
        }
        /* Processor badge colour */
        ImVec4 badge;
        if      (strcmp(s->processor, "45gs02") == 0) badge = ImVec4(0.5f, 0.8f, 1.0f, 1.0f);
        else if (strcmp(s->processor, "65c02")  == 0) badge = ImVec4(0.6f, 1.0f, 0.6f, 1.0f);
        else                                           badge = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);

        ImGui::TextColored(badge, "%-8s", s->processor);
        ImGui::SameLine();
        bool sel = (g_pat_selected == i);
        if (ImGui::Selectable(s->name, sel, ImGuiSelectableFlags_AllowOverlap))
            g_pat_selected = i;
    }
    ImGui::EndChild();

    ImGui::SameLine();

    /* Right: detail / code view */
    ImGui::BeginChild("##pat_body", ImVec2(0.0f, 0.0f), false);
    if (g_pat_selected >= 0 && g_pat_selected < g_snippet_count) {
        const snippet_t *s = &g_snippets[g_pat_selected];
        ImGui::TextDisabled("%s  [%s / %s]", s->name, s->category, s->processor);
        ImGui::TextWrapped("%s", s->summary);
        ImGui::Spacing();
        if (ImGui::Button("Copy to Clipboard"))
            ImGui::SetClipboardText(s->body);
        ImGui::Separator();
        /* Monospace scrollable code block */
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 1
                        ? ImGui::GetIO().Fonts->Fonts[1] : nullptr);
        ImGui::InputTextMultiline("##code",
            const_cast<char *>(s->body), strlen(s->body) + 1,
            ImVec2(-1.0f, -1.0f),
            ImGuiInputTextFlags_ReadOnly);
        ImGui::PopFont();
    } else {
        ImGui::TextDisabled("Select a snippet on the left.");
    }
    ImGui::EndChild();

    ImGui::End();
}

/* --------------------------------------------------------------------------
 * Pane: Memory Snapshot Diff (Phase 4)
 *
 * Take a snapshot of current memory, run code, then diff to see what changed.
 * -------------------------------------------------------------------------- */
#define SNAP_DIFF_CAP 4096
static sim_diff_entry_t g_snap_diff_buf[SNAP_DIFF_CAP];
static int              g_snap_diff_count = -1; /* -1 = not yet diffed */

static void draw_pane_snap_diff(void)
{
    if (!show_snap_diff) return;
    ImGui::Begin("Snap Diff", &show_snap_diff);

    bool snap_valid = sim_snapshot_valid(g_sim) != 0;

    if (ImGui::Button("Take Snapshot")) {
        sim_snapshot_take(g_sim);
        g_snap_diff_count = -1;  /* invalidate previous diff */
    }
    ImGui::SameLine();
    if (!snap_valid) ImGui::BeginDisabled();
    if (ImGui::Button("Show Diff")) {
        g_snap_diff_count = sim_snapshot_diff(g_sim, g_snap_diff_buf, SNAP_DIFF_CAP);
    }
    if (!snap_valid) ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled(snap_valid ? "(snapshot active)" : "(no snapshot)");
    ImGui::Separator();

    if (g_snap_diff_count < 0) {
        ImGui::TextDisabled("Take a snapshot, run code, then click 'Show Diff'.");
    } else if (g_snap_diff_count == 0) {
        ImGui::TextDisabled("No memory changes since snapshot.");
    } else {
        ImGui::Text("%d change(s)", g_snap_diff_count);
        ImGui::Separator();

        const ImGuiTableFlags tf =
            ImGuiTableFlags_SizingFixedFit |
            ImGuiTableFlags_BordersInnerV  |
            ImGuiTableFlags_ScrollY        |
            ImGuiTableFlags_RowBg;
        float avail_h = ImGui::GetContentRegionAvail().y;
        if (ImGui::BeginTable("##snapdiff", 4, tf, ImVec2(0.0f, avail_h))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Addr",    ImGuiTableColumnFlags_WidthFixed,   52.0f);
            ImGui::TableSetupColumn("Before",  ImGuiTableColumnFlags_WidthFixed,   50.0f);
            ImGui::TableSetupColumn("After",   ImGuiTableColumnFlags_WidthFixed,   50.0f);
            ImGui::TableSetupColumn("Writer",  ImGuiTableColumnFlags_WidthFixed,   54.0f);
            ImGui::TableHeadersRow();

            int i = 0;
            while (i < g_snap_diff_count) {
                /* Find extent of consecutive-address run */
                int j = i;
                while (j + 1 < g_snap_diff_count &&
                       g_snap_diff_buf[j+1].addr == g_snap_diff_buf[j].addr + 1) j++;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (i == j)
                    ImGui::Text("$%04X", (unsigned)g_snap_diff_buf[i].addr);
                else
                    ImGui::Text("$%04X-%04X", (unsigned)g_snap_diff_buf[i].addr,
                                              (unsigned)g_snap_diff_buf[j].addr);
                ImGui::TableSetColumnIndex(1);
                if (i == j) ImGui::Text("%02X", (unsigned)g_snap_diff_buf[i].before);
                else        ImGui::Text("(%d)", j - i + 1);
                ImGui::TableSetColumnIndex(2);
                if (i == j) ImGui::TextColored(ImVec4(1.0f,0.85f,0.3f,1.0f),
                                "%02X", (unsigned)g_snap_diff_buf[i].after);
                else {
                    /* Show first few bytes in amber */
                    char abuf[32] = ""; int ai = 0;
                    for (int k = i; k <= j && k < i+4 && ai < 28; k++)
                        ai += snprintf(abuf+ai, sizeof(abuf)-ai-1,
                                       k==i ? "%02X" : " %02X",
                                       (unsigned)g_snap_diff_buf[k].after);
                    if (j - i >= 4) strncat(abuf, "..", sizeof(abuf)-strlen(abuf)-1);
                    ImGui::TextColored(ImVec4(1.0f,0.85f,0.3f,1.0f), "%s", abuf);
                }
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("$%04X", (unsigned)g_snap_diff_buf[j].writer_pc);

                i = j + 1;
            }
            ImGui::EndTable();
        }
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
 * Execution icon drawing functions + icon_button helper
 * -------------------------------------------------------------------------- */

/* All icon functions draw into dl, centered at c, with "radius" r, color col.
 * Angle convention follows ImGui/screen-space: 0 = right, PI/2 = down. */

static void draw_icon_play(ImDrawList *dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddTriangleFilled(
        ImVec2(c.x + r*0.80f, c.y),
        ImVec2(c.x - r*0.55f, c.y - r*0.80f),
        ImVec2(c.x - r*0.55f, c.y + r*0.80f), col);
}

static void draw_icon_pause(ImDrawList *dl, ImVec2 c, float r, ImU32 col)
{
    float w = r*0.28f, h = r*0.80f, gap = r*0.22f;
    dl->AddRectFilled(ImVec2(c.x-gap-w, c.y-h), ImVec2(c.x-gap,   c.y+h), col);
    dl->AddRectFilled(ImVec2(c.x+gap,   c.y-h), ImVec2(c.x+gap+w, c.y+h), col);
}

/* ↓ Step Into — vertical stem + downward arrowhead */
static void draw_icon_step_into(ImDrawList *dl, ImVec2 c, float r, ImU32 col)
{
    float sw = r*0.20f;
    dl->AddRectFilled(ImVec2(c.x-sw, c.y-r*0.75f), ImVec2(c.x+sw, c.y+r*0.05f), col);
    dl->AddTriangleFilled(
        ImVec2(c.x,        c.y + r*0.85f),
        ImVec2(c.x - r*0.60f, c.y + r*0.05f),
        ImVec2(c.x + r*0.60f, c.y + r*0.05f), col);
}

/* ⤵ Step Over — top-arc with downward arrowhead at right end */
static void draw_icon_step_over(ImDrawList *dl, ImVec2 c, float r, ImU32 col)
{
    float ar = r*0.50f;
    ImVec2 ac = ImVec2(c.x, c.y - r*0.10f);
    /* Arc from PI (left) to 2PI (right) goes left→top→right (top half). */
    dl->PathArcTo(ac, ar, IM_PI, IM_PI*2.0f, 10);
    dl->PathStroke(col, false, r*0.18f);
    /* Arrowhead at right end (angle 0), tangent is downward */
    ImVec2 tip = ImVec2(ac.x + ar, ac.y);
    float  as  = r*0.40f;
    dl->AddTriangleFilled(
        ImVec2(tip.x,          tip.y + as),
        ImVec2(tip.x - as*0.55f, tip.y - as*0.30f),
        ImVec2(tip.x + as*0.55f, tip.y - as*0.30f), col);
}

/* ↺ Reset — 3/4 circle arc (right→bottom→left→top) + arrowhead at top end */
static void draw_icon_reset(ImDrawList *dl, ImVec2 c, float r, ImU32 col)
{
    float ar = r*0.65f;
    dl->PathArcTo(c, ar, 0.0f, IM_PI*1.5f, 18);
    dl->PathStroke(col, false, r*0.20f);
    /* Arc ends at angle PI*1.5 = top.  Tangent there (CCW) = rightward. */
    ImVec2 tip = ImVec2(c.x, c.y - ar);
    float  as  = r*0.42f;
    dl->AddTriangleFilled(
        ImVec2(tip.x + as,        tip.y),
        ImVec2(tip.x - as*0.30f,  tip.y - as*0.60f),
        ImVec2(tip.x - as*0.30f,  tip.y + as*0.60f), col);
}

/* ◀| History step back — left-pointing triangle + right vertical bar */
static void draw_icon_hist_back(ImDrawList *dl, ImVec2 c, float r, ImU32 col)
{
    float off = r*0.12f;
    dl->AddTriangleFilled(
        ImVec2(c.x - r*0.72f - off, c.y),
        ImVec2(c.x + r*0.12f - off, c.y - r*0.75f),
        ImVec2(c.x + r*0.12f - off, c.y + r*0.75f), col);
    float bw = r*0.20f;
    dl->AddRectFilled(
        ImVec2(c.x + r*0.55f, c.y - r*0.75f),
        ImVec2(c.x + r*0.55f + bw, c.y + r*0.75f), col);
}

/* |▶ History step forward — left vertical bar + right-pointing triangle */
static void draw_icon_hist_fwd(ImDrawList *dl, ImVec2 c, float r, ImU32 col)
{
    float bw  = r*0.20f;
    float off = r*0.12f;
    dl->AddRectFilled(
        ImVec2(c.x - r*0.55f - bw, c.y - r*0.75f),
        ImVec2(c.x - r*0.55f,      c.y + r*0.75f), col);
    dl->AddTriangleFilled(
        ImVec2(c.x + r*0.72f + off, c.y),
        ImVec2(c.x - r*0.12f + off, c.y - r*0.75f),
        ImVec2(c.x - r*0.12f + off, c.y + r*0.75f), col);
}

/* 📁 Folder — body rectangle + small tab on top-left */
static void draw_icon_folder(ImDrawList *dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddRectFilled(
        ImVec2(c.x - r*0.80f, c.y - r*0.38f),
        ImVec2(c.x + r*0.80f, c.y + r*0.65f), col, 2.0f);
    dl->AddRectFilled(
        ImVec2(c.x - r*0.80f, c.y - r*0.70f),
        ImVec2(c.x - r*0.10f, c.y - r*0.38f), col, 2.0f);
}

/* --------------------------------------------------------------------------
 * icon_button — InvisibleButton + hover/active bg fill + icon draw.
 * Returns true on the frame the button is clicked (same semantics as
 * ImGui::Button).  When disabled=true the icon is dimmed and clicks are
 * suppressed without using BeginDisabled (to keep draw-list rendering clean).
 * -------------------------------------------------------------------------- */
typedef void (*IconFn)(ImDrawList*, ImVec2, float, ImU32);

static bool icon_button(const char *str_id, IconFn fn, float btn_sz,
                        ImVec4 icon_col, bool disabled = false,
                        const char *tooltip = nullptr)
{
    ImGui::InvisibleButton(str_id, ImVec2(btn_sz, btn_sz));
    bool hovered = ImGui::IsItemHovered();
    bool active  = !disabled && ImGui::IsItemActive();
    bool clicked = !disabled && ImGui::IsItemClicked();

    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 p  = ImGui::GetItemRectMin();
    ImVec2 sz = ImGui::GetItemRectSize();
    ImVec2 center = ImVec2(p.x + sz.x*0.5f, p.y + sz.y*0.5f);
    float  rad    = sz.x * 0.38f;

    if (!disabled) {
        if (active)
            dl->AddRectFilled(p, ImVec2(p.x+sz.x, p.y+sz.y),
                              ImGui::GetColorU32(ImGuiCol_ButtonActive), 4.0f);
        else if (hovered)
            dl->AddRectFilled(p, ImVec2(p.x+sz.x, p.y+sz.y),
                              ImGui::GetColorU32(ImGuiCol_ButtonHovered), 4.0f);
    }

    ImU32 col = disabled
        ? ImGui::ColorConvertFloat4ToU32(
              ImVec4(icon_col.x*0.35f, icon_col.y*0.35f,
                     icon_col.z*0.35f, 0.40f))
        : ImGui::ColorConvertFloat4ToU32(icon_col);
    fn(dl, center, rad, col);

    if (tooltip && hovered && !disabled) ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

/* --------------------------------------------------------------------------
 * Execution bar — second toolbar row with icon buttons for all run controls
 * plus history step-back / step-forward.
 * Layout: [◀|]  |  [↓] [⤵] [▶/‖]  |  [↺]  hist-indicator  |  [|▶]
 * -------------------------------------------------------------------------- */
static void draw_execution_bar(void)
{
    const float btn_sz = floorf(ImGui::GetFrameHeight() * 1.40f);

    sim_state_t state    = sim_get_state(g_sim);
    bool        has_prog = (state != SIM_IDLE);
    bool        can_step = (state == SIM_READY || state == SIM_PAUSED);
    bool        can_run  = can_step || g_running;
    int         hist_pos = sim_history_position(g_sim);
    int         hist_cnt = sim_history_count(g_sim);
    bool        has_back = has_prog && (hist_pos < hist_cnt);
    bool        has_fwd  = has_prog && (hist_pos > 0);

    static const ImVec4 COL_AMBER = { 1.00f, 0.78f, 0.20f, 1.0f };
    static const ImVec4 COL_GREEN = { 0.30f, 0.90f, 0.45f, 1.0f };
    static const ImVec4 COL_PAUSE = { 1.00f, 0.88f, 0.30f, 1.0f };
    static const ImVec4 COL_RESET = { 0.90f, 0.30f, 0.30f, 1.0f };

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  ImVec2(4.0f, 2.0f));

    /* [◀|] History step back */
    if (icon_button("##hback", draw_icon_hist_back, btn_sz, COL_AMBER, !has_back,
                    "Step Back in History  (Shift+F7)"))
        do_step_back();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    /* [↓] Step Into */
    if (icon_button("##into", draw_icon_step_into, btn_sz, COL_GREEN, !can_step,
                    "Step Into  (F7)"))
        do_step_into();
    ImGui::SameLine();

    /* [⤵] Step Over */
    if (icon_button("##over", draw_icon_step_over, btn_sz, COL_GREEN, !can_step,
                    "Step Over  (F8)"))
        do_step_over();
    ImGui::SameLine();

    /* [▶] Run / [‖] Pause toggle */
    if (g_running) {
        if (icon_button("##pause", draw_icon_pause, btn_sz, COL_PAUSE, false,
                        "Pause execution  (F6 / Esc)"))
            g_running = false;
    } else {
        if (icon_button("##run", draw_icon_play, btn_sz, COL_GREEN, !can_run,
                        "Run continuously  (F5)"))
            g_running = true;
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    /* [↺] Reset */
    if (icon_button("##reset", draw_icon_reset, btn_sz, COL_RESET, !has_prog,
                    "Reset CPU  (Ctrl+R)"))
        do_reset();

    /* History depth indicator */
    ImGui::SameLine(0.0f, 10.0f);
    if (hist_pos > 0) {
        ImGui::TextColored(COL_AMBER, "<- %d / %d", hist_pos, hist_cnt);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Rewound %d step(s)  (%d entries in history)", hist_pos, hist_cnt);
    } else if (hist_cnt > 0) {
        ImGui::TextDisabled("hist: %d", hist_cnt);
    } else {
        ImGui::TextDisabled("hist");
    }

    /* Throttle speed badge */
    if (g_throttle) {
        ImGui::SameLine(0.0f, 10.0f);
        ImGui::TextColored(ImVec4(0.30f, 0.85f, 1.0f, 1.0f),
                           "%.2fx", g_throttle_scale);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Speed throttle: %.2fx C64 (%.0f Hz)\nToggle via Run menu",
                              g_throttle_scale, C64_HZ * g_throttle_scale);
    }

    /* Push [|▶] history-forward button to the right edge */
    ImGui::SameLine();
    {
        float avail  = ImGui::GetContentRegionAvail().x;
        float needed = btn_sz + ImGui::GetStyle().ItemSpacing.x + 10.0f;
        float fill   = avail - needed;
        if (fill > 0.0f) ImGui::Dummy(ImVec2(fill, 1.0f));
    }
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    /* [|▶] History step forward */
    if (icon_button("##hfwd", draw_icon_hist_fwd, btn_sz, COL_AMBER, !has_fwd,
                    "Step Forward in History  (Shift+F8)"))
        do_step_fwd();

    ImGui::PopStyleVar(2);
    ImGui::Separator();
}

/* --------------------------------------------------------------------------
 * Toolbar
 * -------------------------------------------------------------------------- */
static void draw_toolbar(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));

    float btn_w    = 60.0f;
    float combo_w  = 110.0f;
    float browse_w = 26.0f;  /* "..." browse icon button */
    float avail    = ImGui::GetContentRegionAvail().x;
    /* Only Load + Browse + separator + Processor combo remain in this row;
     * execution controls moved to draw_execution_bar(). */
    float input_w = avail - combo_w - btn_w - browse_w
                  - ImGui::GetStyle().ItemSpacing.x * 5.0f - 12.0f;
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

    /* Browse icon button — opens the file dialog */
    ImGui::SameLine();
    {
        float frame_h = ImGui::GetFrameHeight();
        ImU32 folder_col = ImGui::ColorConvertFloat4ToU32(
            ImGui::GetStyleColorVec4(ImGuiCol_Text));
        if (icon_button("##toolbar_browse", draw_icon_folder,
                        browse_w, ImGui::GetStyleColorVec4(ImGuiCol_Text),
                        false, "Browse for file...")) {
            static const FileDlgFilter load_filters[] = {
                { "Assembly Files (*.asm)", ".asm"      },
                { "Binary Files (*.bin)",   ".bin"      },
                { "PRG Files (*.prg)",      ".prg"      },
                { "All Files (*.*)",         nullptr    },
            };
            (void)frame_h; (void)folder_col;
            filedlg_open(&g_filedlg, "Open File", false,
                         load_filters, 4,
                         g_filename_buf[0] ? g_filename_buf : nullptr);
            g_filedlg_mode = FILEDLG_OPEN_FILE;
        }
    }

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

    /* History position indicator */
    if (state != SIM_IDLE) {
        int hpos = sim_history_position(g_sim);
        if (hpos > 0) {
            ImGui::SameLine(0, 8);
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.2f, 1.0f), "<- %d", hpos);
        }
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
static void draw_pane_memory(int idx)
{
    MemViewState &mv = g_mem_views[idx];
    if (!mv.open) return;

    /* Lazy default initialisation (base 0x0200) */
    if (!mv.initialized) {
        mv.base = 0x0200;
        snprintf(mv.addr_buf, sizeof(mv.addr_buf), "0200");
        mv.initialized = true;
    }

    /* Build a unique title: "Memory" for instance 0, "Memory 2..N" for others */
    char title[32];
    if (idx == 0) snprintf(title, sizeof(title), "Memory");
    else          snprintf(title, sizeof(title), "Memory %d", idx + 1);

    ImGui::Begin(title, &mv.open);

    sim_state_t state = sim_get_state(g_sim);
    if (state == SIM_IDLE) {
        ImGui::TextDisabled("(no program loaded)");
        ImGui::End();
        return;
    }

    /* Navigation bar — use per-instance unique widget IDs via ##idx suffix */
    char addr_id[16]; snprintf(addr_id, sizeof(addr_id), "##ma%d", idx);
    ImGui::SetNextItemWidth(60);
    if (ImGui::InputText(addr_id, mv.addr_buf, sizeof(mv.addr_buf),
            ImGuiInputTextFlags_CharsHexadecimal |
            ImGuiInputTextFlags_EnterReturnsTrue)) {
        mv.base = (uint16_t)strtol(mv.addr_buf, NULL, 16);
    }
    char pc_id[16]; snprintf(pc_id, sizeof(pc_id), "PC##pc%d", idx);
    ImGui::SameLine();
    if (ImGui::SmallButton(pc_id)) {
        cpu_t *cpu = sim_get_cpu(g_sim);
        if (cpu) {
            mv.base = cpu->pc;
            snprintf(mv.addr_buf, sizeof(mv.addr_buf), "%04X", (unsigned)mv.base);
        }
    }
    char sp_id[16]; snprintf(sp_id, sizeof(sp_id), "SP##sp%d", idx);
    ImGui::SameLine();
    if (ImGui::SmallButton(sp_id)) {
        cpu_t *cpu = sim_get_cpu(g_sim);
        if (cpu) {
            mv.base = (uint16_t)(0x0100 | (cpu->s & 0xFF));
            snprintf(mv.addr_buf, sizeof(mv.addr_buf), "%04X", (unsigned)mv.base);
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
        uint16_t raddr = (uint16_t)(mv.base + row * 16);
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
        ImGui::SetNextItemWidth(265.0f);
        ImGui::InputText("##savepath", g_binsave_path, sizeof(g_binsave_path));
        ImGui::SameLine();
        if (ImGui::SmallButton("Browse...##bsbr")) {
            static const FileDlgFilter save_filters[] = {
                { "PRG Files (*.prg)", ".prg"   },
                { "Binary (*.bin)",    ".bin"   },
                { "All Files (*.*)",    nullptr },
            };
            filedlg_open(&g_filedlg, "Save Binary As", true,
                         save_filters, 3,
                         g_binsave_path[0] ? g_binsave_path : nullptr);
            g_filedlg_mode = FILEDLG_SAVE_BIN;
            ImGui::CloseCurrentPopup();
        }

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
 * Symbol-file load popup (Load File... button in Symbols pane)
 * -------------------------------------------------------------------------- */
static void draw_symload_popup(void)
{
    if (g_symload_open) {
        ImGui::OpenPopup("Load Symbol File##slp");
        g_symload_open = false;
    }
    if (ImGui::BeginPopupModal("Load Symbol File##slp", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Path to .sym file:");
        ImGui::SetNextItemWidth(265.0f);
        bool enter = ImGui::InputText("##slppath", g_symload_path,
                        sizeof(g_symload_path),
                        ImGuiInputTextFlags_EnterReturnsTrue |
                        ImGuiInputTextFlags_AutoSelectAll);
        ImGui::SameLine();
        if (ImGui::SmallButton("Browse...##slpbr")) {
            static const FileDlgFilter sym_filters[] = {
                { "Symbol Files (*.sym)", ".sym"   },
                { "All Files (*.*)",       nullptr },
            };
            filedlg_open(&g_filedlg, "Load Symbol File", false, sym_filters, 2,
                         g_symload_path[0] ? g_symload_path : nullptr);
            g_filedlg_mode = FILEDLG_OPEN_SYM;
            ImGui::CloseCurrentPopup();
        }
        bool ok = ImGui::Button("Load") || enter;
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        if (ok && g_symload_path[0]) {
            int added = sim_sym_load_file(g_sim, g_symload_path);
            if (added > 0)
                con_add(ImVec4(0.4f,1.0f,0.4f,1.0f),
                        "Loaded %d symbol(s) from %s", added, g_symload_path);
            else
                con_add(ImVec4(1.0f,0.4f,0.4f,1.0f),
                        "Could not load symbols from %s", g_symload_path);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

/* --------------------------------------------------------------------------
 * File-dialog popup dispatcher
 * -------------------------------------------------------------------------- */
static void draw_file_dialog_popup(void)
{
    filedlg_draw(&g_filedlg);
    if (!g_filedlg.confirmed) return;
    g_filedlg.confirmed = false;

    const char *path = g_filedlg.result;

    switch (g_filedlg_mode) {
    case FILEDLG_OPEN_FILE:
        /* g_filename_buf is 512 bytes; result is at most 1023 — truncation is
         * safe here (any real path that fits in the OS will fit in 512 chars). */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(g_filename_buf, sizeof(g_filename_buf), "%s", path);
#pragma GCC diagnostic pop
        if (ext_is(g_filename_buf, ".bin") || ext_is(g_filename_buf, ".prg")) {
            open_binload_dialog(g_filename_buf);
        } else {
            do_load();
            if (sim_get_state(g_sim) != SIM_IDLE)
                con_add(CON_COL_OK,  "Loaded: %s", g_filename_buf);
            else
                con_add(CON_COL_ERR, "Failed to load: %s", g_filename_buf);
        }
        break;

    case FILEDLG_OPEN_SYM: {
        int added = sim_sym_load_file(g_sim, path);
        if (added > 0)
            con_add(CON_COL_OK,  "Loaded %d symbol(s) from %s", added, path);
        else
            con_add(CON_COL_ERR, "Could not load symbols from %s", path);
        break;
    }

    case FILEDLG_SAVE_BIN:
        /* Update binsave path and re-open the save popup for address settings.
         * g_binsave_path is 512 bytes; filedlg result is at most 1023.  Any
         * real filesystem path that we can open will fit within 511 chars. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
        strncpy(g_binsave_path, path, sizeof(g_binsave_path) - 1);
#pragma GCC diagnostic pop
        g_binsave_path[sizeof(g_binsave_path) - 1] = '\0';
        g_binsave_open = true;
        break;

    default:
        break;
    }
    g_filedlg_mode = FILEDLG_NONE;
}

/* --------------------------------------------------------------------------
 * Layout preset helpers (#19)
 * -------------------------------------------------------------------------- */

/* Directory where layout preset .ini files are stored (relative to CWD). */
static const char *layout_preset_dir(void) { return "presets"; }

/* Save the current ImGui layout to presets/<name>.ini */
static void layout_save_preset(const char *name)
{
    mkdir(layout_preset_dir(), 0755);
    char path[576];
    snprintf(path, sizeof(path), "%s/%s.ini", layout_preset_dir(), name);
    ImGui::SaveIniSettingsToDisk(path);
    con_add(ImVec4(0.4f,1.0f,0.4f,1.0f), "Layout saved as '%s'", name);
}

/* Load a preset by name from presets/<name>.ini */
static void layout_load_preset(const char *name)
{
    char path[576];
    snprintf(path, sizeof(path), "%s/%s.ini", layout_preset_dir(), name);
    ImGui::LoadIniSettingsFromDisk(path);
    con_add(ImVec4(0.4f,1.0f,0.4f,1.0f), "Layout loaded: '%s'", name);
}

/* Save-layout popup: prompts for a preset name */
static void draw_layout_save_popup(void)
{
    if (g_layout_save_open) {
        ImGui::OpenPopup("Save Layout Preset##lsp");
        g_layout_save_open = false;
    }
    if (ImGui::BeginPopupModal("Save Layout Preset##lsp", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Preset name:");
        ImGui::SetNextItemWidth(220.0f);
        bool enter = ImGui::InputText("##lspname", g_layout_save_name,
                        sizeof(g_layout_save_name),
                        ImGuiInputTextFlags_EnterReturnsTrue |
                        ImGuiInputTextFlags_AutoSelectAll);
        bool ok = ImGui::Button("Save") || enter;
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        if (ok && g_layout_save_name[0]) {
            layout_save_preset(g_layout_save_name);
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
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

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
        "sim6502-gui v0.99",
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
            int v; float f;
            if (sscanf(line, "FontSize=%d", &v) == 1 && v >= 8 && v <= 32)
                if (v != g_base_font_size) { g_base_font_size = v; g_font_rebuild = true; }
            if (sscanf(line, "Theme=%d", &v) == 1 && v >= 0 && v <= 2)
                if (v != g_theme) { g_theme = v; g_theme_rebuild = true; }
            if (sscanf(line, "Throttle=%d", &v) == 1) g_throttle = (v != 0);
            if (sscanf(line, "ThrottleScale=%f", &f) == 1 && f >= 0.1f && f <= 100.0f)
                g_throttle_scale = f;
        };
        h.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
            int x = 0, y = 0, w = 800, wh = 600;
            if (g_window) {
                SDL_GetWindowPosition(g_window, &x, &y);
                SDL_GetWindowSize(g_window, &w, &wh);
            }
            buf->appendf("[%s][Settings]\n", handler->TypeName);
            buf->appendf("FontSize=%d\n",      g_base_font_size);
            buf->appendf("Theme=%d\n",         g_theme);
            buf->appendf("WindowX=%d\n",       x);
            buf->appendf("WindowY=%d\n",       y);
            buf->appendf("WindowW=%d\n",       w);
            buf->appendf("WindowH=%d\n",       wh);
            buf->appendf("Throttle=%d\n",      g_throttle ? 1 : 0);
            buf->appendf("ThrottleScale=%.4f\n", g_throttle_scale);
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

    /* VIC-II screen texture: 384×272 RGB (border + 320×200 active area) */
    glGenTextures(1, &g_vic_tex);
    glBindTexture(GL_TEXTURE_2D, g_vic_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, VIC2_FRAME_W, VIC2_FRAME_H,
                 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* VIC-II sprite textures: 8 × 24×21 RGBA (transparent background) */
    glGenTextures(8, g_spr_tex);
    for (int sn = 0; sn < 8; sn++) {
        glBindTexture(GL_TEXTURE_2D, g_spr_tex[sn]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 24, 21,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    fprintf(stdout, "sim6502-gui: display %d  scale=%.2fx  font=%.0fpx  window=%dx%d%s\n",
            display_index, ui_scale, font_cfg.SizePixels,
            win_w, win_h, first_run ? "  (first run)" : "");

    /* Create simulator session */
    g_sim = sim_create("6502");
    if (!g_sim) {
        fprintf(stderr, "sim6502-gui: failed to create sim session\n");
        return 1;
    }

    /* Initialise multi-instance memory view: instance 0 starts open */
    g_mem_views[0].open = true;

    /* Initialise file dialog (sets cwd to current working directory) */
    filedlg_init(&g_filedlg);

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
            if (ev.type == SDL_DROPFILE && ev.drop.file) {
                const char *dropped = ev.drop.file;
                if (ext_is(dropped, ".asm")) {
                    snprintf(g_filename_buf, sizeof(g_filename_buf), "%s", dropped);
                    do_load();
                    con_add(CON_COL_OK, "Dropped: %s", dropped);
                } else if (ext_is(dropped, ".bin") || ext_is(dropped, ".prg")) {
                    open_binload_dialog(dropped);
                } else if (ext_is(dropped, ".sym")) {
                    int added = sim_sym_load_file(g_sim, dropped);
                    if (added > 0)
                        con_add(ImVec4(0.4f,1.0f,0.4f,1.0f),
                                "Loaded %d symbol(s) from %s", added, dropped);
                    else
                        con_add(CON_COL_ERR, "Could not load symbols: %s", dropped);
                } else {
                    /* Unknown extension — attempt as assembly */
                    snprintf(g_filename_buf, sizeof(g_filename_buf), "%s", dropped);
                    do_load();
                    con_add(CON_COL_OK, "Dropped: %s", dropped);
                }
                SDL_free(ev.drop.file);
            }
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

            /* Shift+F7: Step back in history / F7: Step into */
            if (ImGui::IsKeyPressed(ImGuiKey_F7)) {
                if (shift)
                    do_step_back();
                else
                    do_step_into();
            }

            /* Shift+F8: Step forward in history / F8: Step over */
            if (ImGui::IsKeyPressed(ImGuiKey_F8)) {
                if (shift)
                    do_step_fwd();
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
                /* Ctrl+Shift+R: Reverse-continue (not yet impl) / Ctrl+R: Reset */
                if (ImGui::IsKeyPressed(ImGuiKey_R)) {
                    if (shift)
                        con_add(CON_COL_WARN, "Reverse-continue not yet available.");
                    else
                        do_reset();
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

                /* Ctrl+Shift+E: Open VIC-II Screen pane / remind about PNG export */
                if (shift && ImGui::IsKeyPressed(ImGuiKey_E)) {
                    show_vic_screen = true;
                    con_add(CON_COL_WARN,
                            "VIC-II Screen pane opened.  PNG export coming in Phase 28.");
                }
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

            /* Reset throttle timer on the first frame after (re)starting */
            if (!g_was_running) {
                g_thr_last = SDL_GetPerformanceCounter();
                g_thr_debt = 0.0;
            }
            g_was_running = true;

            int ev_code;
            if (g_throttle) {
                Uint64 now     = SDL_GetPerformanceCounter();
                double elapsed = (double)(now - g_thr_last) /
                                 (double)SDL_GetPerformanceFrequency();
                g_thr_last = now;
                if (elapsed > 0.05) elapsed = 0.05;   /* clamp: max 50 ms burst */
                g_thr_debt += elapsed * C64_HZ * (double)g_throttle_scale;
                unsigned long budget = (unsigned long)g_thr_debt;
                g_thr_debt -= (double)budget;
                ev_code = sim_step_cycles(g_sim, budget);
            } else {
                ev_code = sim_step(g_sim, g_speed);
            }

            g_last_write_count = sim_get_last_writes(g_sim, g_last_writes, 256);
            g_vic_dirty = true;
            g_spr_dirty = true;
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
            if (!g_running) g_was_running = false;
        } else {
            g_was_running = false;
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
                    if (ImGui::MenuItem("Browse to Load...", nullptr)) {
                        static const FileDlgFilter load_filters[] = {
                            { "Assembly Files (*.asm)", ".asm"   },
                            { "Binary Files (*.bin)",   ".bin"   },
                            { "PRG Files (*.prg)",      ".prg"   },
                            { "All Files (*.*)",         nullptr },
                        };
                        filedlg_open(&g_filedlg, "Open File", false, load_filters, 4);
                        g_filedlg_mode = FILEDLG_OPEN_FILE;
                    }
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
                    if (ImGui::MenuItem("Step Into",  "F7"))   do_step_into();
                    if (ImGui::MenuItem("Step Over",  "F8"))   do_step_over();
                    if (ImGui::MenuItem("Run",        "F5"))   g_running = true;
                    if (ImGui::MenuItem("Pause",      "F6"))   g_running = false;
                    if (ImGui::MenuItem("Reset",      "Ctrl+R")) do_reset();
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
                    if (ImGui::MenuItem("Step Back",        "Shift+F7")) do_step_back();
                    if (ImGui::MenuItem("Step Forward",     "Shift+F8")) do_step_fwd();
                    if (ImGui::MenuItem("Reverse Continue", "Ctrl+Shift+R"))
                        con_add(CON_COL_WARN, "Reverse-continue not yet available.");
                    ImGui::Separator();
                    ImGui::MenuItem("Throttle Speed", nullptr, &g_throttle);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Limit run speed to a fraction of C64 speed.\n1.0x = ~985 kHz (PAL C64)");
                    if (g_throttle) {
                        ImGui::SetNextItemWidth(160.0f);
                        ImGui::SliderFloat("Scale##thr", &g_throttle_scale, 0.1f, 10.0f, "%.2fx C64");
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("1.0x = PAL C64 (~985 kHz)\n0.5x = half speed\n2.0x = double speed");
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("View")) {
                    ImGui::MenuItem("Registers",   nullptr, &show_registers);
                    ImGui::MenuItem("Disassembly", nullptr, &show_disassembly);
                    ImGui::MenuItem("Memory",      nullptr, &g_mem_views[0].open);
                    /* Multi-instance memory views */
                    for (int _mi = 1; _mi < MAX_MEM_VIEWS; _mi++) {
                        if (g_mem_views[_mi].open) {
                            char label[32];
                            snprintf(label, sizeof(label), "Memory %d", _mi + 1);
                            ImGui::MenuItem(label, nullptr, &g_mem_views[_mi].open);
                        }
                    }
                    {
                        /* Count open views; show "Add Memory View" if a slot is free */
                        int open_count = 0;
                        for (int _mi = 0; _mi < MAX_MEM_VIEWS; _mi++)
                            if (g_mem_views[_mi].open) open_count++;
                        if (open_count < MAX_MEM_VIEWS) {
                            if (ImGui::MenuItem("Add Memory View")) {
                                for (int _mi = 1; _mi < MAX_MEM_VIEWS; _mi++) {
                                    if (!g_mem_views[_mi].open) {
                                        g_mem_views[_mi].open        = true;
                                        g_mem_views[_mi].initialized = false;
                                        break;
                                    }
                                }
                            }
                        }
                    }
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
                    ImGui::MenuItem("Snap Diff",      nullptr, &show_snap_diff);
                    ImGui::MenuItem("Test Runner",    nullptr, &show_test_runner);
                    ImGui::MenuItem("Pattern Library",nullptr, &show_patterns);
                    ImGui::Separator();
                    ImGui::MenuItem("VIC-II Screen",    nullptr, &show_vic_screen);
                    ImGui::MenuItem("VIC-II Sprites",   nullptr, &show_vic_sprites);
                    ImGui::MenuItem("VIC-II Registers", nullptr, &show_vic_regs);
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
                /* ---- Layout menu (#19) ---- */
                if (ImGui::BeginMenu("Layout")) {
                    if (ImGui::MenuItem("Save Layout...")) {
                        g_layout_save_name[0] = '\0';
                        g_layout_save_open    = true;
                    }
                    if (ImGui::MenuItem("Reset to Default"))
                        g_reset_layout = true;
                    /* List saved presets */
                    DIR *dp = opendir(layout_preset_dir());
                    if (dp) {
                        bool any = false;
                        struct dirent *de;
                        while ((de = readdir(dp)) != nullptr) {
                            const char *n = de->d_name;
                            size_t nl = strlen(n);
                            if (nl > 4 && strcmp(n + nl - 4, ".ini") == 0) {
                                if (!any) { ImGui::Separator(); any = true; }
                                char label[68];
                                /* strip .ini for display */
                                size_t base_len = nl - 4;
                                if (base_len >= sizeof(label)) base_len = sizeof(label) - 1;
                                memcpy(label, n, base_len);
                                label[base_len] = '\0';
                                if (ImGui::MenuItem(label))
                                    layout_load_preset(label);
                            }
                        }
                        closedir(dp);
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            /* Toolbar (file load + processor selector) */
            draw_toolbar();

            /* Execution bar (icon buttons for run/step/history controls) */
            draw_execution_bar();

            /* Reserve height for status bar */
            float sb_h = ImGui::GetFrameHeight()
                       + ImGui::GetStyle().ItemSpacing.y * 2.0f
                       + ImGui::GetStyle().SeparatorTextBorderSize + 2.0f;
            float ds_h = ImGui::GetContentRegionAvail().y - sb_h;
            if (ds_h < 0.0f) ds_h = 0.0f;

            /* Apply default layout on first run or when explicitly reset */
            const ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
            if ((first_run && !layout_applied) || g_reset_layout) {
                layout_applied  = true;
                g_reset_layout  = false;
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
        for (int _mi = 0; _mi < MAX_MEM_VIEWS; _mi++) draw_pane_memory(_mi);
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
        draw_pane_snap_diff();
        draw_pane_test_runner();
        draw_pane_patterns();

        /* Phase 6 panes */
        draw_pane_vic_screen();
        draw_pane_vic_sprites();
        draw_pane_vic_regs();

        /* Popups */
        draw_binload_popup();
        draw_binsave_popup();
        draw_symload_popup();
        draw_layout_save_popup();
        draw_goto_popup();
        draw_file_dialog_popup();

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
    if (g_vic_tex)     { glDeleteTextures(1, &g_vic_tex);     g_vic_tex     = 0; }
    if (g_spr_tex[0])  { glDeleteTextures(8, g_spr_tex); for (int i=0;i<8;i++) g_spr_tex[i]=0; }
    sim_destroy(g_sim);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
