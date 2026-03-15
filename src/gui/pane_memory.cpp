#include "pane_memory.h"
#include <wx/toolbar.h>
#include <wx/stattext.h>
#include <wx/artprov.h>
#include <wx/sizer.h>

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
    toolBar->AddTool(1001, "Page Up (-256)", wxArtProvider::GetBitmap(wxART_GO_UP));
    toolBar->AddTool(1002, "Page Down (+256)", wxArtProvider::GetBitmap(wxART_GO_DOWN));
    toolBar->AddSeparator();
    toolBar->AddControl(new wxStaticText(toolBar, wxID_ANY, " Address: $"));
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
    toolBar->Bind(wxEVT_TOOL, &PaneMemory::OnPrevPage, this, 1001);
    toolBar->Bind(wxEVT_TOOL, &PaneMemory::OnNextPage, this, 1002);
}

void PaneMemory::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    m_list->Refresh();
}

void PaneMemory::ScrollTo(uint16_t addr) {
    m_list->EnsureVisible(addr / 16);
    if (!m_addrSearch->HasFocus()) {
        m_addrSearch->ChangeValue(wxString::Format("%04X", addr));
    }
}

wxString PaneMemory::GetPaneTitle() const { return wxString::Format("Memory %d", m_index + 1); }
wxString PaneMemory::GetPaneName() const { return wxString::Format("Memory%d", m_index); }

void PaneMemory::OnGoToAddress(wxCommandEvent& WXUNUSED(event)) {
    wxString addrStr = m_addrSearch->GetValue();
    unsigned long addr;
    if (addrStr.ToULong(&addr, 16)) {
        ScrollTo((uint16_t)addr);
    }
}

void PaneMemory::OnPrevPage(wxCommandEvent& WXUNUSED(event)) {
    long top = m_list->GetTopItem();
    long next = (top * 16) - 256;
    if (next < 0) next = 0;
    ScrollTo((uint16_t)next);
}

void PaneMemory::OnNextPage(wxCommandEvent& WXUNUSED(event)) {
    long top = m_list->GetTopItem();
    long next = (top * 16) + 256;
    if (next > 65535) next = 65535;
    ScrollTo((uint16_t)next);
}
