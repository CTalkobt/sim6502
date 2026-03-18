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
    m_list->InsertColumn(2, "Dec", wxLIST_FORMAT_LEFT, 50);
    m_list->InsertColumn(3, "Prev", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(4, "Flags / Bits", wxLIST_FORMAT_LEFT, 150);

    m_list->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

    sizer->Add(m_list, 1, wxEXPAND);
    SetSizer(sizer);

    m_prev_valid = false;
    m_editRow = -1;
    m_editCol = -1;
    m_textEditor = nullptr;
    m_last_cpu_type = CPU_6502;
    m_has_z_b = false;
    memset(&m_prev_cpu, 0, sizeof(m_prev_cpu));
    memset(&m_current_cpu, 0, sizeof(m_current_cpu));

    // Insert the base 6 rows (no Z/B); RebuildRows adjusts when CPU type changes
    for (int i = 0; i < 6; i++) {
        m_list->InsertItem(i, "");
    }

    m_list->Bind(wxEVT_LEFT_DOWN, &PaneRegisters::OnLeftClick, this);
    m_list->Bind(wxEVT_MOTION, &PaneRegisters::OnMouseMove, this);
    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, &PaneRegisters::OnEditRegister, this);
}

void PaneRegisters::RebuildRows(cpu_type_t cpu_type) {
    bool needs_z_b = (cpu_type == CPU_65CE02 || cpu_type == CPU_45GS02);
    if (needs_z_b == m_has_z_b && cpu_type == m_last_cpu_type) return;

    HideEditor();
    m_list->DeleteAllItems();
    m_has_z_b = needs_z_b;
    m_last_cpu_type = cpu_type;
    m_prev_valid = false;

    int count = m_has_z_b ? 8 : 6;
    for (int i = 0; i < count; i++) {
        m_list->InsertItem(i, "");
    }
}

void PaneRegisters::RefreshPane(const SimSnapshot &snap) {
    if (!snap.cpu) return;

    RebuildRows(sim_get_cpu_type(m_sim));

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
    if (m_has_z_b) {
        UpdateRow(4, "Z",  m_current_cpu.z,  m_prev_cpu.z);
        UpdateRow(5, "B",  m_current_cpu.b,  m_prev_cpu.b);
        UpdateRow(6, "SP", m_current_cpu.s,  m_prev_cpu.s, true);
        UpdateRow(7, "P",  m_current_cpu.p,  m_prev_cpu.p);
    } else {
        UpdateRow(4, "SP", m_current_cpu.s,  m_prev_cpu.s, true);
        UpdateRow(5, "P",  m_current_cpu.p,  m_prev_cpu.p);
    }
}

void PaneRegisters::UpdateRow(int row, const wxString& name, uint32_t val, uint32_t prev, bool is16) {
    m_list->SetItem(row, 0, name);
    
    // If this row is being edited, hide the underlying text to prevent overlay ghosting
    // and clear other columns to focus on the new input.
    if (row == m_editRow) {
        m_list->SetItem(row, 1, "");
        m_list->SetItem(row, 2, "");
        m_list->SetItem(row, 3, "");
        m_list->SetItem(row, 4, "");
    } else {
        wxString valStr = is16 ? wxString::Format("%04X", val) : wxString::Format("%02X", val);
        m_list->SetItem(row, 1, valStr);
        
        // Dec column
        m_list->SetItem(row, 2, wxString::Format("%u", val));

        // Prev column
        if (m_prev_valid) {
            wxString prevStr = is16 ? wxString::Format("%04X", prev) : wxString::Format("%02X", prev);
            m_list->SetItem(row, 3, prevStr);
            if (val != prev) {
                m_list->SetItemTextColour(row, *wxRED);
            } else {
                m_list->SetItemTextColour(row, m_list->GetForegroundColour());
            }
        } else {
            m_list->SetItem(row, 3, "—");
            m_list->SetItemTextColour(row, m_list->GetForegroundColour());
        }

        // Special handling for Processor Status flags column
        if (name == "P") {
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
            m_list->SetItem(row, 4, flags);
        } else {
            m_list->SetItem(row, 4, "");
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

void PaneRegisters::OpenEditorForRow(long row, int col) {
    // Check if we are already editing this item to avoid redundant triggers
    if (m_editRow == (int)row && m_editCol == col && m_textEditor && m_textEditor->IsShown()) {
        return;
    }

    m_editRow = (int)row;
    m_editCol = col;
    wxRect rect;
    m_list->GetSubItemRect(row, col, rect);

    if (!m_textEditor) {
        m_textEditor = new wxTextCtrl(m_list, wxID_ANY, "", rect.GetPosition(), rect.GetSize(), wxTE_PROCESS_ENTER | wxNO_BORDER);
        m_textEditor->Bind(wxEVT_TEXT_ENTER, &PaneRegisters::OnEditorEnter, this);
        m_textEditor->Bind(wxEVT_KILL_FOCUS, &PaneRegisters::OnEditorKillFocus, this);
    } else {
        m_textEditor->SetSize(rect);
    }

    wxString regName = m_list->GetItemText(row, 0);
    uint32_t val = 0;
    if      (regName == "PC") val = m_current_cpu.pc;
    else if (regName == "A")  val = m_current_cpu.a;
    else if (regName == "X")  val = m_current_cpu.x;
    else if (regName == "Y")  val = m_current_cpu.y;
    else if (regName == "Z")  val = m_current_cpu.z;
    else if (regName == "B")  val = m_current_cpu.b;
    else if (regName == "SP") val = m_current_cpu.s;
    else if (regName == "P")  val = m_current_cpu.p;

    wxString currentVal;
    if (col == 1) { // Hex
        bool is16 = (regName == "PC" || regName == "SP");
        currentVal.Printf(is16 ? "%04X" : "%02X", val);
    } else { // Dec
        currentVal.Printf("%u", val);
    }

    m_textEditor->SetValue(currentVal);
    m_textEditor->Show();
    m_textEditor->SetFocus();
    m_textEditor->SelectAll();
}

void PaneRegisters::OnEditRegister(wxListEvent& event) {
    long itemIndex = event.GetIndex();

    // Check if activation was via mouse (double-click) by verifying mouse position.
    // If it was a mouse event, ensure it occurred on column 1 or 2.
    wxPoint mousePos = m_list->ScreenToClient(wxGetMousePosition());
    wxSize clientSize = m_list->GetClientSize();
    
    if (mousePos.x >= 0 && mousePos.y >= 0 && mousePos.x < clientSize.x && mousePos.y < clientSize.y) {
        int col = GetColumnAt(mousePos);
        if (col != 1 && col != 2) {
            return;
        }
        OpenEditorForRow(itemIndex, col);
    } else {
        // Keyboard activation, default to column 1
        OpenEditorForRow(itemIndex, 1);
    }
}

void PaneRegisters::OnLeftClick(wxMouseEvent& event) {
    long subitem = GetColumnAt(event.GetPosition());
    int flags = 0;
    long item = m_list->HitTest(event.GetPosition(), flags);

    if (item != wxNOT_FOUND) {
        if (subitem == 1 || subitem == 2) {
            OpenEditorForRow(item, subitem);
            return;
        } else if (subitem == 4) {
            // Check if this is the P register row
            wxString regName = m_list->GetItemText(item, 0);
            if (regName == "P") {
                // Determine which bit was clicked
                // Flags are shown as "N V U B D I Z C (XX)"
                // They are at fixed character positions because we use a monospaced font.
                // However, character-based hit testing is complex in wxListCtrl.
                // We'll use a simpler approach: toggle bits based on X position within the column.
                wxRect rect;
                m_list->GetSubItemRect(item, 4, rect);
                int localX = event.GetPosition().x - rect.x;
                
                // Estimate bit position. There are 8 flags + spaces/parens.
                // "N V U B D I Z C" is 15 characters long.
                int charWidth = 8; // approximate for monospaced 10pt
                int bitIdx = localX / (charWidth * 2); 
                if (bitIdx >= 0 && bitIdx < 8) {
                    uint8_t mask = 0x80 >> bitIdx;
                    uint8_t newP = m_current_cpu.p ^ mask;
                    sim_set_reg_byte(m_sim, "P", newP);
                    RefreshPane(SimSnapshot{ (CPU*)&m_current_cpu, sim_get_memory(m_sim) });
                    return;
                }
            }
        }
    }
    
    HideEditor();
    event.Skip();
}

void PaneRegisters::OnMouseMove(wxMouseEvent& event) {
    long subitem = GetColumnAt(event.GetPosition());
    int flags = 0;
    long item = m_list->HitTest(event.GetPosition(), flags);

    if (item != wxNOT_FOUND && (subitem == 1 || subitem == 2 || (subitem == 4 && m_list->GetItemText(item, 0) == "P"))) {
        m_list->SetCursor(wxCursor(wxCURSOR_PENCIL));
    } else {
        m_list->SetCursor(wxNullCursor);
    }
    event.Skip();
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
    m_editCol = -1;
}

void PaneRegisters::CommitEdit(int row, const wxString& newValue) {
    wxString regName = m_list->GetItemText(row, 0);
    unsigned long val;
    bool ok = false;
    if (m_editCol == 1) { // Hex
        ok = newValue.ToULong(&val, 16);
    } else if (m_editCol == 2) { // Dec
        ok = newValue.ToULong(&val, 10);
    }

    if (ok) {
        if (regName == "PC") sim_set_pc(m_sim, (uint16_t)val);
        else if (regName == "SP") sim_set_reg_value(m_sim, "S", (uint16_t)val);
        else sim_set_reg_value(m_sim, regName.ToStdString().c_str(), (uint16_t)val);
    }
    m_editRow = -1;
    m_editCol = -1;
}

