#include "main_frame.h"
#include <wx/statusbr.h>
#include <wx/msgdlg.h>
#include <wx/settings.h>
#include <wx/config.h>
#include <wx/display.h>

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_TIMER(1, MainFrame::OnTimer)
    EVT_MENU(wxID_EXIT, MainFrame::OnQuit)
    EVT_MENU(wxID_ABOUT, MainFrame::OnAbout)
wxEND_EVENT_TABLE()

MainFrame::MainFrame(const wxString& title)
    : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(1024, 768)),
      m_timer(this, 1),
      m_base_font_size(13),
      m_theme(2),
      m_ui_scale(1.0f)
{
    LoadSettings();

    m_sim = sim_create("6502");

    // Initialize AUI
    m_aui.SetManagedWindow(this);

    // Create a status bar
    CreateStatusBar(3);
    SetStatusText("Ready", 0);
    SetStatusText("6502", 1);
    SetStatusText("Cycles: 0", 2);

    // Create a simple menu
    wxMenu *fileMenu = new wxMenu;
    fileMenu->Append(wxID_EXIT);

    wxMenu *helpMenu = new wxMenu;
    helpMenu->Append(wxID_ABOUT);

    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(helpMenu, "&Help");
    SetMenuBar(menuBar);

    // Apply initial theme
    ApplyTheme();

    // Start simulation timer (e.g., 60Hz)
    m_timer.Start(16);

    m_aui.Update();
}

MainFrame::~MainFrame() {
    SaveSettings();
    m_timer.Stop();
    m_aui.UnInit();
    if (m_sim) {
        sim_destroy(m_sim);
    }
}

void MainFrame::OnTimer(wxTimerEvent& WXUNUSED(event)) {
    // This will replace the ImGui frame loop's execute_steps() call
    if (m_sim) {
        CPU *cpu = sim_get_cpu(m_sim);
        if (cpu) {
            SetStatusText(wxString::Format("Cycles: %lu", cpu->cycles), 2);
        }
    }
}

void MainFrame::OnQuit(wxCommandEvent& WXUNUSED(event)) {
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent& WXUNUSED(event)) {
    wxMessageBox("6502 Simulator (wxWidgets)", "About", wxOK | wxICON_INFORMATION);
}

void MainFrame::ApplyTheme() {
    bool is_dark = false;
    if (m_theme == 2) {
        // Auto-detect using wxSystemAppearance if available (wx 3.1.3+)
#if wxCHECK_VERSION(3, 1, 3)
        is_dark = wxSystemSettings::GetAppearance().IsDark();
#else
        // Fallback or manual detection
        wxColour window_bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
        is_dark = (window_bg.Red() + window_bg.Green() + window_bg.Blue()) / 3 < 128;
#endif
    } else {
        is_dark = (m_theme == 0);
    }

    // Set frame background
    if (is_dark) {
        SetBackgroundColour(wxColour(32, 32, 32));
    } else {
        SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    }
}

void MainFrame::LoadSettings() {
    wxConfigBase *cfg = wxConfigBase::Get();
    if (cfg) {
        cfg->Read("Settings/FontSize", &m_base_font_size, 13);
        cfg->Read("Settings/Theme", &m_theme, 2);
        
        int w, h, x, y;
        if (cfg->Read("Window/Width", &w) && cfg->Read("Window/Height", &h)) {
            SetSize(w, h);
        }
        if (cfg->Read("Window/X", &x) && cfg->Read("Window/Y", &y)) {
            SetPosition(wxPoint(x, y));
        }
    }

    // Detect UI scale (port from main.cpp logic)
    wxString env_scale;
    if (wxGetEnv("SIM6502_SCALE", &env_scale)) {
        double v;
        if (env_scale.ToDouble(&v) && v >= 0.5 && v <= 8.0) m_ui_scale = v;
    } else if (wxGetEnv("GDK_SCALE", &env_scale)) {
        double v;
        if (env_scale.ToDouble(&v) && v >= 1.0 && v <= 8.0) m_ui_scale = v;
    } else if (wxGetEnv("QT_SCALE_FACTOR", &env_scale)) {
        double v;
        if (env_scale.ToDouble(&v) && v >= 1.0 && v <= 8.0) m_ui_scale = v;
    } else {
        // Simple DPI-based detection
        wxDisplay display(this);
        wxSize ppi = display.GetPPI();
        if (ppi.y > 0 && ppi.y != 96) {
            m_ui_scale = floorf((float)ppi.y / 96.0f * 4.0f + 0.5f) / 4.0f;
        } else {
            wxSize mode = display.GetGeometry().GetSize();
            if (mode.x >= 3840) m_ui_scale = 2.00f;
            else if (mode.x >= 2560) m_ui_scale = 1.50f;
        }
    }
}

void MainFrame::SaveSettings() {
    wxConfigBase *cfg = wxConfigBase::Get();
    if (cfg) {
        cfg->Write("Settings/FontSize", m_base_font_size);
        cfg->Write("Settings/Theme", m_theme);
        
        wxSize sz = GetSize();
        cfg->Write("Window/Width", sz.x);
        cfg->Write("Window/Height", sz.y);
        
        wxPoint pos = GetPosition();
        cfg->Write("Window/X", pos.x);
        cfg->Write("Window/Y", pos.y);
        
        cfg->Flush();
    }
}
