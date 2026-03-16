#include "app.h"
#include "main_frame.h"
#include <wx/config.h>

IMPLEMENT_APP(Sim6502App)

extern int g_verbose;

static const wxCmdLineEntryDesc g_cmdLineDesc[] = {
    { wxCMD_LINE_SWITCH, "v", "", "verbosity 1", wxCMD_LINE_VAL_NONE, 0 },
    { wxCMD_LINE_SWITCH, "vv", "", "verbosity 2", wxCMD_LINE_VAL_NONE, 0 },
    { wxCMD_LINE_SWITCH, "vvv", "", "verbosity 3", wxCMD_LINE_VAL_NONE, 0 },
    { wxCMD_LINE_SWITCH, "h", "help", "show help", wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
    { wxCMD_LINE_PARAM,  NULL, NULL, "input file", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_NONE, NULL, NULL, NULL, wxCMD_LINE_VAL_NONE, 0 }
};

bool Sim6502App::OnInit() {
    // Manual parsing of verbosity flags to match sim6502 CLI exactly
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

    if (!m_filename.IsEmpty()) {
        frame->LoadFile(m_filename);
    }

    return true;
}

void Sim6502App::OnInitCmdLine(wxCmdLineParser& parser) {
    parser.SetDesc(g_cmdLineDesc);
}

bool Sim6502App::OnCmdLineParsed(wxCmdLineParser& parser) {
    if (parser.GetParamCount() > 0) {
        m_filename = parser.GetParam(0);
    }
    return true;
}
