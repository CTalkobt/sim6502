#ifndef SIM_PANE_STACK_H
#define SIM_PANE_STACK_H

#include "pane_base.h"
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/textctrl.h>

class PaneStack : public SimPane {
public:
    PaneStack(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Stack"; }
    wxString GetPaneName() const override { return "Stack"; }

private:
    wxNotebook* m_notebook;
    wxTextCtrl* m_memdump;   // "Stack Memory" tab — full $0100-$01FF hex dump
    wxListCtrl* m_list;      // "Recent" tab — active stack entries
};

#endif
