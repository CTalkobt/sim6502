#ifndef SIM_PANE_STACK_H
#define SIM_PANE_STACK_H

#include "pane_base.h"
#include <wx/listctrl.h>
<<<<<<< HEAD
#include <wx/notebook.h>

class StackMemListCtrl : public wxListCtrl {
public:
    StackMemListCtrl(wxWindow* parent, sim_session_t* sim);
    wxString OnGetItemText(long item, long col) const override;
    wxListItemAttr* OnGetItemAttr(long item) const override;
    wxListItemAttr* OnGetItemColumnAttr(long item, long col) const override;

    uint8_t m_sp = 0xFF;  // current SP (0x00–0xFF); the cell at $0100|SP is highlighted

private:
    sim_session_t*          m_sim;
    mutable wxListItemAttr  m_sp_attr;  // amber highlight for the SP cell
};
=======
>>>>>>> e01e3fe (Debugging Panes)

class PaneStack : public SimPane {
public:
    PaneStack(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Stack"; }
    wxString GetPaneName() const override { return "Stack"; }

private:
<<<<<<< HEAD
    wxNotebook*       m_notebook;
    StackMemListCtrl* m_memdump;  // "Stack Memory" tab — virtual list $0100-$01FF
    wxListCtrl*       m_list;     // "Recent" tab — active stack entries
=======
    wxListCtrl* m_list;
>>>>>>> e01e3fe (Debugging Panes)
};

#endif
