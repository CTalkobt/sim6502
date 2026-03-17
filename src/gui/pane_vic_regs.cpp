#include "pane_vic_regs.h"
#include <wx/sizer.h>

PaneVICRegs::PaneVICRegs(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    m_pg = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxPG_BOLD_MODIFIED | wxPG_SPLITTER_AUTO_CENTER);
    sizer->Add(m_pg, 1, wxEXPAND);
    SetSizer(sizer);

    m_pg->Bind(wxEVT_PG_CHANGED, &PaneVICRegs::OnPropertyChange, this);

    InitProperties();
}

void PaneVICRegs::OnPropertyChange(wxPropertyGridEvent& event) {
    wxPGProperty* prop = event.GetProperty();
    if (!prop) return;

    wxString name = prop->GetName();
    wxVariant value = prop->GetValue();
    wxString valStr = value.GetString();

    long addrVal;
    if (name.ToLong(&addrVal, 16)) {
        long byteVal;
        if (valStr.ToLong(&byteVal, 16)) {
            sim_mem_write_byte(m_sim, (uint16_t)addrVal, (uint8_t)byteVal);
        }
    }
}

void PaneVICRegs::InitProperties() {
    m_pg->Clear();
    
    m_pg->Append(new wxPropertyCategory("Control"));
    m_pg->Append(new wxStringProperty("D011 (Control 1)", "D011", "00"));
    m_pg->Append(new wxStringProperty("D012 (Raster)",    "D012", "00"));
    m_pg->Append(new wxStringProperty("D016 (Control 2)", "D016", "00"));
    m_pg->Append(new wxStringProperty("D018 (Memory)",    "D018", "00"));
    m_pg->Append(new wxStringProperty("D019 (Interrupt)", "D019", "00"));
    m_pg->Append(new wxStringProperty("D01A (Int Mask)",  "D01A", "00"));

    m_pg->Append(new wxPropertyCategory("Colours"));
    m_pg->Append(new wxStringProperty("D020 (Border)",     "D020", "00"));
    m_pg->Append(new wxStringProperty("D021 (BG 0)",      "D021", "00"));
    m_pg->Append(new wxStringProperty("D022 (BG 1)",      "D022", "00"));
    m_pg->Append(new wxStringProperty("D023 (BG 2)",      "D023", "00"));
    m_pg->Append(new wxStringProperty("D024 (BG 3)",      "D024", "00"));

    m_pg->Append(new wxPropertyCategory("Sprites"));
    for (int i = 0; i < 8; i++) {
        m_pg->Append(new wxStringProperty(wxString::Format("D027 (Sprite %d Col)", i), 
                                          wxString::Format("D02%X", 7+i), "00"));
    }
}

void PaneVICRegs::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    if (IsShown()) {
        static const uint16_t addrs[] = { 0xD011, 0xD012, 0xD016, 0xD018, 0xD019, 0xD01A, 0xD020, 0xD021, 0xD022, 0xD023, 0xD024 };
        for (uint16_t addr : addrs) {
            uint8_t val = sim_mem_read_byte(m_sim, addr);
            m_pg->SetPropertyValue(wxString::Format("%04X", addr), wxString::Format("%02X", val));
        }
        for (int i = 0; i < 8; i++) {
            uint16_t addr = 0xD027 + i;
            uint8_t val = sim_mem_read_byte(m_sim, addr);
            m_pg->SetPropertyValue(wxString::Format("D02%X", 7+i), wxString::Format("%02X", val));
        }
    }
}
