#include "pane_disassembly.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/artprov.h>

class DisasmListCtrl : public wxListCtrl {
public:
    DisasmListCtrl(wxWindow* parent, sim_session_t* sim)
        : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
          m_sim(sim), m_current_pc(0) {}

    void SetPC(uint16_t pc) {
        if (m_current_pc != pc) {
            m_current_pc = pc;
            Refresh();
        }
    }

    wxString OnGetItemText(long item, long column) const override {
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

    wxListItemAttr* OnGetItemAttr(long item) const override {
        if (item == (long)m_current_pc) {
            static wxListItemAttr pcAttr(*wxWHITE, *wxBLUE, wxNullFont);
            return &pcAttr;
        }
        return NULL;
    }

private:
    sim_session_t* m_sim;
    uint16_t       m_current_pc;
};

PaneDisassembly::PaneDisassembly(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim), m_followPC(true) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    // Toolbar
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddTool(901, "Sync to PC", wxArtProvider::GetBitmap(wxART_GO_HOME));
    toolBar->AddSeparator();
    toolBar->AddTool(902, "Page Up (-256)", wxArtProvider::GetBitmap(wxART_GO_UP));
    toolBar->AddTool(903, "Page Down (+256)", wxArtProvider::GetBitmap(wxART_GO_DOWN));
    toolBar->AddSeparator();
    toolBar->AddControl(new wxStaticText(toolBar, wxID_ANY, " Address: $"));
    m_addrSearch = new wxTextCtrl(toolBar, wxID_ANY, "0000", wxDefaultPosition, wxSize(60, -1), wxTE_PROCESS_ENTER);
    toolBar->AddControl(m_addrSearch);
    toolBar->Realize();
    sizer->Add(toolBar, 0, wxEXPAND);

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
    toolBar->Bind(wxEVT_TOOL, &PaneDisassembly::OnSyncToPC, this, 901);
    toolBar->Bind(wxEVT_TOOL, &PaneDisassembly::OnPrevPage, this, 902);
    toolBar->Bind(wxEVT_TOOL, &PaneDisassembly::OnNextPage, this, 903);
    m_addrSearch->Bind(wxEVT_TEXT_ENTER, &PaneDisassembly::OnGoToAddress, this);
}

void PaneDisassembly::RefreshPane(const SimSnapshot &snap) {
    if (snap.cpu) {
        m_list->SetPC(snap.cpu->pc);
        if (m_followPC) {
            ScrollTo(snap.cpu->pc);
        }
    }
}

void PaneDisassembly::ScrollTo(uint16_t addr) {
    m_list->EnsureVisible(addr);
    // Refresh address search text
    if (!m_addrSearch->HasFocus()) {
        m_addrSearch->ChangeValue(wxString::Format("%04X", addr));
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
    long top = m_list->GetTopItem();
    long next = top - 256;
    if (next < 0) next = 0;
    ScrollTo((uint16_t)next);
}

void PaneDisassembly::OnNextPage(wxCommandEvent& WXUNUSED(event)) {
    m_followPC = false;
    long top = m_list->GetTopItem();
    long next = top + 256;
    if (next > 65535) next = 65535;
    ScrollTo((uint16_t)next);
}

wxString PaneDisassembly::GetPaneTitle() const { return "Disassembly"; }
wxString PaneDisassembly::GetPaneName() const { return "Disassembly"; }
