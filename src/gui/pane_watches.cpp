#include "pane_watches.h"
#include <wx/menu.h>
#include <wx/textctrl.h>
#include <wx/msgdlg.h>
#include <wx/config.h>

PaneWatches::PaneWatches(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, "Label", wxLIST_FORMAT_LEFT, 100);
    m_list->InsertColumn(1, "Address", wxLIST_FORMAT_LEFT, 60);
    m_list->InsertColumn(2, "Hex", wxLIST_FORMAT_LEFT, 40);
    m_list->InsertColumn(3, "Dec", wxLIST_FORMAT_LEFT, 40);
    m_list->InsertColumn(4, "ASCII", wxLIST_FORMAT_LEFT, 40);

    sizer->Add(m_list, 1, wxEXPAND);
    SetSizer(sizer);

    m_list->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &PaneWatches::OnContextMenu, this);
}

void PaneWatches::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    m_list->DeleteAllItems();
    for (size_t i = 0; i < m_watches.size(); i++) {
        uint8_t val = sim_mem_read_byte(m_sim, m_watches[i].address);
        
        long item = m_list->InsertItem((long)i, m_watches[i].label);
        m_list->SetItem(item, 1, wxString::Format("%04X", m_watches[i].address));
        m_list->SetItem(item, 2, wxString::Format("%02X", val));
        m_list->SetItem(item, 3, wxString::Format("%d", val));
        
        char c = (val >= 32 && val < 127) ? (char)val : '.';
        m_list->SetItem(item, 4, wxString::Format("%c", c));
        
        if (val != m_watches[i].last_val) {
            m_list->SetItemTextColour(item, *wxRED);
            m_watches[i].last_val = val;
        }
    }
}

void PaneWatches::SaveState(wxConfigBase* cfg) {
    cfg->Write("Watches/Count", (int)m_watches.size());
    for (int i = 0; i < (int)m_watches.size(); i++) {
        cfg->Write(wxString::Format("Watches/Label_%d", i), m_watches[i].label);
        cfg->Write(wxString::Format("Watches/Addr_%d", i), (int)m_watches[i].address);
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
        w.last_val = sim_mem_read_byte(m_sim, w.address);
        m_watches.push_back(w);
    }
}

void PaneWatches::OnContextMenu(wxListEvent& WXUNUSED(event)) {
    wxMenu menu;
    menu.Append(301, "Add Watch...");
    menu.Append(302, "Delete Watch");
    
    int id = GetPopupMenuSelectionFromUser(menu);
    if (id == 301) {
        wxTextEntryDialog dlg(this, "Enter address (hex):", "Add Watch");
        if (dlg.ShowModal() == wxID_OK) {
            unsigned long addr;
            if (dlg.GetValue().ToULong(&addr, 16)) {
                Watch w;
                w.address = (uint16_t)addr;
                w.label = wxString::Format("Watch at %04X", w.address);
                w.last_val = sim_mem_read_byte(m_sim, w.address);
                m_watches.push_back(w);
                RefreshPane(SimSnapshot{});
            }
        }
    } else if (id == 302) {
        long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel != -1) {
            m_watches.erase(m_watches.begin() + sel);
            RefreshPane(SimSnapshot{});
        }
    }
}
