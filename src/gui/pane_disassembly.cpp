#include "pane_disassembly.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/artprov.h>

class DisasmListCtrl : public wxListCtrl {
public:
    DisasmListCtrl(wxWindow* parent, sim_session_t* sim, PaneDisassembly* owner)
        : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
          m_sim(sim), m_owner(owner), m_current_pc(0) 
    {
        Bind(wxEVT_LEFT_DOWN, &DisasmListCtrl::OnLeftDown, this);
    }

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
        bool is_pc = (addr == m_current_pc);
        bool is_bp = sim_has_breakpoint(m_sim, addr);
        
        wxString filter = m_owner->GetFilterText();
        bool is_match = false;
        if (!filter.IsEmpty()) {
            for (int col = 1; col <= 6; col++) {
                if (OnGetItemText(item, col).Upper().Contains(filter)) {
                    is_match = true;
                    break;
                }
            }
        }

        if (is_pc) {
            static wxListItemAttr pcAttr(*wxWHITE, *wxBLUE, wxNullFont);
            if (is_bp) {
                // Task 5: PC + BP. Use a slightly different color or just ensure BP is visible.
                // We can use a different background like a darker blue or orange.
                static wxListItemAttr pcBpAttr(*wxWHITE, wxColour(0, 0, 128), wxNullFont);
                return &pcBpAttr;
            }
            return &pcAttr;
        }
        
        if (is_match) {
            // Task 6: Search match
            static wxListItemAttr matchAttr(*wxBLACK, *wxYELLOW, wxNullFont);
            return &matchAttr;
        }

        return NULL;
    }

    void OnLeftDown(wxMouseEvent& event) {
        int x, y;
        CalcUnscrolledPosition(event.GetX(), event.GetY(), &x, &y);
        
        int flags;
        long item = HitTest(wxPoint(x, y), flags);
        if (item != wxNOT_FOUND) {
            // Task 3: BP gutter click
            int col0_width = GetColumnWidth(0);
            if (x < col0_width) {
                uint16_t addr = m_owner->GetAddressForRow((int)item);
                if (sim_has_breakpoint(m_sim, addr)) {
                    sim_break_clear(m_sim, addr);
                } else {
                    sim_break_set(m_sim, addr, NULL);
                }
                RefreshItem(item);
                return;
            }
        }
        event.Skip();
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
    m_toolbar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    m_toolbar->AddCheckTool(901, "Follow PC", wxArtProvider::GetBitmap(wxART_GO_HOME), wxNullBitmap, "Follow the current Program Counter");
    m_toolbar->ToggleTool(901, true);
    m_toolbar->AddSeparator();
    m_toolbar->AddTool(902, "Page Up", wxArtProvider::GetBitmap(wxART_GO_UP), "Scroll disassembly up by one page");
    m_toolbar->AddTool(903, "Page Down", wxArtProvider::GetBitmap(wxART_GO_DOWN), "Scroll disassembly down by one page");
    m_toolbar->AddSeparator();
    m_toolbar->AddControl(new wxStaticText(m_toolbar, wxID_ANY, " Address: $"));
    m_addrSearch = new wxTextCtrl(m_toolbar, wxID_ANY, "0000", wxDefaultPosition, wxSize(60, -1), wxTE_PROCESS_ENTER);
    m_toolbar->AddControl(m_addrSearch);
    m_toolbar->Realize();
    sizer->Add(m_toolbar, 0, wxEXPAND);

    // Task 6: Search Bar (initially hidden)
    m_searchSizer = new wxBoxSizer(wxHORIZONTAL);
    m_searchSizer->Add(new wxStaticText(this, wxID_ANY, " Filter: "), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
    m_filterCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_searchSizer->Add(m_filterCtrl, 1, wxEXPAND | wxALL, 2);
    wxButton* closeBtn = new wxButton(this, wxID_ANY, "X", wxDefaultPosition, wxSize(24, 24), wxBU_EXACTFIT);
    m_searchSizer->Add(closeBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    
    sizer->Add(m_searchSizer, 0, wxEXPAND);
    sizer->Hide(m_searchSizer);

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
    m_list->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &PaneDisassembly::OnContextMenu, this);
    m_toolbar->Bind(wxEVT_TOOL, &PaneDisassembly::OnFollowToggle, this, 901);
    m_toolbar->Bind(wxEVT_TOOL, &PaneDisassembly::OnPrevPage, this, 902);
    m_toolbar->Bind(wxEVT_TOOL, &PaneDisassembly::OnNextPage, this, 903);
    m_addrSearch->Bind(wxEVT_TEXT_ENTER, &PaneDisassembly::OnGoToAddress, this);
    
    m_filterCtrl->Bind(wxEVT_TEXT, &PaneDisassembly::OnSearch, this);
    closeBtn->Bind(wxEVT_BUTTON, &PaneDisassembly::OnSearchClose, this);

    // Bind Ctrl+F for search
    wxAcceleratorEntry entries[1];
    entries[0].Set(wxACCEL_CTRL, (int)'F', 1001);
    wxAcceleratorTable accel(1, entries);
    SetAcceleratorTable(accel);
    Bind(wxEVT_MENU, &PaneDisassembly::OnSearch, this, 1001);
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

uint16_t PaneDisassembly::HeuristicBackStep(uint16_t top_addr, int rows) {
    if (top_addr == 0) return 0;

    // Step back a safe maximum distance
    int max_bytes = rows * 4;
    uint16_t start_probe = (top_addr > max_bytes) ? (uint16_t)(top_addr - max_bytes) : 0;

    // 1. Try to find a symbol in this range
    for (uint16_t a = top_addr; a >= start_probe; a--) {
        if (sim_sym_by_addr(m_sim, a)) {
            // Found a symbol, check if it's a "sync point"
            // Disassemble from here and see if we hit top_addr
            uint16_t curr = a;
            std::vector<uint16_t> path;
            while (curr < top_addr) {
                path.push_back(curr);
                sim_disasm_entry_t entry;
                sim_disassemble_entry(m_sim, curr, &entry);
                curr += entry.size;
            }
            if (curr == top_addr) {
                // Hits exactly! Use the rows before top_addr
                if ((int)path.size() >= rows) {
                    return path[path.size() - rows];
                }
                // Too few rows from this symbol, but it's a sync point.
                // We could continue searching back.
            }
        }
        if (a == 0) break;
    }

    // 2. Heuristic alignment: Try various offsets and see which one hits top_addr
    for (int offset = 1; offset <= max_bytes; offset++) {
        uint16_t probe = (top_addr > offset) ? (uint16_t)(top_addr - offset) : 0;
        uint16_t curr = probe;
        std::vector<uint16_t> path;
        while (curr < top_addr) {
            path.push_back(curr);
            sim_disasm_entry_t entry;
            sim_disassemble_entry(m_sim, curr, &entry);
            curr += entry.size;
        }
        if (curr == top_addr) {
            if ((int)path.size() >= rows) {
                return path[path.size() - rows];
            }
        }
        if (probe == 0) break;
    }

    return start_probe;
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
        
        // Task 1: Update Follow PC button state if it changed externally
        m_toolbar->ToggleTool(901, m_followPC);

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
        // Task 2: Use heuristic backstep to show some context before target
        uint16_t back = HeuristicBackStep(addr, 5);
        UpdateRowAddresses(back);
        
        // Find again
        for (size_t i = 0; i < m_row_addresses.size(); i++) {
            if (m_row_addresses[i] == addr) {
                m_list->EnsureVisible((int)i);
                break;
            }
        }
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
    m_toolbar->ToggleTool(901, true);
    CPU* cpu = sim_get_cpu(m_sim);
    if (cpu) {
        ScrollTo(cpu->pc);
    }
}

void PaneDisassembly::OnFollowToggle(wxCommandEvent& event) {
    m_followPC = event.IsChecked();
    if (m_followPC) {
        CPU* cpu = sim_get_cpu(m_sim);
        if (cpu) ScrollTo(cpu->pc);
    }
}

void PaneDisassembly::OnGoToAddress(wxCommandEvent& WXUNUSED(event)) {
    wxString addrStr = m_addrSearch->GetValue();
    unsigned long addr;
    if (addrStr.ToULong(&addr, 16)) {
        m_followPC = false;
        m_toolbar->ToggleTool(901, false);
        ScrollTo((uint16_t)addr);
    }
}

void PaneDisassembly::OnPrevPage(wxCommandEvent& WXUNUSED(event)) {
    m_followPC = false;
    m_toolbar->ToggleTool(901, false);
    
    int count = m_list->GetCountPerPage();
    uint16_t first_addr = GetAddressForRow(m_list->GetTopItem());
    uint16_t new_base = HeuristicBackStep(first_addr, count);
    
    UpdateRowAddresses(new_base);
}

void PaneDisassembly::OnNextPage(wxCommandEvent& WXUNUSED(event)) {
    m_followPC = false;
    m_toolbar->ToggleTool(901, false);
    
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

void PaneDisassembly::OnContextMenu(wxListEvent& event) {
    int row = (int)event.GetIndex();
    m_list->SetItemState(row, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    uint16_t addr = GetAddressForRow(row);

    wxMenu menu;
    menu.Append(101, wxString::Format("Set PC to $%04X", addr));
    menu.Append(102, "Go to address in Memory View");
    menu.Append(103, wxString::Format("Add Watch at $%04X", addr));
    
    sim_disasm_entry_t entry;
    if (sim_disassemble_entry(m_sim, addr, &entry) == 0 && entry.has_target) {
        menu.Append(105, wxString::Format("Add Watch at Operand ($%04X)", (uint16_t)entry.target_addr));
    }

    Bind(wxEVT_MENU, &PaneDisassembly::OnSetPC, this, 101);
    Bind(wxEVT_MENU, &PaneDisassembly::OnGoToMemory, this, 102);
    Bind(wxEVT_MENU, &PaneDisassembly::OnAddWatch, this, 103);
    Bind(wxEVT_MENU, &PaneDisassembly::OnAddWatchOperand, this, 105);

    PopupMenu(&menu);
}

void PaneDisassembly::OnSetPC(wxCommandEvent& WXUNUSED(event)) {
    long item = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item != -1) {
        uint16_t addr = GetAddressForRow((int)item);
        sim_set_pc(m_sim, addr);
        m_list->SetPC(addr);
    }
}

#include "main_frame.h"

void PaneDisassembly::OnGoToMemory(wxCommandEvent& WXUNUSED(event)) {
    long item = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item != -1) {
        uint16_t addr = GetAddressForRow((int)item);
        wxWindow* p = GetParent();
        while (p && !dynamic_cast<MainFrame*>(p)) p = p->GetParent();
        if (p) {
            ((MainFrame*)p)->NavigateMemory(addr); 
        }
    }
}

void PaneDisassembly::OnAddWatch(wxCommandEvent& WXUNUSED(event)) {
    long item = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item != -1) {
        uint16_t addr = GetAddressForRow((int)item);
        wxWindow* p = GetParent();
        while (p && !dynamic_cast<MainFrame*>(p)) p = p->GetParent();
        if (p) {
            ((MainFrame*)p)->AddWatch(addr); 
        }
    }
}

void PaneDisassembly::OnAddWatchOperand(wxCommandEvent& WXUNUSED(event)) {
    long item = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item != -1) {
        uint16_t addr = GetAddressForRow((int)item);
        sim_disasm_entry_t entry;
        if (sim_disassemble_entry(m_sim, addr, &entry) == 0 && entry.has_target) {
            wxWindow* p = GetParent();
            while (p && !dynamic_cast<MainFrame*>(p)) p = p->GetParent();
            if (p) {
                ((MainFrame*)p)->AddWatch((uint16_t)entry.target_addr); 
            }
        }
    }
}

void PaneDisassembly::OnSearch(wxCommandEvent& event) {
    if (event.GetId() == 1001) {
        // Ctrl+F pressed
        GetSizer()->Show(m_searchSizer);
        Layout();
        m_filterCtrl->SetFocus();
        return;
    }

    m_filterText = m_filterCtrl->GetValue().Upper();
    // In a virtual list, we just Refresh to trigger OnGetItemText/Attr which could highlight
    // but here the user wants to "highlight matching rows".
    // We could use different colors in OnGetItemAttr.
    m_list->Refresh();
}

void PaneDisassembly::OnSearchClose(wxCommandEvent& WXUNUSED(event)) {
    m_filterText = "";
    m_filterCtrl->Clear();
    GetSizer()->Hide(m_searchSizer);
    Layout();
    m_list->Refresh();
}

wxString PaneDisassembly::GetPaneTitle() const { return "Disassembly"; }
wxString PaneDisassembly::GetPaneName() const { return "Disassembly"; }
