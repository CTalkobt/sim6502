#ifndef SIM_PANE_REGISTERS_H
#define SIM_PANE_REGISTERS_H

#include "pane_base.h"
#include "cpu_types.h"
#include <wx/listctrl.h>

class PaneRegisters : public SimPane {
public:
    PaneRegisters(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Registers"; }
    wxString GetPaneName() const override { return "Registers"; }

private:
    void UpdateRow(int row, const wxString& name, uint32_t val, uint32_t prev, bool is16 = false);
    void RebuildRows(cpu_type_t cpu_type);
    void OnEditRegister(wxListEvent& event);
    void OnLeftClick(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnEditorEnter(wxCommandEvent& event);
    void OnEditorKillFocus(wxFocusEvent& event);
    void CommitEdit(int row, const wxString& newValue);
    void HideEditor();
    void OpenEditorForRow(long row, int col);
    long GetColumnAt(const wxPoint& pos);

    wxListCtrl* m_list;
    wxTextCtrl* m_textEditor;
    int         m_editRow;
    int         m_editCol;
    CPUState    m_current_cpu;
    CPUState    m_prev_cpu;
    bool        m_prev_valid;
    cpu_type_t  m_last_cpu_type;
    bool        m_has_z_b;   // true for 65CE02 / 45GS02
};

#endif
