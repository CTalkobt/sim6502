#include "pane_symbols.h"
#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <wx/filedlg.h>
#include <wx/stattext.h>

SymbolsListCtrl::SymbolsListCtrl(wxWindow* parent, sim_session_t* sim)
    : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
      m_sim(sim) 
{
    InsertColumn(0, "Name", wxLIST_FORMAT_LEFT, 150);
    InsertColumn(1, "Address", wxLIST_FORMAT_LEFT, 60);
    InsertColumn(2, "Type", wxLIST_FORMAT_LEFT, 80);
    InsertColumn(3, "Source", wxLIST_FORMAT_LEFT, 200);
}

void SymbolsListCtrl::SetFilter(const wxString& filter) {
    m_filter = filter.Upper();
    UpdateList();
}

void SymbolsListCtrl::UpdateList() {
    m_filteredIndices.clear();
    int total = sim_sym_count(m_sim);
    for (int i = 0; i < total; i++) {
        uint16_t addr;
        char name[128];
        int type;
        if (sim_sym_get_idx(m_sim, i, &addr, name, sizeof(name), &type, NULL, 0) == 0) {
            if (m_filter.IsEmpty() || wxString(name).Upper().Contains(m_filter)) {
                m_filteredIndices.push_back(i);
            }
        }
    }
    SetItemCount((long)m_filteredIndices.size());
    Refresh();
}

wxString SymbolsListCtrl::OnGetItemText(long item, long column) const {
    if (item < 0 || item >= (long)m_filteredIndices.size()) return "";
    
    int realIdx = m_filteredIndices[item];
    uint16_t addr;
    char name[128];
    int type;
    char comment[256];
    if (sim_sym_get_idx(m_sim, realIdx, &addr, name, sizeof(name), &type, comment, sizeof(comment)) != 0) return "";

    switch (column) {
        case 0: return wxString(name);
        case 1: return wxString::Format("%04X", addr);
        case 2: return wxString(sim_sym_type_name(type));
        case 3: return wxString(comment);
    }
    return "";
}

PaneSymbols::PaneSymbols(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddTool(501, "Load", wxArtProvider::GetBitmap(wxART_FILE_OPEN), "Load symbols from a .sym file");
    toolBar->AddTool(502, "Save", wxArtProvider::GetBitmap(wxART_FILE_SAVE), "Save symbols to a .sym file");
    toolBar->AddSeparator();
    toolBar->AddControl(new wxStaticText(toolBar, wxID_ANY, " Filter: "));
    m_filter = new wxTextCtrl(toolBar, wxID_ANY, "", wxDefaultPosition, wxSize(150, -1));
    toolBar->AddControl(m_filter);
    toolBar->Realize();
    mainSizer->Add(toolBar, 0, wxEXPAND);

    m_list = new SymbolsListCtrl(this, m_sim);
    mainSizer->Add(m_list, 1, wxEXPAND);
    SetSizer(mainSizer);

    m_filter->Bind(wxEVT_TEXT, &PaneSymbols::OnFilter, this);
    toolBar->Bind(wxEVT_TOOL, &PaneSymbols::OnLoad, this, 501);
    toolBar->Bind(wxEVT_TOOL, &PaneSymbols::OnSave, this, 502);

    m_list->UpdateList();
}

void PaneSymbols::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    m_list->UpdateList();
}

void PaneSymbols::OnFilter(wxCommandEvent& WXUNUSED(event)) {
    m_list->SetFilter(m_filter->GetValue());
}

void PaneSymbols::OnLoad(wxCommandEvent& WXUNUSED(event)) {
    wxFileDialog dlg(this, "Load Symbols", "", "", "Symbol files (*.sym)|*.sym|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        sim_sym_load_file(m_sim, dlg.GetPath().mb_str());
        m_list->UpdateList();
    }
}

void PaneSymbols::OnSave(wxCommandEvent& WXUNUSED(event)) {
    wxFileDialog dlg(this, "Save Symbols", "", "", "Symbol files (*.sym)|*.sym|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        sim_sym_save_file(m_sim, dlg.GetPath().mb_str());
    }
}
