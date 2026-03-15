#ifndef SIM_PANE_SNAP_DIFF_H
#define SIM_PANE_SNAP_DIFF_H

#include "pane_base.h"
#include <wx/listctrl.h>

class PaneSnapDiff : public SimPane {
public:
    PaneSnapDiff(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Snapshot Diff"; }
    wxString GetPaneName() const override { return "SnapDiff"; }

private:
    void OnTakeSnapshot(wxCommandEvent& event);
    void OnClear(wxCommandEvent& event);

    wxListCtrl* m_list;
};

#endif
