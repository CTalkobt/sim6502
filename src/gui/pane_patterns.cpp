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
    toolBar->AddTool(801, "Copy to Clipboard", wxArtProvider::GetBitmap(wxART_COPY), "Copy the current preview text to the clipboard");
    toolBar->AddTool(802, "Reset", wxArtProvider::GetBitmap(wxART_UNDO), "Reset preview to original snippet text");
    toolBar->Realize();
    mainSizer->Add(toolBar, 0, wxEXPAND);

    m_search = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_search->SetHint("Search idioms...");
    mainSizer->Add(m_search, 0, wxEXPAND | wxALL, 2);

    m_splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    
    m_list = new wxListBox(m_splitter, wxID_ANY);
    m_preview = new wxStyledTextCtrl(m_splitter, wxID_ANY);
    m_preview->SetReadOnly(false);
    m_preview->SetLexer(wxSTC_LEX_ASM);

    m_splitter->SplitVertically(m_list, m_preview, 200);
    m_splitter->SetMinimumPaneSize(50);

    mainSizer->Add(m_splitter, 1, wxEXPAND);
    SetSizer(mainSizer);

    m_list->Bind(wxEVT_LISTBOX, &PanePatterns::OnItemSelected, this);
    m_search->Bind(wxEVT_TEXT, &PanePatterns::OnSearch, this);
    toolBar->Bind(wxEVT_TOOL, &PanePatterns::OnInsert, this, 801);
    toolBar->Bind(wxEVT_TOOL, &PanePatterns::OnReset, this, 802);

    UpdateList();
}

void PanePatterns::UpdateList() {
    m_list->Clear();
    m_filteredIndices.clear();

    wxString filter = m_search->GetValue().Lower();
    int count = sim_snippet_count();
    for (int i = 0; i < count; i++) {
        sim_snippet_t s;
        if (sim_snippet_get(i, &s) == 0) {
            wxString name = s.name ? wxString::FromUTF8(s.name).Lower() : "";
            wxString summary = s.summary ? wxString::FromUTF8(s.summary).Lower() : "";
            wxString category = s.category ? wxString::FromUTF8(s.category).Lower() : "";
            
            if (filter.IsEmpty() || name.Contains(filter) || summary.Contains(filter) || category.Contains(filter)) {
                m_list->Append(s.name);
                m_filteredIndices.push_back(i);
            }
        }
    }
}

void PanePatterns::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
}

void PanePatterns::OnSearch(wxCommandEvent& WXUNUSED(event)) {
    UpdateList();
}

void PanePatterns::OnReset(wxCommandEvent& WXUNUSED(event)) {
    int sel = m_list->GetSelection();
    if (sel != wxNOT_FOUND) {
        int originalIdx = m_filteredIndices[sel];
        sim_snippet_t s;
        if (sim_snippet_get(originalIdx, &s) == 0) {
            wxString text;
            text << "; Category: " << s.category << "\n";
            text << "; Processor: " << s.processor << "\n";
            if (s.requires_device && strlen(s.requires_device) > 0) {
                text << "; Requires: " << s.requires_device << "\n";
            }
            text << "; Summary: " << s.summary << "\n\n";
            text += wxString::FromUTF8(s.body ? s.body : "");
            m_preview->SetValue(text);
        }
    }
}

void PanePatterns::OnItemSelected(wxCommandEvent& event) {
    int sel = event.GetSelection();
    if (sel != wxNOT_FOUND) {
        int originalIdx = m_filteredIndices[sel];
        sim_snippet_t s;
        if (sim_snippet_get(originalIdx, &s) == 0) {
            wxString text;
            text << "; Category: " << s.category << "\n";
            text << "; Processor: " << s.processor << "\n";
            if (s.requires_device && strlen(s.requires_device) > 0) {
                text << "; Requires: " << s.requires_device << "\n";
            }
            text << "; Summary: " << s.summary << "\n\n";
            text += wxString::FromUTF8(s.body ? s.body : "");
            m_preview->SetValue(text);
        }
    }
}

void PanePatterns::OnInsert(wxCommandEvent& WXUNUSED(event)) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(m_preview->GetText()));
        wxTheClipboard->Close();
    }
}
