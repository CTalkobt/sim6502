#include "pane_breakpoints.h"
#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <wx/textctrl.h>
#include <wx/msgdlg.h>

PaneBreakpoints::PaneBreakpoints(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddTool(101, "Add", wxArtProvider::GetBitmap(wxART_NEW), "Add a new breakpoint at a specific address");
    toolBar->AddTool(102, "Delete", wxArtProvider::GetBitmap(wxART_DELETE), "Delete the selected breakpoint");
    toolBar->AddTool(103, "Clear All", wxArtProvider::GetBitmap(wxART_CROSS_MARK), "Clear all breakpoints");
    toolBar->Realize();
    sizer->Add(toolBar, 0, wxEXPAND);

    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, "#", wxLIST_FORMAT_LEFT, 30);
    m_list->InsertColumn(1, "Address", wxLIST_FORMAT_LEFT, 80);
    m_list->InsertColumn(2, "Symbol", wxLIST_FORMAT_LEFT, 150);
    m_list->InsertColumn(3, "Type", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(4, "Condition", wxLIST_FORMAT_LEFT, 200);

    sizer->Add(m_list, 1, wxEXPAND);
    SetSizer(sizer);

    toolBar->Bind(wxEVT_TOOL, &PaneBreakpoints::OnAdd, this, 101);
    toolBar->Bind(wxEVT_TOOL, &PaneBreakpoints::OnDelete, this, 102);
    toolBar->Bind(wxEVT_TOOL, &PaneBreakpoints::OnClearAll, this, 103);
    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, &PaneBreakpoints::OnItemActivated, this);
}

void PaneBreakpoints::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    m_list->DeleteAllItems();
    int count = sim_break_count(m_sim);
    for (int i = 0; i < count; i++) {
        uint16_t addr;
        char cond[128];
        if (sim_break_get(m_sim, i, &addr, cond, sizeof(cond)) == 0) {
            long item = m_list->InsertItem(i, wxString::Format("%d", i));
            m_list->SetItem(item, 1, wxString::Format("%04X", addr));
            
            const char* sym = sim_sym_by_addr(m_sim, addr);
            m_list->SetItem(item, 2, sym ? wxString(sym) : "");
            m_list->SetItem(item, 3, "Exec");
            m_list->SetItem(item, 4, wxString(cond));
            
            if (!sim_break_is_enabled(m_sim, i)) {
                m_list->SetItemTextColour(item, *wxLIGHT_GREY);
            }
        }
    }
}

void PaneBreakpoints::OnAdd(wxCommandEvent& WXUNUSED(event)) {
    wxTextEntryDialog dlg(this, "Enter address (hex) or symbol:", "Add Breakpoint");
    if (dlg.ShowModal() == wxID_OK) {
        wxString val = dlg.GetValue();
        unsigned long addr;
        if (val.ToULong(&addr, 16)) {
            sim_break_set(m_sim, (uint16_t)addr, NULL);
            RefreshPane(SimSnapshot{}); // Dummy snap
        }
    }
}

void PaneBreakpoints::OnDelete(wxCommandEvent& WXUNUSED(event)) {
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel != -1) {
        uint16_t addr;
        char cond[128];
        if (sim_break_get(m_sim, (int)sel, &addr, cond, sizeof(cond)) == 0) {
            sim_break_clear(m_sim, addr);
            RefreshPane(SimSnapshot{});
        }
    }
}

void PaneBreakpoints::OnClearAll(wxCommandEvent& WXUNUSED(event)) {
    int count = sim_break_count(m_sim);
    for (int i = count - 1; i >= 0; i--) {
        uint16_t addr;
        char cond[128];
        if (sim_break_get(m_sim, i, &addr, cond, sizeof(cond)) == 0) {
            sim_break_clear(m_sim, addr);
        }
    }
    RefreshPane(SimSnapshot{});
}

void PaneBreakpoints::OnItemActivated(wxListEvent& event) {
    int idx = (int)event.GetIndex();
    sim_break_toggle(m_sim, idx);
    RefreshPane(SimSnapshot{});
}
