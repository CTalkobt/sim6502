#include "pane_stack.h"
#include <cstring>
#include <unordered_set>

// ─── StackMemListCtrl ────────────────────────────────────────────────────────

StackMemListCtrl::StackMemListCtrl(wxWindow* parent, sim_session_t* sim)
    : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                 wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
      m_sim(sim)
{
    // A slightly deeper amber for better contrast on white backgrounds.
    m_sp_attr.SetBackgroundColour(wxColour(255, 190, 40)); 
    SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
}

wxString StackMemListCtrl::OnGetItemText(long item, long col) const {
    // Rows are ordered high→low: row 0 = $01F0, row 15 = $0100.
    uint16_t base = (uint16_t)(0x01F0 - item * 16);
    if (col == 0) return wxString::Format("%04X", base);
    if (col >= 1 && col <= 16) {
        uint8_t val = sim_mem_read_byte(m_sim, base + col - 1);
        return wxString::Format("%02X", val);
    }
    if (col == 17) {
        wxString ascii;
        for (int i = 0; i < 16; i++) {
            uint8_t v = sim_mem_read_byte(m_sim, base + i);
            ascii << (char)(v >= 32 && v < 127 ? v : '.');
        }
        return ascii;
    }
    return "";
}

wxListItemAttr* StackMemListCtrl::OnGetItemAttr(long item) const {
    // Always provide a row-level attribute if we intend to use column-level ones later.
    // Some platforms (like GTK) require a base attribute to enable per-cell lookups.
    int sp_row = 15 - (m_sp >> 4);
    if (item == sp_row) {
        // We return an empty/default attribute for the row, which allows 
        // OnGetItemColumnAttr to be queried for specific cells in this row.
        static wxListItemAttr row_attr;
        return &row_attr;
    }
    return nullptr;
}

wxListItemAttr* StackMemListCtrl::OnGetItemColumnAttr(long item, long col) const {
    // Highlight only the single cell at $0100|SP.
    // In high→low layout: the row for SP nibble N is (15 - (m_sp >> 4)).
    // The hex column for SP's low nibble is (m_sp & 0x0F) + 1 (cols 1..16).
    int sp_row = 15 - (m_sp >> 4);
    int sp_col = (m_sp & 0x0F) + 1;
    if (item == sp_row && col == sp_col)
        return const_cast<wxListItemAttr*>(&m_sp_attr);
    return nullptr;
}

// ─── PaneStack ───────────────────────────────────────────────────────────────

PaneStack::PaneStack(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    m_notebook = new wxNotebook(this, wxID_ANY);

    // --- "Stack Memory" tab: virtual hex list of $0100-$01FF ---
    wxPanel* memPanel = new wxPanel(m_notebook);
    wxBoxSizer* memSizer = new wxBoxSizer(wxVERTICAL);
    m_memdump = new StackMemListCtrl(memPanel, sim);
    m_memdump->InsertColumn(0, "Addr", wxLIST_FORMAT_LEFT, 60);
    for (int i = 0; i < 16; i++)
        m_memdump->InsertColumn(i + 1, wxString::Format("%X", i), wxLIST_FORMAT_LEFT, 35);
    m_memdump->InsertColumn(17, "ASCII", wxLIST_FORMAT_LEFT, 150);
    m_memdump->SetItemCount(16);  // 16 rows × 16 bytes = $0100-$01FF
    memSizer->Add(m_memdump, 1, wxEXPAND);
    memPanel->SetSizer(memSizer);

    // --- "Recent" tab: active stack entries above current SP ---
    wxPanel* recentPanel = new wxPanel(m_notebook);
    wxBoxSizer* recentSizer = new wxBoxSizer(wxVERTICAL);
    m_list = new wxListCtrl(recentPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    m_list->InsertColumn(0, "Depth",           wxLIST_FORMAT_LEFT,  50);
    m_list->InsertColumn(1, "Addr",            wxLIST_FORMAT_LEFT,  60);
    m_list->InsertColumn(2, "Value",           wxLIST_FORMAT_LEFT,  50);
    m_list->InsertColumn(3, "Symbol / Return", wxLIST_FORMAT_LEFT, 200);
    recentSizer->Add(m_list, 1, wxEXPAND);
    recentPanel->SetSizer(recentSizer);

    m_notebook->AddPage(memPanel,    "Stack Memory", true);
    m_notebook->AddPage(recentPanel, "Recent",       false);

    sizer->Add(m_notebook, 1, wxEXPAND);
    SetSizer(sizer);
}

void PaneStack::RefreshPane(const SimSnapshot &snap) {
    if (!snap.cpu) return;

    uint8_t sp = snap.cpu->s;

    // --- Stack Memory tab: update SP row highlight and redraw ---
    m_memdump->m_sp = sp;
    m_memdump->RefreshItems(0, 15);

    // --- Recent tab: active stack entries above current SP ---
    {
        // Build set of confirmed JSR return addresses from trace history.
        // A JSR at address A pushes (A+2) onto the stack; we record that value.
        // Labels are suppressed entirely when trace is unavailable (empty set).
        std::unordered_set<uint16_t> jsr_ret_vals;
        if (sim_trace_is_enabled(m_sim)) {
            int tc = sim_trace_count(m_sim);
            sim_trace_entry_t te;
            for (int i = 0; i < tc; i++) {
                if (sim_trace_get(m_sim, i, &te) == 0 &&
                    strncmp(te.disasm, "JSR", 3) == 0)
                {
                    jsr_ret_vals.insert((uint16_t)(te.pc + 2));
                }
            }
        }

        m_list->DeleteAllItems();

        int row = 0;
        for (uint16_t s = (uint16_t)(sp + 1); s <= 0xFF; s++) {
            uint16_t addr = 0x0100 | s;
            uint8_t  val  = sim_mem_read_byte(m_sim, addr);

            long item = m_list->InsertItem(row, wxString::Format("%d", row));
            m_list->SetItem(item, 1, wxString::Format("%04X", addr));
            m_list->SetItem(item, 2, wxString::Format("%02X", val));

            // Annotate as return address only when confirmed by trace history.
            if (s < 0xFF && !jsr_ret_vals.empty()) {
                uint8_t  val_hi  = sim_mem_read_byte(m_sim, 0x0100 | (uint16_t)(s + 1));
                uint16_t ret_val = (uint16_t)val | ((uint16_t)val_hi << 8);
                if (jsr_ret_vals.count(ret_val)) {
                    uint16_t ret_pc = ret_val + 1;  // where RTS will jump
                    const char* sym = sim_sym_by_addr(m_sim, ret_pc);
                    if (sym)
                        m_list->SetItem(item, 3, wxString::Format("RTN to %04X (%s)", ret_pc, sym));
                    else
                        m_list->SetItem(item, 3, wxString::Format("RTN to %04X", ret_pc));
                }
            }

            if (++row >= 127) break;
        }
    }
}
