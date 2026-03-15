#ifndef SIM_PANE_MEMORY_H
#define SIM_PANE_MEMORY_H

#include "pane_base.h"
#include <wx/listctrl.h>
#include <wx/textctrl.h>

class MemoryListCtrl : public wxListCtrl {
public:
    MemoryListCtrl(wxWindow* parent, sim_session_t* sim);
    wxString OnGetItemText(long item, long column) const override;

private:
    sim_session_t* m_sim;
};

class PaneMemory : public SimPane {
public:
    PaneMemory(wxWindow* parent, sim_session_t *sim, int index);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override;
    wxString GetPaneName() const override;

private:
    void OnGoToAddress(wxCommandEvent& event);

    int             m_index;
    MemoryListCtrl* m_list;
    wxTextCtrl*     m_addrSearch;
};

#endif
