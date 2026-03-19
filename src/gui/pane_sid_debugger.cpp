#include "pane_sid_debugger.h"
#include <wx/sizer.h>

PaneSIDDebugger::PaneSIDDebugger(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    m_pg = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxPG_BOLD_MODIFIED | wxPG_SPLITTER_AUTO_CENTER);
    sizer->Add(m_pg, 1, wxEXPAND);
    SetSizer(sizer);

    InitProperties();
}

void PaneSIDDebugger::InitProperties() {
    m_pg->Clear();
    
    for (int v = 0; v < 3; v++) {
        m_pg->Append(new wxPropertyCategory(wxString::Format("Voice %d", v+1)));
        m_pg->Append(new wxStringProperty("Freq Lo", wxString::Format("D4%02X", 0 + v*7), "00"));
        m_pg->Append(new wxStringProperty("Freq Hi", wxString::Format("D4%02X", 1 + v*7), "00"));
        m_pg->Append(new wxStringProperty("PW Lo",   wxString::Format("D4%02X", 2 + v*7), "00"));
        m_pg->Append(new wxStringProperty("PW Hi",   wxString::Format("D4%02X", 3 + v*7), "00"));
        m_pg->Append(new wxStringProperty("Control", wxString::Format("D4%02X", 4 + v*7), "00"));
        m_pg->Append(new wxStringProperty("Attack/Dec", wxString::Format("D4%02X", 5 + v*7), "00"));
        m_pg->Append(new wxStringProperty("Sust/Rel",   wxString::Format("D4%02X", 6 + v*7), "00"));
    }

    m_pg->Append(new wxPropertyCategory("Filter & Volume"));
    m_pg->Append(new wxStringProperty("Cutoff Lo", "D415", "00"));
    m_pg->Append(new wxStringProperty("Cutoff Hi", "D416", "00"));
    m_pg->Append(new wxStringProperty("Reso/Filt", "D417", "00"));
    m_pg->Append(new wxStringProperty("Mode/Vol",  "D418", "00"));
}

void PaneSIDDebugger::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    if (IsShown()) {
        for (int i = 0; i < 0x19; i++) {
            uint16_t addr = 0xD400 + i;
            uint8_t val = sim_mem_read_byte(m_sim, addr);
            m_pg->SetPropertyValue(wxString::Format("D4%02X", i), wxString::Format("%02X", val));
        }
    }
}
