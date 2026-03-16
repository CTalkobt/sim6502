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

private:
    void UpdateRowAddresses(uint16_t start_addr);
    void OnToggleBreakpoint(wxListEvent& event);
    void OnSyncToPC(wxCommandEvent& event);
    void OnGoToAddress(wxCommandEvent& event);
    void OnPrevPage(wxCommandEvent& event);
    void OnNextPage(wxCommandEvent& event);

    DisasmListCtrl* m_list;
    wxTextCtrl*     m_addrSearch;
    uint16_t        m_base_addr;
    std::vector<uint16_t> m_row_addresses;
    cpu_type_t      m_last_cpu_type;
    sim_state_t     m_last_state;
    bool            m_followPC;
};

#endif
