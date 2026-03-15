#include "pane_watches.h"
#include <wx/menu.h>
#include <wx/textctrl.h>
#include <wx/msgdlg.h>
#include <wx/config.h>
#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <algorithm>

// --- Add Watch Dialog ---
class AddWatchDialog : public wxDialog {
public:
    AddWatchDialog(wxWindow* parent) : wxDialog(parent, wxID_ANY, "Add Watch") {
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        wxFlexGridSizer* gridSizer = new wxFlexGridSizer(2, 5, 5);

        gridSizer->Add(new wxStaticText(this, wxID_ANY, "Address (hex):"), 0, wxALIGN_CENTER_VERTICAL);
        m_addrCtrl = new wxTextCtrl(this, wxID_ANY, "0200");
        gridSizer->Add(m_addrCtrl, 1, wxEXPAND);

        gridSizer->Add(new wxStaticText(this, wxID_ANY, "Label:"), 0, wxALIGN_CENTER_VERTICAL);
        m_labelCtrl = new wxTextCtrl(this, wxID_ANY, "");
        gridSizer->Add(m_labelCtrl, 1, wxEXPAND);

        gridSizer->Add(new wxStaticText(this, wxID_ANY, "Width:"), 0, wxALIGN_CENTER_VERTICAL);
        wxArrayString choices;
        choices.Add("1 Byte (8-bit)");
        choices.Add("2 Bytes (16-bit)");
        choices.Add("4 Bytes (32-bit)");
        m_widthChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices);
        m_widthChoice->SetSelection(0);
        gridSizer->Add(m_widthChoice, 1, wxEXPAND);

        mainSizer->Add(gridSizer, 1, wxEXPAND | wxALL, 10);
        mainSizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);

        SetSizerAndFit(mainSizer);
    }

    uint16_t GetAddress() const {
        unsigned long addr;
        m_addrCtrl->GetValue().ToULong(&addr, 16);
        return (uint16_t)addr;
    }

    wxString GetLabel() const { return m_labelCtrl->GetValue(); }

    int GetWidth() const {
        int sel = m_widthChoice->GetSelection();
        if (sel == 1) return 2;
        if (sel == 2) return 4;
        return 1;
    }

private:
    wxTextCtrl* m_addrCtrl;
    wxTextCtrl* m_labelCtrl;
    wxChoice* m_widthChoice;
};

PaneWatches::PaneWatches(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    // Toolbar
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER);
    toolBar->AddTool(301, "Add Watch", wxArtProvider::GetBitmap(wxART_PLUS, wxART_TOOLBAR, wxSize(16,16)), "Add a new watch");
    toolBar->AddTool(302, "Delete Watch", wxArtProvider::GetBitmap(wxART_MINUS, wxART_TOOLBAR, wxSize(16,16)), "Delete selected watch");
    toolBar->AddSeparator();
    toolBar->AddTool(303, "Clear All", wxArtProvider::GetBitmap(wxART_DELETE, wxART_TOOLBAR, wxSize(16,16)), "Clear all watches");
    toolBar->Realize();
    sizer->Add(toolBar, 0, wxEXPAND);

    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_EDIT_LABELS);
    m_list->InsertColumn(0, "Label", wxLIST_FORMAT_LEFT, 120);
    m_list->InsertColumn(1, "Address", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(2, "Width", wxLIST_FORMAT_LEFT, 40);
    m_list->InsertColumn(3, "Hex", wxLIST_FORMAT_LEFT, 80);
    m_list->InsertColumn(4, "Dec", wxLIST_FORMAT_LEFT, 80);
    m_list->InsertColumn(5, "ASCII", wxLIST_FORMAT_LEFT, 60);

    sizer->Add(m_list, 1, wxEXPAND);
    SetSizer(sizer);

    m_list->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &PaneWatches::OnContextMenu, this);
    m_list->Bind(wxEVT_LIST_END_LABEL_EDIT, &PaneWatches::OnEndLabelEdit, this);
    m_list->Bind(wxEVT_LIST_KEY_DOWN, &PaneWatches::OnKeyDown, this);

    Bind(wxEVT_TOOL, &PaneWatches::OnAddTool, this, 301);
    Bind(wxEVT_TOOL, &PaneWatches::OnDeleteTool, this, 302);
    Bind(wxEVT_TOOL, &PaneWatches::OnClearAllTool, this, 303);
}

void PaneWatches::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    int count = (int)m_watches.size();
    
    // Only rebuild the list structure if the count changed
    if (m_list->GetItemCount() != count) {
        m_list->DeleteAllItems();
        for (int i = 0; i < count; i++) {
            m_list->InsertItem(i, m_watches[i].label);
        }
    }

    for (int i = 0; i < count; i++) {
        uint32_t val = 0;
        for (int b = 0; b < m_watches[i].width; b++) {
            val |= (uint32_t)sim_mem_read_byte(m_sim, m_watches[i].address + b) << (b * 8);
        }

        bool changed = (val != m_watches[i].last_val);
        
        // Always update values if they changed, or if the list was just rebuilt
        if (changed || m_list->GetItemText(i, 3).IsEmpty()) {
            m_list->SetItem(i, 0, m_watches[i].label);
            m_list->SetItem(i, 1, wxString::Format("%04X", m_watches[i].address));
            m_list->SetItem(i, 2, wxString::Format("%d", m_watches[i].width));
            
            wxString hexStr, decStr, ascStr;
            if (m_watches[i].width == 1) {
                hexStr = wxString::Format("%02X", val);
                decStr = wxString::Format("%u", val);
                char c = (val >= 32 && val < 127) ? (char)val : '.';
                ascStr = wxString::Format("%c", c);
            } else if (m_watches[i].width == 2) {
                hexStr = wxString::Format("%04X", val);
                decStr = wxString::Format("%u", val);
            } else if (m_watches[i].width == 4) {
                hexStr = wxString::Format("%08X", val);
                decStr = wxString::Format("%u", val);
            }
            m_list->SetItem(i, 3, hexStr);
            m_list->SetItem(i, 4, decStr);
            m_list->SetItem(i, 5, ascStr);
        }
        
        // Update color: red for the frame it changed, then back to default
        m_list->SetItemTextColour(i, changed ? *wxRED : m_list->GetForegroundColour());
        
        m_watches[i].changed = changed;
        m_watches[i].last_val = val;
    }
}

void PaneWatches::SaveState(wxConfigBase* cfg) {
    cfg->Write("Watches/Count", (int)m_watches.size());
    for (int i = 0; i < (int)m_watches.size(); i++) {
        cfg->Write(wxString::Format("Watches/Label_%d", i), m_watches[i].label);
        cfg->Write(wxString::Format("Watches/Addr_%d", i), (int)m_watches[i].address);
        cfg->Write(wxString::Format("Watches/Width_%d", i), m_watches[i].width);
    }
}

void PaneWatches::LoadState(wxConfigBase* cfg) {
    m_watches.clear();
    int count = 0;
    cfg->Read("Watches/Count", &count, 0);
    for (int i = 0; i < count; i++) {
        Watch w;
        wxString defaultLabel = wxString::Format("Watch_%d", i);
        cfg->Read(wxString::Format("Watches/Label_%d", i), &w.label, defaultLabel);
        int addr = 0;
        cfg->Read(wxString::Format("Watches/Addr_%d", i), &addr, 0);
        w.address = (uint16_t)addr;
        cfg->Read(wxString::Format("Watches/Width_%d", i), &w.width, 1);
        
        uint32_t val = 0;
        for (int b = 0; b < w.width; b++) {
            val |= (uint32_t)sim_mem_read_byte(m_sim, w.address + b) << (b * 8);
        }
        w.last_val = val;
        w.changed = false;
        m_watches.push_back(w);
    }
    RefreshPane(SimSnapshot{});
}

void PaneWatches::AddWatch(uint16_t addr, const wxString& label, int width) {
    Watch w;
    w.address = addr;
    w.width = width;
    w.label = label.IsEmpty() ? wxString::Format("Watch at %04X", addr) : label;
    
    uint32_t val = 0;
    for (int b = 0; b < w.width; b++) {
        val |= (uint32_t)sim_mem_read_byte(m_sim, w.address + b) << (b * 8);
    }
    w.last_val = val;
    w.changed = false;
    m_watches.push_back(w);
    RefreshPane(SimSnapshot{});
}

void PaneWatches::OnAddTool(wxCommandEvent& WXUNUSED(event)) {
    AddWatchDialog dlg(this);
    if (dlg.ShowModal() == wxID_OK) {
        AddWatch(dlg.GetAddress(), dlg.GetLabel(), dlg.GetWidth());
    }
}

void PaneWatches::OnDeleteTool(wxCommandEvent& WXUNUSED(event)) {
    std::vector<int> to_delete;
    long item = -1;
    while ((item = m_list->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != -1) {
        to_delete.push_back((int)item);
    }
    
    if (to_delete.empty()) return;
    
    // Sort descending to keep indexes stable
    std::sort(to_delete.rbegin(), to_delete.rend());
    for (int idx : to_delete) {
        if (idx < (int)m_watches.size()) {
            m_watches.erase(m_watches.begin() + idx);
        }
    }
    RefreshPane(SimSnapshot{});
}

void PaneWatches::OnClearAllTool(wxCommandEvent& WXUNUSED(event)) {
    if (m_watches.empty()) return;
    if (wxMessageBox("Remove all watches?", "Clear All", wxYES_NO | wxICON_QUESTION) == wxYES) {
        m_watches.clear();
        RefreshPane(SimSnapshot{});
    }
}

void PaneWatches::OnContextMenu(wxListEvent& WXUNUSED(event)) {
    wxMenu menu;
    menu.Append(301, "Add Watch...");
    menu.Append(302, "Delete Watch");
    menu.Append(303, "Clear All");
    
    int id = GetPopupMenuSelectionFromUser(menu);
    if (id == 301) {
        wxCommandEvent dummy;
        OnAddTool(dummy);
    } else if (id == 302) {
        wxCommandEvent dummy;
        OnDeleteTool(dummy);
    } else if (id == 303) {
        wxCommandEvent dummy;
        OnClearAllTool(dummy);
    }
}

void PaneWatches::OnEndLabelEdit(wxListEvent& event) {
    if (event.IsEditCancelled()) return;
    int idx = event.GetIndex();
    if (idx >= 0 && idx < (int)m_watches.size()) {
        m_watches[idx].label = event.GetLabel();
    }
}

void PaneWatches::OnKeyDown(wxListEvent& event) {
    if (event.GetKeyCode() == WXK_DELETE || event.GetKeyCode() == WXK_BACK) {
        wxCommandEvent dummy;
        OnDeleteTool(dummy);
    } else {
        event.Skip();
    }
}
