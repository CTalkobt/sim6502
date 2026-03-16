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
    wxString      m_filename;
    wxString      m_processor;    // from -p/--processor
    wxString      m_machine;      // from -t/--target
    wxString      m_breakpoint;   // from -b/--break
    unsigned long m_cycle_limit;  // from -L/--limit  (0 = none)
    double        m_speed_scale;  // from -S/--speed  (-1 = not set)
    bool          m_debug;        // from --debug
};

DECLARE_APP(Sim6502App)

#endif // SIM6502_APP_H
