#include "pane_devices.h"
#include <wx/sizer.h>

PaneDevices::PaneDevices(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    m_pg = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxPG_BOLD_MODIFIED | wxPG_SPLITTER_AUTO_CENTER);
    sizer->Add(m_pg, 1, wxEXPAND);
    SetSizer(sizer);

    UpdateProperties();
}

void PaneDevices::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    
    int devCount = sim_get_device_count(m_sim);
    for (int i = 0; i < devCount; i++) {
        char name[128];
        uint16_t start, end;
        if (sim_get_device_info(m_sim, i, name, sizeof(name), &start, &end) == 0) {
            for (uint32_t addr = start; addr <= end; addr++) {
                wxString propName = wxString::Format("%s_%04X", name, addr);
                uint8_t val = sim_mem_read_byte(m_sim, (uint16_t)addr);
                m_pg->SetPropertyValue(propName, wxString::Format("%02X", val));
            }
        }
    }
}

void PaneDevices::UpdateProperties() {
    m_pg->Clear();
    
    int devCount = sim_get_device_count(m_sim);
    for (int i = 0; i < devCount; i++) {
        char name[128];
        uint16_t start, end;
        if (sim_get_device_info(m_sim, i, name, sizeof(name), &start, &end) == 0) {
            m_pg->Append(new wxPropertyCategory(name));
            for (uint32_t addr = start; addr <= end; addr++) {
                wxString propName = wxString::Format("%s_%04X", name, addr);
                wxString label = wxString::Format("$%04X", addr);
                uint8_t val = sim_mem_read_byte(m_sim, (uint16_t)addr);
                m_pg->Append(new wxStringProperty(label, propName, wxString::Format("%02X", val)));
            }
        }
    }
}
