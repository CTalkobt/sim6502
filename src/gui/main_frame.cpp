#include "main_frame.h"
#include "gui_ids.h"
#include "pane_generic.h"
#include "pane_registers.h"
#include "pane_disassembly.h"
#include "pane_memory.h"
#include "pane_console.h"
#include "pane_breakpoints.h"
#include "pane_trace.h"
#include "pane_stack.h"
#include "pane_watches.h"
#include "pane_snap_diff.h"
#include "pane_iref.h"
#include "pane_symbols.h"
#include "pane_source.h"
#include "pane_profiler.h"
#include "pane_test_runner.h"
#include "pane_devices.h"
#include "pane_patterns.h"
#include "pane_vic_screen.h"
#include "pane_vic_sprites.h"
#include "pane_vic_regs.h"
#include "pane_sid_debugger.h"
#include "pane_audio_mixer.h"
#include "dialogs.h"
#include <wx/statusbr.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/textdlg.h>
#include <wx/choicdlg.h>
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
    EVT_MENU(ID_MACH_ADD_DEVICE, MainFrame::OnAddDevice)
    EVT_MENU(ID_MACH_SID_DEBUGGER, MainFrame::OnTogglePane)
    EVT_MENU(ID_MACH_AUDIO_MIXER, MainFrame::OnTogglePane)
    EVT_MENU_RANGE(ID_VIEW_PANE_REGISTERS, ID_VIEW_PANE_VIC_REGS, MainFrame::OnTogglePane)
    EVT_MENU(ID_VIEW_LAYOUT_SAVE, MainFrame::OnTogglePane)
    EVT_MENU(ID_VIEW_LAYOUT_RESET, MainFrame::OnTogglePane)
wxEND_EVENT_TABLE()

MainFrame::MainFrame(const wxString& title)
    : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(1280, 800)),
      m_timer(this, 1),
      m_toolbar(NULL),
      m_procCombo(NULL),
      m_machCombo(NULL),
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

    RegisterPane(new PaneBreakpoints(this, m_sim), ID_VIEW_PANE_BREAKPOINTS, wxAuiPaneInfo().Name("Breakpoints").Caption("Breakpoints").Bottom().Position(1).Hide());
    RegisterPane(new PaneTrace(this, m_sim), ID_VIEW_PANE_TRACE, wxAuiPaneInfo().Name("TraceLog").Caption("Trace Log").Bottom().Position(2).Hide());
    RegisterPane(new PaneStack(this, m_sim), ID_VIEW_PANE_STACK, wxAuiPaneInfo().Name("Stack").Caption("Stack").Right().Position(4).Hide());
    RegisterPane(new PaneWatches(this, m_sim), ID_VIEW_PANE_WATCHES, wxAuiPaneInfo().Name("Watches").Caption("Watch List").Right().Position(5).Hide());
    RegisterPane(new PaneSnapDiff(this, m_sim), ID_VIEW_PANE_SNAP_DIFF, wxAuiPaneInfo().Name("SnapDiff").Caption("Snapshot Diff").Bottom().Position(3).Hide());

    RegisterPane(new PaneIRef(this, m_sim), ID_VIEW_PANE_IREF, wxAuiPaneInfo().Name("IRef").Caption("Instruction Ref").Bottom().Position(4).Hide());
    RegisterPane(new PaneSymbols(this, m_sim), ID_VIEW_PANE_SYMBOLS, wxAuiPaneInfo().Name("Symbols").Caption("Symbols").Right().Position(6).Hide());
    RegisterPane(new PaneSource(this, m_sim), ID_VIEW_PANE_SOURCE, wxAuiPaneInfo().Name("Source").Caption("Source View").CenterPane().Hide());
    RegisterPane(new PaneProfiler(this, m_sim), ID_VIEW_PANE_PROFILER, wxAuiPaneInfo().Name("Profiler").Caption("Profiler").Bottom().Position(5).Hide());
    RegisterPane(new PaneTestRunner(this, m_sim), ID_VIEW_PANE_TEST_RUNNER, wxAuiPaneInfo().Name("TestRunner").Caption("Test Runner").Bottom().Position(6).Hide());
    RegisterPane(new PaneDevices(this, m_sim), ID_VIEW_PANE_DEVICES, wxAuiPaneInfo().Name("Devices").Caption("I/O Devices").Right().Position(7).Hide());
    RegisterPane(new PanePatterns(this, m_sim), ID_VIEW_PANE_PATTERNS, wxAuiPaneInfo().Name("Patterns").Caption("Idiom Library").Right().Position(8).Hide());

    RegisterPane(new PaneVICScreen(this, m_sim), ID_VIEW_PANE_VIC_SCREEN, wxAuiPaneInfo().Name("VICScreen").Caption("VIC-II Screen").Right().Position(9).Hide());
    RegisterPane(new PaneVICSprites(this, m_sim), ID_VIEW_PANE_VIC_SPRITES, wxAuiPaneInfo().Name("VICSprites").Caption("VIC-II Sprites").Right().Position(10).Hide());
    RegisterPane(new PaneVICRegs(this, m_sim), ID_VIEW_PANE_VIC_REGS, wxAuiPaneInfo().Name("VICRegs").Caption("VIC-II Registers").Right().Position(11).Hide());
    RegisterPane(new PaneSIDDebugger(this, m_sim), ID_MACH_SID_DEBUGGER, wxAuiPaneInfo().Name("SIDDebugger").Caption("SID Debugger").Right().Position(12).Hide());
    RegisterPane(new PaneAudioMixer(this, m_sim), ID_MACH_AUDIO_MIXER, wxAuiPaneInfo().Name("AudioMixer").Caption("Audio Mixer").Right().Position(13).Hide());
}

void MainFrame::RegisterPane(SimPane* pane, int menu_id, const wxAuiPaneInfo& info) {
    m_panes[menu_id] = pane;
    m_pane_list.push_back(pane);
    m_aui.AddPane(pane, info);
    
    // Initial menu state
    CheckMenuItem(menu_id, info.IsShown());
}

void MainFrame::UpdatePaneVisibility(int menu_id) {
    if (m_panes.count(menu_id)) {
        wxMenuItem* item = GetMenuBar()->FindItem(menu_id);
        bool show = (item && item->IsCheckable()) ? item->IsChecked() : m_aui.GetPane(m_panes[menu_id]).IsShown();
        m_aui.GetPane(m_panes[menu_id]).Show(show);
        m_aui.Update();
    }
}

void MainFrame::CheckMenuItem(int menu_id, bool check) {
    wxMenuItem* item = GetMenuBar()->FindItem(menu_id);
    if (item && item->IsCheckable()) {
        item->Check(check);
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
    m_procCombo = new wxComboBox(m_toolbar, ID_TOOLBAR_PROC_COMBO, "6502", wxDefaultPosition, wxSize(100, -1), procs, wxCB_READONLY);
    m_toolbar->AddControl(m_procCombo);

    wxArrayString machs;
    machs.Add("raw6502"); machs.Add("c64"); machs.Add("c128"); machs.Add("mega65"); machs.Add("x16");
    m_machCombo = new wxComboBox(m_toolbar, ID_TOOLBAR_MACH_COMBO, "c64", wxDefaultPosition, wxSize(100, -1), machs, wxCB_READONLY);
    m_toolbar->AddControl(m_machCombo);

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
        wxString procName = sim_processor_name(m_sim);
        SetStatusText(procName, 1);
        if (m_procCombo && m_procCombo->GetValue() != procName) {
            m_procCombo->SetValue(procName);
        }

        machine_type_t machType = sim_get_machine_type(m_sim);
        wxString machName = sim_machine_name(machType);
        if (m_machCombo && m_machCombo->GetValue() != machName) {
            m_machCombo->SetValue(machName);
        }

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
    const char* path = sim_get_filename(m_sim);
    if (path && strcmp(path, "(none)") != 0) {
        m_running = false;
        if (sim_load_asm(m_sim, path) != 0) {
            wxMessageBox(wxString::Format("Failed to reload '%s':\n%s", path, sim_get_last_error(m_sim)),
                         "Assembly Error", wxOK | wxICON_ERROR);
        } else {
            SetStatusText(wxString::Format("Reloaded: %s", path), 0);
        }
    } else {
        wxCommandEvent dummy;
        OnBrowseLoad(dummy);
    }
}

void MainFrame::OnBrowseLoad(wxCommandEvent& WXUNUSED(event)) {
    wxFileDialog dlg(this, "Load File", "", "",
                     "Supported files (*.asm;*.prg;*.bin)|*.asm;*.prg;*.bin|Assembly (*.asm)|*.asm|PRG files (*.prg)|*.prg|Binary (*.bin)|*.bin|All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        LoadFile(dlg.GetPath());
    }
}

void MainFrame::LoadFile(const wxString& path) {
    if (path.Lower().EndsWith(".bin") || path.Lower().EndsWith(".prg")) {
        LoadBinaryDialog binDlg(this, path);
        if (binDlg.ShowModal() == wxID_OK) {
            m_running = false;
            if (path.Lower().EndsWith(".prg")) {
                sim_load_prg(m_sim, path.mb_str(), binDlg.ShouldOverride() ? binDlg.GetAddress() : 0);
            } else {
                sim_load_bin(m_sim, path.mb_str(), binDlg.GetAddress());
            }
        }
    } else {
        m_running = false;
        if (sim_load_asm(m_sim, path.mb_str()) != 0) {
            wxMessageBox(wxString::Format("Failed to load '%s':\n%s", path, sim_get_last_error(m_sim)),
                         "Assembly Error", wxOK | wxICON_ERROR);
        }
    }
}

void MainFrame::OnSaveBin(wxCommandEvent& WXUNUSED(event)) {
    SaveBinaryDialog dlg(this);
    if (dlg.ShowModal() == wxID_OK) {
        wxFileDialog fileDlg(this, "Save Binary", "", "",
                             "Binary files (*.bin)|*.bin|PRG files (*.prg)|*.prg|All files (*.*)|*.*",
                             wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (fileDlg.ShowModal() == wxID_OK) {
            wxString path = fileDlg.GetPath();
            if (path.Lower().EndsWith(".prg")) {
                sim_save_prg(m_sim, path.mb_str(), dlg.GetStart(), dlg.GetCount());
            } else {
                sim_save_bin(m_sim, path.mb_str(), dlg.GetStart(), dlg.GetCount());
            }
        }
    }
}

void MainFrame::OnNewProject(wxCommandEvent& WXUNUSED(event)) {
    NewProjectWizard wizard(this);
    if (wizard.RunWizard((wxWizardPage*)wizard.GetPageAreaSizer()->GetItem((size_t)0)->GetWindow())) {
        auto res = wizard.GetResult();
        if (res.success) {
            std::string err;
            if (ProjectManager::create_project(res.templateId.ToStdString(), 
                                             res.path.ToStdString(), res.vars, err)) {
                wxString mainAsm = res.path + "/src/main.asm";
                if (wxFileExists(mainAsm)) {
                    sim_load_asm(m_sim, mainAsm.mb_str());
                }
            } else {
                wxMessageBox("Failed to create project: " + wxString(err), "Error", wxOK | wxICON_ERROR);
            }
        }
    }
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
            CheckMenuItem(menu_id, show);
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
                CheckMenuItem(menu_id, m_aui.GetPane(pane).IsShown());
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
    wxTextEntryDialog dlg(this, "Enter hex address:", "Go to Address", "");
    if (dlg.ShowModal() == wxID_OK) {
        unsigned long addr;
        if (dlg.GetValue().ToULong(&addr, 16)) {
            // Find disassembly pane and scroll
            for (auto pane : m_pane_list) {
                if (pane->GetPaneName() == "Disassembly") {
                    // We need a public method in PaneDisassembly to scroll.
                    // For Phase H I will just update the status text as confirmation.
                    SetStatusText(wxString::Format("Go to $%04X", (unsigned)addr), 0);
                    // TODO: call pane->ScrollTo(addr)
                }
            }
        }
    }
}

void MainFrame::OnAddDevice(wxCommandEvent& WXUNUSED(event)) {
    wxArrayString choices;
    choices.Add("sid");
    choices.Add("vic2");
    choices.Add("mega65_math");
    choices.Add("mega65_dma");
    
    wxSingleChoiceDialog dlg(this, "Select a device to add:", "Add Optional Device", choices);
    if (dlg.ShowModal() == wxID_OK) {
        wxString name = dlg.GetStringSelection();
        wxTextEntryDialog addrDlg(this, "Enter base address (hex):", "Device Address", "D420");
        if (addrDlg.ShowModal() == wxID_OK) {
            unsigned long addr;
            if (addrDlg.GetValue().ToULong(&addr, 16)) {
                sim_device_add(m_sim, name.mb_str(), (uint16_t)addr);
            }
        }
    }
}
