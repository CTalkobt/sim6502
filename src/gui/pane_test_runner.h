#ifndef SIM_PANE_TEST_RUNNER_H
#define SIM_PANE_TEST_RUNNER_H

#include "pane_base.h"
#include <wx/listctrl.h>
#include <wx/textctrl.h>

struct TestRow {
    char label[48];
    char in_a[4], in_x[4], in_y[4], in_z[4], in_p[4];
    char ex_a[4], ex_x[4], ex_y[4], ex_z[4], ex_p[4];
    int  result; // -1=none, 0=fail, 1=pass
    char fail_msg[128];
};

class PaneTestRunner : public SimPane {
public:
    PaneTestRunner(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Test Runner"; }
    wxString GetPaneName() const override { return "TestRunner"; }

private:
    void OnAddRow(wxCommandEvent& event);
    void OnRunAll(wxCommandEvent& event);
    void OnItemEdit(wxListEvent& event);

    wxTextCtrl* m_routineAddr;
    wxTextCtrl* m_scratchAddr;
    wxListCtrl* m_list;
    std::vector<TestRow> m_rows;
};

#endif
