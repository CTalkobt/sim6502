#include "main_frame.h"
#include "gui_ids.h"
#include "pane_generic.h"
#include "pane_registers.h"
#include "pane_disassembly.h"
#include "pane_memory.h"
#include "pane_console.h"
#include <wx/statusbr.h>
#include <wx/msgdlg.h>
#include <wx/settings.h>
#include <wx/config.h>
#include <wx/display.h>
#include <wx/toolbar.h>
#include <wx/combobox.h>
#include <wx/artprov.h>

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_TIMER(1, MainFrame::OnTimer)
    EVT_MENU(wxID_EXIT, MainFrame::OnQuit)
    EVT_MENU(wxID_ABOUT, MainFrame::OnAbout)

    EVT_MENU(ID_SIM_RUN, MainFrame::OnRun)
    EVT_MENU(ID_SIM_PAUSE, MainFrame::OnPause)
    EVT_MENU(ID_SIM_STEP_INTO, MainFrame::OnStepInto)
    EVT_MENU(ID_SIM_STEP_OVER, MainFrame::OnStepOver)
    EVT_MENU(ID_SIM_RESET, MainFrame::OnReset)
    EVT_MENU(ID_SIM_TOGGLE_BREAKPOINT, MainFrame::OnToggleBreakpoint)
    EVT_MENU(ID_SIM_STEP_BACK, MainFrame::OnStepBack)
    EVT_MENU(ID_SIM_STEP_FORWARD, MainFrame::OnStepForward)
    EVT_MENU(ID_SIM_REVERSE_CONTINUE, MainFrame::OnReverseContinue)

    EVT_MENU(ID_FILE_LOAD, MainFrame::OnLoad)
    EVT_MENU(ID_FILE_BROWSE_LOAD, MainFrame::OnBrowseLoad)
    EVT_MENU(ID_FILE_SAVE_BIN, MainFrame::OnSaveBin)
    EVT_MENU(ID_FILE_NEW_PROJECT, MainFrame::OnNewProject)

    EVT_COMBOBOX(ID_TOOLBAR_PROC_COMBO, MainFrame::OnSelectProcessor)
    EVT_COMBOBOX(ID_TOOLBAR_MACH_COMBO, MainFrame::OnSelectMachine)

    EVT_MENU(ID_VIEW_GO_TO_ADDRESS, MainFrame::OnGoToAddress)
    EVT_MENU_RANGE(ID_VIEW_PANE_REGISTERS, ID_VIEW_PANE_VIC_REGS, MainFrame::OnTogglePane)
    EVT_MENU(ID_VIEW_LAYOUT_SAVE, MainFrame::OnTogglePane)
    EVT_MENU(ID_VIEW_LAYOUT_RESET, MainFrame::OnTogglePane)
wxEND_EVENT_TABLE()

MainFrame::MainFrame(const wxString& title)
    : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(1280, 800)),
      m_timer(this, 1),
      m_toolbar(NULL),
      m_base_font_size(13),
      m_theme(2),
      m_ui_scale(1.0f),
      m_running(false),
      m_initial_layout_done(false)
{
    m_sim = sim_create("6502");

    // Initialize AUI
    m_aui.SetManagedWindow(this);

    InitMenuBar();
    InitToolBar();
    InitStatusBar();
    InitPanes();

    LoadSettings();

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

void MainFrame::InitPanes() {
    RegisterPane(new PaneRegisters(this, m_sim), ID_VIEW_PANE_REGISTERS, wxAuiPaneInfo().Name("Registers").Caption("Registers").Left().Position(0));
    RegisterPane(new PaneDisassembly(this, m_sim), ID_VIEW_PANE_DISASSEMBLY, wxAuiPaneInfo().Name("Disassembly").Caption("Disassembly").CenterPane());
    RegisterPane(new PaneConsole(this, m_sim), ID_VIEW_PANE_CONSOLE, wxAuiPaneInfo().Name("Console").Caption("Console").Bottom().Position(0));

    RegisterPane(new PaneMemory(this, m_sim, 0), ID_VIEW_PANE_MEMORY_1, wxAuiPaneInfo().Name("Memory1").Caption("Memory 1").Right().Position(0).Hide());
    RegisterPane(new PaneMemory(this, m_sim, 1), ID_VIEW_PANE_MEMORY_2, wxAuiPaneInfo().Name("Memory2").Caption("Memory 2").Right().Position(1).Hide());
    RegisterPane(new PaneMemory(this, m_sim, 2), ID_VIEW_PANE_MEMORY_3, wxAuiPaneInfo().Name("Memory3").Caption("Memory 3").Right().Position(2).Hide());
    RegisterPane(new PaneMemory(this, m_sim, 3), ID_VIEW_PANE_MEMORY_4, wxAuiPaneInfo().Name("Memory4").Caption("Memory 4").Right().Position(3).Hide());

    RegisterPane(new GenericSimPane(this, m_sim, "Breakpoints", "Breakpoints"), ID_VIEW_PANE_BREAKPOINTS, wxAuiPaneInfo().Name("Breakpoints").Caption("Breakpoints").Bottom().Position(1).Hide());
    RegisterPane(new GenericSimPane(this, m_sim, "Trace Log", "TraceLog"), ID_VIEW_PANE_TRACE, wxAuiPaneInfo().Name("TraceLog").Caption("Trace Log").Bottom().Position(2).Hide());
    RegisterPane(new GenericSimPane(this, m_sim, "Stack", "Stack"), ID_VIEW_PANE_STACK, wxAuiPaneInfo().Name("Stack").Caption("Stack").Right().Position(4).Hide());
    RegisterPane(new GenericSimPane(this, m_sim, "Watch List", "Watches"), ID_VIEW_PANE_WATCHES, wxAuiPaneInfo().Name("Watches").Caption("Watch List").Right().Position(5).Hide());
    RegisterPane(new GenericSimPane(this, m_sim, "Snapshot Diff", "SnapDiff"), ID_VIEW_PANE_SNAP_DIFF, wxAuiPaneInfo().Name("SnapDiff").Caption("Snapshot Diff").Bottom().Position(3).Hide());

    RegisterPane(new GenericSimPane(this, m_sim, "Instruction Ref", "IRef"), ID_VIEW_PANE_IREF, wxAuiPaneInfo().Name("IRef").Caption("Instruction Ref").Bottom().Position(4).Hide());
    RegisterPane(new GenericSimPane(this, m_sim, "Symbols", "Symbols"), ID_VIEW_PANE_SYMBOLS, wxAuiPaneInfo().Name("Symbols").Caption("Symbols").Right().Position(6).Hide());
    RegisterPane(new GenericSimPane(this, m_sim, "Source View", "Source"), ID_VIEW_PANE_SOURCE, wxAuiPaneInfo().Name("Source").Caption("Source View").CenterPane().Hide());
    RegisterPane(new GenericSimPane(this, m_sim, "Profiler", "Profiler"), ID_VIEW_PANE_PROFILER, wxAuiPaneInfo().Name("Profiler").Caption("Profiler").Bottom().Position(5).Hide());
    RegisterPane(new GenericSimPane(this, m_sim, "Test Runner", "TestRunner"), ID_VIEW_PANE_TEST_RUNNER, wxAuiPaneInfo().Name("TestRunner").Caption("Test Runner").Bottom().Position(6).Hide());
    RegisterPane(new GenericSimPane(this, m_sim, "I/O Devices", "Devices"), ID_VIEW_PANE_DEVICES, wxAuiPaneInfo().Name("Devices").Caption("I/O Devices").Right().Position(7).Hide());
    RegisterPane(new GenericSimPane(this, m_sim, "Idiom Library", "Patterns"), ID_VIEW_PANE_PATTERNS, wxAuiPaneInfo().Name("Patterns").Caption("Idiom Library").Right().Position(8).Hide());

    RegisterPane(new GenericSimPane(this, m_sim, "VIC-II Screen", "VICScreen"), ID_VIEW_PANE_VIC_SCREEN, wxAuiPaneInfo().Name("VICScreen").Caption("VIC-II Screen").Right().Position(9).Hide());
    RegisterPane(new GenericSimPane(this, m_sim, "VIC-II Sprites", "VICSprites"), ID_VIEW_PANE_VIC_SPRITES, wxAuiPaneInfo().Name("VICSprites").Caption("VIC-II Sprites").Right().Position(10).Hide());
    RegisterPane(new GenericSimPane(this, m_sim, "VIC-II Registers", "VICRegs"), ID_VIEW_PANE_VIC_REGS, wxAuiPaneInfo().Name("VICRegs").Caption("VIC-II Registers").Right().Position(11).Hide());
}

void MainFrame::RegisterPane(SimPane* pane, int menu_id, const wxAuiPaneInfo& info) {
    m_panes[menu_id] = pane;
    m_pane_list.push_back(pane);
    m_aui.AddPane(pane, info);
    
    // Initial menu state
    GetMenuBar()->Check(menu_id, info.IsShown());
}

void MainFrame::UpdatePaneVisibility(int menu_id) {
    if (m_panes.count(menu_id)) {
        bool show = GetMenuBar()->IsChecked(menu_id);
        m_aui.GetPane(m_panes[menu_id]).Show(show);
        m_aui.Update();
    }
}

void MainFrame::InitToolBar() {
    m_toolbar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 
                                  wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_HORIZONTAL);

    // Placeholder for icons - in a real app we'd load bitmaps
    m_toolbar->AddTool(ID_FILE_LOAD, "Load", wxArtProvider::GetBitmap(wxART_FILE_OPEN));
    m_toolbar->AddSeparator();
    m_toolbar->AddTool(ID_SIM_STEP_INTO, "Step Into", wxArtProvider::GetBitmap(wxART_GO_FORWARD));
    m_toolbar->AddTool(ID_SIM_STEP_OVER, "Step Over", wxArtProvider::GetBitmap(wxART_REDO));
    m_toolbar->AddTool(ID_SIM_RUN, "Run", wxArtProvider::GetBitmap(wxART_GO_FORWARD));
    m_toolbar->AddTool(ID_SIM_PAUSE, "Pause", wxArtProvider::GetBitmap(wxART_DELETE));
    m_toolbar->AddTool(ID_SIM_RESET, "Reset", wxArtProvider::GetBitmap(wxART_UNDO));
    m_toolbar->AddSeparator();
    m_toolbar->AddTool(ID_SIM_TOGGLE_BREAKPOINT, "Breakpoint", wxArtProvider::GetBitmap(wxART_LIST_VIEW));

    m_toolbar->AddStretchSpacer();

    wxArrayString procs;
    procs.Add("6502"); procs.Add("6502-undoc"); procs.Add("65C02"); procs.Add("65CE02"); procs.Add("45GS02");
    wxComboBox *procCombo = new wxComboBox(m_toolbar, ID_TOOLBAR_PROC_COMBO, "6502", wxDefaultPosition, wxSize(100, -1), procs, wxCB_READONLY);
    m_toolbar->AddControl(procCombo);

    wxArrayString machs;
    machs.Add("raw6502"); machs.Add("c64"); machs.Add("c128"); machs.Add("mega65"); machs.Add("x16");
    wxComboBox *machCombo = new wxComboBox(m_toolbar, ID_TOOLBAR_MACH_COMBO, "c64", wxDefaultPosition, wxSize(100, -1), machs, wxCB_READONLY);
    m_toolbar->AddControl(machCombo);

    m_toolbar->Realize();
    
    m_aui.AddPane(m_toolbar, wxAuiPaneInfo().
                  Name("Toolbar").Caption("Toolbar").
                  ToolbarPane().Top().
                  LeftDockable(false).RightDockable(false));
}

void MainFrame::InitStatusBar() {
    wxStatusBar *sb = CreateStatusBar(3);
    int widths[] = { -1, 150, 150 };
    sb->SetStatusWidths(3, widths);
    UpdateStatus();
}

void MainFrame::UpdateStatus() {
    SetStatusText(m_running ? "RUNNING" : "PAUSED", 0);
    if (m_sim) {
        SetStatusText(sim_processor_name(m_sim), 1);
        CPU *cpu = sim_get_cpu(m_sim);
        if (cpu) {
            SetStatusText(wxString::Format("Cycles: %lu", cpu->cycles), 2);
        }
    }
}

void MainFrame::OnTimer(wxTimerEvent& WXUNUSED(event)) {
    if (m_sim && m_running) {
        sim_step(m_sim, 5000); 
    }
    
    // Refresh all panes
    SimSnapshot snap;
    snap.cpu = sim_get_cpu(m_sim);
    snap.mem = sim_get_memory(m_sim);
    snap.state = sim_get_state(m_sim);

    for (auto pane : m_pane_list) {
        if (m_aui.GetPane(pane).IsShown()) {
            pane->RefreshPane(snap);
        }
    }

    UpdateStatus();
}

void MainFrame::OnRun(wxCommandEvent& WXUNUSED(event)) {
    m_running = true;
    UpdateStatus();
}

void MainFrame::OnPause(wxCommandEvent& WXUNUSED(event)) {
    m_running = false;
    UpdateStatus();
}

void MainFrame::OnStepInto(wxCommandEvent& WXUNUSED(event)) {
    if (m_sim) {
        sim_step(m_sim, 1);
        UpdateStatus();
    }
}

void MainFrame::OnStepOver(wxCommandEvent& WXUNUSED(event)) {
    if (m_sim) {
        sim_step_over(m_sim);
        UpdateStatus();
    }
}

void MainFrame::OnReset(wxCommandEvent& WXUNUSED(event)) {
    if (m_sim) {
        sim_reset(m_sim);
        m_running = false;
        UpdateStatus();
    }
}

void MainFrame::OnToggleBreakpoint(wxCommandEvent& WXUNUSED(event)) {
}

void MainFrame::OnStepBack(wxCommandEvent& WXUNUSED(event)) {
    if (m_sim) {
        sim_history_step_back(m_sim);
        UpdateStatus();
    }
}

void MainFrame::OnStepForward(wxCommandEvent& WXUNUSED(event)) {
    if (m_sim) {
        sim_history_step_fwd(m_sim);
        UpdateStatus();
    }
}

void MainFrame::OnReverseContinue(wxCommandEvent& WXUNUSED(event)) {
}

void MainFrame::OnLoad(wxCommandEvent& WXUNUSED(event)) {
}

void MainFrame::OnBrowseLoad(wxCommandEvent& WXUNUSED(event)) {
}

void MainFrame::OnSaveBin(wxCommandEvent& WXUNUSED(event)) {
}

void MainFrame::OnNewProject(wxCommandEvent& WXUNUSED(event)) {
}

void MainFrame::OnSelectProcessor(wxCommandEvent& event) {
    if (m_sim) {
        sim_set_processor(m_sim, event.GetString().ToStdString().c_str());
        UpdateStatus();
    }
}

void MainFrame::OnSelectMachine(wxCommandEvent& event) {
    if (m_sim) {
        wxString mach = event.GetString();
        machine_type_t mt = MACHINE_RAW6502;
        if (mach == "c64") mt = MACHINE_C64;
        else if (mach == "c128") mt = MACHINE_C128;
        else if (mach == "mega65") mt = MACHINE_MEGA65;
        else if (mach == "x16") mt = MACHINE_X16;
        sim_set_machine_type(m_sim, mt);
        UpdateStatus();
    }
}

void MainFrame::OnQuit(wxCommandEvent& WXUNUSED(event)) {
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent& WXUNUSED(event)) {
    wxMessageBox("6502 Simulator (wxWidgets)", "About", wxOK | wxICON_INFORMATION);
}

void MainFrame::OnTogglePane(wxCommandEvent& event) {
    int id = event.GetId();
    if (id == ID_VIEW_LAYOUT_RESET) {
        m_initial_layout_done = false;
        // Reset to defaults: Registers, Disasm, Console shown, others hidden
        for (auto const& [menu_id, pane] : m_panes) {
            bool show = (menu_id == ID_VIEW_PANE_REGISTERS || 
                         menu_id == ID_VIEW_PANE_DISASSEMBLY || 
                         menu_id == ID_VIEW_PANE_CONSOLE);
            m_aui.GetPane(pane).Show(show);
            GetMenuBar()->Check(menu_id, show);
        }
        m_aui.Update();
    } else if (id == ID_VIEW_LAYOUT_SAVE) {
        // Just SaveSettings() is enough as it's called on exit, but we can force it
        SaveSettings();
        wxMessageBox("Layout saved to config.", "Layout");
    } else {
        UpdatePaneVisibility(id);
    }
}

void MainFrame::ApplyTheme() {
    bool is_dark = false;
    if (m_theme == 2) {
#if wxCHECK_VERSION(3, 1, 3)
        is_dark = wxSystemSettings::GetAppearance().IsDark();
#else
        wxColour window_bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
        is_dark = (window_bg.Red() + window_bg.Green() + window_bg.Blue()) / 3 < 128;
#endif
    } else {
        is_dark = (m_theme == 0);
    }

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

        wxString perspective;
        if (cfg->Read("Layout/Perspective", &perspective)) {
            m_aui.LoadPerspective(perspective);
            m_initial_layout_done = true;

            // Update menu checkmarks from loaded perspective
            for (auto const& [menu_id, pane] : m_panes) {
                GetMenuBar()->Check(menu_id, m_aui.GetPane(pane).IsShown());
            }
        }
    }

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

        cfg->Write("Layout/Perspective", m_aui.SavePerspective());
        
        cfg->Flush();
    }
}

void MainFrame::OnGoToAddress(wxCommandEvent& WXUNUSED(event)) {
}

void MainFrame::OnAddDevice(wxCommandEvent& WXUNUSED(event)) {
}
