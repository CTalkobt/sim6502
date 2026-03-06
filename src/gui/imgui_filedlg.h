/* --------------------------------------------------------------------------
 * imgui_filedlg.h  —  Lightweight ImGui file-open / file-save dialog
 *
 * Self-contained header-only implementation.  Uses only POSIX dirent/stat
 * and Dear ImGui — no external dependencies.
 *
 * Include from exactly one translation unit (main.cpp).
 *
 * Public API:
 *   filedlg_init(&s)              — call once at startup
 *   filedlg_open(&s, ...)         — arm the dialog; call every time it should open
 *   filedlg_draw(&s)              — call every frame inside the popup-draw section
 *
 *   After filedlg_draw returns, check s.confirmed:
 *     if (s.confirmed) { s.confirmed = false; use(s.result); }
 * -------------------------------------------------------------------------- */
#pragma once

#include <algorithm>    /* std::sort            */
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>    /* strcasecmp           */
#include <sys/stat.h>
#include <unistd.h>
#include "imgui.h"

/* Silence GCC's over-eager -Wstringop-truncation / -Wformat-truncation on
 * the safe strncpy/snprintf patterns used throughout this file.
 * All buffer sizes are chosen so actual runtime truncation cannot occur for
 * any real filesystem path (< 1024 chars) or filename (< 256 chars). */
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wstringop-truncation"
#  pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */
#define FILEDLG_MAX_ENTRIES  2048   /* max files shown per directory   */
#define FILEDLG_MAX_FILTERS     8   /* max filter entries per session   */
#define FILEDLG_DLG_W       660.0f  /* fixed dialog width  (px)        */
#define FILEDLG_DLG_H       480.0f  /* fixed dialog height (px)        */

/* --------------------------------------------------------------------------
 * Structs
 * -------------------------------------------------------------------------- */

struct FileDlgEntry {
    char name[256];
    bool is_dir;
    long size;          /* -1 for directories */
};

/* One entry in the filter combo (ext may be comma-separated, e.g. ".bin,.prg",
 * or nullptr / "" for "all files"). */
struct FileDlgFilter {
    const char *label;  /* e.g. "Assembly Files (*.asm)"  */
    const char *ext;    /* e.g. ".asm"  or nullptr for all */
};

struct FileDlgState {
    /* ---- persistent across open() calls ---- */
    char  cwd[1024];            /* currently-browsed directory      */
    bool  show_hidden;          /* show dot-files                   */
    int   filter_idx;           /* which filter is active           */

    /* ---- per-session (reset on each filedlg_open call) ---- */
    bool          open_req;
    bool          is_save;
    char          title[80];
    FileDlgFilter filters[FILEDLG_MAX_FILTERS];
    int           filter_count;
    char          filename_edit[512]; /* editable name at the bottom    */

    /* ---- directory listing (rebuilt on navigation) ---- */
    FileDlgEntry  entries[FILEDLG_MAX_ENTRIES];
    int           entry_count;
    int           selected_idx;
    bool          needs_refresh;
    bool          path_error;         /* true if opendir() failed       */
    char          error_msg[160];

    /* ---- path-bar (editable current directory) ---- */
    char  path_edit[1024];

    /* ---- result (caller reads after confirmed) ---- */
    char  result[1024];
    bool  confirmed;
    bool  cancelled;
};

/* --------------------------------------------------------------------------
 * Internal helpers (all static; safe to define in a header included once)
 * -------------------------------------------------------------------------- */

/* Case-insensitive, comma-separated extension match.
 * ext_spec examples: ".asm"  ".bin,.prg"  nullptr (= all files) */
static bool filedlg_ext_match(const char *name, const char *ext_spec)
{
    if (!ext_spec || !ext_spec[0]) return true;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    const char *p = ext_spec;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (strlen(dot) == len) {
            bool ok = true;
            for (size_t i = 0; i < len && ok; i++)
                if (tolower((unsigned char)dot[i]) != tolower((unsigned char)p[i]))
                    ok = false;
            if (ok) return true;
        }
        if (!comma) break;
        p = comma + 1;
    }
    return false;
}

/* Navigate up one directory level. */
static void filedlg_go_up(FileDlgState *s)
{
    char *last = strrchr(s->cwd, '/');
    if (!last) return;
    if (last == s->cwd) {
        /* "/something" or already "/" — truncate to "/" */
        s->cwd[1] = '\0';
    } else {
        *last = '\0';
    }
    strncpy(s->path_edit, s->cwd, sizeof(s->path_edit) - 1);
    s->needs_refresh = true;
    s->filename_edit[0] = '\0';
    s->selected_idx = -1;
}

/* Navigate to newdir (absolute or relative to cwd). */
static void filedlg_navigate(FileDlgState *s, const char *newdir)
{
    char buf[1024];
    if (!newdir || !newdir[0]) return;

    if (newdir[0] == '/') {
        strncpy(buf, newdir, sizeof(buf) - 1);
    } else {
        /* Relative: append to cwd.  Avoid double-slash when cwd == "/". */
        snprintf(buf, sizeof(buf), "%s/%s",
                 strcmp(s->cwd, "/") == 0 ? "" : s->cwd, newdir);
    }

    /* Strip trailing slash (except root) */
    size_t len = strlen(buf);
    if (len > 1 && buf[len-1] == '/') buf[--len] = '\0';

    /* Verify it's an accessible directory */
    struct stat st;
    if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) return;

    strncpy(s->cwd,       buf, sizeof(s->cwd) - 1);
    strncpy(s->path_edit, buf, sizeof(s->path_edit) - 1);
    s->needs_refresh = true;
    s->filename_edit[0] = '\0';
    s->selected_idx = -1;
}

/* Read and sort directory entries for the current cwd. */
static void filedlg_refresh(FileDlgState *s)
{
    s->needs_refresh = false;
    s->path_error    = false;
    s->entry_count   = 0;
    s->selected_idx  = -1;

    DIR *dp = opendir(s->cwd);
    if (!dp) {
        s->path_error = true;
        snprintf(s->error_msg, sizeof(s->error_msg),
                 "Cannot open directory: %s", strerror(errno));
        return;
    }

    const bool at_root = (strcmp(s->cwd, "/") == 0);

    struct dirent *de;
    while ((de = readdir(dp)) != nullptr) {
        const char *n = de->d_name;
        if (strcmp(n, ".") == 0) continue;
        /* Skip ".." when already at root */
        if (strcmp(n, "..") == 0 && at_root) continue;
        /* Skip hidden files unless requested */
        if (!s->show_hidden && n[0] == '.' && strcmp(n, "..") != 0) continue;
        if (s->entry_count >= FILEDLG_MAX_ENTRIES - 1) break;

        FileDlgEntry &e = s->entries[s->entry_count];
        strncpy(e.name, n, sizeof(e.name) - 1);
        e.name[sizeof(e.name) - 1] = '\0';

        char full[1280];
        snprintf(full, sizeof(full), "%s/%s",
                 at_root ? "" : s->cwd, n);

        struct stat st;
        if (stat(full, &st) == 0) {
            e.is_dir = S_ISDIR(st.st_mode);
            e.size   = e.is_dir ? -1L : (long)st.st_size;
        } else {
            e.is_dir = false;
            e.size   = 0;
        }
        s->entry_count++;
    }
    closedir(dp);

    /* Sort: ".." first, then dirs alpha, then files alpha */
    std::sort(s->entries, s->entries + s->entry_count,
              [](const FileDlgEntry &a, const FileDlgEntry &b) -> bool {
                  bool a_up = (strcmp(a.name, "..") == 0);
                  bool b_up = (strcmp(b.name, "..") == 0);
                  if (a_up) return true;
                  if (b_up) return false;
                  if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
                  return strcasecmp(a.name, b.name) < 0;
              });
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/* Call once at application startup. */
static void filedlg_init(FileDlgState *s)
{
    memset(s, 0, sizeof(*s));
    s->selected_idx  = -1;
    s->needs_refresh = true;
    if (!getcwd(s->cwd, sizeof(s->cwd)))
        strncpy(s->cwd, "/", sizeof(s->cwd) - 1);
    strncpy(s->path_edit, s->cwd, sizeof(s->path_edit) - 1);
}

/* Arm the dialog to open on the next frame.
 *   title          — popup title string
 *   is_save        — true = save dialog (OK button says "Save")
 *   filters        — array of FileDlgFilter, count = nfilters
 *   initial_file   — optional pre-fill for the filename box (basename only)
 */
static void filedlg_open(FileDlgState *s,
                          const char *title,
                          bool is_save,
                          const FileDlgFilter *filters,
                          int nfilters,
                          const char *initial_file = nullptr)
{
    strncpy(s->title, title, sizeof(s->title) - 1);
    s->is_save      = is_save;
    s->filter_count = (nfilters < FILEDLG_MAX_FILTERS) ? nfilters : FILEDLG_MAX_FILTERS;
    for (int i = 0; i < s->filter_count; i++) s->filters[i] = filters[i];
    if (s->filter_idx >= s->filter_count) s->filter_idx = 0;

    s->filename_edit[0] = '\0';
    if (initial_file) {
        /* If it looks like a full path, extract the basename */
        const char *base = strrchr(initial_file, '/');
        strncpy(s->filename_edit, base ? base + 1 : initial_file,
                sizeof(s->filename_edit) - 1);
    }

    s->confirmed  = false;
    s->cancelled  = false;
    s->open_req   = true;
    s->needs_refresh = true;
    strncpy(s->path_edit, s->cwd, sizeof(s->path_edit) - 1);
}

/* Call every frame in the popup-draw section.
 * Internally calls ImGui::OpenPopup / BeginPopupModal / EndPopupModal.
 * Sets s->confirmed + s->result when the user accepts. */
static void filedlg_draw(FileDlgState *s)
{
    if (s->open_req) {
        ImGui::OpenPopup(s->title);
        s->open_req = false;
    }
    if (s->needs_refresh) filedlg_refresh(s);

    /* Centre the dialog on the main viewport */
    ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(FILEDLG_DLG_W, FILEDLG_DLG_H), ImGuiCond_Always);

    if (!ImGui::BeginPopupModal(s->title, nullptr,
                                ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_NoMove   |
                                ImGuiWindowFlags_NoScrollbar))
        return;

    const ImVec4 COL_DIR = ImVec4(1.00f, 0.85f, 0.40f, 1.0f); /* amber for dirs  */
    const ImVec4 COL_ERR = ImVec4(1.00f, 0.35f, 0.35f, 1.0f); /* red for errors  */

    /* ------------------------------------------------------------------ */
    /* Navigation bar: [^ Up]  [path text input — press Enter to navigate] */
    /* ------------------------------------------------------------------ */
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));

    if (ImGui::Button("^ Up")) filedlg_go_up(s);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Go to parent directory");

    ImGui::SameLine();
    {
        float nav_w = ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(nav_w);
        if (ImGui::InputText("##filedlg_path", s->path_edit, sizeof(s->path_edit),
                             ImGuiInputTextFlags_EnterReturnsTrue |
                             ImGuiInputTextFlags_AutoSelectAll)) {
            filedlg_navigate(s, s->path_edit);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Type a path and press Enter to navigate");
    }
    ImGui::PopStyleVar();

    ImGui::Separator();

    /* ------------------------------------------------------------------ */
    /* File list — scrollable child window                                 */
    /* ------------------------------------------------------------------ */
    /* Reserve space for: separator + filename row + filter/button row + padding */
    float bottom_h = ImGui::GetFrameHeightWithSpacing() * 2.2f
                   + ImGui::GetStyle().SeparatorTextBorderSize
                   + ImGui::GetStyle().ItemSpacing.y * 3.0f
                   + ImGui::GetStyle().WindowPadding.y;
    float list_h = ImGui::GetContentRegionAvail().y - bottom_h;
    if (list_h < 60.0f) list_h = 60.0f;

    const char *active_ext = (s->filter_count > 0 && s->filter_idx < s->filter_count)
                             ? s->filters[s->filter_idx].ext : nullptr;

    if (ImGui::BeginChild("##filedlg_list", ImVec2(0.0f, list_h), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        if (s->path_error) {
            ImGui::TextColored(COL_ERR, "%s", s->error_msg);
            ImGui::Spacing();
            ImGui::TextDisabled("Click '^ Up' to return to the parent directory.");
        } else if (s->entry_count == 0) {
            ImGui::TextDisabled("(empty directory)");
        } else {
            const ImGuiTableFlags tf =
                ImGuiTableFlags_SizingFixedFit |
                ImGuiTableFlags_RowBg          |
                ImGuiTableFlags_BordersInnerV;

            if (ImGui::BeginTable("##filedlg_tbl", 2, tf)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);

                for (int i = 0; i < s->entry_count; i++) {
                    const FileDlgEntry &e = s->entries[i];
                    const bool is_dotdot  = (strcmp(e.name, "..") == 0);

                    /* Show all directories; filter files by active extension */
                    if (!e.is_dir && !filedlg_ext_match(e.name, active_ext))
                        continue;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    if (e.is_dir) ImGui::PushStyleColor(ImGuiCol_Text, COL_DIR);

                    char sel_label[264];
                    if (e.is_dir)
                        snprintf(sel_label, sizeof(sel_label), "[%s]", e.name);
                    else
                        snprintf(sel_label, sizeof(sel_label), "%s", e.name);

                    bool sel = (s->selected_idx == i);
                    ImGui::PushID(i);
                    bool clicked = ImGui::Selectable(
                        sel_label, sel,
                        ImGuiSelectableFlags_SpanAllColumns |
                        ImGuiSelectableFlags_AllowDoubleClick,
                        ImVec2(0, 0));
                    ImGui::PopID();

                    if (e.is_dir) ImGui::PopStyleColor();

                    if (clicked) {
                        s->selected_idx = i;
                        if (!e.is_dir)
                            strncpy(s->filename_edit, e.name,
                                    sizeof(s->filename_edit) - 1);

                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            if (e.is_dir) {
                                if (is_dotdot)
                                    filedlg_go_up(s);
                                else
                                    filedlg_navigate(s, e.name);
                            } else {
                                /* Double-click on file → accept immediately */
                                const char *sep = (strcmp(s->cwd, "/") == 0) ? "" : "/";
                                snprintf(s->result, sizeof(s->result),
                                         "%s%s%s", s->cwd, sep, e.name);
                                s->confirmed = true;
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }

                    /* Size column */
                    ImGui::TableSetColumnIndex(1);
                    if (e.size < 0) {
                        ImGui::TextDisabled("--");
                    } else if (e.size < 1024) {
                        ImGui::TextDisabled("%ld B", e.size);
                    } else if (e.size < 1024 * 1024) {
                        ImGui::TextDisabled("%.1f KB", (double)e.size / 1024.0);
                    } else {
                        ImGui::TextDisabled("%.1f MB",
                                            (double)e.size / (1024.0 * 1024.0));
                    }
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    /* ------------------------------------------------------------------ */
    /* Filename text input (full width)                                    */
    /* ------------------------------------------------------------------ */
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputText("##filedlg_name", s->filename_edit, sizeof(s->filename_edit));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(s->is_save ? "Output filename" : "Selected filename");

    /* ------------------------------------------------------------------ */
    /* Filter combo  |  Hidden checkbox  |  Open/Save  |  Cancel          */
    /* ------------------------------------------------------------------ */
    {
        const float btn_ok_w  = 80.0f;
        const float btn_can_w = 70.0f;
        const float chk_w     = 65.0f;  /* "Hidden" checkbox approximate width */
        float combo_w = ImGui::GetContentRegionAvail().x
                      - btn_ok_w - btn_can_w - chk_w
                      - ImGui::GetStyle().ItemSpacing.x * 4.0f;
        if (combo_w < 80.0f) combo_w = 80.0f;

        ImGui::SetNextItemWidth(combo_w);
        const char *combo_preview = (s->filter_count > 0)
                                    ? s->filters[s->filter_idx].label
                                    : "All Files (*.*)";
        if (ImGui::BeginCombo("##filedlg_flt", combo_preview)) {
            for (int i = 0; i < s->filter_count; i++) {
                bool fsel = (i == s->filter_idx);
                if (ImGui::Selectable(s->filters[i].label, fsel)) {
                    s->filter_idx   = i;
                    s->selected_idx = -1;
                }
                if (fsel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Checkbox("Hidden", &s->show_hidden))
            s->needs_refresh = true;

        ImGui::SameLine();
        bool ok = ImGui::Button(s->is_save ? "Save##fdok" : "Open##fdok",
                                ImVec2(btn_ok_w, 0));
        ImGui::SameLine();
        if (ImGui::Button("Cancel##fdcan", ImVec2(btn_can_w, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            s->cancelled = true;
            ImGui::CloseCurrentPopup();
        }

        /* Confirm on OK (or Enter in filename field is handled via ok above) */
        if (ok && s->filename_edit[0]) {
            if (s->filename_edit[0] == '/') {
                /* User typed an absolute path directly */
                strncpy(s->result, s->filename_edit, sizeof(s->result) - 1);
            } else {
                const char *sep = (strcmp(s->cwd, "/") == 0) ? "" : "/";
                snprintf(s->result, sizeof(s->result),
                         "%s%s%s", s->cwd, sep, s->filename_edit);
            }
            s->confirmed = true;
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::EndPopup();
}

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
