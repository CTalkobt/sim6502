#include "pane_stack.h"

PaneStack::PaneStack(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    m_notebook = new wxNotebook(this, wxID_ANY);

    // --- "Stack Memory" tab: full hex dump of $0100-$01FF ---
    wxPanel* memPanel = new wxPanel(m_notebook);
    wxBoxSizer* memSizer = new wxBoxSizer(wxVERTICAL);
    m_memdump = new wxTextCtrl(memPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                               wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    m_memdump->SetFont(wxFont(wxFontInfo(9).FaceName("Monospace").Family(wxFONTFAMILY_TELETYPE)));
    memSizer->Add(m_memdump, 1, wxEXPAND);
    memPanel->SetSizer(memSizer);

    // --- "Recent" tab: active stack entries list ---
    wxPanel* recentPanel = new wxPanel(m_notebook);
    wxBoxSizer* recentSizer = new wxBoxSizer(wxVERTICAL);
    m_list = new wxListCtrl(recentPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, "Depth",         wxLIST_FORMAT_LEFT,  50);
    m_list->InsertColumn(1, "Addr",          wxLIST_FORMAT_LEFT,  60);
    m_list->InsertColumn(2, "Value",         wxLIST_FORMAT_LEFT,  50);
    m_list->InsertColumn(3, "Symbol / Return", wxLIST_FORMAT_LEFT, 200);
    recentSizer->Add(m_list, 1, wxEXPAND);
    recentPanel->SetSizer(recentSizer);

    // Add pages — Stack Memory first so it is the default
    m_notebook->AddPage(memPanel,    "Stack Memory", true);
    m_notebook->AddPage(recentPanel, "Recent",       false);

    sizer->Add(m_notebook, 1, wxEXPAND);
    SetSizer(sizer);
}

void PaneStack::RefreshPane(const SimSnapshot &snap) {
    if (!snap.cpu) return;

    // --- Stack Memory tab: hex dump of the full stack page $0100-$01FF ---
    {
        wxString dump;
        for (int row = 0; row < 16; row++) {
            uint16_t base = 0x0100 + row * 16;
            dump += wxString::Format("%04X: ", base);
            for (int col = 0; col < 16; col++) {
                uint8_t val = sim_mem_read_byte(m_sim, (uint16_t)(base + col));
                dump += wxString::Format("%02X ", val);
            }
            dump += "\n";
        }
        m_memdump->SetValue(dump);
    }

    // --- Recent tab: active stack entries above current SP ---
    {
        m_list->DeleteAllItems();

        uint16_t sp = snap.cpu->s;
        int row = 0;
        for (uint16_t s = sp + 1; s <= 0xFF; s++) {
            uint16_t addr = 0x0100 | s;
            uint8_t val = sim_mem_read_byte(m_sim, addr);

            long item = m_list->InsertItem(row, wxString::Format("%d", row));
            m_list->SetItem(item, 1, wxString::Format("%04X", addr));
            m_list->SetItem(item, 2, wxString::Format("%02X", val));

            if (s < 0xFF) {
                uint8_t val_hi = sim_mem_read_byte(m_sim, 0x0100 | (s + 1));
                uint16_t ret_addr = (uint16_t)val | ((uint16_t)val_hi << 8);
                const char* sym = sim_sym_by_addr(m_sim, ret_addr + 1);
                if (sym) {
                    m_list->SetItem(item, 3, wxString::Format("RTN to %04X (%s)", ret_addr + 1, sym));
                }
            }

            row++;
            if (row > 32) break;
        }
    }
}
