#include "CreationDock/DockTabStrip.h"
#include "CreationDock/DockPanel.h"
#include "CreationDock/DockZone.h"

#include <algorithm>

namespace CreationDock {

namespace {
constexpr int kOverflowButtonWidth = 26;
}

DockTabStrip::DockTabStrip(DockZone& ownerZone)
    : zone(ownerZone)
{
    overflowButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xc2\xbb")));
    overflowButton.setVisible(false);
    overflowButton.onClick = [this] { showOverflowMenu(); };
    addChildComponent(overflowButton);
}

void DockTabStrip::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff202020));
}

void DockTabStrip::resized()
{
    auto bounds = getLocalBounds();
    hiddenTabs.clear();

    if (tabViews.empty()) {
        overflowButton.setVisible(false);
        return;
    }

    int totalWidth = 0;
    for (auto& tab : tabViews) totalWidth += tab->getPreferredWidth();

    if (totalWidth <= bounds.getWidth()) {
        overflowButton.setVisible(false);
        for (auto& tab : tabViews) {
            tab->setVisible(true);
            tab->setBounds(bounds.removeFromLeft(tab->getPreferredWidth()));
        }
        return;
    }

    // Doesn't all fit: the active tab always wins a slot first (so the
    // current selection never silently disappears off-edge), then the rest
    // fill in left-to-right order until the remaining width runs out.
    // Whatever's left becomes reachable only through the overflow menu.
    auto tabArea = bounds.removeFromLeft(juce::jmax(0, bounds.getWidth() - kOverflowButtonWidth));

    std::vector<DockTab*> fitOrder;
    DockTab* activeTab = nullptr;
    for (auto& tab : tabViews) if (tab->isActive()) activeTab = tab.get();
    if (activeTab != nullptr) fitOrder.push_back(activeTab);
    for (auto& tab : tabViews) if (tab.get() != activeTab) fitOrder.push_back(tab.get());

    std::vector<DockTab*> visible;
    int used = 0;
    for (auto* tab : fitOrder) {
        const int w = tab->getPreferredWidth();
        if (used + w <= tabArea.getWidth()) {
            visible.push_back(tab);
            used += w;
        } else {
            hiddenTabs.push_back(tab);
        }
    }

    // Lay the visible set back out in their ORIGINAL left-to-right order
    // (not fit order) so the strip doesn't visually reshuffle just because
    // the active tab happened to be mid-list.
    auto area = tabArea;
    for (auto& tab : tabViews) {
        const bool isVisible = std::find(visible.begin(), visible.end(), tab.get()) != visible.end();
        tab->setVisible(isVisible);
        if (isVisible) tab->setBounds(area.removeFromLeft(tab->getPreferredWidth()));
    }

    overflowButton.setVisible(true);
    overflowButton.setBounds(bounds);
}

void DockTabStrip::showOverflowMenu()
{
    if (hiddenTabs.empty()) return;

    juce::PopupMenu menu;
    std::vector<DockPanel*> hiddenPanels;
    for (auto* tab : hiddenTabs) {
        hiddenPanels.push_back(&tab->getPanel());
        menu.addItem(static_cast<int>(hiddenPanels.size()), tab->getPanel().getTitle());
    }

    // hiddenPanels is captured by value, not the `hiddenTabs` member -- a
    // resize while this async menu is open would otherwise leave the
    // callback reading a rebuilt (or cleared) vector.
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&overflowButton),
                        [this, hiddenPanels](int result) {
                            if (result <= 0 || result > static_cast<int>(hiddenPanels.size())) return;
                            zone.setActivePanel(hiddenPanels[static_cast<size_t>(result - 1)]);
                        });
}

DockTab* DockTabStrip::addTabFor(DockPanel& panel)
{
    auto tab = std::make_unique<DockTab>(zone, panel);
    auto* raw = tab.get();
    addAndMakeVisible(*tab);
    tabViews.push_back(std::move(tab));
    resized();
    return raw;
}

void DockTabStrip::removeTabFor(DockPanel* panel)
{
    for (auto it = tabViews.begin(); it != tabViews.end(); ++it) {
        if (&(*it)->getPanel() == panel) {
            tabViews.erase(it);
            resized();
            return;
        }
    }
}

void DockTabStrip::setActivePanel(DockPanel* panel)
{
    for (auto& tab : tabViews)
        tab->setActive(&tab->getPanel() == panel);
}

DockPanel* DockTabStrip::getPanelAt(int index) const
{
    if (index < 0 || index >= (int) tabViews.size()) return nullptr;
    return &tabViews[(size_t) index]->getPanel();
}

int DockTabStrip::indexOfPanel(DockPanel* panel) const
{
    for (int i = 0; i < (int) tabViews.size(); ++i)
        if (&tabViews[(size_t) i]->getPanel() == panel) return i;
    return -1;
}

} // namespace CreationDock
