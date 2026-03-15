#ifndef SIM_PANE_DISASSEMBLY_H
#define SIM_PANE_DISASSEMBLY_H

#include "pane_base.h"
#include <wx/listctrl.h>

class DisasmListCtrl : public wxListCtrl {
public:
    DisasmListCtrl(wxWindow* parent, sim_session_t* sim);
    void SetPC(uint16_t pc);
    wxString OnGetItemText(long item, long column) const override;
    wxListItemAttr* OnGetItemAttr(long item) const override;

private:
    sim_session_t* m_sim;
    uint16_t       m_current_pc;
};

class PaneDisassembly : public SimPane {
public:
    PaneDisassembly(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Disassembly"; }
    wxString GetPaneName() const override { return "Disassembly"; }

private:
    void OnToggleBreakpoint(wxListEvent& event);

    DisasmListCtrl* m_list;
};

#endif
