#include "pane_registers.h"
#include <wx/textctrl.h>
#include <wx/msgdlg.h>

PaneRegisters::PaneRegisters(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, "Register", wxLIST_FORMAT_LEFT, 80);
    m_list->InsertColumn(1, "Value", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(2, "Prev", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(3, "Flags / Bits", wxLIST_FORMAT_LEFT, 150);

    sizer->Add(m_list, 1, wxEXPAND);
    SetSizer(sizer);

    m_prev_valid = false;
    memset(&m_prev_cpu, 0, sizeof(m_prev_cpu));

    for (int i = 0; i < 8; i++) {
        m_list->InsertItem(i, "");
    }

    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, &PaneRegisters::OnEditRegister, this);
}

void PaneRegisters::RefreshPane(const SimSnapshot &snap) {
    if (!snap.cpu) return;

    UpdateRow(0, "PC", snap.cpu->pc, m_prev_cpu.pc, true);
    UpdateRow(1, "A",  snap.cpu->a,  m_prev_cpu.a);
    UpdateRow(2, "X",  snap.cpu->x,  m_prev_cpu.x);
    UpdateRow(3, "Y",  snap.cpu->y,  m_prev_cpu.y);
    UpdateRow(4, "Z",  snap.cpu->z,  m_prev_cpu.z);
    UpdateRow(5, "B",  snap.cpu->b,  m_prev_cpu.b);
    UpdateRow(6, "SP", snap.cpu->s,  m_prev_cpu.s, true);
    
    UpdateRow(7, "P", snap.cpu->p, m_prev_cpu.p);
    wxString flags;
    uint8_t p = snap.cpu->p;
    flags.Printf("%c%c%c%c%c%c%c%c (%02X)",
        (p & 0x80) ? 'N' : '.',
        (p & 0x40) ? 'V' : '.',
        (p & 0x20) ? 'U' : '.',
        (p & 0x10) ? 'B' : '.',
        (p & 0x08) ? 'D' : '.',
        (p & 0x04) ? 'I' : '.',
        (p & 0x02) ? 'Z' : '.',
        (p & 0x01) ? 'C' : '.',
        p);
    m_list->SetItem(7, 3, flags);

    m_prev_cpu = *(CPUState*)snap.cpu;
    m_prev_valid = true;
}

void PaneRegisters::UpdateRow(int row, const wxString& name, uint32_t val, uint32_t prev, bool is16) {
    m_list->SetItem(row, 0, name);
    wxString valStr = is16 ? wxString::Format("%04X", val) : wxString::Format("%02X", val);
    m_list->SetItem(row, 1, valStr);
    
    if (m_prev_valid) {
        wxString prevStr = is16 ? wxString::Format("%04X", prev) : wxString::Format("%02X", prev);
        m_list->SetItem(row, 2, prevStr);
        if (val != prev) {
            m_list->SetItemTextColour(row, *wxRED);
        } else {
            m_list->SetItemTextColour(row, m_list->GetForegroundColour());
        }
    }
}

void PaneRegisters::OnEditRegister(wxListEvent& event) {
    long itemIndex = event.GetIndex();
    wxString regName = m_list->GetItemText(itemIndex, 0);
    wxString currentVal = m_list->GetItemText(itemIndex, 1);

    wxTextEntryDialog dlg(this, "Enter new value for " + regName + " (hex):", "Edit Register", currentVal);
    if (dlg.ShowModal() == wxID_OK) {
        wxString newStr = dlg.GetValue();
        unsigned long val;
        if (newStr.ToULong(&val, 16)) {
            if (regName == "PC") sim_set_pc(m_sim, (uint16_t)val);
            else if (regName == "A") sim_set_reg_byte(m_sim, "A", (uint8_t)val);
            else if (regName == "X") sim_set_reg_byte(m_sim, "X", (uint8_t)val);
            else if (regName == "Y") sim_set_reg_byte(m_sim, "Y", (uint8_t)val);
            else if (regName == "Z") sim_set_reg_byte(m_sim, "Z", (uint8_t)val);
            else if (regName == "B") sim_set_reg_byte(m_sim, "B", (uint8_t)val);
            else if (regName == "SP") sim_set_reg_byte(m_sim, "S", (uint8_t)val);
            else if (regName == "P") sim_set_reg_byte(m_sim, "P", (uint8_t)val);
        }
    }
}
