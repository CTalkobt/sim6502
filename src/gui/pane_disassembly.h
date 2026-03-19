#ifndef SIM_PANE_DISASSEMBLY_H
#define SIM_PANE_DISASSEMBLY_H

#include "pane_base.h"
#include <wx/listctrl.h>
#include <wx/toolbar.h>
#include <wx/textctrl.h>

class DisasmListCtrl;

class PaneDisassembly : public SimPane {
public:
    PaneDisassembly(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override;
    wxString GetPaneName() const override;

    void ScrollTo(uint16_t addr);
    uint16_t GetAddressForRow(int row) const;
    wxString GetFilterText() const { return m_filterText; }

private:
    void UpdateRowAddresses(uint16_t start_addr);
    uint16_t HeuristicBackStep(uint16_t top_addr, int rows);

    void OnToggleBreakpoint(wxListEvent& event);
    void OnSyncToPC(wxCommandEvent& event);
    void OnGoToAddress(wxCommandEvent& event);
    void OnPrevPage(wxCommandEvent& event);
    void OnNextPage(wxCommandEvent& event);
    void OnFollowToggle(wxCommandEvent& event);
    
    // Task 4 & 6
    void OnContextMenu(wxListEvent& event);
    void OnSetPC(wxCommandEvent& event);
    void OnGoToMemory(wxCommandEvent& event);
    void OnAddWatch(wxCommandEvent& event);
    void OnAddWatchOperand(wxCommandEvent& event);
    void OnSearch(wxCommandEvent& event);
    void OnSearchClose(wxCommandEvent& event);

    DisasmListCtrl* m_list;
    wxToolBar*      m_toolbar;
    wxTextCtrl*     m_addrSearch;
    wxBoxSizer*     m_searchSizer;
    wxTextCtrl*     m_filterCtrl;
    uint16_t        m_base_addr;
    std::vector<uint16_t> m_row_addresses;
    cpu_type_t      m_last_cpu_type;
    sim_state_t     m_last_state;
    bool            m_followPC;
    wxString        m_filterText;
};

#endif
