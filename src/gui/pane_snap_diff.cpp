#include "pane_snap_diff.h"
#include "main_frame.h"
#include <wx/toolbar.h>
#include <wx/artprov.h>

#include <wx/stattext.h>

wxString VirtualDiffList::OnGetItemText(long item, long column) const {
    if (item < 0 || item >= (long)m_data.size()) return "";
    const auto& d = m_data[item];
    switch (column) {
        case 0: return wxString::Format("%04X", d.addr);
        case 1: return wxString::Format("%02X", d.before);
        case 2: return wxString::Format("%02X", d.after);
        case 3: return wxString::Format("%04X", d.writer_pc);
    }
    return "";
}

PaneSnapDiff::PaneSnapDiff(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim), m_last_snap_cycles(0), m_last_snap_timestamp(0), m_last_cpu_cycles(0), m_last_count(-1)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddTool(401, "Take Snapshot", wxArtProvider::GetBitmap(wxART_FILE_SAVE), "Take a memory baseline to track subsequent modifications");
    toolBar->AddTool(402, "Clear Snapshot", wxArtProvider::GetBitmap(wxART_DELETE), "Clear the memory baseline and the list of differences");
    toolBar->Realize();
    sizer->Add(toolBar, 0, wxEXPAND);

    m_status_text = new wxStaticText(this, wxID_ANY, "No snapshot active.");
    m_status_text->SetForegroundColour(*wxBLACK);
    sizer->Add(m_status_text, 0, wxEXPAND | wxALL, 5);

    m_list = new VirtualDiffList(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_VIRTUAL);
    m_list->InsertColumn(0, "Address", wxLIST_FORMAT_LEFT, 80);
    m_list->InsertColumn(1, "Before", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(2, "After", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(3, "Writer PC", wxLIST_FORMAT_LEFT, 80);
    m_list->SetItemCount(0);

    sizer->Add(m_list, 1, wxEXPAND);
    SetSizer(sizer);

    toolBar->Bind(wxEVT_TOOL, &PaneSnapDiff::OnTakeSnapshot, this, 401);
    toolBar->Bind(wxEVT_TOOL, &PaneSnapDiff::OnClear, this, 402);
    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, &PaneSnapDiff::OnItemActivated, this);
}

void PaneSnapDiff::RefreshPane(const SimSnapshot &snap) {
    if (!m_sim) return;
    if (!m_list || !m_status_text) return;

    if (!sim_snapshot_valid(m_sim)) {
        if (m_last_count != -1) {
            m_list->SetDiffData({});
            m_status_text->SetLabel("No snapshot active.");
            m_last_count = -1;
        }
        return;
    }

    uint64_t snap_cycles = sim_snapshot_cycles(m_sim);
    uint32_t snap_ts = sim_snapshot_timestamp(m_sim);

    sim_diff_entry_t diffs[256];
    int count = sim_snapshot_diff(m_sim, diffs, 256);
    if (count < 0) return;

    uint64_t current_cpu_cycles = (snap.cpu ? snap.cpu->cycles : 0);

    if (count == m_last_count && snap_cycles == m_last_snap_cycles && snap_ts == m_last_snap_timestamp && current_cpu_cycles == m_last_cpu_cycles) {
        return;
    }

    m_last_count = count;
    m_last_snap_cycles = snap_cycles;
    m_last_snap_timestamp = snap_ts;
    m_last_cpu_cycles = current_cpu_cycles;

    // Formatting timestamp: snapshot ts is in milliseconds
    uint32_t sec = (snap_ts / 1000) % 60;
    uint32_t min = (snap_ts / 60000) % 60;
    uint32_t hour = (snap_ts / 3600000) % 24;

    wxString status;
    if (count > 256) {
        status = wxString::Format("Snapshot @ cycle %lu / %02u:%02u:%02u -- Showing 256 of %d changes (capped)",
                                  (unsigned long)snap_cycles, hour, min, sec, count);
        m_status_text->SetForegroundColour(*wxRED);
    } else {
        status = wxString::Format("Snapshot @ cycle %lu / %02u:%02u:%02u -- %d changes",
                                  (unsigned long)snap_cycles, hour, min, sec, count);
        m_status_text->SetForegroundColour(*wxBLACK);
    }
    m_status_text->SetLabel(status);

    int display_count = count > 256 ? 256 : count;
    std::vector<sim_diff_entry_t> data;
    data.reserve((size_t)display_count);
    for (int i = 0; i < display_count; i++) data.push_back(diffs[i]);
    m_list->SetDiffData(data);
}

void PaneSnapDiff::OnTakeSnapshot(wxCommandEvent& WXUNUSED(event)) {
    sim_snapshot_take(m_sim);
    RefreshPane(SimSnapshot{sim_get_cpu(m_sim), sim_get_memory(m_sim), sim_get_state(m_sim), false});
}

void PaneSnapDiff::OnClear(wxCommandEvent& WXUNUSED(event)) {
    sim_snapshot_clear(m_sim);
    RefreshPane(SimSnapshot{sim_get_cpu(m_sim), sim_get_memory(m_sim), sim_get_state(m_sim), false});
}

void PaneSnapDiff::OnItemActivated(wxListEvent& event) {
    long index = event.GetIndex();
    const auto& data = m_list->GetDiffData();
    if (index < 0 || index >= (long)data.size()) return;
    
    uint16_t addr = data[index].writer_pc;
    MainFrame* frame = dynamic_cast<MainFrame*>(wxGetTopLevelParent(this));
    if (frame) {
        frame->NavigateDisassembly(addr);
    }
}
