#ifndef SIM_PANE_TRACE_H
#define SIM_PANE_TRACE_H

#include "pane_base.h"
#include <wx/listctrl.h>

class TraceListCtrl : public wxListCtrl {
public:
    TraceListCtrl(wxWindow* parent, sim_session_t* sim);
    wxString OnGetItemText(long item, long column) const override;

private:
    sim_session_t* m_sim;
};

class PaneTrace : public SimPane {
public:
    PaneTrace(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Trace"; }
    wxString GetPaneName() const override { return "Trace"; }

private:
    void OnClear(wxCommandEvent& event);
    void OnEnable(wxCommandEvent& event);
    void OnToggleFollow(wxCommandEvent& event);

    TraceListCtrl*  m_traceList;
    bool            m_enabled;
    bool            m_follow;
    uint64_t        m_last_trace_total;
    uint64_t        m_last_cycles;
};

#endif
