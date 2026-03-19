#include "pane_devices.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <map>
#include <vector>

PaneDevices::PaneDevices(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim), m_lastDevCount(-1) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddTool(601, "Expand All", wxArtProvider::GetBitmap(wxART_PLUS), "Expand all device categories");
    toolBar->AddTool(602, "Collapse All", wxArtProvider::GetBitmap(wxART_MINUS), "Collapse all device categories");
    toolBar->Realize();
    sizer->Add(toolBar, 0, wxEXPAND);

    m_pg = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxPG_BOLD_MODIFIED | wxPG_SPLITTER_AUTO_CENTER);
    m_pg->SetExtraStyle(wxPG_EX_ENABLE_TLP_TRACKING);
    m_pg->SetColumnCount(3);
    sizer->Add(m_pg, 1, wxEXPAND);
    
    m_msg = new wxStaticText(this, wxID_ANY, "Load a machine (e.g., C64) to see I/O device state.", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    sizer->Add(m_msg, 0, wxALL | wxALIGN_CENTER, 20);
    m_msg->Hide();
    
    SetSizer(sizer);

    m_pg->Bind(wxEVT_PG_CHANGED, &PaneDevices::OnPropertyChange, this);
    toolBar->Bind(wxEVT_TOOL, &PaneDevices::OnExpandAll, this, 601);
    toolBar->Bind(wxEVT_TOOL, &PaneDevices::OnCollapseAll, this, 602);

    UpdateProperties();
}

void PaneDevices::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    
    int devCount = sim_get_device_count(m_sim);
    if (devCount != m_lastDevCount) {
        UpdateProperties();
    }

    if (devCount > 0 && m_pg->IsShown()) {
        for (int i = 0; i < devCount; i++) {
            char name[128];
            uint16_t start, end;
            if (sim_get_device_info(m_sim, i, name, sizeof(name), &start, &end) == 0) {
                for (uint32_t addr = start; addr <= end; addr++) {
                    wxString propName = wxString::Format("addr_%04X", addr);
                    if (m_pg->GetPropertyByName(propName)) {
                        uint8_t val = sim_mem_read_byte(m_sim, (uint16_t)addr);
                        m_pg->SetPropertyValue(propName, wxString::Format("%02X", val));
                    }
                }
            }
        }
    }
}

void PaneDevices::UpdateProperties() {
    m_pg->Clear();
    
    int devCount = sim_get_device_count(m_sim);
    m_lastDevCount = devCount;

    if (devCount == 0) {
        m_pg->Hide();
        m_msg->Show();
    } else {
        m_pg->Show();
        m_msg->Hide();
        
        // Group registrations by name
        std::map<wxString, std::vector<std::pair<uint16_t, uint16_t>>> groups;
        for (int i = 0; i < devCount; i++) {
            char name[128];
            uint16_t start, end;
            if (sim_get_device_info(m_sim, i, name, sizeof(name), &start, &end) == 0) {
                groups[wxString(name)].push_back({start, end});
            }
        }

        int symCount = sim_sym_count(m_sim);

        for (auto const& [name, ranges] : groups) {
            wxString catTitle = name + " (";
            for (size_t r = 0; r < ranges.size(); r++) {
                if (r > 0) catTitle += ", ";
                catTitle += wxString::Format("$%04X-$%04X", ranges[r].first, ranges[r].second);
            }
            catTitle += ")";

            wxPGProperty* cat = m_pg->Append(new wxPropertyCategory(catTitle));
            
            for (auto const& range : ranges) {
                for (uint32_t addr = range.first; addr <= range.second; addr++) {
                    wxString propName = wxString::Format("addr_%04X", addr);
                    wxString label = wxString::Format("$%04X", addr);
                    uint8_t val = sim_mem_read_byte(m_sim, (uint16_t)addr);
                    
                    wxString desc;
                    for (int s = 0; s < symCount; s++) {
                        uint16_t s_addr;
                        char s_name[128];
                        char s_comment[256];
                        if (sim_sym_get_idx(m_sim, s, &s_addr, s_name, sizeof(s_name), NULL, s_comment, sizeof(s_comment)) == 1) {
                            if (s_addr == (uint16_t)addr) {
                                if (!desc.IsEmpty()) desc += ", ";
                                desc += s_name;
                                if (s_comment[0]) {
                                    desc += " (";
                                    desc += s_comment;
                                    desc += ")";
                                }
                            }
                        }
                    }

                    wxPGProperty* prop = m_pg->AppendIn(cat, new wxStringProperty(label, propName, wxString::Format("%02X", val)));
                    if (!desc.IsEmpty()) {
                        m_pg->SetPropertyCell(prop, 2, desc);
                    }
                }
            }
        }
    }
    GetSizer()->Layout();
}

void PaneDevices::OnPropertyChange(wxPropertyGridEvent& event) {
    wxPGProperty* prop = event.GetProperty();
    if (!prop) return;

    wxString name = prop->GetName();
    // name is formatted as "addr_ADDR"
    if (name.StartsWith("addr_")) {
        wxString addrStr = name.Mid(5);
        unsigned long addr;
        if (addrStr.ToULong(&addr, 16)) {
            wxString valStr = prop->GetValueAsString();
            unsigned long val;
            if (valStr.ToULong(&val, 16)) {
                sim_mem_write_byte(m_sim, (uint16_t)addr, (uint8_t)val);
            }
        }
    }
}

void PaneDevices::OnExpandAll(wxCommandEvent& WXUNUSED(event)) {
    if (m_pg) m_pg->ExpandAll(true);
}

void PaneDevices::OnCollapseAll(wxCommandEvent& WXUNUSED(event)) {
    if (m_pg) m_pg->ExpandAll(false);
}
