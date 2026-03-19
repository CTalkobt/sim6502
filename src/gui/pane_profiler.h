#ifndef SIM_PANE_PROFILER_H
#define SIM_PANE_PROFILER_H

#include "pane_base.h"
#include <wx/bitmap.h>
#include <wx/notebook.h>
#include <wx/listctrl.h>
#include <wx/scrolwin.h>

class PaneProfiler : public SimPane {
public:
    PaneProfiler(wxWindow* parent, sim_session_t *sim);
    void RefreshPane(const SimSnapshot &snap) override;
    wxString GetPaneTitle() const override { return "Profiler"; }
    wxString GetPaneName() const override { return "Profiler"; }

private:
    void OnPaint(wxPaintEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnLeftClick(wxMouseEvent& event);
    void OnRightClick(wxMouseEvent& event);
    void OnClear(wxCommandEvent& event);
    void OnSaveImage(wxCommandEvent& event);
    void OnColumnClick(wxListEvent& event);
    void OnListItemActivated(wxListEvent& event);
    void OnZoomChange(wxCommandEvent& event);
    void OnCanvasSize(wxSizeEvent& event);

    uint16_t GetAddressAtPos(int x, int y);
    int      GetDrawSize();
    void UpdateList();
    void UpdateCanvasScroll();

    wxBitmap m_bitmap;
    wxImage  m_image;
    uint32_t m_maxHits;
    wxScrolledWindow* m_canvas;
    wxNotebook* m_notebook;
    wxListCtrl* m_list;
    wxStaticText* m_statusText;
    wxSlider*     m_zoomSlider;

    struct ProfilerItem {
        uint16_t addr;
        uint32_t hits;
        uint32_t cycles;
    };
    std::vector<ProfilerItem> m_items;
    int m_sortCol;
    bool m_sortAsc;
    int  m_zoom;
};

#endif
