#ifndef SIM6502_MAIN_FRAME_H
#define SIM6502_MAIN_FRAME_H

#include <wx/wx.h>
#include <wx/aui/aui.h>
#include "sim_api.h"
#include "pane_base.h"
#include <vector>
#include <map>

class MainFrame : public wxFrame {
public:
    MainFrame(const wxString& title);
    virtual ~MainFrame();

    void LoadFile(const wxString& path);
    void ApplyProcessor(const wxString& proc);
    void ApplyMachine(const wxString& target);
    void ApplyBreakpoint(const wxString& addr);
    void ApplyCycleLimit(unsigned long limit);
    void ApplySpeedScale(float scale);
    void ApplyDebug();
    void NavigateDisassembly(uint16_t addr);
    void NavigateMemory(uint16_t addr);
    void AddWatch(uint16_t addr, const wxString& label = "");
    void UpdatePaneCaption(SimPane* pane, const wxString& caption);

private:
    void InitMenuBar();
    void InitToolBar();
    void InitStatusBar();
    void InitPanes();

    void RegisterPane(SimPane* pane, int menu_id, const wxAuiPaneInfo& info);
    void UpdatePaneVisibility(int menu_id);
    void CheckMenuItem(int menu_id, bool check);

    // Event Handlers - Simulation
    void OnRun(wxCommandEvent& event);
    void OnPause(wxCommandEvent& event);
    void OnStepInto(wxCommandEvent& event);
    void OnStepOver(wxCommandEvent& event);
    void OnReset(wxCommandEvent& event);
    void OnClearCycles(wxCommandEvent& event);
    void OnToggleBreakpoint(wxCommandEvent& event);
    void OnStepBack(wxCommandEvent& event);
    void OnStepForward(wxCommandEvent& event);
    void OnReverseContinue(wxCommandEvent& event);
    
    // Event Handlers - File
    void OnLoad(wxCommandEvent& event);
    void OnBrowseLoad(wxCommandEvent& event);
    void OnSaveBin(wxCommandEvent& event);
    void OnNewProject(wxCommandEvent& event);

    // Event Handlers - Machine
    void OnSelectProcessor(wxCommandEvent& event);
    void OnSelectMachine(wxCommandEvent& event);
    void OnAddDevice(wxCommandEvent& event);

    // Event Handlers - View
    void OnGoToAddress(wxCommandEvent& event);
    void OnTogglePane(wxCommandEvent& event);
    void OnWindowArrange(wxCommandEvent& event);
    void OnPaneClose(wxAuiManagerEvent& event);

    void OnTimer(wxTimerEvent& event);
    void OnQuit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);

    void ApplyTheme();
    void LoadSettings();
    void SaveSettings();
    void UpdateStatus();

    sim_session_t *m_sim;
    wxAuiManager   m_aui;
    wxTimer        m_timer;
    wxAuiToolBar  *m_toolbar;
    wxComboBox    *m_procCombo;
    wxComboBox    *m_machCombo;

    std::map<int, SimPane*> m_panes;
    std::vector<SimPane*>   m_pane_list;

    // Settings
    int           m_base_font_size;
    int           m_theme; // 0=Dark, 1=Light, 2=Auto
    float         m_ui_scale;
    bool          m_running;      // UI-level "Run" state; true if the timer is actively calling sim_step()
    bool          m_initial_layout_done;
    unsigned long m_cycle_limit;  // 0 = no limit
    float         m_speed_scale;  // 0.0 = unlimited

    wxDECLARE_EVENT_TABLE();
};

#endif // SIM6502_MAIN_FRAME_H
