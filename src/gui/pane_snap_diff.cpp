#include "pane_snap_diff.h"
#include <wx/toolbar.h>
#include <wx/artprov.h>

PaneSnapDiff::PaneSnapDiff(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddTool(401, "Take Snapshot", wxArtProvider::GetBitmap(wxART_FILE_SAVE));
    toolBar->AddTool(402, "Clear", wxArtProvider::GetBitmap(wxART_DELETE));
    toolBar->Realize();
    sizer->Add(toolBar, 0, wxEXPAND);

    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, "Address", wxLIST_FORMAT_LEFT, 80);
    m_list->InsertColumn(1, "Before", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(2, "After", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(3, "Writer PC", wxLIST_FORMAT_LEFT, 80);

    sizer->Add(m_list, 1, wxEXPAND);
    SetSizer(sizer);

    toolBar->Bind(wxEVT_TOOL, &PaneSnapDiff::OnTakeSnapshot, this, 401);
    toolBar->Bind(wxEVT_TOOL, &PaneSnapDiff::OnClear, this, 402);
}

void PaneSnapDiff::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    if (!sim_snapshot_valid(m_sim)) return;

    sim_diff_entry_t diffs[256];
    int count = sim_snapshot_diff(m_sim, diffs, 256);
    
    m_list->DeleteAllItems();
    for (int i = 0; i < count; i++) {
        long item = m_list->InsertItem(i, wxString::Format("%04X", diffs[i].addr));
        m_list->SetItem(item, 1, wxString::Format("%02X", diffs[i].before));
        m_list->SetItem(item, 2, wxString::Format("%02X", diffs[i].after));
        m_list->SetItem(item, 3, wxString::Format("%04X", diffs[i].writer_pc));
    }
}

void PaneSnapDiff::OnTakeSnapshot(wxCommandEvent& WXUNUSED(event)) {
    sim_snapshot_take(m_sim);
    RefreshPane(SimSnapshot{});
}

void PaneSnapDiff::OnClear(wxCommandEvent& WXUNUSED(event)) {
    m_list->DeleteAllItems();
}
