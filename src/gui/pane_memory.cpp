#include "pane_memory.h"
#include <wx/toolbar.h>
#include <wx/stattext.h>

MemoryListCtrl::MemoryListCtrl(wxWindow* parent, sim_session_t* sim)
    : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
      m_sim(sim) {}

wxString MemoryListCtrl::OnGetItemText(long item, long column) const {
    uint16_t base_addr = (uint16_t)item * 16;
    
    if (column == 0) return wxString::Format("%04X", base_addr);
    
    if (column >= 1 && column <= 16) {
        uint8_t val = sim_mem_read_byte(m_sim, base_addr + column - 1);
        return wxString::Format("%02X", val);
    }
    
    if (column == 17) {
        wxString ascii;
        for (int i = 0; i < 16; i++) {
            uint8_t val = sim_mem_read_byte(m_sim, base_addr + i);
            if (val >= 32 && val < 127) ascii << (char)val;
            else ascii << '.';
        }
        return ascii;
    }
    
    return "";
}

PaneMemory::PaneMemory(wxWindow* parent, sim_session_t *sim, int index)
    : SimPane(parent, sim), m_index(index) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddControl(new wxStaticText(toolBar, wxID_ANY, " Address: "));
    m_addrSearch = new wxTextCtrl(toolBar, wxID_ANY, "0000", wxDefaultPosition, wxSize(60, -1), wxTE_PROCESS_ENTER);
    toolBar->AddControl(m_addrSearch);
    toolBar->Realize();
    sizer->Add(toolBar, 0, wxEXPAND);

    m_list = new MemoryListCtrl(this, m_sim);
    m_list->InsertColumn(0, "Addr", wxLIST_FORMAT_LEFT, 60);
    for (int i = 0; i < 16; i++) {
        m_list->InsertColumn(i + 1, wxString::Format("%X", i), wxLIST_FORMAT_LEFT, 30);
    }
    m_list->InsertColumn(17, "ASCII", wxLIST_FORMAT_LEFT, 150);

    m_list->SetItemCount(4096);

    sizer->Add(m_list, 1, wxEXPAND);
    SetSizer(sizer);

    m_addrSearch->Bind(wxEVT_TEXT_ENTER, &PaneMemory::OnGoToAddress, this);
}

void PaneMemory::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    m_list->Refresh();
}

wxString PaneMemory::GetPaneTitle() const { return wxString::Format("Memory %d", m_index + 1); }
wxString PaneMemory::GetPaneName() const { return wxString::Format("Memory%d", m_index); }

void PaneMemory::OnGoToAddress(wxCommandEvent& WXUNUSED(event)) {
    wxString addrStr = m_addrSearch->GetValue();
    unsigned long addr;
    if (addrStr.ToULong(&addr, 16)) {
        m_list->EnsureVisible(addr / 16);
    }
}
