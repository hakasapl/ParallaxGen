#pragma once

#include <wx/arrstr.h>
#include <wx/dnd.h>
#include <wx/dragimag.h>
#include <wx/listbox.h>
#include <wx/listctrl.h>
#include <wx/msw/textctrl.h>
#include <wx/overlay.h>
#include <wx/sizer.h>
#include <wx/wx.h>

#include <string>
#include <unordered_set>
#include <vector>

/**
 * @brief wxDialog that allows the user to sort the mods in the order they want
 */
class ModSortDialog : public wxDialog {
private:
    wxListCtrl* m_listCtrl; /** Main list object that stores all the mods */
    wxTimer m_scrollTimer; /** Timer that is responsible for autoscroll */
    int m_listCtrlHeaderHeight = 0; /** Stores list ctrl header height for use in autoscroll */

    //
    // Item Highlighting
    //
    std::unordered_map<std::wstring, std::unordered_set<std::wstring>>
        m_conflictsMap; /** Stores the conflicts for each mod for highlighting */
    std::unordered_map<std::wstring, wxColour>
        m_originalBackgroundColors; /** Stores the original highlight of elements to be able to restore it later */

    //
    // Dragging
    //
    int m_targetLineIndex = -1; /** Stores the index of the element where an element is being dropped */
    std::vector<long> m_draggedIndices; /** Stores the indices being dragged in the case of multi selection */
    wxOverlay m_overlay; /** Overlay used to paint guide lines for dragging */

    bool m_sortAscending; /** Stores whether the list is in asc or desc order */

    constexpr static int DEFAULT_WIDTH = 300;
    constexpr static int DEFAULT_HEIGHT = 600;
    constexpr static int MIN_HEIGHT = 400;
    constexpr static int DEFAULT_PADDING = 20;
    constexpr static int DEFAULT_BORDER = 10;
    constexpr static int TIMER_INTERVAL = 250;

public:
    /**
     * @brief Construct a new Mod Sort Dialog object
     *
     * @param mods vector of mod strings
     * @param shaders vector of shader strings
     * @param isNew vector of bools indicating whether each mod is new or not (for highlighting)
     * @param conflicts map that stores mod conflicts for highlighting
     */
    ModSortDialog(const std::vector<std::wstring>& mods, const std::vector<std::wstring>& shaders,
        const std::vector<bool>& isNew,
        const std::unordered_map<std::wstring, std::unordered_set<std::wstring>>& conflicts);

    /**
     * @brief Get the list of sorted mods (meant to be called after the user presses okay)
     *
     * @return std::vector<std::wstring> list of sorted mods
     */
    [[nodiscard]] auto getSortedItems() const -> std::vector<std::wstring>;

private:
    /**
     * @brief Event handler that triggers when an item is selected in the list (highlighting)
     *
     * @param event wxWidgets event object
     */
    void onItemSelected(wxListEvent& event);

    /**
     * @brief Event handler that triggers when an item is deselected in the list
     *
     * @param event wxWidgets event object
     */
    void onItemDeselected(wxListEvent& event);

    /**
     * @brief Event handler that triggers when the left mouse button is pressed down (dragging)
     *
     * @param event wxWidgets event object
     */
    void onMouseLeftDown(wxMouseEvent& event);

    /**
     * @brief Event handler that triggers when the mouse is moved (dragging)
     *
     * @param event wxWidgets event object
     */
    void onMouseMotion(wxMouseEvent& event);

    /**
     * @brief Event handler that triggers when the left mouse button is released (dragging)
     *
     * @param event wxWidgets event object
     */
    void onMouseLeftUp(wxMouseEvent& event);

    /**
     * @brief Event handler that triggers when a column is clicked (changing from asc to desc order)
     *
     * @param event wxWidgets event object
     */
    void onColumnClick(wxListEvent& event);

    /**
     * @brief Event handler that triggers from timer for autoscrolling while dragging
     *
     * @param event wxWidgets event object
     */
    void onTimer(wxTimerEvent& event);

    /**
     * @brief Resets indices for the list after drag or sort
     *
     * @param event wxWidgets event object
     */
    void onClose(wxCloseEvent& event);

    /**
     * @brief Get the Header Height for positioning
     *
     * @return int Header height
     */
    auto getHeaderHeight() -> int;

    /**
     * @brief Calculates the width of a column in the list
     *
     * @param colIndex Index of column to calculate
     * @return int Width of column
     */
    auto calculateColumnWidth(int colIndex) -> int;

    /**
     * @brief Highlights the conflicting items for a selected mod
     *
     * @param selectedMod Mod that is selected
     */
    void highlightConflictingItems(const std::wstring& selectedMod);

    /**
     * @brief Clear all yellow highlights from the list
     */
    void clearAllHighlights();

    /**
     * @brief Draws a drop indicator during drag and drop
     *
     * @param targetIndex Index of the target drop item
     */
    void drawDropIndicator(int targetIndex);

    /**
     * @brief Resets indices for the list after drag or sort
     */
    void resetIndices();

    /**
     * @brief Reverses the order of the list
     */
    void reverseListOrder();
};
