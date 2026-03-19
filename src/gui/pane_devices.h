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
    void OnPropertyChange(wxPropertyGridEvent& event);
    void OnExpandAll(wxCommandEvent& event);
    void OnCollapseAll(wxCommandEvent& event);

    wxPropertyGrid* m_pg;
    wxStaticText*   m_msg;
    int             m_lastDevCount;
};

#endif
