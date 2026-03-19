#ifndef SIM_PANE_SYMBOLS_H
#define SIM_PANE_SYMBOLS_H

#include "pane_base.h"
#include <wx/listctrl.h>
#include <wx/textctrl.h>

class SymbolsListCtrl : public wxListCtrl {
public:
    SymbolsListCtrl(wxWindow* parent, sim_session_t* sim);
    void SetFilter(const wxString& filter);
    void UpdateList();
    void SortByColumn(int column);
    wxString OnGetItemText(long item, long column) const override;
    
    int GetRealIndex(long item) const { return (item >= 0 && item < (long)m_filteredIndices.size()) ? m_filteredIndices[item] : -1; }

private:
    sim_session_t* m_sim;
    wxString       m_filter;
    std::vector<int> m_filteredIndices;
    int            m_sortColumn;
    bool           m_sortAscending;
};

class PaneSymbols : public SimPane {
public:
    PaneSymbols(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Symbols"; }
    wxString GetPaneName() const override { return "Symbols"; }

private:
    void OnFilter(wxCommandEvent& event);
    void OnLoad(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnAdd(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void OnItemActivated(wxListEvent& event);
    void OnColumnClick(wxListEvent& event);
    void OnEndLabelEdit(wxListEvent& event);

    wxTextCtrl*       m_filter;
    SymbolsListCtrl*  m_list;
};

#endif
