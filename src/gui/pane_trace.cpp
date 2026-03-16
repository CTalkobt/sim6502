#include "pane_trace.h"
#include <wx/toolbar.h>
#include <wx/artprov.h>

TraceListCtrl::TraceListCtrl(wxWindow* parent, sim_session_t* sim)
    : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
      m_sim(sim) 
{
    InsertColumn(0, "Time", wxLIST_FORMAT_LEFT, 80);
    InsertColumn(1, "PC", wxLIST_FORMAT_LEFT, 50);
    InsertColumn(2, "OpCode", wxLIST_FORMAT_LEFT, 80);
    InsertColumn(3, "Instruction", wxLIST_FORMAT_LEFT, 120);
    InsertColumn(4, "A", wxLIST_FORMAT_LEFT, 30);
    InsertColumn(5, "X", wxLIST_FORMAT_LEFT, 30);
    InsertColumn(6, "Y", wxLIST_FORMAT_LEFT, 30);
    InsertColumn(7, "SP", wxLIST_FORMAT_LEFT, 40);
    InsertColumn(8, "P", wxLIST_FORMAT_LEFT, 30);
    InsertColumn(9, "ΔCyc", wxLIST_FORMAT_LEFT, 40);
    InsertColumn(10, "Total Cycles", wxLIST_FORMAT_LEFT, 100);
}

wxString TraceListCtrl::OnGetItemText(long item, long column) const {
    sim_trace_entry_t entry;
    if (sim_trace_get(m_sim, (int)item, &entry) == 0) return "";

    // For OpCode and Instruction, we use sim_disassemble_entry for a clean split
    sim_disasm_entry_t disasm;
    int res = sim_disassemble_entry(m_sim, entry.pc, &disasm);
    bool has_disasm = (res > 0);

    switch (column) {
        case 0: { // Time: mm:ss.sss
            uint32_t ms = entry.timestamp % 1000;
            uint32_t sec = (entry.timestamp / 1000) % 60;
            uint32_t min = (entry.timestamp / 60000);
            return wxString::Format("%02u:%02u.%03u", min, sec, ms);
        }
        case 1: return wxString::Format("%04X", entry.pc);
        case 2: return has_disasm ? wxString(disasm.bytes) : "";
        case 3: return has_disasm ? wxString::Format("%s %s", disasm.mnemonic, disasm.operand) : "";
        case 4: return wxString::Format("%02X", entry.cpu.a);
        case 5: return wxString::Format("%02X", entry.cpu.x);
        case 6: return wxString::Format("%02X", entry.cpu.y);
        case 7: return wxString::Format("%02X", (uint8_t)entry.cpu.s);
        case 8: return wxString::Format("%02X", entry.cpu.p);
        case 9: return wxString::Format("%d", entry.cycles_delta);
        case 10: return wxString::Format("%llu", (unsigned long long)entry.cpu.cycles);
    }
    return "";
}

PaneTrace::PaneTrace(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim), m_last_trace_total(0), m_last_cycles(0)
{
    m_enabled = true;
    sim_trace_enable(m_sim, 1);
    m_follow = true;

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddCheckTool(201, "Enable", wxArtProvider::GetBitmap(wxART_TICK_MARK), wxNullBitmap, "Enable Tracing");
    toolBar->AddTool(202, "Clear", wxArtProvider::GetBitmap(wxART_DELETE), "Clear Trace");
    toolBar->AddSeparator();
    toolBar->AddCheckTool(203, "Follow", wxArtProvider::GetBitmap(wxART_GO_DOWN), wxNullBitmap, "Auto-scroll to latest");
    toolBar->Realize();
    sizer->Add(toolBar, 0, wxEXPAND);

    m_traceList = new TraceListCtrl(this, m_sim);
    sizer->Add(m_traceList, 1, wxEXPAND);

    SetSizer(sizer);

    toolBar->Bind(wxEVT_TOOL, &PaneTrace::OnEnable, this, 201);
    toolBar->Bind(wxEVT_TOOL, &PaneTrace::OnClear, this, 202);
    toolBar->Bind(wxEVT_TOOL, &PaneTrace::OnToggleFollow, this, 203);
    
    toolBar->ToggleTool(201, m_enabled);
    toolBar->ToggleTool(203, m_follow);
}

void PaneTrace::RefreshPane(const SimSnapshot &snap) {
    // Sync enabled state in case it was changed elsewhere (though unlikely)
    m_enabled = sim_trace_is_enabled(m_sim);

    if (!m_enabled) {
        m_last_trace_total = sim_trace_total_count(m_sim);
        m_last_cycles = snap.cpu ? snap.cpu->cycles : 0;
        return;
    }

    int count = sim_trace_count(m_sim);
    uint64_t total = sim_trace_total_count(m_sim);

    // Always update item count and refresh to ensure virtual list is in sync
    if (m_traceList->GetItemCount() != count) {
        m_traceList->SetItemCount(count);
        m_traceList->Refresh();
    }
    
    if (m_follow && total != m_last_trace_total && count > 0) {
        m_traceList->EnsureVisible(count - 1);
    }
    
    m_last_trace_total = total;
    if (snap.cpu) m_last_cycles = snap.cpu->cycles;
}

void PaneTrace::OnClear(wxCommandEvent& WXUNUSED(event)) {
    sim_trace_clear(m_sim);
    m_traceList->SetItemCount(0);
    m_last_trace_total = 0;
}

void PaneTrace::OnEnable(wxCommandEvent& event) {
    m_enabled = event.IsChecked();
    sim_trace_enable(m_sim, m_enabled);
    
    // When re-enabling, reset the sync counter to catch current trace state
    m_last_trace_total = 0;
    
    // Force immediate refresh logic in next tick
    SimSnapshot snap;
    snap.cpu = sim_get_cpu(m_sim);
    snap.mem = sim_get_memory(m_sim);
    snap.state = sim_get_state(m_sim);
    RefreshPane(snap);
}

void PaneTrace::OnToggleFollow(wxCommandEvent& event) {
    m_follow = event.IsChecked();
}
