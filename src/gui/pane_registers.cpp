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
    memset(&m_current_cpu, 0, sizeof(m_current_cpu));

    for (int i = 0; i < 8; i++) {
        m_list->InsertItem(i, "");
    }

    m_list->Bind(wxEVT_LEFT_DOWN, &PaneRegisters::OnLeftClick, this);
    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, &PaneRegisters::OnEditRegister, this);
}

void PaneRegisters::RefreshPane(const SimSnapshot &snap) {
    if (!snap.cpu) return;

    // Rotate states if cycle count has moved
    if (snap.cpu->cycles != m_current_cpu.cycles) {
        m_prev_cpu = m_current_cpu;
        m_current_cpu = *(CPUState*)snap.cpu;
        m_prev_valid = true;
    } else {
        // Even if cycles haven't changed, sync current state to reflect manual edits
        m_current_cpu = *(CPUState*)snap.cpu;
    }

    UpdateRow(0, "PC", m_current_cpu.pc, m_prev_cpu.pc, true);
    UpdateRow(1, "A",  m_current_cpu.a,  m_prev_cpu.a);
    UpdateRow(2, "X",  m_current_cpu.x,  m_prev_cpu.x);
    UpdateRow(3, "Y",  m_current_cpu.y,  m_prev_cpu.y);
    UpdateRow(4, "Z",  m_current_cpu.z,  m_prev_cpu.z);
    UpdateRow(5, "B",  m_current_cpu.b,  m_prev_cpu.b);
    UpdateRow(6, "SP", m_current_cpu.s,  m_prev_cpu.s, true);
    UpdateRow(7, "P",  m_current_cpu.p,  m_prev_cpu.p);
}

void PaneRegisters::UpdateRow(int row, const wxString& name, uint32_t val, uint32_t prev, bool is16) {
    m_list->SetItem(row, 0, name);
    
    // If this row is being edited, hide the underlying text to prevent overlay ghosting
    // and clear the 'Prev' and 'Flags' columns to focus on the new input.
    if (row == m_editRow) {
        m_list->SetItem(row, 1, "");
        m_list->SetItem(row, 2, "");
        if (row == 7) m_list->SetItem(row, 3, "");
    } else {
        wxString valStr = is16 ? wxString::Format("%04X", val) : wxString::Format("%02X", val);
        m_list->SetItem(row, 1, valStr);

        // Special handling for Processor Status flags column
        if (row == 7) {
            wxString flags;
            uint8_t p = (uint8_t)val;
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
    }
    
    if (m_prev_valid) {
        if (row != m_editRow) {
            wxString prevStr = is16 ? wxString::Format("%04X", prev) : wxString::Format("%02X", prev);
            m_list->SetItem(row, 2, prevStr);
            if (val != prev) {
                m_list->SetItemTextColour(row, *wxRED);
            } else {
                m_list->SetItemTextColour(row, m_list->GetForegroundColour());
            }
        }
    }
}

long PaneRegisters::GetColumnAt(const wxPoint& pos) {
    int flags = 0;
    long subitem = -1;
    m_list->HitTest(pos, flags, &subitem);
    
    // If HitTest failed or didn't return a subitem (common on GTK),
    // we determine it by iterating column widths.
    if (subitem < 0) {
        int x = pos.x;
        int totalWidth = 0;
        for (int i = 0; i < m_list->GetColumnCount(); i++) {
            int w = m_list->GetColumnWidth(i);
            if (x >= totalWidth && x < totalWidth + w) {
                return i;
            }
            totalWidth += w;
        }
    }
    return subitem;
}

void PaneRegisters::OpenEditorForRow(long row) {
    // Check if we are already editing this item to avoid redundant triggers
    if (m_editRow == (int)row && m_textEditor && m_textEditor->IsShown()) {
        return;
    }

    m_editRow = (int)row;
    wxRect rect;
    m_list->GetSubItemRect(row, 1, rect);

    if (!m_textEditor) {
        m_textEditor = new wxTextCtrl(m_list, wxID_ANY, "", rect.GetPosition(), rect.GetSize(), wxTE_PROCESS_ENTER | wxNO_BORDER);
        m_textEditor->Bind(wxEVT_TEXT_ENTER, &PaneRegisters::OnEditorEnter, this);
        m_textEditor->Bind(wxEVT_KILL_FOCUS, &PaneRegisters::OnEditorKillFocus, this);
    } else {
        m_textEditor->SetSize(rect);
    }

    wxString currentVal;
    switch (row) {
        case 0: currentVal.Printf("%04X", m_current_cpu.pc); break;
        case 1: currentVal.Printf("%02X", m_current_cpu.a);  break;
        case 2: currentVal.Printf("%02X", m_current_cpu.x);  break;
        case 3: currentVal.Printf("%02X", m_current_cpu.y);  break;
        case 4: currentVal.Printf("%02X", m_current_cpu.z);  break;
        case 5: currentVal.Printf("%02X", m_current_cpu.b);  break;
        case 6: currentVal.Printf("%04X", m_current_cpu.s);  break;
        case 7: currentVal.Printf("%02X", m_current_cpu.p);  break;
        default: break;
    }
    m_textEditor->SetValue(currentVal);
    m_textEditor->Show();
    m_textEditor->SetFocus();
    m_textEditor->SelectAll();
}

void PaneRegisters::OnEditRegister(wxListEvent& event) {
    long itemIndex = event.GetIndex();

    // Check if activation was via mouse (double-click) by verifying mouse position.
    // If it was a mouse event, ensure it occurred on column 1.
    wxPoint mousePos = m_list->ScreenToClient(wxGetMousePosition());
    wxSize clientSize = m_list->GetClientSize();
    
    if (mousePos.x >= 0 && mousePos.y >= 0 && mousePos.x < clientSize.x && mousePos.y < clientSize.y) {
        if (GetColumnAt(mousePos) != 1) {
            return;
        }
    }

    OpenEditorForRow(itemIndex);
}

void PaneRegisters::OnLeftClick(wxMouseEvent& event) {
    long subitem = GetColumnAt(event.GetPosition());
    int flags = 0;
    long item = m_list->HitTest(event.GetPosition(), flags);

    if (item != wxNOT_FOUND && subitem == 1) {
        OpenEditorForRow(item);
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

