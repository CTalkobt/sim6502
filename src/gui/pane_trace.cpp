#include "pane_trace.h"
#include <wx/toolbar.h>
#include <wx/artprov.h>

TraceListCtrl::TraceListCtrl(wxWindow* parent, sim_session_t* sim)
    : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
      m_sim(sim) 
{
    InsertColumn(0, "#", wxLIST_FORMAT_LEFT, 50);
    InsertColumn(1, "PC", wxLIST_FORMAT_LEFT, 60);
    InsertColumn(2, "Instruction", wxLIST_FORMAT_LEFT, 150);
    InsertColumn(3, "A", wxLIST_FORMAT_LEFT, 30);
    InsertColumn(4, "X", wxLIST_FORMAT_LEFT, 30);
    InsertColumn(5, "Y", wxLIST_FORMAT_LEFT, 30);
    InsertColumn(6, "SP", wxLIST_FORMAT_LEFT, 40);
    InsertColumn(7, "P", wxLIST_FORMAT_LEFT, 30);
    InsertColumn(8, "CycΔ", wxLIST_FORMAT_LEFT, 50);
}

wxString TraceListCtrl::OnGetItemText(long item, long column) const {
    sim_trace_entry_t entry;
    if (sim_trace_get(m_sim, (int)item, &entry) != 0) return "";

    switch (column) {
        case 0: return wxString::Format("%ld", item);
        case 1: return wxString::Format("%04X", entry.pc);
        case 2: return wxString(entry.disasm);
        case 3: return wxString::Format("%02X", entry.cpu.a);
        case 4: return wxString::Format("%02X", entry.cpu.x);
        case 5: return wxString::Format("%02X", entry.cpu.y);
        case 6: return wxString::Format("%02X", (uint8_t)entry.cpu.s);
        case 7: return wxString::Format("%02X", entry.cpu.p);
        case 8: return wxString::Format("%d", entry.cycles_delta);
    }
    return "";
}

PaneTrace::PaneTrace(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddCheckTool(201, "Enable", wxArtProvider::GetBitmap(wxART_TICK_MARK));
    toolBar->AddTool(202, "Clear", wxArtProvider::GetBitmap(wxART_DELETE));
    toolBar->Realize();
    sizer->Add(toolBar, 0, wxEXPAND);

    m_notebook = new wxNotebook(this, wxID_ANY);
    
    m_liveLog = new wxTextCtrl(m_notebook, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    m_traceList = new TraceListCtrl(m_notebook, m_sim);

    m_notebook->AddPage(m_liveLog, "Live Log");
    m_notebook->AddPage(m_traceList, "Run Trace");

    sizer->Add(m_notebook, 1, wxEXPAND);
    SetSizer(sizer);

    toolBar->Bind(wxEVT_TOOL, &PaneTrace::OnEnable, this, 201);
    toolBar->Bind(wxEVT_TOOL, &PaneTrace::OnClear, this, 202);
    
    m_enabled = sim_trace_is_enabled(m_sim);
    toolBar->ToggleTool(201, m_enabled);
}

void PaneTrace::RefreshPane(const SimSnapshot &snap) {
    if (!m_enabled) return;

    int count = sim_trace_count(m_sim);
    if (count > 0) {
        m_traceList->SetItemCount(count);
        
        // Update live log with last entry if we just stepped
        if (snap.cpu) {
            sim_trace_entry_t entry;
            if (sim_trace_get(m_sim, count - 1, &entry) == 0) {
                m_liveLog->AppendText(wxString::Format("[%d] %04X: %s\n", entry.cycles_delta, entry.pc, entry.disasm));
            }
        }
    }
}

void PaneTrace::OnClear(wxCommandEvent& WXUNUSED(event)) {
    sim_trace_clear(m_sim);
    m_liveLog->Clear();
    m_traceList->SetItemCount(0);
}

void PaneTrace::OnEnable(wxCommandEvent& event) {
    m_enabled = event.IsChecked();
    sim_trace_enable(m_sim, m_enabled);
}
