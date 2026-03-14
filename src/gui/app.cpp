#include "app.h"
#include "main_frame.h"
#include <wx/config.h>

IMPLEMENT_APP(Sim6502App)

bool Sim6502App::OnInit() {
    if (!wxApp::OnInit())
        return false;

    SetAppName("sim6502-gui-wx");
    SetVendorName("6502-Simulator");

    // Initialize config
    wxConfigBase *cfg = wxConfigBase::Get();
    if (cfg) {
        cfg->SetRecordDefaults();
    }

    MainFrame *frame = new MainFrame("6502 Simulator");
    frame->Show(true);

    return true;
}
