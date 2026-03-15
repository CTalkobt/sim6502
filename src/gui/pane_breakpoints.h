#ifndef SIM_PANE_BREAKPOINTS_H
#define SIM_PANE_BREAKPOINTS_H

#include "pane_base.h"
#include <wx/listctrl.h>

class PaneBreakpoints : public SimPane {
public:
    PaneBreakpoints(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Breakpoints"; }
    wxString GetPaneName() const override { return "Breakpoints"; }

private:
    void OnAdd(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void OnClearAll(wxCommandEvent& event);
    void OnItemActivated(wxListEvent& event);
    void OnItemSelected(wxListEvent& event);
    void OnItemChecked(wxListEvent& event);
    void OnItemUnchecked(wxListEvent& event);

    wxListCtrl* m_list;
};

#endif
