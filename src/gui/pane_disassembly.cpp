#include "pane_disassembly.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/artprov.h>

class DisasmListCtrl : public wxListCtrl {
public:
    DisasmListCtrl(wxWindow* parent, sim_session_t* sim, PaneDisassembly* owner)
        : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
          m_sim(sim), m_owner(owner), m_current_pc(0) {}

    void SetPC(uint16_t pc) {
        if (m_current_pc != pc) {
            m_current_pc = pc;
            Refresh();
        }
    }

    wxString OnGetItemText(long item, long column) const override {
        uint16_t addr = m_owner->GetAddressForRow((int)item);
        sim_disasm_entry_t entry;
        sim_disassemble_entry(m_sim, addr, &entry);

        switch (column) {
            case 0: return sim_has_breakpoint(m_sim, addr) ? "●" : "";
            case 1: return wxString::Format("%04X", addr);
            case 2: return wxString(entry.bytes);
            case 3: return wxString(entry.mnemonic);
            case 4: return wxString(entry.operand);
            case 5: return wxString::Format("%d", entry.cycles);
            case 6: {
                const char* sym = sim_sym_by_addr(m_sim, addr);
                return sym ? wxString(sym) : "";
            }
        }
        return "";
    }

    wxListItemAttr* OnGetItemAttr(long item) const override {
        uint16_t addr = m_owner->GetAddressForRow((int)item);
        if (addr == m_current_pc) {
            static wxListItemAttr pcAttr(*wxWHITE, *wxBLUE, wxNullFont);
            return &pcAttr;
        }
        return NULL;
    }

private:
    sim_session_t*   m_sim;
    PaneDisassembly* m_owner;
    uint16_t         m_current_pc;
};

PaneDisassembly::PaneDisassembly(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim), m_base_addr(0), m_followPC(true) 
{
    m_last_cpu_type = sim_get_cpu_type(m_sim);
    m_last_state = SIM_IDLE;
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    // Toolbar
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddTool(901, "Sync to PC", wxArtProvider::GetBitmap(wxART_GO_HOME), "Scroll disassembly to follow the current Program Counter");
    toolBar->AddSeparator();
    toolBar->AddTool(902, "Page Up", wxArtProvider::GetBitmap(wxART_GO_UP), "Scroll disassembly up by one page");
    toolBar->AddTool(903, "Page Down", wxArtProvider::GetBitmap(wxART_GO_DOWN), "Scroll disassembly down by one page");
    toolBar->AddSeparator();
    toolBar->AddControl(new wxStaticText(toolBar, wxID_ANY, " Address: $"));
    m_addrSearch = new wxTextCtrl(toolBar, wxID_ANY, "0000", wxDefaultPosition, wxSize(60, -1), wxTE_PROCESS_ENTER);
    toolBar->AddControl(m_addrSearch);
    toolBar->Realize();
    sizer->Add(toolBar, 0, wxEXPAND);

    m_list = new DisasmListCtrl(this, m_sim, this);
    m_list->InsertColumn(0, "BP", wxLIST_FORMAT_CENTER, 30);
    m_list->InsertColumn(1, "Addr", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(2, "Bytes", wxLIST_FORMAT_LEFT, 100);
    m_list->InsertColumn(3, "Mnem", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(4, "Operand", wxLIST_FORMAT_LEFT, 120);
    m_list->InsertColumn(5, "Cyc", wxLIST_FORMAT_LEFT, 40);
    m_list->InsertColumn(6, "Symbol", wxLIST_FORMAT_LEFT, 150);

    UpdateRowAddresses(0x0000);

    sizer->Add(m_list, 1, wxEXPAND);
    SetSizer(sizer);

    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, &PaneDisassembly::OnToggleBreakpoint, this);
    toolBar->Bind(wxEVT_TOOL, &PaneDisassembly::OnSyncToPC, this, 901);
    toolBar->Bind(wxEVT_TOOL, &PaneDisassembly::OnPrevPage, this, 902);
    toolBar->Bind(wxEVT_TOOL, &PaneDisassembly::OnNextPage, this, 903);
    m_addrSearch->Bind(wxEVT_TEXT_ENTER, &PaneDisassembly::OnGoToAddress, this);
}

void PaneDisassembly::UpdateRowAddresses(uint16_t start_addr) {
    m_base_addr = start_addr;
    m_row_addresses.clear();
    
    uint32_t addr = start_addr;
    // Disassemble up to 2000 instructions or until we hit the end of memory
    for (int i = 0; i < 2000 && addr < 0x10000; i++) {
        m_row_addresses.push_back((uint16_t)addr);
        sim_disasm_entry_t entry;
        sim_disassemble_entry(m_sim, (uint16_t)addr, &entry);
        addr += entry.size;
    }
    
    m_list->SetItemCount((long)m_row_addresses.size());
    m_list->Refresh();
}

uint16_t PaneDisassembly::GetAddressForRow(int row) const {
    if (row < 0 || row >= (int)m_row_addresses.size()) return 0;
    return m_row_addresses[row];
}

void PaneDisassembly::RefreshPane(const SimSnapshot &snap) {
    if (snap.cpu) {
        cpu_type_t current_cpu_type = sim_get_cpu_type(m_sim);
        if (current_cpu_type != m_last_cpu_type || (m_last_state == SIM_IDLE && snap.state == SIM_READY)) {
            m_last_cpu_type = current_cpu_type;
            m_last_state = snap.state;
            UpdateRowAddresses(snap.cpu->pc);
        }
        m_last_state = snap.state;

        m_list->SetPC(snap.cpu->pc);
        if (m_followPC) {
            ScrollTo(snap.cpu->pc);
        }
    }
}

void PaneDisassembly::ScrollTo(uint16_t addr) {
    // Check if addr is already in our current range
    int found_row = -1;
    for (size_t i = 0; i < m_row_addresses.size(); i++) {
        if (m_row_addresses[i] == addr) {
            found_row = (int)i;
            break;
        }
    }

    if (found_row != -1) {
        m_list->EnsureVisible(found_row);
    } else {
        // Not in range, re-center view around this address
        // Try to start a bit before the target address if possible, but 6502 is variable length
        // so we just start at the requested address for simplicity in this implementation.
        UpdateRowAddresses(addr);
        m_list->EnsureVisible(0);
    }

    // Refresh address search text
    if (!m_addrSearch->HasFocus()) {
        m_addrSearch->ChangeValue(wxString::Format("%04X", addr));
    }
}

void PaneDisassembly::OnToggleBreakpoint(wxListEvent& event) {
    uint16_t addr = GetAddressForRow((int)event.GetIndex());
    if (sim_has_breakpoint(m_sim, addr)) {
        sim_break_clear(m_sim, addr);
    } else {
        sim_break_set(m_sim, addr, NULL);
    }
    m_list->RefreshItem(event.GetIndex());
}

void PaneDisassembly::OnSyncToPC(wxCommandEvent& WXUNUSED(event)) {
    m_followPC = true;
    CPU* cpu = sim_get_cpu(m_sim);
    if (cpu) {
        ScrollTo(cpu->pc);
    }
}

void PaneDisassembly::OnGoToAddress(wxCommandEvent& WXUNUSED(event)) {
    wxString addrStr = m_addrSearch->GetValue();
    unsigned long addr;
    if (addrStr.ToULong(&addr, 16)) {
        m_followPC = false;
        ScrollTo((uint16_t)addr);
    }
}

void PaneDisassembly::OnPrevPage(wxCommandEvent& WXUNUSED(event)) {
    m_followPC = false;
    // For flow-based, page up means starting the base address earlier.
    // We'll just subtract some bytes and hope for the best, or better, subtract row count.
    int top = m_list->GetTopItem();
    if (top > 0) {
        m_list->EnsureVisible(0);
    } else {
        uint16_t new_base = (m_base_addr > 0x100) ? m_base_addr - 0x100 : 0;
        UpdateRowAddresses(new_base);
    }
}

void PaneDisassembly::OnNextPage(wxCommandEvent& WXUNUSED(event)) {
    m_followPC = false;
    int count = m_list->GetCountPerPage();
    int top = m_list->GetTopItem();
    int target = top + count;
    
    if (target < (int)m_row_addresses.size()) {
        m_list->EnsureVisible(target);
    } else {
        uint16_t last_addr = m_row_addresses.back();
        UpdateRowAddresses(last_addr);
    }
}

wxString PaneDisassembly::GetPaneTitle() const { return "Disassembly"; }
wxString PaneDisassembly::GetPaneName() const { return "Disassembly"; }
