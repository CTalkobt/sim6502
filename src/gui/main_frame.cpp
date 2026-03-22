#include "dialogs.h"
#include "gui_ids.h"
#include "main_frame.h"
#include "pane_audio_mixer.h"
#include "pane_breakpoints.h"
#include "pane_console.h"
#include "pane_devices.h"
#include "pane_disassembly.h"
#include "pane_generic.h"
#include "pane_iref.h"
#include "pane_memory.h"
#include "pane_patterns.h"
#include "pane_profiler.h"
#include "pane_registers.h"
#include "pane_sid_debugger.h"
#include "pane_snap_diff.h"
#include "pane_source.h"
#include "pane_stack.h"
#include "pane_symbols.h"
#include "pane_test_runner.h"
#include "pane_trace.h"
#include "pane_vic_char_editor.h"
#include "pane_vic_regs.h"
#include "pane_vic_screen.h"
#include "pane_vic_sprites.h"
#include "pane_watches.h"
#include <wx/artprov.h>
#include <wx/choicdlg.h>
#include <wx/combobox.h>
#include <wx/config.h>
#include <wx/display.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/msgdlg.h>
#include <wx/settings.h>
#include <wx/statusbr.h>
#include <wx/textdlg.h>
#include <wx/toolbar.h>

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_TIMER(1, MainFrame::OnTimer)
    EVT_MENU(wxID_EXIT, MainFrame::OnQuit)
    EVT_MENU(wxID_ABOUT, MainFrame::OnAbout)

    EVT_MENU(ID_SIM_RUN, MainFrame::OnRun)
    EVT_MENU(ID_SIM_PAUSE, MainFrame::OnPause)
    EVT_MENU(ID_SIM_STEP_INTO, MainFrame::OnStepInto)
    EVT_MENU(ID_SIM_STEP_OVER, MainFrame::OnStepOver)
    EVT_MENU(ID_SIM_RESET, MainFrame::OnReset)
    EVT_MENU(ID_SIM_BOOT,  MainFrame::OnBoot)
    EVT_MENU(ID_SIM_CLEAR_CYCLES, MainFrame::OnClearCycles)
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
    EVT_MENU(ID_SETTINGS_DIALOG, MainFrame::OnSettings)
    EVT_MENU(ID_MACH_ADD_DEVICE, MainFrame::OnAddDevice)
    EVT_MENU_RANGE(ID_VIEW_PANE_REGISTERS, ID_VIEW_PANE_AUDIO_MIXER, MainFrame::OnTogglePane)
    EVT_MENU(ID_VIEW_LAYOUT_SAVE, MainFrame::OnTogglePane)
    EVT_MENU(ID_VIEW_LAYOUT_RESET, MainFrame::OnTogglePane)
    EVT_MENU(ID_WINDOW_ARRANGE, MainFrame::OnWindowArrange)
    EVT_AUI_PANE_CLOSE(MainFrame::OnPaneClose)
    EVT_AUI_PANE_BUTTON(MainFrame::OnPaneButton)
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
      m_initial_layout_done(false),
      m_cycle_limit(0),
      m_cycle_run_start(0),
      m_speed_scale(0.0f)
{
    m_sim = sim_create("6502");

    // Initialize AUI
    m_aui.SetManagedWindow(this);

    // Read font settings before InitPanes() so all child widgets inherit the correct font.
    // SIM6502_SCALE env var takes priority over config even at this early stage.
    {
        wxConfigBase *cfg = wxConfigBase::Get();
        if (cfg) {
            cfg->Read("Settings/FontSize", &m_base_font_size, 13);
            double s = 1.0;
            if (cfg->Read("Settings/UIScale", &s) && s >= 0.5 && s <= 8.0)
                m_ui_scale = (float)s;
        }
        wxString env_scale;
        if (wxGetEnv("SIM6502_SCALE", &env_scale)) {
            double v;
            if (env_scale.ToDouble(&v) && v >= 0.5 && v <= 8.0) m_ui_scale = (float)v;
        }
        ApplyFontSize();
    }

    InitMenuBar();
    InitToolBar();
    InitStatusBar();
    InitPanes();

    LoadSettings();

    /* Try to load the VICE character ROM now that the VICE data path is known. */
    LoadViceChargen();

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
    RegisterPane(new PaneSource(this, m_sim), ID_VIEW_PANE_SOURCE, wxAuiPaneInfo().Name("Source").Caption("Source View").Bottom().Position(7).Hide());
    RegisterPane(new PaneProfiler(this, m_sim), ID_VIEW_PANE_PROFILER, wxAuiPaneInfo().Name("Profiler").Caption("Profiler").Bottom().Position(5).Hide());
    RegisterPane(new PaneTestRunner(this, m_sim), ID_VIEW_PANE_TEST_RUNNER, wxAuiPaneInfo().Name("TestRunner").Caption("Test Runner").Bottom().Position(6).Hide());
    RegisterPane(new PaneDevices(this, m_sim), ID_VIEW_PANE_DEVICES, wxAuiPaneInfo().Name("Devices").Caption("I/O Devices").Right().Position(7).Hide());
    RegisterPane(new PanePatterns(this, m_sim), ID_VIEW_PANE_PATTERNS, wxAuiPaneInfo().Name("Patterns").Caption("Idiom Library").Right().Position(8).Hide());

    RegisterPane(new PaneVICScreen(this, m_sim), ID_VIEW_PANE_VIC_SCREEN, wxAuiPaneInfo().Name("VICScreen").Caption("VIC-II Screen").Right().Position(9).Hide());
    RegisterPane(new PaneVICChars(this, m_sim), ID_VIEW_PANE_VIC_CHARS, wxAuiPaneInfo().Name("VICChars").Caption("VIC-II Character Editor").Right().Position(10).Hide());
    RegisterPane(new PaneVICSprites(this, m_sim), ID_VIEW_PANE_VIC_SPRITES, wxAuiPaneInfo().Name("VICSprites").Caption("VIC-II Sprites").Right().Position(11).Hide());
    RegisterPane(new PaneVICRegs(this, m_sim), ID_VIEW_PANE_VIC_REGS, wxAuiPaneInfo().Name("VICRegs").Caption("VIC-II Registers").Right().Position(12).Hide());
    RegisterPane(new PaneSIDDebugger(this, m_sim), ID_VIEW_PANE_SID_DEBUGGER, wxAuiPaneInfo().Name("SIDDebugger").Caption("SID Debugger").Right().Position(13).Hide());
    RegisterPane(new PaneAudioMixer(this, m_sim), ID_VIEW_PANE_AUDIO_MIXER, wxAuiPaneInfo().Name("AudioMixer").Caption("Audio Mixer").Right().Position(14).Hide());
}

void MainFrame::RegisterPane(SimPane* pane, int menu_id, const wxAuiPaneInfo& info) {
    m_panes[menu_id] = pane;
    m_pane_list.push_back(pane);
    wxAuiPaneInfo infoCopy = info;
    infoCopy.PinButton(true);
    m_aui.AddPane(pane, infoCopy);
    
    // Initial menu state
    CheckMenuItem(menu_id, info.IsShown());
}

void MainFrame::UpdatePaneVisibility(int menu_id) {
    if (m_panes.count(menu_id)) {
        SimPane* pane = m_panes[menu_id];
        wxMenuItem* item = GetMenuBar()->FindItem(menu_id);
        bool show = (item && item->IsCheckable()) ? item->IsChecked() : m_aui.GetPane(pane).IsShown();
        
        m_aui.GetPane(pane).Show(show);
        if (show) {
            // Ensure the pane is not buried in a tabbed group
            wxAuiPaneInfo& info = m_aui.GetPane(pane);
            if (info.IsDocked()) {
                m_aui.Update(); // Ensure everything is calculated
            }
        }
        m_aui.Update();
    }
}

void MainFrame::OnWindowArrange(wxCommandEvent& WXUNUSED(event)) {
    // Re-sequence all active panes to be visible on the screen.
    // We'll reset to a known-good docked state and force updates.
    m_initial_layout_done = false;

    // Show core panes that must always be visible
    int core_ids[] = {
        ID_VIEW_PANE_REGISTERS,
        ID_VIEW_PANE_DISASSEMBLY,
        ID_VIEW_PANE_CONSOLE,
        ID_VIEW_PANE_TRACE,
        ID_VIEW_PANE_SNAP_DIFF
    };

    for (int id : core_ids) {
        if (m_panes.count(id)) {
            m_aui.GetPane(m_panes[id]).Show(true);
            CheckMenuItem(id, true);
        }
    }

    // Ensure all floating panes are docked for "Arrangement"
    for (auto const& [menu_id, pane] : m_panes) {
        if (m_aui.GetPane(pane).IsShown()) {
            m_aui.GetPane(pane).Dock();
        }
    }

    m_aui.Update();
    UpdateStatus();
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
    m_toolbar->AddTool(ID_FILE_LOAD, "Load", wxArtProvider::GetBitmap(wxART_FILE_OPEN), "Load/Reload source file (Ctrl+L)");
    m_toolbar->AddSeparator();
    m_toolbar->AddTool(ID_SIM_STEP_INTO, "Step Into", wxArtProvider::GetBitmap(wxART_GO_FORWARD), "Step Into — execute one instruction (F7)");
    m_toolbar->AddTool(ID_SIM_STEP_OVER, "Step Over", wxArtProvider::GetBitmap(wxART_REDO), "Step Over — execute through subroutine (F8)");
    m_toolbar->AddTool(ID_SIM_RUN, "Run", wxArtProvider::GetBitmap(wxART_GO_FORWARD), "Run simulation (F5)");
    m_toolbar->AddTool(ID_SIM_PAUSE, "Pause", wxArtProvider::GetBitmap(wxART_DELETE), "Pause simulation (F6 / Esc)");
    m_toolbar->AddTool(ID_SIM_RESET, "Reset", wxArtProvider::GetBitmap(wxART_UNDO), "Reset CPU and registers (Ctrl+R)");
    m_toolbar->AddTool(ID_SIM_CLEAR_CYCLES, "Clear Cyc", wxArtProvider::GetBitmap(wxART_DELETE), "Clear total cycle count");
    m_toolbar->AddSeparator();
    m_toolbar->AddTool(ID_SIM_TOGGLE_BREAKPOINT, "Breakpoint", wxArtProvider::GetBitmap(wxART_LIST_VIEW), "Toggle breakpoint at current PC (F9)");

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
    if (m_sim) {
        sim_state_t state = sim_get_state(m_sim);
        wxString stateStr;
        if (state == SIM_IDLE) {
            stateStr = "\u25cf IDLE";        // ● IDLE
        } else if (m_running) {
            stateStr = "\u25ba RUNNING";     // ► RUNNING
        } else if (state == SIM_FINISHED) {
            stateStr = "\u25a0 FINISHED";    // ■ FINISHED
        } else {
            stateStr = "\u23f8 PAUSED";      // ⏸ PAUSED
        }
        SetStatusText(stateStr, 0);

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

        // Keep title bar in sync with the currently loaded file
        const char* filename = sim_get_filename(m_sim);
        if (filename && strcmp(filename, "(none)") != 0) {
            wxFileName fn(filename);
            SetTitle(wxString("6502 Simulator \u2014 ") + fn.GetFullName());
        } else {
            SetTitle("6502 Simulator");
        }
    } else {
        SetStatusText("\u25cf IDLE", 0);
    }
}

void MainFrame::OnTimer(wxTimerEvent& WXUNUSED(event)) {
    if (m_sim && m_running) {
        int ev;
        if (m_speed_scale > 0.0f) {
            // Run exactly the right number of cycles for the requested speed.
            // Timer fires at ~62.5 Hz (16 ms); C64 PAL clock = 985248 Hz.
            unsigned long cycles_per_tick = (unsigned long)(m_speed_scale * 985248.0f / 62.5f);
            if (cycles_per_tick < 1) cycles_per_tick = 1;
            ev = sim_step_cycles(m_sim, cycles_per_tick);
        } else {
            ev = sim_step(m_sim, 5000);
        }
        if (ev == SIM_EVENT_BREAK || ev == SIM_EVENT_BRK) {
            m_running = false;
        } else if (m_cycle_limit > 0) {
            const CPU *cpu = sim_get_cpu(m_sim);
            uint64_t elapsed = cpu ? (cpu->cycles - m_cycle_run_start) : 0;
            if (elapsed >= (uint64_t)m_cycle_limit)
                m_running = false;
        } else if (ev < 0) {
            m_running = false;
        }
    }
    
    // Refresh all panes
    SimSnapshot snap;
    snap.cpu = sim_get_cpu(m_sim);
    snap.mem = sim_get_memory(m_sim);
    snap.state = sim_get_state(m_sim);
    snap.running = m_running;

    for (auto pane : m_pane_list) {
        if (m_aui.GetPane(pane).IsShown()) {
            pane->RefreshPane(snap);
        }
    }

    UpdateStatus();
}

void MainFrame::OnRun(wxCommandEvent& WXUNUSED(event)) {
    m_running = true;
    const CPU *cpu = sim_get_cpu(m_sim);
    m_cycle_run_start = cpu ? cpu->cycles : 0;
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
        m_running = false;
        /* Ensure ROMs are loaded (duplicate guard in LoadConfiguredROMs makes this safe
         * to call repeatedly; on a fresh start this is the first load). */
        LoadConfiguredROMs();
        /* For machines with a KERNAL ROM, simulate pressing the reset button:
         * re-trigger the PLA and jump to the reset vector.
         * Falls back to sim_reset() (PC = start_addr) when no KERNAL is loaded. */
        if (sim_boot(m_sim) != 0)
            sim_reset(m_sim);
        UpdateStatus();
    }
}

void MainFrame::LoadConfiguredROMs() {
    if (!m_sim) return;
    wxConfigBase *cfg = wxConfigBase::Get();
    if (!cfg) return;

    static const char* const kTargetIds[]       = { "raw6502", "c64", "c128", "mega65", "x16" };
    static const machine_type_t kMachTypes[]    = { MACHINE_RAW6502, MACHINE_C64, MACHINE_C128,
                                                     MACHINE_MEGA65, MACHINE_X16 };
    machine_type_t mach = sim_get_machine_type(m_sim);
    int t = 0;
    for (int i = 0; i < 5; i++) { if (kMachTypes[i] == mach) { t = i; break; } }

    wxString base = wxString::Format("ROMs/%s/", kTargetIds[t]);
    int count = 0;
    cfg->Read(base + "Count", &count, 0);

    for (int i = 0; i < count; i++) {
        wxString eb = wxString::Format("%s%d/", base, i);
        wxString addrStr, typeStr, fileStr;
        cfg->Read(eb + "Addr", &addrStr, "");
        cfg->Read(eb + "Type", &typeStr, "");
        cfg->Read(eb + "File", &fileStr, "");
        if (addrStr.IsEmpty() || fileStr.IsEmpty() || !wxFileExists(fileStr)) continue;

        wxString hex = addrStr.StartsWith("$") ? addrStr.Mid(1) : addrStr;
        unsigned long physAddr = 0;
        if (!hex.ToULong(&physAddr, 16)) continue;

        int romType = ROM_TYPE_OTHER;
        if      (typeStr == "Kernal")    romType = ROM_TYPE_KERNAL;
        else if (typeStr == "Basic")     romType = ROM_TYPE_BASIC;
        else if (typeStr == "Character") romType = ROM_TYPE_CHARACTER;
        else if (typeStr == "Expansion") romType = ROM_TYPE_EXPANSION;

        /* CHARACTER ROM: the overlay structure at $D000/$1000/$9000 is created
         * by machine_init_hardware and backed by char_rom[].  Update the data
         * in-place via sim_load_charset_file rather than adding a new overlay. */
        if (romType == ROM_TYPE_CHARACTER) {
            sim_load_charset_file(m_sim, fileStr.mb_str());
            continue;
        }

        /* Skip if an overlay is already registered at this address
         * (prevents stacking duplicates on repeated calls). */
        if (sim_overlay_find(m_sim, (uint32_t)physAddr) >= 0) continue;

        /* KERNAL, BASIC are PLA-controlled — start inactive.
         * sim_boot() will trigger C64PlaHandler to activate them correctly.
         * EXPANSION and OTHER are always visible. */
        int active = (romType == ROM_TYPE_KERNAL || romType == ROM_TYPE_BASIC) ? 0 : 1;

        sim_overlay_load(m_sim, (uint32_t)physAddr, fileStr.mb_str(),
                         romType, /*cpu_visible*/1, /*vic_visible*/0, active);
    }
}

void MainFrame::LoadViceChargen() {
    if (!m_sim) return;
    wxConfigBase *cfg = wxConfigBase::Get();
    if (!cfg) return;
    wxString viceData;
    cfg->Read("Emulators/VICEData", &viceData, "");
    if (viceData.IsEmpty()) return;

    machine_type_t mach = sim_get_machine_type(m_sim);
    const char *subdir = (mach == MACHINE_C128) ? "C128" : "C64";
    wxString chargen = viceData + wxFILE_SEP_PATH + subdir + wxFILE_SEP_PATH + "chargen";
    if (!wxFileExists(chargen)) return;

    sim_load_charset_file(m_sim, chargen.mb_str());
}

void MainFrame::BootMachine() {
    if (!m_sim) return;
    m_running = false;
    /* Reinitialise hardware: clears all overlays and re-registers built-in
     * char ROM overlays and CIA/VIC/SID handlers cleanly. */
    sim_set_machine_type(m_sim, sim_get_machine_type(m_sim));
    LoadViceChargen();
    LoadConfiguredROMs();
    if (sim_boot(m_sim) != 0) {
        wxMessageBox(
            "Boot failed: reset vector at $FFFC/$FFFD is $0000.\n"
            "Configure a KERNAL ROM in Settings > ROM Mapping.",
            "Boot Error", wxOK | wxICON_ERROR);
        return;
    }
    UpdateStatus();
}

void MainFrame::OnBoot(wxCommandEvent& WXUNUSED(event)) {
    BootMachine();
}

void MainFrame::OnClearCycles(wxCommandEvent& WXUNUSED(event)) {
    if (m_sim) {
        sim_clear_cycles(m_sim);
        UpdateStatus();
    }
}

void MainFrame::OnToggleBreakpoint(wxCommandEvent& WXUNUSED(event)) {
    if (!m_sim) return;
    const CPU *cpu = sim_get_cpu(m_sim);
    if (!cpu) return;
    uint16_t addr = cpu->pc;
    if (sim_has_breakpoint(m_sim, addr))
        sim_break_clear(m_sim, addr);
    else
        sim_break_set(m_sim, addr, nullptr);
    UpdateStatus();
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
    if (!m_sim) return;
    if (!sim_history_is_enabled(m_sim) || sim_history_count(m_sim) == 0) return;
    m_running = false;
    // Step back at least once before checking breakpoints so we don't
    // stop immediately on a breakpoint at the current PC.
    while (sim_history_step_back(m_sim)) {
        const CPU *cpu = sim_get_cpu(m_sim);
        if (cpu && sim_has_breakpoint(m_sim, cpu->pc))
            break;
    }
    UpdateStatus();
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
    bool loaded = false;
    if (path.Lower().EndsWith(".bin") || path.Lower().EndsWith(".prg")) {
        LoadBinaryDialog binDlg(this, path);
        if (binDlg.ShowModal() == wxID_OK) {
            m_running = false;
            int rc;
            if (path.Lower().EndsWith(".prg"))
                rc = sim_load_prg(m_sim, path.mb_str(), binDlg.ShouldOverride() ? binDlg.GetAddress() : 0);
            else
                rc = sim_load_bin(m_sim, path.mb_str(), binDlg.GetAddress());
            loaded = (rc == 0);
        }
    } else {
        m_running = false;
        if (sim_load_asm(m_sim, path.mb_str()) != 0) {
            wxMessageBox(wxString::Format("Failed to load '%s':\n%s", path, sim_get_last_error(m_sim)),
                         "Assembly Error", wxOK | wxICON_ERROR);
        } else {
            loaded = true;
        }
    }
    if (loaded) {
        /* Re-apply ROMs after load: sim_load_* may trigger machine_init_hardware
         * (via apply_session_processor_symbols) which clears overlays.
         * sim_boot() reads $FFFC/$FFFD and sets PC to the reset vector.
         * If no KERNAL is configured, PC stays at the load/entry-symbol address. */
        LoadViceChargen();
        LoadConfiguredROMs();
        sim_boot(m_sim);
        UpdateStatus();
    }
}

void MainFrame::ApplyProcessor(const wxString& proc) {
    if (m_sim)
        sim_set_processor(m_sim, proc.mb_str());
}

void MainFrame::ApplyMachine(const wxString& target) {
    if (!m_sim) return;
    machine_type_t mt = MACHINE_RAW6502;
    if      (target == "c64")    mt = MACHINE_C64;
    else if (target == "c128")   mt = MACHINE_C128;
    else if (target == "mega65") mt = MACHINE_MEGA65;
    else if (target == "x16")    mt = MACHINE_X16;
    sim_set_machine_type(m_sim, mt);
}

void MainFrame::ApplyBreakpoint(const wxString& addrStr) {
    if (!m_sim) return;
    wxString s = addrStr;
    if (s.StartsWith("$"))                         s = s.Mid(1);
    else if (s.StartsWith("0x") || s.StartsWith("0X")) s = s.Mid(2);
    unsigned long addr = 0;
    s.ToULong(&addr, 16);
    sim_break_set(m_sim, (uint16_t)addr, nullptr);
}

void MainFrame::ApplyCycleLimit(unsigned long limit) {
    m_cycle_limit = limit;
}

void MainFrame::ApplySpeedScale(float scale) {
    m_speed_scale = scale;
}

void MainFrame::ApplyDebug() {
    if (m_sim) sim_set_debug(m_sim, true);
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
        /* Load any configured ROMs for the new machine and boot from the
         * reset vector.  Silent if no ROMs are configured (sim_boot returns -1). */
        LoadConfiguredROMs();
        sim_boot(m_sim);
        UpdateStatus();
    }
}

void MainFrame::OnQuit(wxCommandEvent& WXUNUSED(event)) {
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent& WXUNUSED(event)) {
    wxMessageBox("6502 Simulator (wxWidgets)", "About", wxOK | wxICON_INFORMATION);
}

void MainFrame::OnSettings(wxCommandEvent& WXUNUSED(event)) {
    wxString curMachine   = m_sim ? wxString(sim_machine_name(sim_get_machine_type(m_sim))) : "raw6502";
    wxString curProcessor = m_sim ? wxString(sim_processor_name(m_sim)) : "6502";

    SettingsDialog dlg(this, m_base_font_size, m_theme, m_ui_scale,
                       m_speed_scale, curMachine, curProcessor);
    if (dlg.ShowModal() == wxID_OK) {
        bool fontChanged = (dlg.GetFontSize() != m_base_font_size ||
                            dlg.GetUIScale()  != m_ui_scale);
        m_base_font_size = dlg.GetFontSize();
        m_theme          = dlg.GetTheme();
        m_ui_scale       = dlg.GetUIScale();
        ApplyTheme();

        // Apply speed immediately
        ApplySpeedScale(dlg.GetSpeedScale());

        // Apply machine and processor to current session
        if (m_sim) {
            wxString newMach = dlg.GetDefaultMachine();
            wxString newProc = dlg.GetDefaultProcessor();
            if (newMach != curMachine) {
                machine_type_t mt = MACHINE_RAW6502;
                if      (newMach == "c64")    mt = MACHINE_C64;
                else if (newMach == "c128")   mt = MACHINE_C128;
                else if (newMach == "mega65") mt = MACHINE_MEGA65;
                else if (newMach == "x16")    mt = MACHINE_X16;
                sim_set_machine_type(m_sim, mt);
            }
            if (newProc != curProcessor)
                sim_set_processor(m_sim, newProc.mb_str());
        }

        // Persist emulator paths (not MainFrame members; written directly to config)
        wxConfigBase* cfg = wxConfigBase::Get();
        if (cfg) {
            cfg->Write("Emulators/VICEBinDir",  dlg.GetVICEBin());
            cfg->Write("Emulators/VICEData",    dlg.GetVICEData());
            cfg->Write("Emulators/XemuBinDir",  dlg.GetXemuBin());
            cfg->Write("Emulators/XemuData",    dlg.GetXemuData());
            // ROM entries per machine target (address-based layout)
            static const char* const kRomTargetIds[] = {
                "raw6502", "c64", "c128", "mega65", "x16"
            };
            for (int t = 0; t < SettingsDialog::kRomTargetCount; t++) {
                wxString base = wxString::Format("ROMs/%s/", kRomTargetIds[t]);
                const auto& entries = dlg.GetROMEntries(t);
                // Erase stale indexed entries beyond the new count
                int oldCount = 0;
                cfg->Read(base + "Count", &oldCount, 0);
                for (int i = (int)entries.size(); i < oldCount; i++)
                    cfg->DeleteGroup(wxString::Format("/ROMs/%s/%d", kRomTargetIds[t], i));
                cfg->Write(base + "Count", (int)entries.size());
                for (int i = 0; i < (int)entries.size(); i++) {
                    wxString eb = wxString::Format("%s%d/", base, i);
                    cfg->Write(eb + "Addr", entries[i].addr);
                    cfg->Write(eb + "Type", entries[i].type);
                    cfg->Write(eb + "File", entries[i].file);
                }
            }
            // Speed and machine defaults
            cfg->Write("Settings/SpeedScale",       (double)dlg.GetSpeedScale());
            cfg->Write("Settings/DefaultMachine",   dlg.GetDefaultMachine());
            cfg->Write("Settings/DefaultProcessor", dlg.GetDefaultProcessor());
            // Limits
            cfg->Write("Limits/HistDepth",      dlg.GetHistDepth());
            cfg->Write("Limits/TraceDepth",     dlg.GetTraceDepth());
            cfg->Write("Limits/MaxBreakpoints", dlg.GetMaxBreakpoints());
            cfg->Write("Limits/MaxWatches",     dlg.GetMaxWatches());
            cfg->Write("Limits/MaxSnapDiff",    dlg.GetMaxSnapDiff());
            cfg->Write("Limits/CycleLimit",     (long)dlg.GetCycleLimit());
        }
        ApplyCycleLimit(dlg.GetCycleLimit());
        /* VICE data path may have changed — reload chargen immediately. */
        LoadViceChargen();
        SaveSettings();
        if (fontChanged) {
            wxMessageBox("Font size and scaling changes will take effect after restarting the application.",
                         "Restart Required", wxOK | wxICON_INFORMATION, this);
        }
    }
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

void MainFrame::OnPaneClose(wxAuiManagerEvent& event) {
    wxAuiPaneInfo* pane = event.GetPane();
    if (pane) {
        // Find the menu ID for this pane window
        for (auto const& [menu_id, p] : m_panes) {
            if (p == pane->window) {
                CheckMenuItem(menu_id, false);
                break;
            }
        }
    }
}

void MainFrame::OnPaneButton(wxAuiManagerEvent& event) {
    if (event.GetButton() == wxAUI_BUTTON_PIN) {
        wxAuiPaneInfo* pane = event.GetPane();
        if (pane && pane->IsFloating()) {
            pane->Dock();
            m_aui.Update();
        }
    } else {
        event.Skip();
    }
}

void MainFrame::ApplyFontSize() {
    int pt = (int)((float)m_base_font_size * m_ui_scale + 0.5f);
    if (pt < 6)  pt = 6;
    if (pt > 64) pt = 64;
    wxFont f(pt, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    if (f.IsOk())
        SetFont(f);
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

        double speedVal = 0.0;
        if (cfg->Read("Settings/SpeedScale", &speedVal))
            m_speed_scale = (float)speedVal;

        long cycLimit = 0;
        cfg->Read("Limits/CycleLimit", &cycLimit, 0L);
        m_cycle_limit = (unsigned long)cycLimit;
        
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

        // Restore breakpoints
        if (m_sim) {
            int bpCount = 0;
            cfg->Read("Breakpoints/Count", &bpCount, 0);
            for (int i = 0; i < bpCount; i++) {
                int addr = 0;
                wxString cond;
                bool enabled = true;
                cfg->Read(wxString::Format("Breakpoints/Addr_%d", i), &addr, 0);
                cfg->Read(wxString::Format("Breakpoints/Cond_%d", i), &cond, "");
                cfg->Read(wxString::Format("Breakpoints/Enabled_%d", i), &enabled, true);
                sim_break_set(m_sim, (uint16_t)addr,
                              cond.IsEmpty() ? nullptr : cond.mb_str().data());
                if (!enabled) {
                    // Toggle off: find the index of the newly added BP
                    int newIdx = sim_break_count(m_sim) - 1;
                    if (newIdx >= 0) sim_break_toggle(m_sim, newIdx);
                }
            }
        }

        // Let each pane restore its own state (watches, etc.)
        for (auto pane : m_pane_list) {
            pane->LoadState(cfg);
        }
    }

    // UIScale priority: SIM6502_SCALE env > config > GDK/QT env > DPI auto-detect
    wxString env_scale;
    if (wxGetEnv("SIM6502_SCALE", &env_scale)) {
        double v;
        if (env_scale.ToDouble(&v) && v >= 0.5 && v <= 8.0) m_ui_scale = (float)v;
    } else {
        double configScale = 1.0;
        if (cfg && cfg->Read("Settings/UIScale", &configScale)
                && configScale >= 0.5 && configScale <= 8.0) {
            m_ui_scale = (float)configScale;
        } else if (wxGetEnv("GDK_SCALE", &env_scale)) {
            double v;
            if (env_scale.ToDouble(&v) && v >= 1.0 && v <= 8.0) m_ui_scale = (float)v;
        } else if (wxGetEnv("QT_SCALE_FACTOR", &env_scale)) {
            double v;
            if (env_scale.ToDouble(&v) && v >= 1.0 && v <= 8.0) m_ui_scale = (float)v;
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
}

void MainFrame::SaveSettings() {
    wxConfigBase *cfg = wxConfigBase::Get();
    if (!cfg) return;

    cfg->Write("Settings/FontSize", m_base_font_size);
    cfg->Write("Settings/Theme",    m_theme);
    cfg->Write("Settings/UIScale",  (double)m_ui_scale);

    wxSize sz = GetSize();
    cfg->Write("Window/Width", sz.x);
    cfg->Write("Window/Height", sz.y);

    wxPoint pos = GetPosition();
    cfg->Write("Window/X", pos.x);
    cfg->Write("Window/Y", pos.y);

    cfg->Write("Layout/Perspective", m_aui.SavePerspective());

    // Save breakpoints
    if (m_sim) {
        int bpCount = sim_break_count(m_sim);
        cfg->Write("Breakpoints/Count", bpCount);
        for (int i = 0; i < bpCount; i++) {
            uint16_t addr;
            char cond[128];
            cond[0] = '\0';
            if (sim_break_get(m_sim, i, &addr, cond, sizeof(cond)) != 0) {
                cfg->Write(wxString::Format("Breakpoints/Addr_%d", i), (int)addr);
                cfg->Write(wxString::Format("Breakpoints/Cond_%d", i), wxString(cond));
                cfg->Write(wxString::Format("Breakpoints/Enabled_%d", i),
                           (bool)sim_break_is_enabled(m_sim, i));
            }
        }
    }

    // Let each pane save its own state (watches, etc.)
    for (auto pane : m_pane_list) {
        pane->SaveState(cfg);
    }

    cfg->Flush();
}

void MainFrame::NavigateDisassembly(uint16_t addr) {
    for (auto pane : m_pane_list) {
        PaneDisassembly* pd = dynamic_cast<PaneDisassembly*>(pane);
        if (pd) {
            pd->ScrollTo(addr);
            // Ensure shown
            m_aui.GetPane(pd).Show(true);
            CheckMenuItem(ID_VIEW_PANE_DISASSEMBLY, true);
            m_aui.Update();
            break;
        }
    }
}

void MainFrame::NavigateMemory(uint16_t addr) {
    for (auto pane : m_pane_list) {
        PaneMemory* pm = dynamic_cast<PaneMemory*>(pane);
        if (pm) {
            // Show and lay out the pane first so EnsureVisible works on a
            // visible list; calling ScrollTo on a hidden pane is a no-op.
            m_aui.GetPane(pm).Show(true);
            for (auto const& [menu_id, p] : m_panes) {
                if (p == pm) {
                    CheckMenuItem(menu_id, true);
                    break;
                }
            }
            m_aui.Update();
            // Defer scroll until after AUI has finished laying out the pane;
            // EnsureVisible on a freshly-shown list gives the wrong offset otherwise.
            pm->CallAfter([pm, addr]() { pm->ScrollTo(addr); });
            break;
        }
    }
}

void MainFrame::AddWatch(uint16_t addr, const wxString& label) {
    for (auto pane : m_pane_list) {
        PaneWatches* pw = dynamic_cast<PaneWatches*>(pane);
        if (pw) {
            pw->AddWatch(addr, label);
            // Ensure shown
            m_aui.GetPane(pw).Show(true);
            CheckMenuItem(ID_VIEW_PANE_WATCHES, true);
            m_aui.Update();
            break;
        }
    }
}

void MainFrame::UpdatePaneCaption(SimPane* pane, const wxString& caption) {
    if (pane) {
        m_aui.GetPane(pane).Caption(caption);
        m_aui.Update();

        // Also update the menu item text if this pane has an associated menu ID
        for (auto const& [menu_id, p] : m_panes) {
            if (p == pane) {
                wxMenuItem* item = GetMenuBar()->FindItem(menu_id);
                if (item) {
                    item->SetItemLabel(caption);
                }
                break;
            }
        }
    }
}

void MainFrame::OnGoToAddress(wxCommandEvent& WXUNUSED(event)) {
    wxTextEntryDialog dlg(this, "Enter hex address:", "Go to Address", "");
    if (dlg.ShowModal() == wxID_OK) {
        unsigned long addr;
        if (dlg.GetValue().ToULong(&addr, 16)) {
            NavigateDisassembly((uint16_t)addr);
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
