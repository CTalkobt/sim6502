#ifndef SIM_PANE_SID_DEBUGGER_H
#define SIM_PANE_SID_DEBUGGER_H

#include "pane_base.h"
#include <wx/propgrid/propgrid.h>

class PaneSIDDebugger : public SimPane {
public:
    PaneSIDDebugger(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "SID Debugger"; }
    wxString GetPaneName() const override { return "SIDDebugger"; }

private:
    void InitProperties();

    wxPropertyGrid* m_pg;
};

#endif
