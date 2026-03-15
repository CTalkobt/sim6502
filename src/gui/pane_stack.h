#ifndef SIM_PANE_STACK_H
#define SIM_PANE_STACK_H

#include "pane_base.h"
#include <wx/listctrl.h>

class PaneStack : public SimPane {
public:
    PaneStack(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Stack"; }
    wxString GetPaneName() const override { return "Stack"; }

private:
    wxListCtrl* m_list;
};

#endif
