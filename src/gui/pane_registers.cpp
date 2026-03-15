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
    m_editRow = -1;
    m_textEditor = nullptr;
    memset(&m_prev_cpu, 0, sizeof(m_prev_cpu));

    for (int i = 0; i < 8; i++) {
        m_list->InsertItem(i, "");
    }

    m_list->Bind(wxEVT_LEFT_DOWN, &PaneRegisters::OnLeftClick, this);
    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, &PaneRegisters::OnEditRegister, this);
}

void PaneRegisters::RefreshPane(const SimSnapshot &snap) {
    if (!snap.cpu) return;

    if (m_editRow != 0) UpdateRow(0, "PC", snap.cpu->pc, m_prev_cpu.pc, true);
    if (m_editRow != 1) UpdateRow(1, "A",  snap.cpu->a,  m_prev_cpu.a);
    if (m_editRow != 2) UpdateRow(2, "X",  snap.cpu->x,  m_prev_cpu.x);
    if (m_editRow != 3) UpdateRow(3, "Y",  snap.cpu->y,  m_prev_cpu.y);
    if (m_editRow != 4) UpdateRow(4, "Z",  snap.cpu->z,  m_prev_cpu.z);
    if (m_editRow != 5) UpdateRow(5, "B",  snap.cpu->b,  m_prev_cpu.b);
    if (m_editRow != 6) UpdateRow(6, "SP", snap.cpu->s,  m_prev_cpu.s, true);
    
    if (m_editRow != 7) {
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
    }

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
    m_editRow = (int)itemIndex;
    
    wxRect rect;
    m_list->GetSubItemRect(itemIndex, 1, rect);

    if (!m_textEditor) {
        m_textEditor = new wxTextCtrl(m_list, wxID_ANY, "", rect.GetPosition(), rect.GetSize(), wxTE_PROCESS_ENTER | wxNO_BORDER);
        m_textEditor->Bind(wxEVT_TEXT_ENTER, &PaneRegisters::OnEditorEnter, this);
        m_textEditor->Bind(wxEVT_KILL_FOCUS, &PaneRegisters::OnEditorKillFocus, this);
    } else {
        m_textEditor->SetSize(rect);
    }

    wxString currentVal = m_list->GetItemText(itemIndex, 1);
    m_textEditor->SetValue(currentVal);
    m_textEditor->Show();
    m_textEditor->SetFocus();
    m_textEditor->SelectAll();
}

void PaneRegisters::OnLeftClick(wxMouseEvent& event) {
    int flags = 0;
    long subitem = -1;
    long item = m_list->HitTest(event.GetPosition(), flags, &subitem);

    if (item != wxNOT_FOUND && subitem == 1) {
        m_editRow = (int)item;
        wxRect rect;
        m_list->GetSubItemRect(item, 1, rect);

        if (!m_textEditor) {
            m_textEditor = new wxTextCtrl(m_list, wxID_ANY, "", rect.GetPosition(), rect.GetSize(), wxTE_PROCESS_ENTER | wxNO_BORDER);
            m_textEditor->Bind(wxEVT_TEXT_ENTER, &PaneRegisters::OnEditorEnter, this);
            m_textEditor->Bind(wxEVT_KILL_FOCUS, &PaneRegisters::OnEditorKillFocus, this);
        } else {
            m_textEditor->SetSize(rect);
        }

        wxString currentVal = m_list->GetItemText(item, 1);
        m_textEditor->SetValue(currentVal);
        m_textEditor->Show();
        m_textEditor->SetFocus();
        m_textEditor->SelectAll();
    } else {
        HideEditor();
        event.Skip();
    }
}

void PaneRegisters::OnEditorEnter(wxCommandEvent& WXUNUSED(event)) {
    if (m_editRow != -1) {
        CommitEdit(m_editRow, m_textEditor->GetValue());
    }
    HideEditor();
}

void PaneRegisters::OnEditorKillFocus(wxFocusEvent& WXUNUSED(event)) {
    if (m_editRow != -1) {
        CommitEdit(m_editRow, m_textEditor->GetValue());
    }
    HideEditor();
}

void PaneRegisters::HideEditor() {
    if (m_textEditor && m_textEditor->IsShown()) {
        m_textEditor->Hide();
    }
    m_editRow = -1;
}

void PaneRegisters::CommitEdit(int row, const wxString& newValue) {
    wxString regName = m_list->GetItemText(row, 0);
    unsigned long val;
    if (newValue.ToULong(&val, 16)) {
        if (regName == "PC") sim_set_pc(m_sim, (uint16_t)val);
        else if (regName == "SP") sim_set_reg_value(m_sim, "S", (uint16_t)val);
        else sim_set_reg_value(m_sim, regName.ToStdString().c_str(), (uint16_t)val);
    }
    m_editRow = -1;
}

