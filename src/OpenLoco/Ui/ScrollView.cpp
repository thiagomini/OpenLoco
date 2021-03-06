#include "ScrollView.h"
#include "../Input.h"
#include "../Interop/Interop.hpp"
#include "../Ui.h"
#include "WindowManager.h"
#include <cmath>

using namespace OpenLoco::Interop;

namespace OpenLoco::Ui::ScrollView
{
    static loco_global<Ui::ScrollView::scroll_part, 0x00523396> _currentScrollArea;
    // TODO: Convert to a scrollIndex when all scroll functions implemented
    static loco_global<uint32_t, 0x00523398> _currentScrollOffset;
    static void setCurrentScrollIndex(size_t index)
    {
        _currentScrollOffset = index * sizeof(Ui::scroll_area_t);
    }
    static size_t getCurrentScrollIndex()
    {
        return _currentScrollOffset / sizeof(Ui::scroll_area_t);
    }

    // 0x004C87E1
    // regs.bp: deltaX
    static void horizontalFollow(Ui::window* const w, Ui::widget_t* const widget, const widget_index widgetIndex, const size_t scrollIndex, const int16_t deltaX)
    {
        scroll_area_t& scrollArea = w->scroll_areas[scrollIndex];
        scrollArea.flags |= ScrollFlags::HSCROLLBAR_THUMB_PRESSED;

        uint16_t trackWidth = widget->width() - 2 - thumbSize - thumbSize;
        if (scrollArea.flags & ScrollFlags::VSCROLLBAR_VISIBLE)
        {
            trackWidth -= barWidth;
        }

        if (trackWidth == 0)
        {
            return;
        }
        auto contentDeltaX = deltaX * scrollArea.contentWidth / trackWidth;

        int16_t newOffset = scrollArea.contentOffsetX + contentDeltaX;

        int frameWidth = widget->width() - 2;
        if (scrollArea.flags & ScrollFlags::VSCROLLBAR_VISIBLE)
        {
            frameWidth -= barWidth;
        }

        int16_t maxOffset = scrollArea.contentWidth - frameWidth;
        maxOffset = std::max<int16_t>(maxOffset, 0);

        scrollArea.contentOffsetX = std::clamp<int16_t>(newOffset, 0, maxOffset);

        ScrollView::updateThumbs(w, widgetIndex);
        WindowManager::invalidateWidget(w->type, w->number, widgetIndex);
    }

    // 0x004C8898
    // regs.bp: deltaY
    static void verticalFollow(Ui::window* const w, Ui::widget_t* const widget, const widget_index widgetIndex, const size_t scrollIndex, const int16_t deltaY)
    {
        scroll_area_t& scrollArea = w->scroll_areas[scrollIndex];
        scrollArea.flags |= ScrollFlags::VSCROLLBAR_THUMB_PRESSED;

        uint16_t trackHeight = widget->height() - 2 - thumbSize - thumbSize;
        if ((scrollArea.flags & ScrollFlags::HSCROLLBAR_VISIBLE) != 0)
        {
            trackHeight -= barWidth;
        }

        if (trackHeight == 0)
        {
            return;
        }
        auto contentDeltaY = deltaY * scrollArea.contentHeight / trackHeight;

        int16_t newOffset = scrollArea.contentOffsetY + contentDeltaY;

        int frameHeight = widget->height() - 2;
        if ((scrollArea.flags & ScrollFlags::HSCROLLBAR_VISIBLE) != 0)
        {
            frameHeight -= barWidth;
        }

        int16_t maxOffset = scrollArea.contentHeight - frameHeight;
        maxOffset = std::max<int16_t>(maxOffset, 0);

        scrollArea.contentOffsetY = std::clamp<int16_t>(newOffset, 0, maxOffset);

        ScrollView::updateThumbs(w, widgetIndex);
        WindowManager::invalidateWidget(w->type, w->number, widgetIndex);
    }

    // 0x004C8CFD
    // regs.bp: deltaX
    void horizontalDragFollow(Ui::window* const w, Ui::widget_t* const widget, const widget_index dragWidgetIndex, const size_t dragScrollIndex, const int16_t deltaX)
    {
        scroll_area_t& scrollArea = w->scroll_areas[dragScrollIndex];
        if ((scrollArea.flags & ScrollFlags::HSCROLLBAR_VISIBLE) == 0)
        {
            return;
        }

        uint16_t trackWidth = widget->width() - 2 - thumbSize - thumbSize;
        if (scrollArea.flags & ScrollFlags::VSCROLLBAR_VISIBLE)
        {
            trackWidth -= barWidth;
        }

        if (trackWidth == 0)
        {
            return;
        }
        auto contentDeltaX = deltaX * scrollArea.contentWidth / trackWidth;

        int16_t newOffset = scrollArea.contentOffsetX + contentDeltaX;

        int frameWidth = widget->width() - 2;
        if (scrollArea.flags & ScrollFlags::VSCROLLBAR_VISIBLE)
        {
            frameWidth -= barWidth;
        }

        int16_t maxOffset = scrollArea.contentWidth - frameWidth;
        maxOffset = std::max<int16_t>(maxOffset, 0);

        scrollArea.contentOffsetX = std::clamp<int16_t>(newOffset, 0, maxOffset);

        ScrollView::updateThumbs(w, dragWidgetIndex);
        WindowManager::invalidateWidget(w->type, w->number, dragWidgetIndex);
    }

    // 0x004C8E2E
    // regs.bp: deltaY
    // possible extraction of common functionality in verticalDragFollow, horizontalDragFollow, verticalFollow, horizontalFollow
    void verticalDragFollow(Ui::window* const w, Ui::widget_t* const widget, const widget_index dragWidgetIndex, const size_t dragScrollIndex, const int16_t deltaY)
    {
        scroll_area_t& scrollArea = w->scroll_areas[dragScrollIndex];
        if ((scrollArea.flags & ScrollFlags::VSCROLLBAR_VISIBLE) == 0)
        {
            return;
        }

        uint16_t trackHeight = widget->height() - 2 - thumbSize - thumbSize;
        if ((scrollArea.flags & ScrollFlags::HSCROLLBAR_VISIBLE) != 0)
        {
            trackHeight -= barWidth;
        }

        if (trackHeight == 0)
        {
            return;
        }
        auto contentDeltaY = deltaY * scrollArea.contentHeight / trackHeight;

        int16_t newOffset = scrollArea.contentOffsetY + contentDeltaY;

        int frameHeight = widget->height() - 2;
        if ((scrollArea.flags & ScrollFlags::HSCROLLBAR_VISIBLE) != 0)
        {
            frameHeight -= barWidth;
        }

        int16_t maxOffset = scrollArea.contentHeight - frameHeight;
        maxOffset = std::max<int16_t>(maxOffset, 0);

        scrollArea.contentOffsetY = std::clamp<int16_t>(newOffset, 0, maxOffset);

        ScrollView::updateThumbs(w, dragWidgetIndex);
        WindowManager::invalidateWidget(w->type, w->number, dragWidgetIndex);
    }

    // 0x004C8EF0
    // Note: Original function returns a scrollAreaOffset not an index
    void getPart(
        Ui::window* window,
        Ui::widget_t* widget,
        int16_t x,
        int16_t y,
        int16_t* output_x,
        int16_t* output_y,
        scroll_part* output_scroll_area,
        size_t* scrollIndex)
    {
        registers regs;
        regs.ax = x;
        regs.bx = y;
        regs.esi = (uint32_t)window;
        regs.edi = (uint32_t)widget;

        call(0x004C8EF0, regs);

        *output_x = regs.ax;
        *output_y = regs.bx;
        *output_scroll_area = (scroll_part)regs.cx;
        *scrollIndex = regs.edx / sizeof(scroll_area_t);
    }

    // 0x004CA1ED
    void updateThumbs(window* window, widget_index widgetIndex)
    {
        registers regs;

        regs.esi = (uintptr_t)window;
        regs.ebx = window->getScrollDataIndex(widgetIndex) * sizeof(scroll_area_t);
        regs.edi = (uintptr_t)&window->widgets[widgetIndex];
        call(0x4CA1ED, regs);
    }

    // 0x004C894F
    static void hButtonLeft(Ui::window* const w, const size_t scrollAreaIndex, const widget_index widgetIndex)
    {
        w->scroll_areas[scrollAreaIndex].flags |= ScrollFlags::HSCROLLBAR_LEFT_PRESSED;
        w->scroll_areas[scrollAreaIndex].contentOffsetX = std::max(w->scroll_areas[scrollAreaIndex].contentOffsetX - buttonClickStep, 0);
        ScrollView::updateThumbs(w, widgetIndex);
        WindowManager::invalidateWidget(w->type, w->number, widgetIndex);
    }

    // 0x004C89AE
    static void hButtonRight(Ui::window* const w, const size_t scrollAreaIndex, const widget_index widgetIndex)
    {
        w->scroll_areas[scrollAreaIndex].flags |= ScrollFlags::HSCROLLBAR_RIGHT_PRESSED;
        int16_t trackWidth = w->widgets[widgetIndex].width() - 2;
        if ((w->scroll_areas[scrollAreaIndex].flags & ScrollFlags::VSCROLLBAR_VISIBLE) != 0)
        {
            trackWidth -= barWidth;
        }
        int16_t widgetContentWidth = std::max(w->scroll_areas[scrollAreaIndex].contentWidth - trackWidth, 0);
        w->scroll_areas[scrollAreaIndex].contentOffsetX = std::min<int16_t>(w->scroll_areas[scrollAreaIndex].contentOffsetX + buttonClickStep, widgetContentWidth);
        ScrollView::updateThumbs(w, widgetIndex);
        WindowManager::invalidateWidget(w->type, w->number, widgetIndex);
    }

    // 0x004C8A36
    static void hTrackLeft(Ui::window* const w, const size_t scrollAreaIndex, const widget_index widgetIndex)
    {
        int16_t trackWidth = w->widgets[widgetIndex].width() - 2;
        if ((w->scroll_areas[scrollAreaIndex].flags & ScrollFlags::VSCROLLBAR_VISIBLE) != 0)
        {
            trackWidth -= barWidth;
        }
        w->scroll_areas[scrollAreaIndex].contentOffsetX = std::max(w->scroll_areas[scrollAreaIndex].contentOffsetX - trackWidth, 0);
        ScrollView::updateThumbs(w, widgetIndex);
        WindowManager::invalidateWidget(w->type, w->number, widgetIndex);
    }

    // 0x004C8AA6
    static void hTrackRight(Ui::window* const w, const size_t scrollAreaIndex, const widget_index widgetIndex)
    {
        int16_t trackWidth = w->widgets[widgetIndex].width() - 2;
        if ((w->scroll_areas[scrollAreaIndex].flags & ScrollFlags::VSCROLLBAR_VISIBLE) != 0)
        {
            trackWidth -= barWidth;
        }
        int16_t widgetContentWidth = std::max(w->scroll_areas[scrollAreaIndex].contentWidth - trackWidth, 0);
        w->scroll_areas[scrollAreaIndex].contentOffsetX = std::min<int16_t>(w->scroll_areas[scrollAreaIndex].contentOffsetX + trackWidth, widgetContentWidth);
        ScrollView::updateThumbs(w, widgetIndex);
        WindowManager::invalidateWidget(w->type, w->number, widgetIndex);
    }

    void verticalNudgeUp(Ui::window* const w, const size_t scrollAreaIndex, const widget_index widgetIndex)
    {
        w->scroll_areas[scrollAreaIndex].contentOffsetY = std::max(w->scroll_areas[scrollAreaIndex].contentOffsetY - buttonClickStep, 0);
        ScrollView::updateThumbs(w, widgetIndex);
        WindowManager::invalidateWidget(w->type, w->number, widgetIndex);
    }

    // 0x004C8B26
    static void vButtonTop(Ui::window* const w, const size_t scrollAreaIndex, const widget_index widgetIndex)
    {
        w->scroll_areas[scrollAreaIndex].flags |= ScrollFlags::VSCROLLBAR_UP_PRESSED;
        verticalNudgeUp(w, scrollAreaIndex, widgetIndex);
    }

    void verticalNudgeDown(Ui::window* const w, const size_t scrollAreaIndex, const widget_index widgetIndex)
    {
        int16_t trackHeight = w->widgets[widgetIndex].height() - 2;
        if ((w->scroll_areas[scrollAreaIndex].flags & ScrollFlags::HSCROLLBAR_VISIBLE) != 0)
        {
            trackHeight -= barWidth;
        }
        int16_t widgetContentHeight = std::max(w->scroll_areas[scrollAreaIndex].contentHeight - trackHeight, 0);
        w->scroll_areas[scrollAreaIndex].contentOffsetY = std::min<int16_t>(w->scroll_areas[scrollAreaIndex].contentOffsetY + buttonClickStep, widgetContentHeight);
        ScrollView::updateThumbs(w, widgetIndex);
        WindowManager::invalidateWidget(w->type, w->number, widgetIndex);
    }

    // 0x004C8B85
    static void vButtonBottom(Ui::window* const w, const size_t scrollAreaIndex, const widget_index widgetIndex)
    {
        w->scroll_areas[scrollAreaIndex].flags |= ScrollFlags::VSCROLLBAR_DOWN_PRESSED;
        verticalNudgeDown(w, scrollAreaIndex, widgetIndex);
    }

    // 0x004C8C0D
    static void vTrackTop(Ui::window* const w, const size_t scrollAreaIndex, const widget_index widgetIndex)
    {
        int16_t trackHeight = w->widgets[widgetIndex].height() - 2;
        if ((w->scroll_areas[scrollAreaIndex].flags & ScrollFlags::HSCROLLBAR_VISIBLE) != 0)
        {
            trackHeight -= barWidth;
        }
        w->scroll_areas[scrollAreaIndex].contentOffsetY = std::max(w->scroll_areas[scrollAreaIndex].contentOffsetY - trackHeight, 0);
        ScrollView::updateThumbs(w, widgetIndex);
        WindowManager::invalidateWidget(w->type, w->number, widgetIndex);
    }

    // 0x004C8C7D
    static void vTrackBottom(Ui::window* const w, const size_t scrollAreaIndex, const widget_index widgetIndex)
    {
        int16_t trackHeight = w->widgets[widgetIndex].height() - 2;
        if ((w->scroll_areas[scrollAreaIndex].flags & ScrollFlags::HSCROLLBAR_VISIBLE) != 0)
        {
            trackHeight -= barWidth;
        }
        int16_t widgetContentHeight = std::max(w->scroll_areas[scrollAreaIndex].contentHeight - trackHeight, 0);
        w->scroll_areas[scrollAreaIndex].contentOffsetY = std::min<int16_t>(w->scroll_areas[scrollAreaIndex].contentOffsetY + trackHeight, widgetContentHeight);
        ScrollView::updateThumbs(w, widgetIndex);
        WindowManager::invalidateWidget(w->type, w->number, widgetIndex);
    }

    // 0x004C8689
    void scrollLeftBegin(const int16_t x, const int16_t y, Ui::window* const w, Ui::widget_t* const widget, const widget_index widgetIndex)
    {
        Ui::ScrollView::scroll_part scrollArea;
        int16_t outX, outY;
        size_t scrollIndex;

        Ui::ScrollView::getPart(w, widget, x, y, &outX, &outY, &scrollArea, &scrollIndex);

        _currentScrollArea = scrollArea;
        setCurrentScrollIndex(scrollIndex);

        // Not implemented for any window
        // window->call_22()
        switch (scrollArea)
        {
            case Ui::ScrollView::scroll_part::view:
                w->callScrollMouseDown(outX, outY, static_cast<uint8_t>(scrollIndex));
                break;
            case Ui::ScrollView::scroll_part::hscrollbar_button_left:
                hButtonLeft(w, scrollIndex, widgetIndex);
                break;
            case Ui::ScrollView::scroll_part::hscrollbar_button_right:
                hButtonRight(w, scrollIndex, widgetIndex);
                break;
            case Ui::ScrollView::scroll_part::hscrollbar_track_left:
                hTrackLeft(w, scrollIndex, widgetIndex);
                break;
            case Ui::ScrollView::scroll_part::hscrollbar_track_right:
                hTrackRight(w, scrollIndex, widgetIndex);
                break;
            case Ui::ScrollView::scroll_part::vscrollbar_button_top:
                vButtonTop(w, scrollIndex, widgetIndex);
                break;
            case Ui::ScrollView::scroll_part::vscrollbar_button_bottom:
                vButtonBottom(w, scrollIndex, widgetIndex);
                break;
            case Ui::ScrollView::scroll_part::vscrollbar_track_top:
                vTrackTop(w, scrollIndex, widgetIndex);
                break;
            case Ui::ScrollView::scroll_part::vscrollbar_track_bottom:
                vTrackBottom(w, scrollIndex, widgetIndex);
                break;
            default:
                break;
        }
    }

    // Based on 0x004C8689
    void scrollModalRight(const int16_t x, const int16_t y, Ui::window* const w, Ui::widget_t* const widget, const widget_index widgetIndex)
    {
        Ui::ScrollView::scroll_part scrollArea;
        int16_t outX, outY;
        size_t scrollIndex;

        Ui::ScrollView::getPart(w, widget, x, y, &outX, &outY, &scrollArea, &scrollIndex);

        _currentScrollArea = scrollArea;
        setCurrentScrollIndex(scrollIndex);

        if (scrollArea == Ui::ScrollView::scroll_part::view)
        {
            w->callScrollMouseDown(outX, outY, static_cast<uint8_t>(scrollIndex));
        }
    }

    // 0x004C72ED
    void clearPressedButtons(const WindowType type, const window_number number, const widget_index widgetIndex)
    {
        auto window = WindowManager::find(type, number);
        if (window == nullptr)
            return;

        auto scrollAreaIndex = getCurrentScrollIndex();

        constexpr uint16_t horizontalFlags = ScrollFlags::HSCROLLBAR_THUMB_PRESSED | ScrollFlags::HSCROLLBAR_LEFT_PRESSED | ScrollFlags::HSCROLLBAR_RIGHT_PRESSED;
        constexpr uint16_t verticalFlags = ScrollFlags::VSCROLLBAR_THUMB_PRESSED | ScrollFlags::VSCROLLBAR_UP_PRESSED | ScrollFlags::VSCROLLBAR_DOWN_PRESSED;

        window->scroll_areas[scrollAreaIndex].flags &= ~(verticalFlags | horizontalFlags);
        WindowManager::invalidateWidget(type, number, widgetIndex);
    }

    // 0x004C7236
    void scrollLeftContinue(const int16_t x, const int16_t y, Ui::window* const w, Ui::widget_t* const widget, const widget_index widgetIndex)
    {
        auto scrollIndex = getCurrentScrollIndex();
        if (_currentScrollArea == ScrollView::scroll_part::hscrollbar_thumb)
        {
            auto toolTipLoc = Input::getTooltipMouseLocation();
            int16_t deltaX = x - toolTipLoc.x;
            toolTipLoc.x = x;
            Input::setTooltipMouseLocation(toolTipLoc);
            ScrollView::horizontalFollow(w, widget, widgetIndex, scrollIndex, deltaX);
        }
        else if (_currentScrollArea == ScrollView::scroll_part::vscrollbar_thumb)
        {
            auto toolTipLoc = Input::getTooltipMouseLocation();
            int16_t deltaY = y - toolTipLoc.y;
            toolTipLoc.y = y;
            Input::setTooltipMouseLocation(toolTipLoc);
            ScrollView::verticalFollow(w, widget, widgetIndex, scrollIndex, deltaY);
        }
        else
        {
            Ui::ScrollView::scroll_part scrollArea;
            Gfx::point_t point;
            Ui::ScrollView::getPart(w, widget, x, y, &point.x, &point.y, &scrollArea, &scrollIndex);
            if (scrollArea != _currentScrollArea)
            {
                clearPressedButtons(w->type, w->number, widgetIndex);
                return;
            }

            switch (scrollArea)
            {
                case scroll_part::view: // 0x004C729A
                    w->callScrollMouseDrag(point.x, point.y, static_cast<uint8_t>(scrollIndex));
                    break;

                case scroll_part::hscrollbar_button_left:
                    hButtonLeft(w, scrollIndex, widgetIndex);
                    break;
                case scroll_part::hscrollbar_button_right:
                    hButtonRight(w, scrollIndex, widgetIndex);
                    break;
                case scroll_part::hscrollbar_track_left:
                case scroll_part::hscrollbar_track_right:
                    break;

                case scroll_part::vscrollbar_button_top:
                    vButtonTop(w, scrollIndex, widgetIndex);
                    break;
                case scroll_part::vscrollbar_button_bottom:
                    vButtonBottom(w, scrollIndex, widgetIndex);
                    break;
                case scroll_part::vscrollbar_track_top:
                case scroll_part::vscrollbar_track_bottom:
                    break;

                default:
                    break;
            }
        }
    }
}
