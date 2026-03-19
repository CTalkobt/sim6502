#include "pane_memory.h"
#include "main_frame.h"
#include <wx/toolbar.h>
#include <wx/stattext.h>
#include <wx/artprov.h>
#include <wx/sizer.h>
#include <wx/choice.h>
#include <wx/textdlg.h>
#include <wx/msgdlg.h>
#include <wx/config.h>
#include <wx/tokenzr.h>

static char petscii_to_ascii(uint8_t c) {
    // Basic PETSCII to ASCII mapping for readable chars
    if (c >= 0x20 && c <= 0x3F) return (char)c;
    if (c >= 0x41 && c <= 0x5A) return (char)(c + 0x20); // Uppercase -> Lowercase
    if (c >= 0x61 && c <= 0x7A) return (char)(c - 0x20); // Lowercase -> Uppercase
    if (c >= 0xC1 && c <= 0xDA) return (char)(c - 0x80 + 0x20); // Alt Uppercase
    if (c == 0x5B) return '[';
    if (c == 0x5D) return ']';
    return '.';
}

MemoryListCtrl::MemoryListCtrl(wxWindow* parent, sim_session_t* sim)
    : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL),
      m_sim(sim), m_charset(0) 
{
    m_attrChanged.SetTextColour(*wxRED);
}

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
            if (m_charset == 1) { // PETSCII
                ascii << petscii_to_ascii(val);
            } else { // ASCII
                if (val >= 32 && val < 127) ascii << (char)val;
                else ascii << '.';
            }
        }
        return ascii;
    }
    
    return "";
}

wxListItemAttr* MemoryListCtrl::OnGetItemAttr(long item) const {
    uint16_t base_addr = (uint16_t)item * 16;
    for (int i = 0; i < 16; i++) {
        if (m_changedAddrs.count(base_addr + i)) {
            return (wxListItemAttr*)&m_attrChanged;
        }
    }
    return nullptr;
}

PaneMemory::PaneMemory(wxWindow* parent, sim_session_t *sim, int index)
    : SimPane(parent, sim), m_index(index), m_followAddr(0)
{
    m_title = wxString::Format("Memory %d", m_index + 1);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddTool(1001, "Page Up", wxArtProvider::GetBitmap(wxART_GO_UP), "Scroll memory view up by one page");
    toolBar->AddTool(1002, "Page Down", wxArtProvider::GetBitmap(wxART_GO_DOWN), "Scroll memory view down by one page");
    toolBar->AddSeparator();
    
    toolBar->AddControl(new wxStaticText(toolBar, wxID_ANY, " Follow: "));
    wxArrayString followChoices;
    followChoices.Add("None");
    followChoices.Add("PC");
    followChoices.Add("SP");
    followChoices.Add("Word Ptr");
    m_followChoice = new wxChoice(toolBar, 1003, wxDefaultPosition, wxDefaultSize, followChoices);
    m_followChoice->SetSelection(0);
    toolBar->AddControl(m_followChoice);

    toolBar->AddControl(new wxStaticText(toolBar, wxID_ANY, " Charset: "));
    wxArrayString charsetChoices;
    charsetChoices.Add("ASCII");
    charsetChoices.Add("PETSCII");
    m_charsetChoice = new wxChoice(toolBar, 1004, wxDefaultPosition, wxDefaultSize, charsetChoices);
    m_charsetChoice->SetSelection(0);
    toolBar->AddControl(m_charsetChoice);

    toolBar->AddSeparator();
    toolBar->AddControl(new wxStaticText(toolBar, wxID_ANY, " Addr: $"));
    m_addrSearch = new wxTextCtrl(toolBar, wxID_ANY, "0000", wxDefaultPosition, wxSize(60, -1), wxTE_PROCESS_ENTER);
    toolBar->AddControl(m_addrSearch);
    
    toolBar->AddSeparator();
    toolBar->AddTool(1005, "Rename", wxArtProvider::GetBitmap(wxART_EDIT), "Rename this memory pane");

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
    toolBar->Bind(wxEVT_TOOL, &PaneMemory::OnRename, this, 1005);
    m_followChoice->Bind(wxEVT_CHOICE, &PaneMemory::OnFollowMode, this, 1003);
    m_charsetChoice->Bind(wxEVT_CHOICE, &PaneMemory::OnCharsetMode, this, 1004);
    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, &PaneMemory::OnItemActivated, this);
}

void PaneMemory::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    
    // Task 3: Highlight recently changed bytes
    uint16_t lastWrites[256];
    int count = sim_get_last_writes(m_sim, lastWrites, 256);
    std::set<uint16_t> changedSet;
    for (int i = 0; i < count; i++) {
        changedSet.insert(lastWrites[i]);
    }
    m_list->SetChangedAddresses(changedSet);

    // Task 5: Follow option
    int followMode = m_followChoice->GetSelection();
    if (followMode != 0) { // Not None
        CPU* cpu = sim_get_cpu(m_sim);
        if (cpu) {
            uint16_t addr = 0;
            bool valid = false;
            if (followMode == 1) { // PC
                addr = cpu->pc;
                valid = true;
            } else if (followMode == 2) { // SP
                addr = 0x0100 + cpu->s;
                valid = true;
            } else if (followMode == 3) { // Word Ptr
                uint8_t lo = sim_mem_read_byte(m_sim, m_followAddr);
                uint8_t hi = sim_mem_read_byte(m_sim, m_followAddr + 1);
                addr = (uint16_t)(lo | (hi << 8));
                valid = true;
            }
            if (valid) {
                ScrollTo(addr);
            }
        }
    }

    m_list->Refresh();
}

void PaneMemory::ScrollTo(uint16_t addr) {
    m_list->EnsureVisible(addr / 16);
    if (!m_addrSearch->HasFocus()) {
        m_addrSearch->ChangeValue(wxString::Format("%04X", addr));
    }
}

wxString PaneMemory::GetPaneName() const { return wxString::Format("Memory%d", m_index); }

void PaneMemory::OnGoToAddress(wxCommandEvent& WXUNUSED(event)) {
    wxString addrStr = m_addrSearch->GetValue();
    unsigned long addr;
    if (addrStr.ToULong(&addr, 16)) {
        if (m_followChoice->GetSelection() == 3) {
            m_followAddr = (uint16_t)addr;
        }
        ScrollTo((uint16_t)addr);
    }
}

void PaneMemory::OnPrevPage(wxCommandEvent& WXUNUSED(event)) {
    int visible = m_list->GetCountPerPage();
    long top = m_list->GetTopItem();
    long next = (top - visible) * 16;
    if (next < 0) next = 0;
    ScrollTo((uint16_t)next);
}

void PaneMemory::OnNextPage(wxCommandEvent& WXUNUSED(event)) {
    int visible = m_list->GetCountPerPage();
    long top = m_list->GetTopItem();
    long next = (top + visible) * 16;
    if (next > 65535) next = 65535;
    ScrollTo((uint16_t)next);
}

void PaneMemory::OnFollowMode(wxCommandEvent& WXUNUSED(event)) {
    int sel = m_followChoice->GetSelection();
    if (sel == 3) { // Word Ptr
        wxTextEntryDialog dlg(this, "Enter address of word pointer (hex):", "Follow Word Pointer", wxString::Format("%04X", m_followAddr));
        if (dlg.ShowModal() == wxID_OK) {
            unsigned long addr;
            if (dlg.GetValue().ToULong(&addr, 16)) {
                m_followAddr = (uint16_t)addr;
            }
        }
    }
}

void PaneMemory::OnCharsetMode(wxCommandEvent& WXUNUSED(event)) {
    m_list->SetCharset(m_charsetChoice->GetSelection());
    m_list->Refresh();
}

void PaneMemory::OnRename(wxCommandEvent& WXUNUSED(event)) {
    wxTextEntryDialog dlg(this, "Enter new name for this memory pane:", "Rename Pane", m_title);
    if (dlg.ShowModal() == wxID_OK) {
        m_title = dlg.GetValue();
        MainFrame* mf = dynamic_cast<MainFrame*>(wxGetTopLevelParent(this));
        if (mf) {
            mf->UpdatePaneCaption(this, m_title);
        }
    }
}

void PaneMemory::OnItemActivated(wxListEvent& event) {
    // Task 1: Inline editing
    long item = event.GetIndex();
    uint16_t base_addr = (uint16_t)item * 16;
    
    // We don't know exactly which column was double-clicked from the event easily
    // without some extra work. Let's ask which byte to edit or edit the whole line.
    wxTextEntryDialog dlg(this, wxString::Format("Edit bytes at $%04X (enter hex bytes separated by space):", base_addr),
                         "Edit Memory", "");
    
    wxString current;
    for (int i = 0; i < 16; i++) {
        current << wxString::Format("%02X ", sim_mem_read_byte(m_sim, base_addr + i));
    }
    dlg.SetValue(current.Trim());

    if (dlg.ShowModal() == wxID_OK) {
        wxString val = dlg.GetValue();
        wxStringTokenizer tk(val, " ");
        int i = 0;
        while (tk.HasMoreTokens() && i < 16) {
            wxString token = tk.GetNextToken();
            unsigned long byteVal;
            if (token.ToULong(&byteVal, 16)) {
                sim_mem_write_byte(m_sim, base_addr + i, (uint8_t)byteVal);
            }
            i++;
        }
        m_list->Refresh();
    }
}

void PaneMemory::LoadState(wxConfigBase* cfg) {
    wxString path = wxString::Format("/Panes/%s/", GetPaneName());
    cfg->Read(path + "Title", &m_title, m_title);
    int follow, charset;
    cfg->Read(path + "FollowMode", &follow, 0);
    cfg->Read(path + "Charset", &charset, 0);
    long followAddr;
    cfg->Read(path + "FollowAddr", &followAddr, 0);
    
    m_followChoice->SetSelection(follow);
    m_charsetChoice->SetSelection(charset);
    m_list->SetCharset(charset);
    m_followAddr = (uint16_t)followAddr;

    MainFrame* mf = dynamic_cast<MainFrame*>(wxGetTopLevelParent(this));
    if (mf) {
        mf->UpdatePaneCaption(this, m_title);
    }
}

void PaneMemory::SaveState(wxConfigBase* cfg) {
    wxString path = wxString::Format("/Panes/%s/", GetPaneName());
    cfg->Write(path + "Title", m_title);
    cfg->Write(path + "FollowMode", m_followChoice->GetSelection());
    cfg->Write(path + "Charset", m_charsetChoice->GetSelection());
    cfg->Write(path + "FollowAddr", (long)m_followAddr);
}
