#ifndef SIM_PANE_WATCHES_H
#define SIM_PANE_WATCHES_H

#include "pane_base.h"
#include <wx/listctrl.h>

struct Watch {
    wxString label;
    uint16_t address;
    uint32_t last_val;
    int      width;     // 1, 2, or 4 bytes
    bool     changed;
};

class PaneWatches : public SimPane {
public:
    PaneWatches(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Watch List"; }
    wxString GetPaneName() const override { return "Watches"; }
    void SaveState(wxConfigBase* cfg) override;
    void LoadState(wxConfigBase* cfg) override;

    void AddWatch(uint16_t addr, const wxString& label = "", int width = 1);

private:
    void OnAddTool(wxCommandEvent& event);
    void OnDeleteTool(wxCommandEvent& event);
    void OnClearAllTool(wxCommandEvent& event);
    void OnContextMenu(wxListEvent& event);
    void OnEndLabelEdit(wxListEvent& event);
    void OnKeyDown(wxListEvent& event);

    wxListCtrl* m_list;
    std::vector<Watch> m_watches;
};

#endif
