#include "pane_symbols.h"
#include "main_frame.h"
#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <wx/filedlg.h>
#include <wx/stattext.h>
#include <wx/msgdlg.h>
#include <wx/textdlg.h>
#include <algorithm>

SymbolsListCtrl::SymbolsListCtrl(wxWindow* parent, sim_session_t* sim)
    : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL | wxLC_EDIT_LABELS),
      m_sim(sim), m_sortColumn(0), m_sortAscending(true) 
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

void SymbolsListCtrl::SortByColumn(int column) {
    if (m_sortColumn == column) {
        m_sortAscending = !m_sortAscending;
    } else {
        m_sortColumn = column;
        m_sortAscending = true;
    }
    UpdateList();
}

void SymbolsListCtrl::UpdateList() {
    m_filteredIndices.clear();
    int total = sim_sym_count(m_sim);
    for (int i = 0; i < total; i++) {
        uint16_t addr;
        char name[128];
        int type;
        if (sim_sym_get_idx(m_sim, i, &addr, name, sizeof(name), &type, NULL, 0) == 1) {
            if (m_filter.IsEmpty() || wxString(name).Upper().Contains(m_filter)) {
                m_filteredIndices.push_back(i);
            }
        }
    }

    // Sorting
    std::sort(m_filteredIndices.begin(), m_filteredIndices.end(), [&](int a, int b) {
        uint16_t addr_a, addr_b;
        char name_a[128], name_b[128];
        int type_a, type_b;
        char comment_a[256], comment_b[256];
        
        sim_sym_get_idx(m_sim, a, &addr_a, name_a, sizeof(name_a), &type_a, comment_a, sizeof(comment_a));
        sim_sym_get_idx(m_sim, b, &addr_b, name_b, sizeof(name_b), &type_b, comment_b, sizeof(comment_b));

        int res = 0;
        switch (m_sortColumn) {
            case 0: res = wxString(name_a).CmpNoCase(name_b); break;
            case 1: res = (int)addr_a - (int)addr_b; break;
            case 2: res = wxString(sim_sym_type_name(type_a)).CmpNoCase(sim_sym_type_name(type_b)); break;
            case 3: res = wxString(comment_a).CmpNoCase(comment_b); break;
        }

        return m_sortAscending ? (res < 0) : (res > 0);
    });

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
    if (sim_sym_get_idx(m_sim, realIdx, &addr, name, sizeof(name), &type, comment, sizeof(comment)) != 1) return "";

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
    toolBar->AddTool(503, "Add", wxArtProvider::GetBitmap(wxART_NEW), "Add a new symbol");
    toolBar->AddTool(504, "Delete", wxArtProvider::GetBitmap(wxART_DELETE), "Delete the selected symbol");
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
    toolBar->Bind(wxEVT_TOOL, &PaneSymbols::OnAdd, this, 503);
    toolBar->Bind(wxEVT_TOOL, &PaneSymbols::OnDelete, this, 504);

    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, &PaneSymbols::OnItemActivated, this);
    m_list->Bind(wxEVT_LIST_COL_CLICK, &PaneSymbols::OnColumnClick, this);
    m_list->Bind(wxEVT_LIST_END_LABEL_EDIT, &PaneSymbols::OnEndLabelEdit, this);

    m_list->UpdateList();
}

void PaneSymbols::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    // We only refresh the list if the simulation is ready (just loaded)
    // or if we detect a change in symbol count (though this doesn't catch all changes).
    // For now, let's keep it simple and update on every refresh if shown.
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

void PaneSymbols::OnAdd(wxCommandEvent& WXUNUSED(event)) {
    wxTextEntryDialog nameDlg(this, "Enter symbol name:", "Add Symbol");
    if (nameDlg.ShowModal() != wxID_OK) return;
    wxString name = nameDlg.GetValue().Trim();
    if (name.IsEmpty()) return;

    wxTextEntryDialog addrDlg(this, "Enter address (hex):", "Add Symbol");
    if (addrDlg.ShowModal() != wxID_OK) return;
    unsigned long addr;
    if (!addrDlg.GetValue().ToULong(&addr, 16)) {
        wxMessageBox("Invalid hex address.", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    if (sim_sym_add(m_sim, (uint16_t)addr, name.mb_str(), "LABEL") == 0) {
        wxMessageBox("Failed to add symbol (duplicate name or table full).", "Error", wxOK | wxICON_ERROR, this);
    } else {
        m_list->UpdateList();
    }
}

void PaneSymbols::OnDelete(wxCommandEvent& WXUNUSED(event)) {
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;

    int realIdx = m_list->GetRealIndex(sel);
    if (realIdx == -1) return;

    uint16_t addr;
    char name[128];
    sim_sym_get_idx(m_sim, realIdx, &addr, name, sizeof(name), NULL, NULL, 0);

    if (wxMessageBox(wxString::Format("Delete symbol '%s' at $%04X?", name, addr), "Confirm Delete", wxYES_NO | wxICON_QUESTION, this) == wxYES) {
        sim_sym_remove_idx(m_sim, realIdx);
        m_list->UpdateList();
    }
}

void PaneSymbols::OnItemActivated(wxListEvent& event) {
    int realIdx = m_list->GetRealIndex(event.GetIndex());
    if (realIdx == -1) return;

    uint16_t addr;
    if (sim_sym_get_idx(m_sim, realIdx, &addr, NULL, 0, NULL, NULL, 0) == 1) {
        MainFrame* mf = dynamic_cast<MainFrame*>(wxGetTopLevelParent(this));
        if (mf) mf->NavigateDisassembly(addr);
    }
}

void PaneSymbols::OnColumnClick(wxListEvent& event) {
    m_list->SortByColumn(event.GetColumn());
}

void PaneSymbols::OnEndLabelEdit(wxListEvent& event) {
    if (event.IsEditCancelled()) return;

    int realIdx = m_list->GetRealIndex(event.GetIndex());
    if (realIdx == -1) return;

    wxString newName = event.GetLabel();
    newName.Trim();
    if (newName.IsEmpty()) {
        event.Veto();
        return;
    }

    if (sim_sym_rename(m_sim, realIdx, newName.mb_str()) == 0) {
        wxMessageBox("Failed to rename symbol (duplicate name).", "Error", wxOK | wxICON_ERROR, this);
        event.Veto();
    } else {
        // List will be refreshed by UpdateList which is called by RefreshPane timer
        // but we can call it now to show it immediately.
        m_list->UpdateList();
    }
}

