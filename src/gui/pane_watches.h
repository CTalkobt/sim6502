#ifndef SIM_PANE_WATCHES_H
#define SIM_PANE_WATCHES_H

#include "pane_base.h"
#include <wx/listctrl.h>

struct Watch {
    wxString label;
    uint16_t address;
    uint8_t  last_val;
};

class PaneWatches : public SimPane {
public:
    PaneWatches(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Watch List"; }
    wxString GetPaneName() const override { return "Watches"; }
    void SaveState(wxConfigBase* cfg) override;
    void LoadState(wxConfigBase* cfg) override;

    void AddWatch(uint16_t addr, const wxString& label = "");

private:
    void OnAdd(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void OnContextMenu(wxListEvent& event);

    wxListCtrl* m_list;
    std::vector<Watch> m_watches;
};

#endif
