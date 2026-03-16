#include "pane_patterns.h"
#include <wx/sizer.h>
#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <wx/clipbrd.h>

PanePatterns::PanePatterns(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddTool(801, "Insert (Copy)", wxArtProvider::GetBitmap(wxART_COPY), "Copy the selected assembly idiom to the clipboard");
    toolBar->Realize();
    mainSizer->Add(toolBar, 0, wxEXPAND);

    m_splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    
    m_list = new wxListBox(m_splitter, wxID_ANY);
    m_preview = new wxStyledTextCtrl(m_splitter, wxID_ANY);
    m_preview->SetReadOnly(true);
    m_preview->SetLexer(wxSTC_LEX_ASM);

    m_splitter->SplitVertically(m_list, m_preview, 200);
    m_splitter->SetMinimumPaneSize(50);

    mainSizer->Add(m_splitter, 1, wxEXPAND);
    SetSizer(mainSizer);

    m_list->Bind(wxEVT_LISTBOX, &PanePatterns::OnItemSelected, this);
    toolBar->Bind(wxEVT_TOOL, &PanePatterns::OnInsert, this, 801);

    int count = sim_snippet_count();
    for (int i = 0; i < count; i++) {
        sim_snippet_t s;
        if (sim_snippet_get(i, &s) == 0) {
            m_list->Append(s.name);
        }
    }
}

void PanePatterns::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
}

void PanePatterns::OnItemSelected(wxCommandEvent& event) {
    int sel = event.GetSelection();
    if (sel != wxNOT_FOUND) {
        sim_snippet_t s;
        if (sim_snippet_get(sel, &s) == 0) {
            m_preview->SetReadOnly(false);
            wxString text;
            text << "; Category: " << s.category << "\n";
            text << "; Processor: " << s.processor << "\n";
            text << "; Summary: " << s.summary << "\n\n";
            text << s.body;
            m_preview->SetValue(text);
            m_preview->SetReadOnly(true);
        }
    }
}

void PanePatterns::OnInsert(wxCommandEvent& WXUNUSED(event)) {
    int sel = m_list->GetSelection();
    if (sel != wxNOT_FOUND) {
        sim_snippet_t s;
        if (sim_snippet_get(sel, &s) == 0) {
            if (wxTheClipboard->Open()) {
                wxTheClipboard->SetData(new wxTextDataObject(s.body));
                wxTheClipboard->Close();
            }
        }
    }
}
