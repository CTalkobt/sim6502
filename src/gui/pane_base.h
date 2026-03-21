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
        : wxPanel(parent), m_sim(sim), m_dockButton(nullptr)
    {
        m_dockButton = new wxButton(this, wxID_ANY, "Dock",
                                    wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        m_dockButton->Hide();

        Bind(wxEVT_SHOW, [this](wxShowEvent& ev) {
            UpdateDockButton();
            ev.Skip();
        });
        Bind(wxEVT_SIZE, [this](wxSizeEvent& ev) {
            UpdateDockButton();  // re-evaluate float state on every resize
            ev.Skip();
        });
        m_dockButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { DockPane(); });
    }

    virtual void RefreshPane(const SimSnapshot &snap) = 0;
    virtual wxString GetPaneTitle() const = 0;
    virtual wxString GetPaneName() const = 0;

    // Session persistence – override in panes that have user-defined state
    virtual void SaveState(wxConfigBase* /*cfg*/) {}
    virtual void LoadState(wxConfigBase* /*cfg*/) {}

    void DockPane() {
        wxFrame* mainFrame = wxDynamicCast(wxTheApp->GetTopWindow(), wxFrame);
        wxAuiManager* mgr = mainFrame ? wxAuiManager::GetManager(mainFrame) : nullptr;
        if (mgr) {
            wxAuiPaneInfo& info = mgr->GetPane(this);
            if (info.IsOk()) { info.Dock(); mgr->Update(); }
        }
    }

protected:
    sim_session_t *m_sim;

    // Call from subclass OnShow overrides to keep the overlay in sync
    void UpdateDockButton() {
        if (!m_dockButton) return;
        bool isFloating = (wxGetTopLevelParent(this) != wxTheApp->GetTopWindow());
        bool show = IsShown() && isFloating;
        m_dockButton->Show(show);
        if (show) PositionDockButton();
    }

    void HideDockButton() {
        if (m_dockButton) m_dockButton->Hide();
    }

private:
    void PositionDockButton() {
        if (!m_dockButton) return;
        wxSize btnSz = m_dockButton->GetBestSize();
        wxSize paneSz = GetClientSize();
        // Top-right corner with a small margin
        m_dockButton->SetSize(paneSz.x - btnSz.x - 4, 4, btnSz.x, btnSz.y);
        m_dockButton->Raise();
    }

    wxButton* m_dockButton;
};

#endif // SIM_PANE_BASE_H
