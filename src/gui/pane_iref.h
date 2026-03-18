#ifndef SIM_PANE_IREF_H
#define SIM_PANE_IREF_H

#include "pane_base.h"
#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/splitter.h>

class PaneIRef : public SimPane {
public:
    PaneIRef(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Instruction Ref"; }
    wxString GetPaneName() const override { return "IRef"; }

private:
    void OnFilter(wxCommandEvent& event);
    void OnItemSelected(wxListEvent& event);
    void OnContextMenu(wxListEvent& event);
    void OnFindInDisasm(wxCommandEvent& event);
    void PopulateList();
    wxString GetDescription(const wxString& mnemonic);

    wxTextCtrl*       m_filter;
    wxChoice*         m_modeFilter;
    wxListCtrl*       m_list;
    wxTextCtrl*       m_detail;
    wxSplitterWindow* m_splitter;
    int               m_last_cpu_type;
};

#endif
