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

private:
    void OnToggleBreakpoint(wxListEvent& event);
    void OnSyncToPC(wxCommandEvent& event);
    void OnGoToAddress(wxCommandEvent& event);
    void OnPrevPage(wxCommandEvent& event);
    void OnNextPage(wxCommandEvent& event);

    DisasmListCtrl* m_list;
    wxTextCtrl*     m_addrSearch;
    bool            m_followPC;
};

#endif
