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
    wxString OnGetItemText(long item, long column) const override;

private:
    sim_session_t* m_sim;
    wxString       m_filter;
    std::vector<int> m_filteredIndices;
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

    wxTextCtrl*       m_filter;
    SymbolsListCtrl*  m_list;
};

#endif
