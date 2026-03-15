#include "pane_disassembly.h"
#include <wx/textctrl.h>

DisasmListCtrl::DisasmListCtrl(wxWindow* parent, sim_session_t* sim)
    : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
      m_sim(sim), m_current_pc(0) {}

void DisasmListCtrl::SetPC(uint16_t pc) {
    if (m_current_pc != pc) {
        m_current_pc = pc;
        Refresh();
    }
}

wxString DisasmListCtrl::OnGetItemText(long item, long column) const {
    uint16_t addr = (uint16_t)item;
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

wxListItemAttr* DisasmListCtrl::OnGetItemAttr(long item) const {
    if (item == (long)m_current_pc) {
        static wxListItemAttr pcAttr(*wxWHITE, *wxBLUE, wxNullFont);
        return &pcAttr;
    }
    return NULL;
}

PaneDisassembly::PaneDisassembly(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    m_list = new DisasmListCtrl(this, m_sim);
    m_list->InsertColumn(0, "BP", wxLIST_FORMAT_CENTER, 30);
    m_list->InsertColumn(1, "Addr", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(2, "Bytes", wxLIST_FORMAT_LEFT, 100);
    m_list->InsertColumn(3, "Mnem", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(4, "Operand", wxLIST_FORMAT_LEFT, 120);
    m_list->InsertColumn(5, "Cyc", wxLIST_FORMAT_LEFT, 40);
    m_list->InsertColumn(6, "Symbol", wxLIST_FORMAT_LEFT, 150);

    m_list->SetItemCount(65536);

    sizer->Add(m_list, 1, wxEXPAND);
    SetSizer(sizer);

    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, &PaneDisassembly::OnToggleBreakpoint, this);
}

void PaneDisassembly::RefreshPane(const SimSnapshot &snap) {
    if (snap.cpu) {
        m_list->SetPC(snap.cpu->pc);
    }
}

void PaneDisassembly::OnToggleBreakpoint(wxListEvent& event) {
    uint16_t addr = (uint16_t)event.GetIndex();
    if (sim_has_breakpoint(m_sim, addr)) {
        sim_break_clear(m_sim, addr);
    } else {
        sim_break_set(m_sim, addr, NULL);
    }
    m_list->RefreshItem(event.GetIndex());
}
