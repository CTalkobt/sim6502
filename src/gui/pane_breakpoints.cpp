#include "pane_breakpoints.h"
#include "main_frame.h"
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
    m_list->EnableCheckBoxes();
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
    m_list->Bind(wxEVT_LIST_ITEM_SELECTED, &PaneBreakpoints::OnItemSelected, this);
    m_list->Bind(wxEVT_LIST_ITEM_CHECKED, &PaneBreakpoints::OnItemChecked, this);
    m_list->Bind(wxEVT_LIST_ITEM_UNCHECKED, &PaneBreakpoints::OnItemUnchecked, this);
}

void PaneBreakpoints::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    m_list->DeleteAllItems();
    int count = sim_break_count(m_sim);
    for (int i = 0; i < count; i++) {
        uint16_t addr;
        char cond[128] = {};
        if (sim_break_get(m_sim, i, &addr, cond, sizeof(cond)) != 0) {
            long item = m_list->InsertItem(i, wxString::Format("%d", i));
            bool enabled = sim_break_is_enabled(m_sim, i) != 0;
            m_list->CheckItem(item, enabled);
            m_list->SetItem(item, 1, wxString::Format("%04X", addr));

            const char* sym = sim_sym_by_addr(m_sim, addr);
            m_list->SetItem(item, 2, sym ? wxString(sym) : "");
            m_list->SetItem(item, 3, "Exec");
            m_list->SetItem(item, 4, wxString(cond));

            if (!enabled) {
                m_list->SetItemTextColour(item, *wxLIGHT_GREY);
            }
        }
    }
}

void PaneBreakpoints::OnAdd(wxCommandEvent& WXUNUSED(event)) {
    wxTextEntryDialog addrDlg(this, "Enter address (hex):", "Add Breakpoint");
    if (addrDlg.ShowModal() != wxID_OK)
        return;

    wxString val = addrDlg.GetValue();
    unsigned long addr;
    if (!val.ToULong(&addr, 16)) {
        wxMessageBox("Invalid hex address.", "Add Breakpoint", wxOK | wxICON_WARNING, this);
        return;
    }

    wxTextEntryDialog condDlg(this, "Enter condition (optional, e.g. \"A > 80\"):", "Add Breakpoint – Condition", "");
    condDlg.ShowModal(); // OK or Cancel both proceed; empty condition is fine

    wxString cond = condDlg.GetValue().Trim();
    std::string condStr = cond.ToStdString();
    sim_break_set(m_sim, (uint16_t)addr, condStr.empty() ? nullptr : condStr.c_str());
    RefreshPane(SimSnapshot{});
}

void PaneBreakpoints::OnDelete(wxCommandEvent& WXUNUSED(event)) {
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel != -1) {
        uint16_t addr;
        char cond[128] = {};
        if (sim_break_get(m_sim, (int)sel, &addr, cond, sizeof(cond)) != 0) {
            sim_break_clear(m_sim, addr);
            RefreshPane(SimSnapshot{});
        }
    }
}

void PaneBreakpoints::OnClearAll(wxCommandEvent& WXUNUSED(event)) {
    int count = sim_break_count(m_sim);
    if (count == 0)
        return;

    if (wxMessageBox(wxString::Format("Clear all %d breakpoint(s)?", count),
                     "Clear All Breakpoints", wxYES_NO | wxICON_QUESTION, this) != wxYES)
        return;

    for (int i = count - 1; i >= 0; i--) {
        uint16_t addr;
        char cond[128] = {};
        if (sim_break_get(m_sim, i, &addr, cond, sizeof(cond)) != 0) {
            sim_break_clear(m_sim, addr);
        }
    }
    RefreshPane(SimSnapshot{});
}

// Double-click: edit the condition for the selected breakpoint
void PaneBreakpoints::OnItemActivated(wxListEvent& event) {
    int idx = (int)event.GetIndex();
    uint16_t addr;
    char cond[128] = {};
    if (sim_break_get(m_sim, idx, &addr, cond, sizeof(cond)) == 0)
        return;

    wxTextEntryDialog dlg(this,
        wxString::Format("Condition for breakpoint at $%04X\n(leave empty for unconditional):", addr),
        "Edit Breakpoint Condition", wxString(cond));
    if (dlg.ShowModal() != wxID_OK)
        return;

    wxString newCond = dlg.GetValue().Trim();
    std::string newCondStr = newCond.ToStdString();
    int was_enabled = sim_break_is_enabled(m_sim, idx);
    sim_break_clear(m_sim, addr);
    sim_break_set(m_sim, addr, newCondStr.empty() ? nullptr : newCondStr.c_str());
    if (!was_enabled) {
        // re-disable: it was re-added at the end
        int new_idx = sim_break_count(m_sim) - 1;
        sim_break_toggle(m_sim, new_idx);
    }
    RefreshPane(SimSnapshot{});
}

// Single-click: navigate disassembly to the breakpoint address
void PaneBreakpoints::OnItemSelected(wxListEvent& event) {
    int idx = (int)event.GetIndex();
    uint16_t addr;
    char cond[128] = {};
    if (sim_break_get(m_sim, idx, &addr, cond, sizeof(cond)) == 0)
        return;

    MainFrame* mf = dynamic_cast<MainFrame*>(wxGetTopLevelParent(this));
    if (mf)
        mf->NavigateDisassembly(addr);
}

// Checkbox checked: ensure breakpoint is enabled
void PaneBreakpoints::OnItemChecked(wxListEvent& event) {
    int idx = (int)event.GetIndex();
    if (!sim_break_is_enabled(m_sim, idx))
        sim_break_toggle(m_sim, idx);
    // Update text colour without full rebuild
    m_list->SetItemTextColour(idx, wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT));
}

// Checkbox unchecked: ensure breakpoint is disabled
void PaneBreakpoints::OnItemUnchecked(wxListEvent& event) {
    int idx = (int)event.GetIndex();
    if (sim_break_is_enabled(m_sim, idx))
        sim_break_toggle(m_sim, idx);
    m_list->SetItemTextColour(idx, *wxLIGHT_GREY);
}
