#include "pane_profiler.h"
#include <wx/dcclient.h>
#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <wx/image.h>

PaneProfiler::PaneProfiler(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim), m_bitmap(256, 256), m_maxHits(1) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddTool(601, "Clear", wxArtProvider::GetBitmap(wxART_DELETE));
    toolBar->Realize();
    sizer->Add(toolBar, 0, wxEXPAND);

    wxPanel* canvas = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(256, 256));
    canvas->SetBackgroundStyle(wxBG_STYLE_PAINT);
    sizer->Add(canvas, 1, wxEXPAND);
    SetSizer(sizer);

    canvas->Bind(wxEVT_PAINT, &PaneProfiler::OnPaint, this);
    canvas->Bind(wxEVT_MOTION, &PaneProfiler::OnMouseMove, this);
    toolBar->Bind(wxEVT_TOOL, &PaneProfiler::OnClear, this, 601);

    sim_profiler_enable(m_sim, 1);
}

void PaneProfiler::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    
    m_maxHits = 1;
    for (int i = 0; i < 65536; i++) {
        uint32_t hits = sim_profiler_get_exec(m_sim, (uint16_t)i);
        if (hits > m_maxHits) m_maxHits = hits;
    }

    wxImage img(256, 256);
    unsigned char* rgb = (unsigned char*)malloc(256 * 256 * 3);
    
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            uint16_t addr = (uint16_t)(y * 256 + x);
            uint32_t hits = sim_profiler_get_exec(m_sim, addr);
            int idx = (y * 256 + x) * 3;
            
            if (hits == 0) {
                rgb[idx] = 0; rgb[idx+1] = 0; rgb[idx+2] = 0;
            } else {
                float intensity = (float)hits / (float)m_maxHits;
                rgb[idx] = (unsigned char)(255 * intensity);
                rgb[idx+1] = (unsigned char)(255 * (1.0f - intensity));
                rgb[idx+2] = 0;
            }
        }
    }
    
    img.SetData(rgb); 
    m_bitmap = wxBitmap(img);
    Refresh();
}

void PaneProfiler::OnPaint(wxPaintEvent& event) {
    wxWindow* win = (wxWindow*)event.GetEventObject();
    wxPaintDC pdc(win);
    if (m_bitmap.IsOk()) {
        pdc.DrawBitmap(m_bitmap, 0, 0, false);
    }
}

void PaneProfiler::OnMouseMove(wxMouseEvent& event) {
    int x = event.GetX();
    int y = event.GetY();
    if (x >= 0 && x < 256 && y >= 0 && y < 256) {
        uint16_t addr = (uint16_t)(y * 256 + x);
        uint32_t hits = sim_profiler_get_exec(m_sim, addr);
        uint32_t cycles = sim_profiler_get_cycles(m_sim, addr);
        ((wxWindow*)event.GetEventObject())->SetToolTip(wxString::Format("Addr: $%04X\nHits: %u\nCycles: %u", addr, hits, cycles));
    }
}

void PaneProfiler::OnClear(wxCommandEvent& WXUNUSED(event)) {
    sim_profiler_clear(m_sim);
    RefreshPane(SimSnapshot{});
}
