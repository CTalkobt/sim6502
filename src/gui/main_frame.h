#ifndef SIM6502_MAIN_FRAME_H
#define SIM6502_MAIN_FRAME_H

#include <wx/wx.h>
#include <wx/aui/aui.h>
#include "sim_api.h"

class MainFrame : public wxFrame {
public:
    MainFrame(const wxString& title);
    virtual ~MainFrame();

private:
    void OnTimer(wxTimerEvent& event);
    void OnQuit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);

    void ApplyTheme();
    void LoadSettings();
    void SaveSettings();

    sim_session_t *m_sim;
    wxAuiManager   m_aui;
    wxTimer        m_timer;

    // Settings
    int   m_base_font_size;
    int   m_theme; // 0=Dark, 1=Light, 2=Auto
    float m_ui_scale;

    wxDECLARE_EVENT_TABLE();
};

#endif // SIM6502_MAIN_FRAME_H
