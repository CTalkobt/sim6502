#include "pane_iref.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

PaneIRef::PaneIRef(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    // Filter toolbar
    wxBoxSizer* filterSizer = new wxBoxSizer(wxHORIZONTAL);
    filterSizer->Add(new wxStaticText(this, wxID_ANY, " Filter: "), 0, wxALIGN_CENTER_VERTICAL);
    m_filter = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0);
    filterSizer->Add(m_filter, 1, wxEXPAND | wxALL, 2);
    mainSizer->Add(filterSizer, 0, wxEXPAND);

    m_splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    
    m_list = new wxListCtrl(m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, "Mnemonic", wxLIST_FORMAT_LEFT, 80);
    m_list->InsertColumn(1, "Mode", wxLIST_FORMAT_LEFT, 150);
    m_list->InsertColumn(2, "Op", wxLIST_FORMAT_LEFT, 40);
    m_list->InsertColumn(3, "Cyc", wxLIST_FORMAT_LEFT, 40);

    m_detail = new wxTextCtrl(m_splitter, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);

    m_splitter->SplitHorizontally(m_list, m_detail, 300);
    m_splitter->SetSashGravity(0.5);
    m_splitter->SetMinimumPaneSize(50);

    mainSizer->Add(m_splitter, 1, wxEXPAND);
    SetSizer(mainSizer);

    m_filter->Bind(wxEVT_TEXT, &PaneIRef::OnFilter, this);
    m_list->Bind(wxEVT_LIST_ITEM_SELECTED, &PaneIRef::OnItemSelected, this);

    PopulateList();
}

void PaneIRef::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
}

void PaneIRef::PopulateList() {
    m_list->DeleteAllItems();
    wxString filter = m_filter->GetValue().Upper();
    
    int count = sim_opcode_count(m_sim);
    int row = 0;
    for (int i = 0; i < count; i++) {
        sim_opcode_info_t info;
        if (sim_opcode_get(m_sim, i, &info) == 0) {
            wxString mnem(info.mnemonic);
            if (!filter.IsEmpty() && !mnem.Upper().Contains(filter)) continue;

            long item = m_list->InsertItem(row, mnem);
            m_list->SetItem(item, 1, sim_mode_name(info.mode));
            m_list->SetItem(item, 2, wxString::Format("%02X", info.opcode_bytes[0]));
            m_list->SetItem(item, 3, wxString::Format("%d", info.cycles));
            m_list->SetItemData(item, (long)i);
            row++;
        }
    }
}

void PaneIRef::OnFilter(wxCommandEvent& WXUNUSED(event)) {
    PopulateList();
}

void PaneIRef::OnItemSelected(wxListEvent& event) {
    int idx = (int)event.GetData();
    sim_opcode_info_t info;
    if (sim_opcode_get(m_sim, idx, &info) == 0) {
        wxString text;
        text << "Instruction: " << info.mnemonic << "\n";
        text << "Addressing Mode: " << sim_mode_name(info.mode) << "\n";
        text << "Opcode: $" << wxString::Format("%02X", info.opcode_bytes[0]) << "\n";
        text << "Length: " << (int)info.instr_bytes << " bytes\n";
        text << "Cycles: " << info.cycles << "\n";
        
        text << "\nDescription placeholder for " << info.mnemonic << ".\n";
        
        m_detail->SetValue(text);
    }
}
