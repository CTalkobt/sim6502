#include "main_frame.h"
#include "gui_ids.h"
#include <wx/menu.h>

void MainFrame::InitMenuBar() {
    // --- File Menu ---
    wxMenu *fileMenu = new wxMenu;
    fileMenu->Append(ID_FILE_NEW_PROJECT, "&New Project	Ctrl+N");
    fileMenu->Append(ID_FILE_LOAD, "&Load	Ctrl+L");
    fileMenu->Append(ID_FILE_BROWSE_LOAD, "&Browse to Load...");
    fileMenu->Append(ID_FILE_SAVE_BIN, "&Save Binary/PRG...");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "E&xit	Alt+F4");

    // --- Simulation Menu ---
    wxMenu *simMenu = new wxMenu;
    simMenu->Append(ID_SIM_RUN, "&Run	F5");
    simMenu->Append(ID_SIM_PAUSE, "&Pause	F6");
    simMenu->Append(ID_SIM_STEP_INTO, "Step &Into	F7");
    simMenu->Append(ID_SIM_STEP_OVER, "Step &Over	F8");
    simMenu->Append(ID_SIM_RESET, "R&eset	Ctrl+R");
    simMenu->AppendSeparator();
    simMenu->Append(ID_SIM_TOGGLE_BREAKPOINT, "Toggle &Breakpoint	F9");
    simMenu->AppendSeparator();
    simMenu->Append(ID_SIM_STEP_BACK, "Step &Back	Shift+F7");
    simMenu->Append(ID_SIM_STEP_FORWARD, "Step &Forward	Shift+F8");
    simMenu->Append(ID_SIM_REVERSE_CONTINUE, "Reverse &Continue	Ctrl+Shift+R");
    simMenu->AppendSeparator();
    simMenu->Append(ID_SIM_THROTTLE_SPEED, "&Throttle Speed");

    // --- Machine Menu ---
    wxMenu *machMenu = new wxMenu;
    machMenu->Append(ID_MACH_ADD_DEVICE, "Add Optional &Device...");
    
    wxMenu *procMenu = new wxMenu;
    procMenu->AppendRadioItem(ID_MACH_PROC_6502, "6502");
    procMenu->AppendRadioItem(ID_MACH_PROC_6502_UNDOC, "6502-undoc");
    procMenu->AppendRadioItem(ID_MACH_PROC_65C02, "65C02");
    procMenu->AppendRadioItem(ID_MACH_PROC_65CE02, "65CE02");
    procMenu->AppendRadioItem(ID_MACH_PROC_45GS02, "45GS02");
    machMenu->AppendSubMenu(procMenu, "Processor Selection");

    wxMenu *typeMenu = new wxMenu;
    typeMenu->AppendRadioItem(ID_MACH_TYPE_RAW, "raw6502");
    typeMenu->AppendRadioItem(ID_MACH_TYPE_C64, "c64");
    typeMenu->AppendRadioItem(ID_MACH_TYPE_C128, "c128");
    typeMenu->AppendRadioItem(ID_MACH_TYPE_MEGA65, "mega65");
    typeMenu->AppendRadioItem(ID_MACH_TYPE_X16, "x16");
    machMenu->AppendSubMenu(typeMenu, "Machine Selection");

    // --- View Menu ---
    wxMenu *viewMenu = new wxMenu;

    // Core panes (always-visible defaults)
    wxMenu *coreMenu = new wxMenu;
    coreMenu->AppendCheckItem(ID_VIEW_PANE_REGISTERS, "Registers");
    coreMenu->AppendCheckItem(ID_VIEW_PANE_DISASSEMBLY, "Disassembly");
    coreMenu->AppendCheckItem(ID_VIEW_PANE_CONSOLE, "Console");
    wxMenu *memMenu = new wxMenu;
    memMenu->AppendCheckItem(ID_VIEW_PANE_MEMORY_1, "Memory View 1");
    memMenu->AppendCheckItem(ID_VIEW_PANE_MEMORY_2, "Memory View 2");
    memMenu->AppendCheckItem(ID_VIEW_PANE_MEMORY_3, "Memory View 3");
    memMenu->AppendCheckItem(ID_VIEW_PANE_MEMORY_4, "Memory View 4");
    coreMenu->AppendSubMenu(memMenu, "Memory Views");
    viewMenu->AppendSubMenu(coreMenu, "Core");

    // Debug panes
    wxMenu *debugMenu = new wxMenu;
    debugMenu->AppendCheckItem(ID_VIEW_PANE_BREAKPOINTS, "Breakpoints");
    debugMenu->AppendCheckItem(ID_VIEW_PANE_TRACE, "Trace Log");
    debugMenu->AppendCheckItem(ID_VIEW_PANE_STACK, "Stack");
    debugMenu->AppendCheckItem(ID_VIEW_PANE_WATCHES, "Watch List");
    debugMenu->AppendCheckItem(ID_VIEW_PANE_SNAP_DIFF, "Snapshot Diff");
    viewMenu->AppendSubMenu(debugMenu, "Debug");

    // Analysis panes
    wxMenu *analysisMenu = new wxMenu;
    analysisMenu->AppendCheckItem(ID_VIEW_PANE_IREF, "Instruction Ref");
    analysisMenu->AppendCheckItem(ID_VIEW_PANE_SYMBOLS, "Symbols");
    analysisMenu->AppendCheckItem(ID_VIEW_PANE_SOURCE, "Source View");
    analysisMenu->AppendCheckItem(ID_VIEW_PANE_PROFILER, "Profiler");
    analysisMenu->AppendCheckItem(ID_VIEW_PANE_TEST_RUNNER, "Test Runner");
    analysisMenu->AppendCheckItem(ID_VIEW_PANE_PATTERNS, "Idiom Library");
    viewMenu->AppendSubMenu(analysisMenu, "Analysis");

    // Hardware panes
    wxMenu *hwMenu = new wxMenu;
    hwMenu->AppendCheckItem(ID_VIEW_PANE_DEVICES, "I/O Devices");

    wxMenu *videoMenu = new wxMenu;
    videoMenu->AppendCheckItem(ID_VIEW_PANE_VIC_SCREEN, "VIC-II Screen");
    videoMenu->AppendCheckItem(ID_VIEW_PANE_VIC_CHARS, "VIC-II Character Editor");
    videoMenu->AppendCheckItem(ID_VIEW_PANE_VIC_SPRITES, "VIC-II Sprites");
    videoMenu->AppendCheckItem(ID_VIEW_PANE_VIC_REGS, "VIC-II Registers");
    hwMenu->AppendSubMenu(videoMenu, "Video");
    
    wxMenu *audioMenu = new wxMenu;
    audioMenu->AppendCheckItem(ID_VIEW_PANE_SID_DEBUGGER, "SID Debugger");
    audioMenu->AppendCheckItem(ID_VIEW_PANE_AUDIO_MIXER, "Audio Mixer");
    hwMenu->AppendSubMenu(audioMenu, "Audio");

    viewMenu->AppendSubMenu(hwMenu, "Hardware");

    viewMenu->AppendSeparator();
    viewMenu->Append(ID_VIEW_GO_TO_ADDRESS, "&Go to Address...\tCtrl+G");
    
    viewMenu->AppendSeparator();
    wxMenu *settingsMenu = new wxMenu;

    wxMenu *fontSizeMenu = new wxMenu;
    fontSizeMenu->AppendRadioItem(ID_VIEW_FONT_SIZE_10, "10 px");
    fontSizeMenu->AppendRadioItem(ID_VIEW_FONT_SIZE_11, "11 px");
    fontSizeMenu->AppendRadioItem(ID_VIEW_FONT_SIZE_12, "12 px");
    fontSizeMenu->AppendRadioItem(ID_VIEW_FONT_SIZE_13, "13 px");
    fontSizeMenu->AppendRadioItem(ID_VIEW_FONT_SIZE_14, "14 px");
    fontSizeMenu->AppendRadioItem(ID_VIEW_FONT_SIZE_15, "15 px");
    fontSizeMenu->AppendRadioItem(ID_VIEW_FONT_SIZE_16, "16 px");
    fontSizeMenu->AppendRadioItem(ID_VIEW_FONT_SIZE_18, "18 px");
    fontSizeMenu->AppendRadioItem(ID_VIEW_FONT_SIZE_20, "20 px");
    fontSizeMenu->AppendRadioItem(ID_VIEW_FONT_SIZE_24, "24 px");
    settingsMenu->AppendSubMenu(fontSizeMenu, "Font Size");

    wxMenu *themeMenu = new wxMenu;
    themeMenu->AppendRadioItem(ID_VIEW_THEME_AUTO, "Auto (OS)");
    themeMenu->AppendRadioItem(ID_VIEW_THEME_DARK, "Dark");
    themeMenu->AppendRadioItem(ID_VIEW_THEME_LIGHT, "Light");
    settingsMenu->AppendSubMenu(themeMenu, "Theme");

    wxMenu *layoutMenu = new wxMenu;
    layoutMenu->Append(ID_VIEW_LAYOUT_SAVE, "Save Layout...");
    layoutMenu->Append(ID_VIEW_LAYOUT_RESET, "Reset to Default");
    settingsMenu->AppendSubMenu(layoutMenu, "Layout");

    viewMenu->AppendSubMenu(settingsMenu, "Settings");

    // --- Window Menu ---
    wxMenu *windowMenu = new wxMenu;
    windowMenu->Append(ID_WINDOW_ARRANGE, "&Arrange	Shift+F12", "Re-sequence all active panes to be visible");

    // --- Help Menu ---
    wxMenu *helpMenu = new wxMenu;
    helpMenu->Append(wxID_ABOUT, "&About...");

    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(simMenu, "&Simulation");
    menuBar->Append(machMenu, "&Machine");
    menuBar->Append(viewMenu, "&View");
    menuBar->Append(windowMenu, "&Window");
    menuBar->Append(helpMenu, "&Help");
    SetMenuBar(menuBar);
}
