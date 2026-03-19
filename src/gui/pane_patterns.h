#ifndef SIM_PANE_PATTERNS_H
#define SIM_PANE_PATTERNS_H

#include "pane_base.h"
#include <wx/listbox.h>
#include <wx/stc/stc.h>
#include <wx/splitter.h>

class PanePatterns : public SimPane {
public:
    PanePatterns(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Idiom Library"; }
    wxString GetPaneName() const override { return "Patterns"; }

private:
    void OnItemSelected(wxCommandEvent& event);
    void OnInsert(wxCommandEvent& event);
    void OnSearch(wxCommandEvent& event);
    void OnReset(wxCommandEvent& event);

    void UpdateList();

    wxTextCtrl*       m_search;
    wxListBox*        m_list;
    wxStyledTextCtrl* m_preview;
    wxSplitterWindow* m_splitter;

    std::vector<int>  m_filteredIndices;
};

#endif
