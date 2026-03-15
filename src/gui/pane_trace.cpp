#include "pane_trace.h"
#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>
#include <wx/filedlg.h>
#include <wx/wfstream.h>
#include <wx/txtstrm.h>

TraceListCtrl::TraceListCtrl(wxWindow* parent, sim_session_t* sim)
    : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
      m_sim(sim), m_hasExtendedRegs(false)
{
    UpdateColumns();
}

void TraceListCtrl::UpdateColumns() {
    DeleteAllColumns();
    
    cpu_type_t type = sim_get_cpu_type(m_sim);
    m_hasExtendedRegs = (type == CPU_45GS02 || type == CPU_65CE02);

    int col = 0;
    InsertColumn(col++, "Time", wxLIST_FORMAT_LEFT, 80);
    InsertColumn(col++, "PC", wxLIST_FORMAT_LEFT, 50);
    InsertColumn(col++, "OpCode", wxLIST_FORMAT_LEFT, 80);
    InsertColumn(col++, "Instruction", wxLIST_FORMAT_LEFT, 120);
    InsertColumn(col++, "A", wxLIST_FORMAT_LEFT, 30);
    InsertColumn(col++, "X", wxLIST_FORMAT_LEFT, 30);
    InsertColumn(col++, "Y", wxLIST_FORMAT_LEFT, 30);
    if (m_hasExtendedRegs) {
        InsertColumn(col++, "Z", wxLIST_FORMAT_LEFT, 30);
        InsertColumn(col++, "B", wxLIST_FORMAT_LEFT, 30);
    }
    InsertColumn(col++, "SP", wxLIST_FORMAT_LEFT, 40);
    InsertColumn(col++, "P", wxLIST_FORMAT_LEFT, 30);
    InsertColumn(col++, "ΔCyc", wxLIST_FORMAT_LEFT, 40);
    InsertColumn(col++, "Total Cycles", wxLIST_FORMAT_LEFT, 100);
}

void TraceListCtrl::SetFilteredIndices(const std::vector<int>& indices) {
    m_filteredIndices = indices;
    SetItemCount((long)m_filteredIndices.empty() ? sim_trace_count(m_sim) : (long)m_filteredIndices.size());
}

wxString TraceListCtrl::OnGetItemText(long item, long column) const {
    int slot = m_filteredIndices.empty() ? (int)item : m_filteredIndices[item];
    sim_trace_entry_t entry;
    if (sim_trace_get(m_sim, slot, &entry) == 0) return "";

    // For OpCode and Instruction, we use sim_disassemble_entry for a clean split
    sim_disasm_entry_t disasm;
    int res = sim_disassemble_entry(m_sim, entry.pc, &disasm);
    bool has_disasm = (res > 0);

    // Adjust column indices if Z/B are shown
    int col_sp = m_hasExtendedRegs ? 9 : 7;
    int col_p = m_hasExtendedRegs ? 10 : 8;
    int col_dcyc = m_hasExtendedRegs ? 11 : 9;
    int col_tcyc = m_hasExtendedRegs ? 12 : 10;

    if (column == 0) {
        uint32_t ms = entry.timestamp % 1000;
        uint32_t sec = (entry.timestamp / 1000) % 60;
        uint32_t min = (entry.timestamp / 60000);
        return wxString::Format("%02u:%02u.%03u", min, sec, ms);
    }
    if (column == 1) return wxString::Format("%04X", entry.pc);
    if (column == 2) return has_disasm ? wxString(disasm.bytes) : "";
    if (column == 3) return has_disasm ? wxString::Format("%s %s", disasm.mnemonic, disasm.operand) : "";
    if (column == 4) return wxString::Format("%02X", entry.cpu.a);
    if (column == 5) return wxString::Format("%02X", entry.cpu.x);
    if (column == 6) return wxString::Format("%02X", entry.cpu.y);
    
    int cur_col = 7;
    if (m_hasExtendedRegs) {
        if (column == cur_col++) return wxString::Format("%02X", entry.cpu.z);
        if (column == cur_col++) return wxString::Format("%02X", entry.cpu.b);
    }
    
    if (column == col_sp) return wxString::Format("%02X", (uint8_t)entry.cpu.s);
    if (column == col_p) return wxString::Format("%02X", entry.cpu.p);
    if (column == col_dcyc) return wxString::Format("%d", entry.cycles_delta);
    if (column == col_tcyc) return wxString::Format("%llu", (unsigned long long)entry.cpu.cycles);

    return "";
}

PaneTrace::PaneTrace(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim), m_last_trace_total(0), m_last_cycles(0)
{
    m_enabled = true;
    sim_trace_enable(m_sim, 1);
    m_follow = true;
    m_last_cpu_type = sim_get_cpu_type(m_sim);

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddCheckTool(201, "Enable", wxArtProvider::GetBitmap(wxART_TICK_MARK), wxNullBitmap, "Enable Tracing");
    toolBar->AddTool(202, "Clear", wxArtProvider::GetBitmap(wxART_DELETE), "Clear Trace");
    toolBar->AddSeparator();
    toolBar->AddCheckTool(203, "Follow", wxArtProvider::GetBitmap(wxART_GO_DOWN), wxNullBitmap, "Auto-scroll to latest");
    toolBar->AddSeparator();
    toolBar->AddTool(205, "Export", wxArtProvider::GetBitmap(wxART_FILE_SAVE), "Export to CSV");
    toolBar->Realize();
    sizer->Add(toolBar, 0, wxEXPAND);

    wxBoxSizer* filterSizer = new wxBoxSizer(wxHORIZONTAL);
    filterSizer->Add(new wxStaticText(this, wxID_ANY, " Filter: "), 0, wxALIGN_CENTER_VERTICAL);
    m_filterText = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    filterSizer->Add(m_filterText, 1, wxEXPAND | wxALL, 2);
    m_statusText = new wxStaticText(this, wxID_ANY, "");
    filterSizer->Add(m_statusText, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    sizer->Add(filterSizer, 0, wxEXPAND);

    m_traceList = new TraceListCtrl(this, m_sim);
    sizer->Add(m_traceList, 1, wxEXPAND);

    SetSizer(sizer);

    toolBar->Bind(wxEVT_TOOL, &PaneTrace::OnEnable, this, 201);
    toolBar->Bind(wxEVT_TOOL, &PaneTrace::OnClear, this, 202);
    toolBar->Bind(wxEVT_TOOL, &PaneTrace::OnToggleFollow, this, 203);
    toolBar->Bind(wxEVT_TOOL, &PaneTrace::OnExport, this, 205);

    m_filterText->Bind(wxEVT_TEXT, &PaneTrace::OnFilterChanged, this);
    m_traceList->Bind(wxEVT_SCROLLWIN_THUMBTRACK, &PaneTrace::OnListScroll, this);
    m_traceList->Bind(wxEVT_MOUSEWHEEL, [this](wxMouseEvent& event) {
        if (m_follow && event.GetWheelRotation() > 0) {
            m_follow = false;
            // Get the toolbar and untoggle the tool
            wxToolBar* tb = dynamic_cast<wxToolBar*>(GetSizer()->GetItem((size_t)0)->GetWindow());
            if (tb) tb->ToggleTool(203, false);
        }
        event.Skip();
    });
    
    toolBar->ToggleTool(201, m_enabled);
    toolBar->ToggleTool(203, m_follow);
}

void PaneTrace::RefreshPane(const SimSnapshot &snap) {
    cpu_type_t current_cpu_type = sim_get_cpu_type(m_sim);
    if (current_cpu_type != m_last_cpu_type) {
        m_last_cpu_type = current_cpu_type;
        m_traceList->UpdateColumns();
    }

    m_enabled = sim_trace_is_enabled(m_sim);

    if (!m_enabled) {
        m_last_trace_total = sim_trace_total_count(m_sim);
        m_last_cycles = snap.cpu ? snap.cpu->cycles : 0;
        return;
    }

    int count = sim_trace_count(m_sim);
    uint64_t total = sim_trace_total_count(m_sim);

    // Overflow warning
    if (total > (uint64_t)count) {
        m_statusText->SetLabelText(wxString::Format("Buffer full - %llu dropped", (unsigned long long)(total - count)));
        m_statusText->SetForegroundColour(*wxRED);
    } else {
        m_statusText->SetLabelText("");
    }

    if (total != m_last_trace_total) {
        UpdateFilter();
    }
    
    if (m_follow && total != m_last_trace_total && count > 0) {
        m_traceList->EnsureVisible(m_traceList->GetItemCount() - 1);
    }
    
    m_last_trace_total = total;
    if (snap.cpu) m_last_cycles = snap.cpu->cycles;
}

void PaneTrace::OnClear(wxCommandEvent& WXUNUSED(event)) {
    sim_trace_clear(m_sim);
    m_traceList->SetFilteredIndices({});
    m_last_trace_total = 0;
}

void PaneTrace::OnEnable(wxCommandEvent& event) {
    m_enabled = event.IsChecked();
    sim_trace_enable(m_sim, m_enabled);
    m_last_trace_total = 0;
}

void PaneTrace::OnToggleFollow(wxCommandEvent& event) {
    m_follow = event.IsChecked();
}

void PaneTrace::OnExport(wxCommandEvent& WXUNUSED(event)) {
    wxFileDialog saveFileDialog(this, "Export Trace to CSV", "", "trace.csv", "CSV files (*.csv)|*.csv", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveFileDialog.ShowModal() == wxID_CANCEL) return;

    wxFileOutputStream output(saveFileDialog.GetPath());
    if (!output.IsOk()) return;

    wxTextOutputStream out(output);
    
    // Header
    bool has_ext = (m_last_cpu_type == CPU_45GS02 || m_last_cpu_type == CPU_65CE02);
    out << "Time,PC,OpCode,Instruction,A,X,Y";
    if (has_ext) out << ",Z,B";
    out << ",SP,P,DeltaCyc,TotalCycles\n";

    int count = sim_trace_count(m_sim);
    for (int i = 0; i < count; ++i) {
        sim_trace_entry_t entry;
        if (sim_trace_get(m_sim, i, &entry) == 0) continue;

        sim_disasm_entry_t disasm;
        sim_disassemble_entry(m_sim, entry.pc, &disasm);

        uint32_t ms = entry.timestamp % 1000;
        uint32_t sec = (entry.timestamp / 1000) % 60;
        uint32_t min = (entry.timestamp / 60000);
        out << wxString::Format("%02u:%02u.%03u", min, sec, ms);

        out << "," << wxString::Format("%04X", entry.pc);
        out << "," << disasm.bytes;
        out << "," << disasm.mnemonic << " " << disasm.operand;
        out << "," << wxString::Format("%02X", entry.cpu.a);
        out << "," << wxString::Format("%02X", entry.cpu.x);
        out << "," << wxString::Format("%02X", entry.cpu.y);
        if (has_ext) {
            out << "," << wxString::Format("%02X", entry.cpu.z);
            out << "," << wxString::Format("%02X", entry.cpu.b);
        }
        out << "," << wxString::Format("%02X", (uint8_t)entry.cpu.s);
        out << "," << wxString::Format("%02X", (uint8_t)entry.cpu.p);
        out << "," << entry.cycles_delta;
        out << "," << wxString::Format("%llu", (unsigned long long)entry.cpu.cycles);
        out << "\n";
    }
}

void PaneTrace::OnFilterChanged(wxCommandEvent& WXUNUSED(event)) {
    UpdateFilter();
}

void PaneTrace::UpdateFilter() {
    wxString filter = m_filterText->GetValue().Upper();
    if (filter.IsEmpty()) {
        m_traceList->SetFilteredIndices({});
        return;
    }

    std::vector<int> indices;
    int count = sim_trace_count(m_sim);

    // Range check: 0200-0300
    bool isRange = false;
    unsigned long startAddr = 0, endAddr = 0;
    if (filter.Contains("-")) {
        wxString startStr = filter.BeforeFirst('-');
        wxString endStr = filter.AfterFirst('-');
        if (startStr.ToULong(&startAddr, 16) && endStr.ToULong(&endAddr, 16)) {
            isRange = true;
        }
    }

    for (int i = 0; i < count; ++i) {
        sim_trace_entry_t entry;
        if (sim_trace_get(m_sim, i, &entry) == 0) continue;

        bool match = false;
        if (isRange) {
            if (entry.pc >= startAddr && entry.pc <= endAddr) match = true;
        } else {
            // Search PC
            if (wxString::Format("%04X", entry.pc).Contains(filter)) match = true;
            
            if (!match) {
                sim_disasm_entry_t disasm;
                if (sim_disassemble_entry(m_sim, entry.pc, &disasm) > 0) {
                    if (wxString(disasm.mnemonic).Upper().Contains(filter)) match = true;
                    if (wxString(disasm.operand).Upper().Contains(filter)) match = true;
                }
            }
        }

        if (match) indices.push_back(i);
    }
    m_traceList->SetFilteredIndices(indices);
}

void PaneTrace::OnListScroll(wxScrollWinEvent& event) {
    if (event.GetOrientation() == wxVERTICAL) {
        int pos = event.GetPosition();
        int max = m_traceList->GetScrollRange(wxVERTICAL) - m_traceList->GetScrollThumb(wxVERTICAL);
        wxEventType type = event.GetEventType();
        
        bool atBottom = (pos >= max - 5 && max > 0);
        bool scrolledUp = (type == wxEVT_SCROLLWIN_LINEUP || type == wxEVT_SCROLLWIN_PAGEUP || 
                          ((type == wxEVT_SCROLLWIN_THUMBTRACK || type == wxEVT_SCROLLWIN_THUMBRELEASE) && pos < max - 5));

        if (m_follow && scrolledUp) {
            m_follow = false;
            wxToolBar* tb = dynamic_cast<wxToolBar*>(GetSizer()->GetItem((size_t)0)->GetWindow());
            if (tb) tb->ToggleTool(203, false);
        } else if (!m_follow && atBottom && type == wxEVT_SCROLLWIN_THUMBRELEASE) {
            m_follow = true;
            wxToolBar* tb = dynamic_cast<wxToolBar*>(GetSizer()->GetItem((size_t)0)->GetWindow());
            if (tb) tb->ToggleTool(203, true);
        }
    }
    event.Skip();
}
