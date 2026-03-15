#ifndef SIM_PANE_DEVICES_H
#define SIM_PANE_DEVICES_H

#include "pane_base.h"
#include <wx/propgrid/propgrid.h>

class PaneDevices : public SimPane {
public:
    PaneDevices(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "I/O Devices"; }
    wxString GetPaneName() const override { return "Devices"; }

private:
    void UpdateProperties();

    wxPropertyGrid* m_pg;
};

#endif
