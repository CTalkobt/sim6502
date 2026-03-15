#ifndef SIM_PANE_TRACE_H
#define SIM_PANE_TRACE_H

#include "pane_base.h"
<<<<<<< HEAD
#include <wx/listctrl.h>
#include <vector>
=======
#include <wx/notebook.h>
#include <wx/listctrl.h>
#include <wx/textctrl.h>
>>>>>>> e01e3fe (Debugging Panes)

class TraceListCtrl : public wxListCtrl {
public:
    TraceListCtrl(wxWindow* parent, sim_session_t* sim);
    wxString OnGetItemText(long item, long column) const override;
<<<<<<< HEAD
    
    void UpdateColumns();
    void SetFilteredIndices(const std::vector<int>& indices);

private:
    sim_session_t* m_sim;
    std::vector<int> m_filteredIndices;
    bool m_hasExtendedRegs;
=======

private:
    sim_session_t* m_sim;
>>>>>>> e01e3fe (Debugging Panes)
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
<<<<<<< HEAD
    void OnToggleFollow(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);
    void OnFilterChanged(wxCommandEvent& event);
    void OnListScroll(wxScrollWinEvent& event);

    void UpdateFilter();

    TraceListCtrl*  m_traceList;
    wxTextCtrl*     m_filterText;
    wxStaticText*   m_statusText;
    bool            m_enabled;
    bool            m_follow;
    uint64_t        m_last_trace_total;
    uint64_t        m_last_cycles;
    cpu_type_t      m_last_cpu_type;
=======

    wxNotebook*     m_notebook;
    wxTextCtrl*     m_liveLog;
    TraceListCtrl*  m_traceList;
    bool            m_enabled;
>>>>>>> e01e3fe (Debugging Panes)
};

#endif
