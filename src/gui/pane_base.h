#ifndef SIM_PANE_BASE_H
#define SIM_PANE_BASE_H

#include <wx/wx.h>
#include <wx/aui/aui.h>
#include "sim_api.h"

struct SimSnapshot {
    CPU *cpu;
    const memory_t *mem;
    sim_state_t state;
};

class SimPane : public wxPanel {
public:
    SimPane(wxWindow* parent, sim_session_t *sim)
        : wxPanel(parent), m_sim(sim) {}

    virtual void RefreshPane(const SimSnapshot &snap) = 0;
    virtual wxString GetPaneTitle() const = 0;
    virtual wxString GetPaneName() const = 0;

protected:
    sim_session_t *m_sim;
};

#endif // SIM_PANE_BASE_H
