#include "pane_profiler.h"
#include "main_frame.h"
#include <wx/dcclient.h>
#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <wx/image.h>
#include <wx/menu.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/slider.h>
#include <algorithm>

enum {
    ID_PROFILER_CLEAR = 601,
    ID_PROFILER_SAVE = 602,
    ID_PROFILER_LIST = 603,
    ID_PROFILER_ZOOM = 604
};

PaneProfiler::PaneProfiler(wxWindow* parent, sim_session_t *sim)
    : SimPane(parent, sim), m_bitmap(256, 256), m_image(256, 256), 
      m_maxHits(1), m_sortCol(1), m_sortAsc(false), m_zoom(1) 
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    wxToolBar* toolBar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
    toolBar->AddTool(ID_PROFILER_SAVE, "Save", wxArtProvider::GetBitmap(wxART_FILE_SAVE), "Save heatmap as PNG");
    toolBar->AddTool(ID_PROFILER_CLEAR, "Clear", wxArtProvider::GetBitmap(wxART_DELETE), "Reset all profiler execution and cycle counts to zero");
    toolBar->AddSeparator();
    
    toolBar->AddControl(new wxStaticText(toolBar, wxID_ANY, " Zoom: "));
    m_zoomSlider = new wxSlider(toolBar, ID_PROFILER_ZOOM, 1, 1, 16, wxDefaultPosition, wxSize(120, -1), wxSL_HORIZONTAL);
    toolBar->AddControl(m_zoomSlider);
    
    toolBar->Realize();
    mainSizer->Add(toolBar, 0, wxEXPAND);

    m_notebook = new wxNotebook(this, wxID_ANY);

    // Tab 1: Heatmap
    wxPanel* heatmapPage = new wxPanel(m_notebook);
    wxBoxSizer* heatmapSizer = new wxBoxSizer(wxVERTICAL);
    
    m_canvas = new wxScrolledWindow(heatmapPage, wxID_ANY, wxDefaultPosition, wxSize(256, 256));
    m_canvas->SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_canvas->SetScrollRate(10, 10);
    heatmapSizer->Add(m_canvas, 1, wxEXPAND);
    
    m_statusText = new wxStaticText(heatmapPage, wxID_ANY, "Hover over map to see stats");
    heatmapSizer->Add(m_statusText, 0, wxEXPAND | wxALL, 5);
    
    heatmapPage->SetSizer(heatmapSizer);
    m_notebook->AddPage(heatmapPage, "Heatmap");

    // Tab 2: List
    m_list = new wxListCtrl(m_notebook, ID_PROFILER_LIST, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->InsertColumn(0, "Address", wxLIST_FORMAT_LEFT, 80);
    m_list->InsertColumn(1, "Hits", wxLIST_FORMAT_RIGHT, 100);
    m_list->InsertColumn(2, "Cycles", wxLIST_FORMAT_RIGHT, 100);
    m_notebook->AddPage(m_list, "List View");

    mainSizer->Add(m_notebook, 1, wxEXPAND);
    SetSizer(mainSizer);

    m_canvas->Bind(wxEVT_PAINT, &PaneProfiler::OnPaint, this);
    m_canvas->Bind(wxEVT_MOTION, &PaneProfiler::OnMouseMove, this);
    m_canvas->Bind(wxEVT_LEFT_DOWN, &PaneProfiler::OnLeftClick, this);
    m_canvas->Bind(wxEVT_RIGHT_DOWN, &PaneProfiler::OnRightClick, this);
    m_canvas->Bind(wxEVT_SIZE, &PaneProfiler::OnCanvasSize, this);
    
    m_list->Bind(wxEVT_LIST_COL_CLICK, &PaneProfiler::OnColumnClick, this);
    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, &PaneProfiler::OnListItemActivated, this);

    toolBar->Bind(wxEVT_TOOL, &PaneProfiler::OnClear, this, ID_PROFILER_CLEAR);
    toolBar->Bind(wxEVT_TOOL, &PaneProfiler::OnSaveImage, this, ID_PROFILER_SAVE);
    m_zoomSlider->Bind(wxEVT_SLIDER, &PaneProfiler::OnZoomChange, this);

    sim_profiler_enable(m_sim, 1);
    UpdateCanvasScroll();
}

void PaneProfiler::RefreshPane(const SimSnapshot &snap) {
    (void)snap;
    
    m_maxHits = 1;
    m_items.clear();

    unsigned char* rgb = (unsigned char*)malloc(256 * 256 * 3);
    
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            uint16_t addr = (uint16_t)(y * 256 + x);
            uint32_t hits = sim_profiler_get_exec(m_sim, addr);
            int idx = (y * 256 + x) * 3;
            
            if (hits > 0) {
                if (hits > m_maxHits) m_maxHits = hits;
                m_items.push_back({addr, hits, sim_profiler_get_cycles(m_sim, addr)});
            }

            if (hits == 0) {
                rgb[idx] = 0; rgb[idx+1] = 0; rgb[idx+2] = 0;
            } else {
                float intensity = (float)hits / (float)m_maxHits;
                if (intensity < 0.33f) {
                    float t = intensity / 0.33f;
                    rgb[idx] = 0; rgb[idx+1] = 0; rgb[idx+2] = (unsigned char)(255 * t);
                } else if (intensity < 0.66f) {
                    float t = (intensity - 0.33f) / 0.33f;
                    rgb[idx] = 0; rgb[idx+1] = (unsigned char)(255 * t); rgb[idx+2] = 255;
                } else {
                    float t = (intensity - 0.66f) / 0.34f;
                    rgb[idx] = (unsigned char)(255 * t); rgb[idx+1] = 255; rgb[idx+2] = 255;
                }
            }
        }
    }
    
    m_image.SetData(rgb); 
    m_bitmap = wxBitmap(m_image);
    m_canvas->Refresh();

    UpdateList();
}

void PaneProfiler::UpdateList() {
    std::sort(m_items.begin(), m_items.end(), [this](const ProfilerItem& a, const ProfilerItem& b) {
        bool res;
        if (m_sortCol == 0) res = a.addr < b.addr;
        else if (m_sortCol == 1) res = a.hits < b.hits;
        else res = a.cycles < b.cycles;
        return m_sortAsc ? res : !res;
    });

    m_list->Freeze();
    m_list->DeleteAllItems();
    
    size_t count = std::min(m_items.size(), (size_t)1000);
    for (size_t i = 0; i < count; i++) {
        long itemIdx = m_list->InsertItem(i, wxString::Format("$%04X", m_items[i].addr));
        m_list->SetItem(itemIdx, 1, wxString::Format("%u", m_items[i].hits));
        m_list->SetItem(itemIdx, 2, wxString::Format("%u", m_items[i].cycles));
        m_list->SetItemData(itemIdx, (long)m_items[i].addr);
    }
    m_list->Thaw();
}

int PaneProfiler::GetDrawSize() {
    wxSize clientSz = m_canvas->GetClientSize();
    int fitSize = std::min(clientSz.GetWidth(), clientSz.GetHeight());
    if (fitSize < 256) fitSize = 256;
    return fitSize * m_zoom;
}

void PaneProfiler::UpdateCanvasScroll() {
    int sz = GetDrawSize();
    m_canvas->SetVirtualSize(sz, sz);
    m_canvas->Refresh();
}

void PaneProfiler::OnCanvasSize(wxSizeEvent& event) {
    UpdateCanvasScroll();
    event.Skip();
}

void PaneProfiler::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxPaintDC pdc(m_canvas);
    m_canvas->DoPrepareDC(pdc);

    int sz = GetDrawSize();

    if (m_bitmap.IsOk()) {
        wxImage scaled = m_image.Scale(sz, sz);
        wxBitmap bmp(scaled);
        pdc.DrawBitmap(bmp, 0, 0, false);
    }

    // Legend (drawn screen-relative, not scroll-relative)
    // We need to use a separate DC or adjust for the view start if we want it fixed
    wxSize clientSz = m_canvas->GetClientSize();
    int vx, vy;
    m_canvas->GetViewStart(&vx, &vy);
    int unitX, unitY;
    m_canvas->GetScrollPixelsPerUnit(&unitX, &unitY);
    int lx_abs = 10 + vx * unitX;
    int ly_abs = clientSz.y - 20 - 5 + vy * unitY;

    int legendWidth = 120;
    int legendHeight = 15;

    pdc.SetPen(*wxWHITE_PEN);
    pdc.SetBrush(*wxTRANSPARENT_BRUSH);
    pdc.DrawRectangle(lx_abs, ly_abs, legendWidth, legendHeight);

    for (int i = 0; i < legendWidth; i++) {
        float intensity = (float)i / (float)legendWidth;
        unsigned char r, g, b;
        if (intensity < 0.33f) {
            float t = intensity / 0.33f;
            r = 0; g = 0; b = (unsigned char)(255 * t);
        } else if (intensity < 0.66f) {
            float t = (intensity - 0.33f) / 0.33f;
            r = 0; g = (unsigned char)(255 * t); b = 255;
        } else {
            float t = (intensity - 0.66f) / 0.34f;
            r = (unsigned char)(255 * t); g = 255; b = 255;
        }
        pdc.SetPen(wxPen(wxColour(r, g, b)));
        pdc.DrawLine(lx_abs + i, ly_abs, lx_abs + i, ly_abs + legendHeight);
    }

    pdc.SetTextForeground(*wxWHITE);
    pdc.SetFont(wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    pdc.DrawText("0", lx_abs, ly_abs - 12);
    pdc.DrawText(wxString::Format("%u", m_maxHits), lx_abs + legendWidth - 30, ly_abs - 12);
}

uint16_t PaneProfiler::GetAddressAtPos(int x, int y) {
    int vx, vy;
    m_canvas->GetViewStart(&vx, &vy);
    int unitX, unitY;
    m_canvas->GetScrollPixelsPerUnit(&unitX, &unitY);
    
    int absX = x + vx * unitX;
    int absY = y + vy * unitY;
    
    int sz = GetDrawSize();
    if (sz <= 0) return 0;

    int imgX = (absX * 256) / sz;
    int imgY = (absY * 256) / sz;

    if (imgX < 0) imgX = 0; 
    if (imgX > 255) imgX = 255;
    if (imgY < 0) imgY = 0; 
    if (imgY > 255) imgY = 255;

    return (uint16_t)(imgY * 256 + imgX);
}

void PaneProfiler::OnMouseMove(wxMouseEvent& event) {
    uint16_t addr = GetAddressAtPos(event.GetX(), event.GetY());
    uint32_t hits = sim_profiler_get_exec(m_sim, addr);
    uint32_t cycles = sim_profiler_get_cycles(m_sim, addr);
    
    wxString status = wxString::Format("Addr: $%04X | Hits: %u | Cycles: %u", addr, hits, cycles);
    m_statusText->SetLabel(status);
    m_canvas->SetToolTip(status);
}

void PaneProfiler::OnLeftClick(wxMouseEvent& event) {
    uint16_t addr = GetAddressAtPos(event.GetX(), event.GetY());
    MainFrame* frame = dynamic_cast<MainFrame*>(wxGetTopLevelParent(this));
    if (frame) {
        frame->NavigateDisassembly(addr);
    }
}

void PaneProfiler::OnRightClick(wxMouseEvent& event) {
    wxMenu menu;
    menu.Append(ID_PROFILER_SAVE, "Save Heatmap as PNG...");
    menu.AppendSeparator();
    menu.Append(ID_PROFILER_CLEAR, "Clear Profiler Data");
    
    m_canvas->PopupMenu(&menu, event.GetPosition());
}

void PaneProfiler::OnColumnClick(wxListEvent& event) {
    int col = event.GetColumn();
    if (col == m_sortCol) {
        m_sortAsc = !m_sortAsc;
    } else {
        m_sortCol = col;
        m_sortAsc = false;
    }
    UpdateList();
}

void PaneProfiler::OnListItemActivated(wxListEvent& event) {
    uint16_t addr = (uint16_t)event.GetData();
    MainFrame* frame = dynamic_cast<MainFrame*>(wxGetTopLevelParent(this));
    if (frame) {
        frame->NavigateDisassembly(addr);
    }
}

void PaneProfiler::OnZoomChange(wxCommandEvent& event) {
    m_zoom = event.GetInt();
    UpdateCanvasScroll();
}

void PaneProfiler::OnClear(wxCommandEvent& WXUNUSED(event)) {
    sim_profiler_clear(m_sim);
    RefreshPane(SimSnapshot{});
}

void PaneProfiler::OnSaveImage(wxCommandEvent& WXUNUSED(event)) {
    wxFileDialog dlg(this, "Save Heatmap", "", "heatmap.png",
                     "PNG files (*.png)|*.png", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        if (m_image.IsOk()) {
            m_image.SaveFile(dlg.GetPath(), wxBITMAP_TYPE_PNG);
        }
    }
}
