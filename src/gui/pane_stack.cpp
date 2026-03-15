#include "pane_stack.h"

PaneStack::PaneStack(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, "Depth", wxLIST_FORMAT_LEFT, 50);
    m_list->InsertColumn(1, "Addr", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(2, "Value", wxLIST_FORMAT_LEFT, 50);
    m_list->InsertColumn(3, "Symbol / Return", wxLIST_FORMAT_LEFT, 200);

    sizer->Add(m_list, 1, wxEXPAND);
    SetSizer(sizer);
}

void PaneStack::RefreshPane(const SimSnapshot &snap) {
    if (!snap.cpu) return;

    m_list->DeleteAllItems();
    
    uint16_t sp = snap.cpu->s;
    // SP points to next free location. 
    // Usually stack is $0100-$01FF. 
    // In 45GS02 it might be different, but sim_api handles read.
    
    int row = 0;
    for (uint16_t s = sp + 1; s <= 0xFF; s++) {
        uint16_t addr = 0x0100 | s;
        uint8_t val = sim_mem_read_byte(m_sim, addr);
        
        long item = m_list->InsertItem(row, wxString::Format("%d", row));
        m_list->SetItem(item, 1, wxString::Format("%04X", addr));
        m_list->SetItem(item, 2, wxString::Format("%02X", val));
        
        // If we have two bytes, check if it looks like a return address
        if (s < 0xFF) {
            uint8_t val_hi = sim_mem_read_byte(m_sim, 0x0100 | (s + 1));
            uint16_t ret_addr = (uint16_t)val | ((uint16_t)val_hi << 8);
            const char* sym = sim_sym_by_addr(m_sim, ret_addr + 1);
            if (sym) {
                m_list->SetItem(item, 3, wxString::Format("RTN to %04X (%s)", ret_addr + 1, sym));
            }
        }
        
        row++;
        if (row > 32) break; // Limit display
    }
}
