#include "pane_source.h"
#include <wx/sizer.h>

PaneSource::PaneSource(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim), m_currentLine(-1) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    m_stc = new wxStyledTextCtrl(this, wxID_ANY);
    sizer->Add(m_stc, 1, wxEXPAND);
    SetSizer(sizer);

    SetupStyles();
    
    m_stc->Bind(wxEVT_STC_MARGINCLICK, &PaneSource::OnMarginClick, this);
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
    m_stc->StyleSetForeground(wxSTC_ASM_COMMENT, wxColour(0, 128, 0));
    m_stc->StyleSetForeground(wxSTC_ASM_DIRECTIVE, wxColour(0, 0, 255));
    m_stc->StyleSetForeground(wxSTC_ASM_CPUINSTRUCTION, wxColour(128, 0, 0));
    m_stc->StyleSetBold(wxSTC_ASM_CPUINSTRUCTION, true);
    
    m_stc->SetReadOnly(true);
}

bool PaneSource::LoadFile(const wxString& path) {
    m_stc->SetReadOnly(false);
    bool ok = m_stc->LoadFile(path);
    m_stc->SetReadOnly(true);
    if (ok) m_loadedPath = path;
    return ok;
}

void PaneSource::RefreshPane(const SimSnapshot &snap) {
    if (!snap.cpu) return;

    char path[512];
    int line;
    if (sim_source_lookup_addr(m_sim, snap.cpu->pc, path, &line)) {
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
