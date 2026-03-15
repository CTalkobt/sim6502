#ifndef SIM_PANE_PROFILER_H
#define SIM_PANE_PROFILER_H

#include "pane_base.h"
#include <wx/bitmap.h>

class PaneProfiler : public SimPane {
public:
    PaneProfiler(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Profiler"; }
    wxString GetPaneName() const override { return "Profiler"; }

private:
    void OnPaint(wxPaintEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnClear(wxCommandEvent& event);

    wxBitmap m_bitmap;
    uint32_t m_maxHits;
};

#endif
