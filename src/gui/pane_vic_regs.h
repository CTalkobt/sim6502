#ifndef SIM_PANE_VIC_REGS_H
#define SIM_PANE_VIC_REGS_H

#include "pane_base.h"
#include <wx/propgrid/propgrid.h>

class PaneVICRegs : public SimPane {
public:
    PaneVICRegs(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "VIC-II Registers"; }
    wxString GetPaneName() const override { return "VICRegs"; }

private:
    void InitProperties();
    void OnPropertyChange(wxPropertyGridEvent& event);

    wxPropertyGrid* m_pg;
};

#endif
