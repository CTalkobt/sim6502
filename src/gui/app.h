#ifndef SIM6502_APP_H
#define SIM6502_APP_H

#include <wx/wx.h>

class Sim6502App : public wxApp {
public:
    virtual bool OnInit() override;
};

DECLARE_APP(Sim6502App)

#endif // SIM6502_APP_H
