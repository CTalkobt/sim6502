#ifndef SIM_PANE_BASE_H
#define SIM_PANE_BASE_H

#include <wx/wx.h>
#include <wx/aui/aui.h>
#include <wx/config.h>
#include "sim_api.h"

struct SimSnapshot {
    CPU *cpu;
    const memory_t *mem;
    sim_state_t state;
    bool running;     // UI-level "Run" state; true if the timer is actively calling sim_step()
};

class SimPane : public wxPanel {
public:
    SimPane(wxWindow* parent, sim_session_t *sim)
        : wxPanel(parent), m_sim(sim) {}

    virtual void RefreshPane(const SimSnapshot &snap) = 0;
    virtual wxString GetPaneTitle() const = 0;
    virtual wxString GetPaneName() const = 0;

    // Session persistence – override in panes that have user-defined state
    virtual void SaveState(wxConfigBase* /*cfg*/) {}
    virtual void LoadState(wxConfigBase* /*cfg*/) {}

protected:
    sim_session_t *m_sim;
};

#endif // SIM_PANE_BASE_H
