#include <algorithm>

#include "GUI/ModSortDialog.hpp"
#include "ParallaxGenHandlers.hpp"

using namespace std;

// Disable owning memory checks because wxWidgets will take care of deleting the objects
// Disable convert member functions to static because these functions need to be non-static for wxWidgets
// NOLINTBEGIN(cppcoreguidelines-owning-memory,readability-convert-member-functions-to-static)

// class ModSortDialog
ModSortDialog::ModSortDialog(const std::vector<std::wstring>& mods, const std::vector<std::wstring>& shaders,
    const std::vector<bool>& isNew, const std::unordered_map<std::wstring, std::unordered_set<std::wstring>>& conflicts)
    : wxDialog(nullptr, wxID_ANY, "Set Mod Priority", wxDefaultPosition, wxSize(DEFAULT_WIDTH, DEFAULT_HEIGHT),
          wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP | wxRESIZE_BORDER)
    , m_scrollTimer(this)
    , m_conflictsMap(conflicts)
    , m_sortAscending(true)
{
    Bind(wxEVT_TIMER, &ModSortDialog::onTimer, this, m_scrollTimer.GetId());

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);
    // Create the m_listCtrl
    m_listCtrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(DEFAULT_WIDTH, DEFAULT_HEIGHT), wxLC_REPORT);
    m_listCtrl->InsertColumn(0, "Mod");
    m_listCtrl->InsertColumn(1, "Shader");
    m_listCtrl->InsertColumn(2, "Priority");

    m_listCtrl->Bind(wxEVT_LIST_ITEM_SELECTED, &ModSortDialog::onItemSelected, this);
    m_listCtrl->Bind(wxEVT_LIST_ITEM_DESELECTED, &ModSortDialog::onItemDeselected, this);
    m_listCtrl->Bind(wxEVT_LIST_COL_CLICK, &ModSortDialog::onColumnClick, this);

    m_listCtrl->Bind(wxEVT_LEFT_DOWN, &ModSortDialog::onMouseLeftDown, this);
    m_listCtrl->Bind(wxEVT_MOTION, &ModSortDialog::onMouseMotion, this);
    m_listCtrl->Bind(wxEVT_LEFT_UP, &ModSortDialog::onMouseLeftUp, this);

    // Add items to m_listCtrl and highlight specific ones
    for (size_t i = 0; i < mods.size(); ++i) {
        const long index = m_listCtrl->InsertItem(static_cast<long>(i), mods[i]);
        m_listCtrl->SetItem(index, 1, shaders[i]);
        m_listCtrl->SetItem(index, 2, std::to_string(i));
        if (isNew[i]) {
            m_listCtrl->SetItemBackgroundColour(index, *wxGREEN); // Highlight color
            m_originalBackgroundColors[mods[i]] = *wxGREEN; // Store the original color using the mod name
        } else {
            m_originalBackgroundColors[mods[i]] = *wxWHITE; // Store the original color using the mod name
        }
    }

    // Calculate minimum width for each column
    const int col1Width = calculateColumnWidth(0);
    m_listCtrl->SetColumnWidth(0, col1Width);
    const int col2Width = calculateColumnWidth(1);
    m_listCtrl->SetColumnWidth(1, col2Width);
    const int col3Width = calculateColumnWidth(2);
    m_listCtrl->SetColumnWidth(2, col3Width);

    const int scrollBarWidth = wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);
    const int totalWidth = col1Width + col2Width + col3Width + (DEFAULT_PADDING * 2) + scrollBarWidth; // Extra padding

    // Add wrapped message at the top
    static const std::wstring message
        = L"The following mods have been detected as potential conflicts. Please set the "
          L"priority order for these mods. Mods that are highlighted in green are new mods "
          L"that do not exist in the saved order and may need to be sorted. Mods "
          L"highlighted in yellow conflict with the currently selected mod. Higher "
          L"priority mods win over lower priority mods, where priority refers to the 3rd "
          L"column. (For example priority 10 would win over priority 1)";
    // Create the wxStaticText and set wrapping
    auto* messageText = new wxStaticText(this, wxID_ANY, message, wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    messageText->Wrap(totalWidth - (DEFAULT_PADDING * 2)); // Adjust wrapping width

    // Let wxWidgets automatically calculate the best height based on wrapped text
    messageText->SetMinSize(wxSize(totalWidth - (DEFAULT_PADDING * 2), messageText->GetBestSize().y));

    // Add the static text to the main sizer
    mainSizer->Add(messageText, 0, wxALL, DEFAULT_BORDER);

    // Adjust dialog width to match the total width of columns and padding
    SetSizeHints(totalWidth, DEFAULT_HEIGHT, totalWidth, wxDefaultCoord); // Adjust minimum width and height
    SetSize(totalWidth, DEFAULT_HEIGHT); // Set dialog size

    mainSizer->Add(m_listCtrl, 1, wxEXPAND | wxALL, DEFAULT_BORDER);

    // Add OK button
    auto* okButton = new wxButton(this, wxID_OK, "OK");
    mainSizer->Add(okButton, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, DEFAULT_BORDER);
    Bind(wxEVT_CLOSE_WINDOW, &ModSortDialog::onClose, this);

    SetSizer(mainSizer);

    m_listCtrlHeaderHeight = getHeaderHeight();
}

// EVENT HANDLERS

void ModSortDialog::onItemSelected(wxListEvent& event)
{
    const long index = event.GetIndex();
    const std::wstring selectedMod = m_listCtrl->GetItemText(index).ToStdWstring();

    if (index == -1) {
        clearAllHighlights(); // Clear all highlights when no item is selected
    } else {
        highlightConflictingItems(selectedMod); // Highlight conflicts for the selected mod
    }
}

void ModSortDialog::onItemDeselected(wxListEvent& event)
{
    // Check if no items are selected
    long selectedItem = -1;
    bool isAnyItemSelected = false;
    while (
        (selectedItem = m_listCtrl->GetNextItem(selectedItem, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND) {
        isAnyItemSelected = true;
        break;
    }

    if (!isAnyItemSelected) {
        clearAllHighlights(); // Clear highlights if no items are selected
    }

    event.Skip();
}

void ModSortDialog::clearAllHighlights()
{
    for (long i = 0; i < m_listCtrl->GetItemCount(); ++i) {
        const std::wstring itemText = m_listCtrl->GetItemText(i).ToStdWstring();
        auto it = m_originalBackgroundColors.find(itemText);
        if (it != m_originalBackgroundColors.end()) {
            m_listCtrl->SetItemBackgroundColour(i, it->second); // Restore original color
        } else {
            m_listCtrl->SetItemBackgroundColour(i, *wxWHITE); // Fallback to white
        }
    }
}

void ModSortDialog::highlightConflictingItems(const std::wstring& selectedMod)
{
    // Clear previous highlights and restore original colors
    for (long i = 0; i < m_listCtrl->GetItemCount(); ++i) {
        const std::wstring itemText = m_listCtrl->GetItemText(i).ToStdWstring();
        auto it = m_originalBackgroundColors.find(itemText);
        if (it != m_originalBackgroundColors.end()) {
            m_listCtrl->SetItemBackgroundColour(i, it->second); // Restore original color
        } else {
            m_listCtrl->SetItemBackgroundColour(i, *wxWHITE); // Fallback to white if not found
        }
    }

    // Highlight selected item and its conflicts
    auto conflictSet = m_conflictsMap.find(selectedMod);
    if (conflictSet != m_conflictsMap.end()) {
        for (long i = 0; i < m_listCtrl->GetItemCount(); ++i) {
            const std::wstring itemText = m_listCtrl->GetItemText(i).ToStdWstring();
            if (itemText == selectedMod || conflictSet->second.contains(itemText)) {
                m_listCtrl->SetItemBackgroundColour(i, *wxYELLOW); // Highlight color
            }
        }
    }
}

void ModSortDialog::onMouseLeftDown(wxMouseEvent& event)
{
    int flags = 0;
    const long itemIndex = m_listCtrl->HitTest(event.GetPosition(), flags);

    if (itemIndex != wxNOT_FOUND) {
        // Select the item if it's not already selected
        if ((m_listCtrl->GetItemState(itemIndex, wxLIST_STATE_SELECTED) & wxLIST_STATE_SELECTED) == 0) {
            m_listCtrl->SetItemState(itemIndex, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        }

        // Capture the indices of all selected items for dragging
        m_draggedIndices.clear();
        long selectedItem = -1;
        while ((selectedItem = m_listCtrl->GetNextItem(selectedItem, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED))
            != wxNOT_FOUND) {
            m_draggedIndices.push_back(selectedItem);
        }
    }
    event.Skip();
}

void ModSortDialog::onMouseLeftUp(wxMouseEvent& event)
{
    if (!m_draggedIndices.empty() && m_targetLineIndex != -1) {
        m_overlay.Reset(); // Clear the m_overlay when the drag operation is complete

        // Sort indices to maintain the order during removal
        std::ranges::sort(m_draggedIndices);

        // Capture item data for all selected items
        std::vector<std::pair<wxString, wxString>> itemData;
        std::vector<wxColour> backgroundColors;

        for (auto index : m_draggedIndices) {
            itemData.emplace_back(m_listCtrl->GetItemText(index, 0), m_listCtrl->GetItemText(index, 1));
            backgroundColors.push_back(m_listCtrl->GetItemBackgroundColour(index));
        }

        // Remove items from their original positions
        for (int i = static_cast<int>(m_draggedIndices.size()) - 1; i >= 0; --i) {
            m_listCtrl->DeleteItem(m_draggedIndices[i]);
            // Adjust m_targetLineIndex if items were removed from above it
            if (m_draggedIndices[i] < m_targetLineIndex) {
                m_targetLineIndex--;
            }
        }

        // Insert items at the new position
        long insertPos = m_targetLineIndex;
        for (size_t i = 0; i < itemData.size(); ++i) {
            const long newIndex = m_listCtrl->InsertItem(insertPos, itemData[i].first);
            m_listCtrl->SetItem(newIndex, 1, itemData[i].second);
            m_listCtrl->SetItemBackgroundColour(newIndex, backgroundColors[i]);
            insertPos++;
        }

        // reset priority values
        resetIndices();

        // Reset indices to prepare for the next drag
        m_draggedIndices.clear();
        m_targetLineIndex = -1;
    }

    // Stop the timer when the drag operation ends
    if (m_scrollTimer.IsRunning()) {
        m_scrollTimer.Stop();
    }

    event.Skip();
}

void ModSortDialog::onMouseMotion(wxMouseEvent& event)
{
    if (!m_draggedIndices.empty() && event.LeftIsDown()) {
        // Start the timer to handle scrolling
        if (!m_scrollTimer.IsRunning()) {
            m_scrollTimer.Start(TIMER_INTERVAL); // Start the timer with a 50ms interval
        }

        int flags = 0;
        auto dropTargetIndex = m_listCtrl->HitTest(event.GetPosition(), flags);

        if (dropTargetIndex != wxNOT_FOUND) {
            wxRect itemRect;
            m_listCtrl->GetItemRect(dropTargetIndex, itemRect);

            // Check if the mouse is in the top or bottom half of the item
            const int midPointY = itemRect.GetTop() + (itemRect.GetHeight() / 2);
            const auto curPosition = event.GetPosition().y;
            const bool targetingTopHalf = curPosition > midPointY;

            if (targetingTopHalf) {
                dropTargetIndex++;
            }

            // Draw the line only if the target index has changed
            drawDropIndicator(dropTargetIndex);
            m_targetLineIndex = dropTargetIndex;
        } else {
            // Clear m_overlay if not hovering over a valid item
            m_overlay.Reset();
            m_targetLineIndex = -1;
        }
    }

    event.Skip();
}

void ModSortDialog::onTimer([[maybe_unused]] wxTimerEvent& event)
{
    // Get the current mouse position relative to the m_listCtrl
    const wxPoint mousePos = ScreenToClient(wxGetMousePosition());
    const wxRect listCtrlRect = m_listCtrl->GetRect();

    // Check if the mouse is within the m_listCtrl bounds
    if (listCtrlRect.Contains(mousePos)) {
        const int scrollMargin = 20; // Margin to trigger scrolling
        const int mouseY = mousePos.y;

        if (mouseY < listCtrlRect.GetTop() + scrollMargin + m_listCtrlHeaderHeight) {
            // Scroll up if the mouse is near the top edge
            m_listCtrl->ScrollLines(-1);
        } else if (mouseY > listCtrlRect.GetBottom() - scrollMargin) {
            // Scroll down if the mouse is near the bottom edge
            m_listCtrl->ScrollLines(1);
        }
    }
}

// HELPERS

auto ModSortDialog::getHeaderHeight() -> int
{
    if (m_listCtrl->GetItemCount() > 0) {
        wxRect firstItemRect;
        if (m_listCtrl->GetItemRect(0, firstItemRect)) {
            // The top of the first item minus the top of the client area gives the header height
            return firstItemRect.GetTop() - m_listCtrl->GetClientRect().GetTop();
        }
    }
    return 0; // Fallback if the list is empty
}

void ModSortDialog::drawDropIndicator(int targetIndex)
{
    wxClientDC dc(m_listCtrl);
    wxDCOverlay dcOverlay(m_overlay, &dc);
    dcOverlay.Clear(); // Clear the existing m_overlay to avoid double lines

    wxRect itemRect;
    int lineY = -1;

    // Validate TargetIndex before calling GetItemRect()
    if (targetIndex >= 0 && targetIndex < m_listCtrl->GetItemCount()) {
        if (m_listCtrl->GetItemRect(targetIndex, itemRect)) {
            lineY = itemRect.GetTop();
        }
    } else if (targetIndex >= m_listCtrl->GetItemCount()) {
        // Handle drawing at the end of the list (after the last item)
        if (m_listCtrl->GetItemRect(m_listCtrl->GetItemCount() - 1, itemRect)) {
            lineY = itemRect.GetBottom();
        }
    }

    // Ensure LineY is set correctly before drawing
    if (lineY != -1) {
        dc.SetPen(wxPen(*wxBLACK, 2)); // Draw the line with a width of 2 pixels
        dc.DrawLine(itemRect.GetLeft(), lineY, itemRect.GetRight(), lineY);
    }
}

void ModSortDialog::resetIndices()
{
    // loop through each item in list and set col 3
    for (long i = 0; i < m_listCtrl->GetItemCount(); ++i) {
        if (m_sortAscending) {
            m_listCtrl->SetItem(i, 2, std::to_string(i));
        } else {
            m_listCtrl->SetItem(i, 2, std::to_string(m_listCtrl->GetItemCount() - i - 1));
        }
    }
}

void ModSortDialog::onColumnClick(wxListEvent& event)
{
    const int column = event.GetColumn();

    // Toggle sort order if the same column is clicked, otherwise reset to ascending
    if (column == 2) {
        // Only sort priority col
        m_sortAscending = !m_sortAscending;

        // Reverse the order of every item
        reverseListOrder();
    }

    event.Skip();
}

void ModSortDialog::reverseListOrder()
{
    // Store all items in a vector
    std::vector<std::vector<wxString>> items;
    std::vector<wxColour> backgroundColors;

    for (long i = 0; i < m_listCtrl->GetItemCount(); ++i) {
        std::vector<wxString> row;
        row.reserve(m_listCtrl->GetColumnCount());
        for (int col = 0; col < m_listCtrl->GetColumnCount(); ++col) {
            row.push_back(m_listCtrl->GetItemText(i, col));
        }
        items.push_back(row);

        // Store original background color using the mod name
        const std::wstring itemText = row[0].ToStdWstring();
        auto it = m_originalBackgroundColors.find(itemText);
        if (it != m_originalBackgroundColors.end()) {
            backgroundColors.push_back(it->second);
        } else {
            backgroundColors.push_back(*wxWHITE); // Default color if not found
        }
    }

    // Clear the m_listCtrl
    m_listCtrl->DeleteAllItems();

    // Insert items back in reverse order and set background colors
    for (size_t i = 0; i < items.size(); ++i) {
        const long newIndex = m_listCtrl->InsertItem(m_listCtrl->GetItemCount(), items[items.size() - 1 - i][0]);
        for (int col = 1; col < m_listCtrl->GetColumnCount(); ++col) {
            m_listCtrl->SetItem(newIndex, col, items[items.size() - 1 - i][col]);
        }

        // Set background color for the new item
        m_listCtrl->SetItemBackgroundColour(newIndex, backgroundColors[items.size() - 1 - i]);
    }
}

auto ModSortDialog::calculateColumnWidth(int colIndex) -> int
{
    int maxWidth = 0;
    wxClientDC dc(m_listCtrl);
    dc.SetFont(m_listCtrl->GetFont());

    for (int i = 0; i < m_listCtrl->GetItemCount(); ++i) {
        const wxString itemText = m_listCtrl->GetItemText(i, colIndex);
        int width = 0;
        int height = 0;
        dc.GetTextExtent(itemText, &width, &height);
        maxWidth = std::max(width, maxWidth);
    }
    return maxWidth + DEFAULT_PADDING; // Add some padding
}

auto ModSortDialog::getSortedItems() const -> std::vector<std::wstring>
{
    std::vector<std::wstring> sortedItems;
    sortedItems.reserve(m_listCtrl->GetItemCount());
    for (long i = 0; i < m_listCtrl->GetItemCount(); ++i) {
        sortedItems.push_back(m_listCtrl->GetItemText(i).ToStdWstring());
    }

    if (!m_sortAscending) {
        // reverse items if descending
        std::ranges::reverse(sortedItems);
    }

    return sortedItems;
}

void ModSortDialog::onClose([[maybe_unused]] wxCloseEvent& event)
{
    ParallaxGenHandlers::nonBlockingExit();
    wxTheApp->Exit();
}

// NOLINTEND(cppcoreguidelines-owning-memory,readability-convert-member-functions-to-static)
