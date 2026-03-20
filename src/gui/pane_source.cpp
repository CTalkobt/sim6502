#include "pane_source.h"
#include "main_frame.h"
#include <wx/sizer.h>
#include <wx/artprov.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/menu.h>
#include <wx/filename.h>

enum {
    ID_SRC_RELOAD     = 601,
    ID_SRC_FIND_NEXT  = 602,
    ID_SRC_FIND_PREV  = 603,
    ID_SRC_FIND_CLOSE = 604,
    ID_SRC_NAV_DISASM = 610,
    ID_SRC_NAV_MEMORY = 611,
};

PaneSource::PaneSource(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim), m_currentLine(-1), m_contextMenuLine(-1)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    // Toolbar
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddTool(ID_SRC_RELOAD, "Reload", wxArtProvider::GetBitmap(wxART_REDO, wxART_TOOLBAR, wxSize(16, 16)), "Reload source file from disk");
    m_titleLabel = new wxStaticText(toolBar, wxID_ANY, "  No source loaded");
    toolBar->AddSeparator();
    toolBar->AddControl(m_titleLabel);
    toolBar->Realize();
    sizer->Add(toolBar, 0, wxEXPAND);
    toolBar->Bind(wxEVT_TOOL, &PaneSource::OnReload, this, ID_SRC_RELOAD);

    // STC editor
    m_stc = new wxStyledTextCtrl(this, wxID_ANY);
    sizer->Add(m_stc, 1, wxEXPAND);

    // Find bar (hidden by default)
    m_findPanel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* findSizer = new wxBoxSizer(wxHORIZONTAL);
    findSizer->Add(new wxStaticText(m_findPanel, wxID_ANY, " Find: "), 0, wxALIGN_CENTER_VERTICAL);
    m_findCtrl = new wxTextCtrl(m_findPanel, wxID_ANY, "", wxDefaultPosition, wxSize(220, -1), wxTE_PROCESS_ENTER);
    findSizer->Add(m_findCtrl, 1, wxALIGN_CENTER_VERTICAL);
    wxButton* btnNext  = new wxButton(m_findPanel, ID_SRC_FIND_NEXT,  "Next", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    wxButton* btnPrev  = new wxButton(m_findPanel, ID_SRC_FIND_PREV,  "Prev", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    wxButton* btnClose = new wxButton(m_findPanel, ID_SRC_FIND_CLOSE, "X",    wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    findSizer->Add(btnNext,  0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    findSizer->Add(btnPrev,  0, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);
    findSizer->Add(btnClose, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    m_findPanel->SetSizer(findSizer);
    sizer->Add(m_findPanel, 0, wxEXPAND);
    m_findPanel->Hide();

    SetSizer(sizer);
    SetupStyles();

    m_stc->Bind(wxEVT_STC_MARGINCLICK, &PaneSource::OnMarginClick, this);
    m_stc->Bind(wxEVT_KEY_DOWN,        &PaneSource::OnKeyDown, this);
    // m_stc->Bind(wxEVT_CONTEXT_MENU,    &PaneSource::OnContextMenu, this); // TODO: re-enable once context menu issues are resolved

    m_findCtrl->Bind(wxEVT_TEXT_ENTER, &PaneSource::OnFindNext,    this);
    m_findCtrl->Bind(wxEVT_KEY_DOWN,   &PaneSource::OnFindKeyDown, this);
    btnNext ->Bind(wxEVT_BUTTON, &PaneSource::OnFindNext,  this);
    btnPrev ->Bind(wxEVT_BUTTON, &PaneSource::OnFindPrev,  this);
    btnClose->Bind(wxEVT_BUTTON, &PaneSource::OnFindClose, this);

    // Bind(wxEVT_MENU, &PaneSource::OnNavDisasm, this, ID_SRC_NAV_DISASM); // TODO: re-enable with context menu
    // Bind(wxEVT_MENU, &PaneSource::OnNavMemory, this, ID_SRC_NAV_MEMORY); // TODO: re-enable with context menu
}

void PaneSource::SetupStyles() {
    m_stc->SetReadOnly(false);

    // Line numbers
    m_stc->SetMarginType(1, wxSTC_MARGIN_NUMBER);
    m_stc->SetMarginWidth(1, 40);

    // Breakpoint/PC margin
    m_stc->SetMarginType(0, wxSTC_MARGIN_SYMBOL);
    m_stc->SetMarginWidth(0, 20);
    m_stc->SetMarginSensitive(0, true);

    // PC Marker (Marker 0)
    m_stc->MarkerDefine(0, wxSTC_MARK_ARROW, *wxBLACK, *wxYELLOW);

    // Breakpoint Marker (Marker 1)
    m_stc->MarkerDefine(1, wxSTC_MARK_CIRCLE, *wxWHITE, *wxRED);

    m_stc->SetLexer(wxSTC_LEX_ASM);
    m_stc->StyleSetForeground(wxSTC_ASM_COMMENT,      wxColour(0, 128, 0));
    m_stc->StyleSetForeground(wxSTC_ASM_DIRECTIVE,    wxColour(0, 0, 255));
    m_stc->StyleSetForeground(wxSTC_ASM_CPUINSTRUCTION, wxColour(128, 0, 0));
    m_stc->StyleSetBold(wxSTC_ASM_CPUINSTRUCTION, true);

    m_stc->SetReadOnly(true);
}

bool PaneSource::LoadFile(const wxString& path) {
    m_stc->SetReadOnly(false);
    bool ok = m_stc->LoadFile(path);
    m_stc->SetReadOnly(true);
    if (ok) {
        m_loadedPath = path;
        m_currentLine = -1;
        wxString fname = wxFileName(path).GetFullName();
        m_titleLabel->SetLabel("  " + fname);
        MainFrame* mf = dynamic_cast<MainFrame*>(wxGetTopLevelParent(this));
        if (mf) mf->UpdatePaneCaption(this, "Source: " + fname);
    }
    return ok;
}

void PaneSource::RefreshPane(const SimSnapshot &snap) {
    if (!snap.cpu) return;

    char path[512];
    int line;
    bool mapped = sim_source_lookup_addr(m_sim, snap.cpu->pc, path, &line) && path[0] != '\0';

    if (mapped) {
        wxString wxPath(path);
        if (wxPath != m_loadedPath) {
            LoadFile(wxPath);
        }
        if (m_currentLine != line - 1) {
            m_stc->MarkerDeleteAll(0);
            m_stc->MarkerAdd(line - 1, 0);
            m_stc->GotoLine(line - 1);
            m_stc->EnsureVisible(line - 1);
            m_currentLine = line - 1;
        }
    } else if (m_loadedPath.IsEmpty()) {
        // No source map path available (e.g. KickAssembler .asm without .list).
        // Load the assembled source file directly so the user can at least read it.
        const char *fn = sim_get_filename(m_sim);
        if (fn && fn[0] && strcmp(fn, "(none)") != 0) {
            wxString wxFn(fn);
            if (wxFn.EndsWith(".asm") || wxFn.EndsWith(".s"))
                LoadFile(wxFn);
        }
    }
}

void PaneSource::ShowFindBar(bool show) {
    m_findPanel->Show(show);
    Layout();
    if (show) {
        m_findCtrl->SetFocus();
        m_findCtrl->SelectAll();
    } else {
        m_stc->SetFocus();
    }
}

void PaneSource::DoFind(bool forward) {
    wxString text = m_findCtrl->GetValue();
    if (text.IsEmpty()) return;

    int docLen = m_stc->GetLength();

    if (forward) {
        int startPos = m_stc->GetSelectionEnd();
        m_stc->SetTargetStart(startPos);
        m_stc->SetTargetEnd(docLen);
        m_stc->SetSearchFlags(0);
        int found = m_stc->SearchInTarget(text);
        if (found == -1) {
            // Wrap around from beginning
            m_stc->SetTargetStart(0);
            m_stc->SetTargetEnd(startPos);
            found = m_stc->SearchInTarget(text);
        }
        if (found != -1) {
            int matchStart = m_stc->GetTargetStart();
            int matchEnd   = m_stc->GetTargetEnd();
            m_stc->GotoPos(matchStart);
            m_stc->SetSelection(matchStart, matchEnd);
            m_stc->EnsureCaretVisible();
        }
    } else {
        int endPos = m_stc->GetSelectionStart();
        m_stc->SetTargetStart(endPos > 0 ? endPos - 1 : 0);
        m_stc->SetTargetEnd(0);
        m_stc->SetSearchFlags(0);
        int found = m_stc->SearchInTarget(text);
        if (found == -1) {
            // Wrap around from end
            m_stc->SetTargetStart(docLen);
            m_stc->SetTargetEnd(endPos);
            found = m_stc->SearchInTarget(text);
        }
        if (found != -1) {
            int matchStart = m_stc->GetTargetStart();
            int matchEnd   = m_stc->GetTargetEnd();
            m_stc->GotoPos(matchStart);
            m_stc->SetSelection(matchStart, matchEnd);
            m_stc->EnsureCaretVisible();
        }
    }
}

void PaneSource::OnMarginClick(wxStyledTextEvent& event) {
    if (event.GetMargin() == 0) {
        int line = m_stc->LineFromPosition(event.GetPosition());
        uint16_t addr;
        if (sim_source_lookup_line(m_sim, m_loadedPath.mb_str(), line + 1, &addr)) {
            if (sim_has_breakpoint(m_sim, addr)) {
                sim_break_clear(m_sim, addr);
                m_stc->MarkerDelete(line, 1);
            } else {
                sim_break_set(m_sim, addr, NULL);
                m_stc->MarkerAdd(line, 1);
            }
        }
    }
}

void PaneSource::OnReload(wxCommandEvent& WXUNUSED(event)) {
    if (!m_loadedPath.IsEmpty()) {
        LoadFile(m_loadedPath);
    }
}

void PaneSource::OnKeyDown(wxKeyEvent& event) {
    if (event.ControlDown() && event.GetKeyCode() == 'F') {
        ShowFindBar(true);
    } else {
        event.Skip();
    }
}

void PaneSource::OnFindKeyDown(wxKeyEvent& event) {
    if (event.GetKeyCode() == WXK_ESCAPE) {
        ShowFindBar(false);
    } else if (event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_NUMPAD_ENTER) {
        DoFind(/* forward= */ !event.ShiftDown());
    } else {
        event.Skip();
    }
}

void PaneSource::OnFindNext(wxCommandEvent& WXUNUSED(event)) {
    DoFind(true);
}

void PaneSource::OnFindPrev(wxCommandEvent& WXUNUSED(event)) {
    DoFind(false);
}

void PaneSource::OnFindClose(wxCommandEvent& WXUNUSED(event)) {
    ShowFindBar(false);
}

// Look up the address for a source line, trying exact path then line-only fallback.
// The fallback handles mismatches between relative (sim_get_filename) and absolute
// (.dbg-stored) paths, which is common when a pre-built .prg was loaded first.
static bool lookup_line_addr(sim_session_t *sim, const wxString& loadedPath, int oneBased, uint16_t *addr) {
    if (sim_source_lookup_line(sim, loadedPath.mb_str(), oneBased, addr))
        return true;
    // Fallback: match by line number only (single-source-file projects)
    return sim_source_lookup_line(sim, nullptr, oneBased, addr) != 0;
}

void PaneSource::OnContextMenu(wxContextMenuEvent& event) {
    wxPoint clientPos = m_stc->ScreenToClient(event.GetPosition());
    int pos = m_stc->PositionFromPoint(clientPos);
    m_contextMenuLine = m_stc->LineFromPosition(pos);

    wxMenu menu;
    bool hasAddr = false;
    if (m_contextMenuLine >= 0) {
        uint16_t addr;
        hasAddr = lookup_line_addr(m_sim, m_loadedPath, m_contextMenuLine + 1, &addr);
    }
    menu.Append(ID_SRC_NAV_DISASM, "Go to Address in Disassembly");
    menu.Append(ID_SRC_NAV_MEMORY, "Go to Address in Memory View");
    menu.Enable(ID_SRC_NAV_DISASM, hasAddr);
    menu.Enable(ID_SRC_NAV_MEMORY, hasAddr);

    PopupMenu(&menu);
}

void PaneSource::OnNavDisasm(wxCommandEvent& WXUNUSED(event)) {
    if (m_contextMenuLine < 0) return;
    uint16_t addr;
    if (lookup_line_addr(m_sim, m_loadedPath, m_contextMenuLine + 1, &addr)) {
        MainFrame* mf = dynamic_cast<MainFrame*>(wxGetTopLevelParent(this));
        if (mf) mf->NavigateDisassembly(addr);
    }
}

void PaneSource::OnNavMemory(wxCommandEvent& WXUNUSED(event)) {
    if (m_contextMenuLine < 0) return;
    uint16_t instrAddr;
    if (!lookup_line_addr(m_sim, m_loadedPath, m_contextMenuLine + 1, &instrAddr)) return;

    // Navigate to the operand's memory address, not the instruction's PC.
    // e.g. "STA ITER_COUNT" → $12, not the address of the STA opcode itself.
    uint16_t navAddr = instrAddr;
    sim_disasm_entry_t entry;
    if (sim_disassemble_entry(m_sim, instrAddr, &entry) && entry.has_target)
        navAddr = (uint16_t)entry.target_addr;

    MainFrame* mf = dynamic_cast<MainFrame*>(wxGetTopLevelParent(this));
    if (mf) mf->NavigateMemory(navAddr);
}
