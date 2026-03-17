#include "pane_audio_mixer.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/checkbox.h>
#include <wx/statline.h>

PaneAudioMixer::PaneAudioMixer(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim) 
{
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    
    m_scrollWin = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    m_scrollWin->SetScrollRate(0, 10);
    
    m_mainSizer = new wxBoxSizer(wxVERTICAL);
    CreateControls();
    
    m_scrollWin->SetSizer(m_mainSizer);
    topSizer->Add(m_scrollWin, 1, wxEXPAND);
    
    SetSizer(topSizer);
}

void PaneAudioMixer::CreateControls() {
    m_mainSizer->Clear(true);
    m_sids.clear();

    int num_sids = 1;
    machine_type_t machine = sim_get_machine_type(m_sim);
    if (machine == MACHINE_MEGA65) {
        num_sids = 4;
    }

    for (int i = 0; i < num_sids; i++) {
        uint16_t addr = 0xD418 + (i * 0x20);
        
        wxString labelText = wxString::Format("SID #%d Settings ($%04X)", i + 1, addr);
        m_mainSizer->Add(new wxStaticText(m_scrollWin, wxID_ANY, labelText), 0, wxALL, 5);
        
        wxBoxSizer* chipSizer = new wxBoxSizer(wxVERTICAL);
        
        // --- Volume Row ---
        wxBoxSizer* volSizer = new wxBoxSizer(wxHORIZONTAL);
        volSizer->Add(new wxStaticText(m_scrollWin, wxID_ANY, "Vol:", wxDefaultPosition, wxSize(40, -1)), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 15);
        
        SIDControl ctrl;
        ctrl.addr = addr;
        ctrl.lastVolume = 15;
        
        ctrl.volumeSlider = new wxSlider(m_scrollWin, wxID_ANY, 0, 0, 15, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
        ctrl.volumeSlider->SetToolTip("Adjust master volume (0-15)");
        volSizer->Add(ctrl.volumeSlider, 1, wxEXPAND | wxALL, 5);
        
        ctrl.volLabel = new wxStaticText(m_scrollWin, wxID_ANY, "0%", wxDefaultPosition, wxSize(40, -1));
        volSizer->Add(ctrl.volLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        
        ctrl.muteCheckbox = new wxCheckBox(m_scrollWin, wxID_ANY, "Mute");
        ctrl.muteCheckbox->SetToolTip("Mute/unmute this SID chip");
        volSizer->Add(ctrl.muteCheckbox, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
        
        chipSizer->Add(volSizer, 0, wxEXPAND);

        // --- Volume Labels Row ---
        wxBoxSizer* volLabelsSizer = new wxBoxSizer(wxHORIZONTAL);
        volLabelsSizer->Add(60, 0); // Spacer to align with slider start

        for (int v = 0; v <= 15; ++v) {
            wxStaticText* label = new wxStaticText(m_scrollWin, wxID_ANY, wxString::Format("%d", v), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);
            label->SetFont(label->GetFont().Smaller().Smaller()); // Use even smaller font for 16 labels
            label->SetForegroundColour(wxColour(128, 128, 128));
            volLabelsSizer->Add(label, 1, wxEXPAND);
        }
        
        volLabelsSizer->Add(100, 0); // Spacer for percentage label and mute checkbox area
        chipSizer->Add(volLabelsSizer, 0, wxEXPAND | wxBOTTOM, 5);
        
        // --- Panning Row (MEGA65 only) ---
        if (machine == MACHINE_MEGA65) {
            wxBoxSizer* panSizer = new wxBoxSizer(wxHORIZONTAL);
            panSizer->Add(new wxStaticText(m_scrollWin, wxID_ANY, "Pan:", wxDefaultPosition, wxSize(40, -1)), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 15);
            
            // Pan from -100 (Left) to 100 (Right), 0 is Center
            int initial_pan = (i < 2) ? 50 : -50; // SIDs 1&2 Right (50), 3&4 Left (-50)
            
            ctrl.panSlider = new wxSlider(m_scrollWin, wxID_ANY, initial_pan, -100, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
            ctrl.panSlider->SetToolTip("Adjust stereo panning (Left - Center - Right)");
            panSizer->Add(ctrl.panSlider, 1, wxEXPAND | wxALL, 5);
            
            ctrl.panLabel = new wxStaticText(m_scrollWin, wxID_ANY, "C", wxDefaultPosition, wxSize(40, -1));
            panSizer->Add(ctrl.panLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
            
            chipSizer->Add(panSizer, 0, wxEXPAND);

            // --- Panning Labels Row ---
            wxBoxSizer* labelsSizer = new wxBoxSizer(wxHORIZONTAL);
            labelsSizer->Add(60, 0); // Spacer to align with slider start (approx offset of "Pan:" + padding)
            
            auto addLabel = [&](const wxString& text, int proportion) {
                wxStaticText* label = new wxStaticText(m_scrollWin, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);
                label->SetFont(label->GetFont().Smaller());
                label->SetForegroundColour(wxColour(128, 128, 128)); // Dim the reference labels
                labelsSizer->Add(label, proportion, wxEXPAND);
            };

            addLabel("-100", 1);
            addLabel("-50", 1);
            addLabel("0", 1);
            addLabel("50", 1);
            addLabel("100", 1);
            
            labelsSizer->Add(50, 0); // Spacer for the pan value label and mute checkbox area
            
            chipSizer->Add(labelsSizer, 0, wxEXPAND | wxBOTTOM, 5);
            
            ctrl.panSlider->SetClientData((void*)(uintptr_t)i);
            ctrl.panSlider->Bind(wxEVT_SLIDER, &PaneAudioMixer::OnPanChange, this);
        }
        
        m_mainSizer->Add(chipSizer, 0, wxEXPAND | wxBOTTOM, 10);
        m_mainSizer->Add(new wxStaticLine(m_scrollWin, wxID_ANY), 0, wxEXPAND | wxLEFT | wxRIGHT, 5);
        
        // Use client data to store the index
        ctrl.volumeSlider->SetClientData((void*)(uintptr_t)i);
        ctrl.muteCheckbox->SetClientData((void*)(uintptr_t)i);
        
        ctrl.volumeSlider->Bind(wxEVT_SLIDER, &PaneAudioMixer::OnVolumeChange, this);
        ctrl.muteCheckbox->Bind(wxEVT_CHECKBOX, &PaneAudioMixer::OnMuteToggle, this);
        
        if (ctrl.panLabel) UpdatePanLabel(ctrl);
        m_sids.push_back(ctrl);
    }
    
    m_mainSizer->Layout();
    m_scrollWin->FitInside();
}

void PaneAudioMixer::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    if (IsShown()) {
        int expected_sids = 1;
        machine_type_t machine = sim_get_machine_type(m_sim);
        if (machine == MACHINE_MEGA65) expected_sids = 4;
        
        // Also check if we need to show/hide panning if machine changed but sid count didn't 
        // (though SID count currently defines panning availability, this is safer)
        bool needs_rebuild = (int)m_sids.size() != expected_sids;
        if (!needs_rebuild && !m_sids.empty()) {
            bool currently_has_pan = (m_sids[0].panSlider != nullptr);
            bool should_have_pan = (machine == MACHINE_MEGA65);
            if (currently_has_pan != should_have_pan) needs_rebuild = true;
        }

        if (needs_rebuild) {
            CreateControls();
        }

        for (auto& ctrl : m_sids) {
            uint8_t val = sim_mem_read_byte(m_sim, ctrl.addr) & 0x0F;
            if (ctrl.volumeSlider->GetValue() != val) {
                ctrl.volumeSlider->SetValue(val);
                UpdateVolumeLabel(ctrl);
                ctrl.muteCheckbox->SetValue(val == 0);
                if (val > 0) {
                    ctrl.lastVolume = val;
                }
            }
        }
    }
}

void PaneAudioMixer::UpdateVolumeLabel(SIDControl& ctrl) {
    int value = ctrl.volumeSlider->GetValue();
    int percent = (value * 100) / 15;
    ctrl.volLabel->SetLabel(wxString::Format("%d%%", percent));
}

void PaneAudioMixer::UpdatePanLabel(SIDControl& ctrl) {
    if (!ctrl.panSlider || !ctrl.panLabel) return;
    int val = ctrl.panSlider->GetValue();
    if (val == 0) ctrl.panLabel->SetLabel("C");
    else if (val < 0) ctrl.panLabel->SetLabel(wxString::Format("L%d", -val));
    else ctrl.panLabel->SetLabel(wxString::Format("R%d", val));
}

void PaneAudioMixer::OnVolumeChange(wxCommandEvent& event) {
    wxSlider* slider = (wxSlider*)event.GetEventObject();
    int idx = (int)(uintptr_t)slider->GetClientData();
    if (idx < 0 || idx >= (int)m_sids.size()) return;
    
    SIDControl& ctrl = m_sids[idx];
    int val = ctrl.volumeSlider->GetValue();
    uint8_t current = sim_mem_read_byte(m_sim, ctrl.addr);
    sim_mem_write_byte(m_sim, ctrl.addr, (current & 0xF0) | ((uint8_t)val & 0x0F));
    
    UpdateVolumeLabel(ctrl);
    ctrl.muteCheckbox->SetValue(val == 0);
    if (val > 0) {
        ctrl.lastVolume = val;
    }
}

void PaneAudioMixer::OnMuteToggle(wxCommandEvent& event) {
    wxCheckBox* cb = (wxCheckBox*)event.GetEventObject();
    int idx = (int)(uintptr_t)cb->GetClientData();
    if (idx < 0 || idx >= (int)m_sids.size()) return;

    SIDControl& ctrl = m_sids[idx];
    uint8_t current = sim_mem_read_byte(m_sim, ctrl.addr);
    int newVal = 0;
    
    if (ctrl.muteCheckbox->IsChecked()) {
        ctrl.lastVolume = ctrl.volumeSlider->GetValue();
        newVal = 0;
    } else {
        newVal = (ctrl.lastVolume > 0) ? ctrl.lastVolume : 15;
    }
    
    ctrl.volumeSlider->SetValue(newVal);
    UpdateVolumeLabel(ctrl);
    sim_mem_write_byte(m_sim, ctrl.addr, (current & 0xF0) | ((uint8_t)newVal & 0x0F));
}

void PaneAudioMixer::OnPanChange(wxCommandEvent& event) {
    wxSlider* slider = (wxSlider*)event.GetEventObject();
    int idx = (int)(uintptr_t)slider->GetClientData();
    if (idx < 0 || idx >= (int)m_sids.size()) return;
    
    SIDControl& ctrl = m_sids[idx];
    UpdatePanLabel(ctrl);
    
    // Future implementation: Write to panning hardware register here
    // For now, it only updates the UI state.
}
