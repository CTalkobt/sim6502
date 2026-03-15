#include "pane_audio_mixer.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

PaneAudioMixer::PaneAudioMixer(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    sizer->Add(new wxStaticText(this, wxID_ANY, "Master Volume (D418 bits 0-3)"), 0, wxALL, 5);
    m_volumeSlider = new wxSlider(this, wxID_ANY, 0, 0, 15, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS);
    sizer->Add(m_volumeSlider, 0, wxEXPAND | wxALL, 5);

    SetSizer(sizer);

    m_volumeSlider->Bind(wxEVT_SLIDER, &PaneAudioMixer::OnVolumeChange, this);
}

void PaneAudioMixer::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    if (IsShown()) {
        uint8_t val = sim_mem_read_byte(m_sim, 0xD418) & 0x0F;
        if (m_volumeSlider->GetValue() != val) {
            m_volumeSlider->SetValue(val);
        }
    }
}

void PaneAudioMixer::OnVolumeChange(wxCommandEvent& WXUNUSED(event)) {
    uint8_t current = sim_mem_read_byte(m_sim, 0xD418);
    uint8_t newVol = (uint8_t)m_volumeSlider->GetValue();
    sim_mem_write_byte(m_sim, 0xD418, (current & 0xF0) | (newVol & 0x0F));
}
