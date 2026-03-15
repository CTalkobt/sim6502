#ifndef SIM_PANE_AUDIO_MIXER_H
#define SIM_PANE_AUDIO_MIXER_H

#include "pane_base.h"
#include <wx/slider.h>

class PaneAudioMixer : public SimPane {
public:
    PaneAudioMixer(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Audio Mixer"; }
    wxString GetPaneName() const override { return "AudioMixer"; }

private:
    void OnVolumeChange(wxCommandEvent& event);

    wxSlider* m_volumeSlider;
};

#endif
