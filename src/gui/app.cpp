#include "app.h"
#include "main_frame.h"
#include <wx/config.h>

IMPLEMENT_APP(Sim6502App)

extern int g_verbose;

static const wxCmdLineEntryDesc g_cmdLineDesc[] = {
    { wxCMD_LINE_SWITCH, "v",   "",          "verbosity 1",                                                    wxCMD_LINE_VAL_NONE,   0 },
    { wxCMD_LINE_SWITCH, "vv",  "",          "verbosity 2",                                                    wxCMD_LINE_VAL_NONE,   0 },
    { wxCMD_LINE_SWITCH, "vvv", "",          "verbosity 3",                                                    wxCMD_LINE_VAL_NONE,   0 },
    { wxCMD_LINE_SWITCH, "h",   "help",      "show help",                                                      wxCMD_LINE_VAL_NONE,   wxCMD_LINE_OPTION_HELP },
    { wxCMD_LINE_OPTION, "p",   "processor", "processor: 6502, 6502-undoc, 65c02, 65ce02, 45gs02",             wxCMD_LINE_VAL_STRING, 0 },
    { wxCMD_LINE_OPTION, "t",   "target",    "machine target: raw6502, c64, c128, mega65, x16",                wxCMD_LINE_VAL_STRING, 0 },
    { wxCMD_LINE_OPTION, "b",   "break",     "set breakpoint at address (e.g. $0200)",                         wxCMD_LINE_VAL_STRING, 0 },
    { wxCMD_LINE_OPTION, "L",   "limit",     "max cycles before pausing (0 = unlimited)",                      wxCMD_LINE_VAL_NUMBER, 0 },
    { wxCMD_LINE_OPTION, "S",   "speed",     "speed scale: 1.0 = C64 PAL, 0.0 = unlimited",                   wxCMD_LINE_VAL_DOUBLE, 0 },
    { wxCMD_LINE_SWITCH, nullptr,"debug",    "enable verbose instruction logging",                              wxCMD_LINE_VAL_NONE,   0 },
    { wxCMD_LINE_PARAM,  NULL,  NULL,        "input file",                                                     wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_NONE,   NULL,  NULL,        NULL,                                                             wxCMD_LINE_VAL_NONE,   0 }
};

bool Sim6502App::OnInit() {
    // Initialise fields that wxCmdLineParser won't touch if the flag is absent
    m_cycle_limit = 0;
    m_speed_scale = -1.0;
    m_debug       = false;

    // Manual pre-pass: verbosity flags (multi-char short names wxCmdLineParser
    // cannot handle natively).
    g_verbose = 0;
    for (int i = 1; i < argc; i++) {
        if (!argv[i]) continue;
        wxString arg(argv[i]);
        if (arg == "-v") g_verbose = 1;
        else if (arg == "-vv") g_verbose = 2;
        else if (arg == "-vvv") g_verbose = 3;
    }

    if (!wxApp::OnInit())
        return false;

    SetAppName("sim6502-gui");
    SetVendorName("6502-Simulator");

    // Initialize config
    wxConfigBase *cfg = wxConfigBase::Get();
    if (cfg) {
        cfg->SetRecordDefaults();
    }

    MainFrame *frame = new MainFrame("6502 Simulator");
    frame->Show(true);

    if (!m_processor.IsEmpty())  frame->ApplyProcessor(m_processor);
    if (!m_machine.IsEmpty())    frame->ApplyMachine(m_machine);
    if (!m_breakpoint.IsEmpty()) frame->ApplyBreakpoint(m_breakpoint);
    if (m_cycle_limit > 0)       frame->ApplyCycleLimit(m_cycle_limit);
    if (m_speed_scale >= 0.0)    frame->ApplySpeedScale((float)m_speed_scale);
    if (m_debug)                 frame->ApplyDebug();
    if (!m_filename.IsEmpty())   frame->LoadFile(m_filename);

    return true;
}

void Sim6502App::OnInitCmdLine(wxCmdLineParser& parser) {
    parser.SetDesc(g_cmdLineDesc);
}

bool Sim6502App::OnCmdLineParsed(wxCmdLineParser& parser) {
    if (parser.GetParamCount() > 0)
        m_filename = parser.GetParam(0);
    parser.Found("processor", &m_processor);
    parser.Found("target",    &m_machine);
    parser.Found("break",     &m_breakpoint);
    long limit = 0;
    if (parser.Found("limit", &limit))
        m_cycle_limit = (unsigned long)limit;
    double speed = 0.0;
    if (parser.Found("speed", &speed))
        m_speed_scale = speed;
    m_debug = parser.Found("debug");
    return true;
}
