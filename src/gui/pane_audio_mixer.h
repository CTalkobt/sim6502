#ifndef SIM_PANE_AUDIO_MIXER_H
#define SIM_PANE_AUDIO_MIXER_H

#include "pane_base.h"
#include <wx/slider.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/scrolwin.h>
#include <vector>

class PaneAudioMixer : public SimPane {
public:
    PaneAudioMixer(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Audio Mixer"; }
    wxString GetPaneName() const override { return "AudioMixer"; }

private:
    struct SIDControl {
        wxSlider* volumeSlider;
        wxCheckBox* muteCheckbox;
        wxStaticText* volLabel;
        wxSlider* panSlider = nullptr;
        wxStaticText* panLabel = nullptr;
        int lastVolume;
        uint16_t addr;
    };

    void OnVolumeChange(wxCommandEvent& event);
    void OnMuteToggle(wxCommandEvent& event);
    void OnPanChange(wxCommandEvent& event);
    void UpdateVolumeLabel(SIDControl& ctrl);
    void UpdatePanLabel(SIDControl& ctrl);
    void CreateControls();

    std::vector<SIDControl> m_sids;
    wxScrolledWindow* m_scrollWin;
    wxBoxSizer* m_mainSizer;
};

#endif
