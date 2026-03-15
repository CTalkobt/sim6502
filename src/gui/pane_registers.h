#ifndef SIM_PANE_REGISTERS_H
#define SIM_PANE_REGISTERS_H

#include "pane_base.h"
#include <wx/listctrl.h>

class PaneRegisters : public SimPane {
public:
    PaneRegisters(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Registers"; }
    wxString GetPaneName() const override { return "Registers"; }

private:
    void UpdateRow(int row, const wxString& name, uint32_t val, uint32_t prev, bool is16 = false);
    void OnEditRegister(wxListEvent& event);
    void OnLeftClick(wxMouseEvent& event);
    void OnEditorEnter(wxCommandEvent& event);
    void OnEditorKillFocus(wxFocusEvent& event);
    void CommitEdit(int row, const wxString& newValue);
    void HideEditor();

    wxListCtrl* m_list;
    wxTextCtrl* m_textEditor;
    int         m_editRow;
    CPUState    m_prev_cpu;
    bool        m_prev_valid;
};

#endif
