#include "pane_iref.h"
#include "gui_ids.h"
#include "instruction_descriptions.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

PaneIRef::PaneIRef(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim), m_last_cpu_type(-1)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    // Filter toolbar
    wxBoxSizer* filterSizer = new wxBoxSizer(wxHORIZONTAL);
    filterSizer->Add(new wxStaticText(this, wxID_ANY, " Filter: "), 0, wxALIGN_CENTER_VERTICAL);
    m_filter = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0);
    filterSizer->Add(m_filter, 1, wxEXPAND | wxALL, 2);

    filterSizer->Add(new wxStaticText(this, wxID_ANY, " Mode: "), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
    wxArrayString modes;
    modes.Add("All");
    for (int i = 0; i <= 20; i++) {
        modes.Add(sim_mode_name((unsigned char)i));
    }
    m_modeFilter = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, modes);
    m_modeFilter->SetSelection(0);
    filterSizer->Add(m_modeFilter, 0, wxEXPAND | wxALL, 2);

    mainSizer->Add(filterSizer, 0, wxEXPAND);

    m_splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    
    m_list = new wxListCtrl(m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, "Mnemonic", wxLIST_FORMAT_LEFT, 80);
    m_list->InsertColumn(1, "Mode", wxLIST_FORMAT_LEFT, 150);
    m_list->InsertColumn(2, "Op", wxLIST_FORMAT_LEFT, 80);
    m_list->InsertColumn(3, "Cyc", wxLIST_FORMAT_LEFT, 40);

    m_detail = new wxTextCtrl(m_splitter, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    m_detail->SetValue("Select an instruction above to see details.");

    m_splitter->SplitHorizontally(m_list, m_detail, 300);
    m_splitter->SetSashGravity(0.5);
    m_splitter->SetMinimumPaneSize(50);

    mainSizer->Add(m_splitter, 1, wxEXPAND);
    SetSizer(mainSizer);

    m_filter->Bind(wxEVT_TEXT, &PaneIRef::OnFilter, this);
    m_modeFilter->Bind(wxEVT_CHOICE, &PaneIRef::OnFilter, this);
    m_list->Bind(wxEVT_LIST_ITEM_SELECTED, &PaneIRef::OnItemSelected, this);
    m_list->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &PaneIRef::OnContextMenu, this);

    PopulateList();
}

void PaneIRef::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    int current_cpu = (int)sim_get_cpu_type(m_sim);
    if (current_cpu != m_last_cpu_type) {
        m_last_cpu_type = current_cpu;
        PopulateList();
    }
}

void PaneIRef::PopulateList() {
    m_last_cpu_type = (int)sim_get_cpu_type(m_sim);
    m_list->DeleteAllItems();
    wxString filter = m_filter->GetValue().Upper();
    int modeIdx = m_modeFilter->GetSelection(); // 0 = All, 1 = mode 0, 2 = mode 1, ...
    
    int count = sim_opcode_count(m_sim);
    int row = 0;
    for (int i = 0; i < count; i++) {
        sim_opcode_info_t info;
        if (sim_opcode_get(m_sim, i, &info) == 0) {
            wxString mnem(info.mnemonic);
            if (!filter.IsEmpty() && !mnem.Upper().Contains(filter)) continue;
            
            if (modeIdx > 0 && info.mode != (unsigned char)(modeIdx - 1)) continue;

            long item = m_list->InsertItem(row, mnem);
            m_list->SetItem(item, 1, sim_mode_name(info.mode));
            
            wxString opStr;
            for (int j = 0; j < info.opcode_len; j++) {
                if (j > 0) opStr << " ";
                opStr << wxString::Format("%02X", info.opcode_bytes[j]);
            }
            m_list->SetItem(item, 2, opStr);
            m_list->SetItem(item, 3, wxString::Format("%d", info.cycles));
            m_list->SetItemData(item, (long)i);
            row++;
        }
    }
}

void PaneIRef::OnFilter(wxCommandEvent& WXUNUSED(event)) {
    PopulateList();
}

void PaneIRef::OnItemSelected(wxListEvent& event) {
    int idx = (int)event.GetData();
    sim_opcode_info_t info;
    if (sim_opcode_get(m_sim, idx, &info) == 0) {
        wxString text;
        text << GetDescription(info.mnemonic) << "\n\n";
        text << "Addressing Mode: " << sim_mode_name(info.mode) << "\n";
        
        wxString opStr;
        for (int j = 0; j < info.opcode_len; j++) {
            if (j > 0) opStr << " ";
            opStr << wxString::Format("%02X", info.opcode_bytes[j]);
        }
        text << "Opcode: $" << opStr << "\n";
        text << "Length: " << (int)info.instr_bytes << " bytes\n";
        text << "Cycles: " << info.cycles << "\n";
        
        m_detail->SetValue(text);
    }
}

void PaneIRef::OnContextMenu(wxListEvent& event) {
    int idx = (int)event.GetIndex();
    if (idx < 0) return;

    wxMenu menu;
    menu.Append(ID_IREF_FIND_IN_DISASM, "Find in Disassembly");
    menu.Bind(wxEVT_MENU, &PaneIRef::OnFindInDisasm, this, ID_IREF_FIND_IN_DISASM);
    PopupMenu(&menu);
}

#include "main_frame.h"

void PaneIRef::OnFindInDisasm(wxCommandEvent& WXUNUSED(event)) {
    long item = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item == -1) return;

    int idx = (int)m_list->GetItemData(item);
    sim_opcode_info_t info;
    if (sim_opcode_get(m_sim, idx, &info) == 0) {
        // Search memory for the opcode bytes
        uint8_t target[4];
        for (int j = 0; j < info.opcode_len; j++) target[j] = info.opcode_bytes[j];

        // Start search from address 0 for now (or could be PC)
        for (uint32_t addr = 0; addr < 0x10000; addr++) {
            bool match = true;
            for (int j = 0; j < info.opcode_len; j++) {
                if (sim_mem_read_byte(m_sim, (uint16_t)(addr + j)) != target[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                // Found! Navigate disassembly
                wxWindow* p = GetParent();
                while (p && !dynamic_cast<MainFrame*>(p)) p = p->GetParent();
                if (p) {
                    ((MainFrame*)p)->NavigateDisassembly((uint16_t)addr);
                }
                return;
            }
        }
        wxMessageBox("Opcode not found in memory.", "Search", wxOK | wxICON_INFORMATION);
    }
}

wxString PaneIRef::GetDescription(const wxString& mnemonic) {
    static std::map<std::string, std::string> details = GetInstructionDetails();
    
    wxString mnem = mnemonic.Upper();
    std::string key = mnem.ToStdString();
    if (details.count(key)) {
        return details[key];
    }

    // Handle numbered instructions like BBR0, BBS1, RMB2, SMB3
    if (mnem.length() > 1 && isdigit(mnem.Last())) {
        wxString base = mnem.Left(mnem.length() - 1);
        std::string baseKey = base.ToStdString();
        if (details.count(baseKey)) {
            return details[baseKey];
        }
    }

    return "Unknown Instruction";
}
