#ifndef SIM6502_APP_H
#define SIM6502_APP_H

#include <wx/wx.h>
#include <wx/cmdline.h>

class Sim6502App : public wxApp {
public:
    virtual bool OnInit() override;
    virtual void OnInitCmdLine(wxCmdLineParser& parser) override;
    virtual bool OnCmdLineParsed(wxCmdLineParser& parser) override;

private:
    wxString m_filename;
};

DECLARE_APP(Sim6502App)

#endif // SIM6502_APP_H
