#include "CreationDock/DockSplitter.h"

namespace CreationDock {

DockSplitter::DockSplitter(SplitterOrientation orientationIn)
    : orientation(orientationIn)
{
    setMouseCursor(orientation == SplitterOrientation::Vertical
        ? juce::MouseCursor::LeftRightResizeCursor
        : juce::MouseCursor::UpDownResizeCursor);
}

void DockSplitter::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2d2d2d));
    g.setColour(isMouseOverOrDragging() ? juce::Colours::cyan : juce::Colour(0xff3f3f3f));

    g.fillRect(orientation == SplitterOrientation::Vertical
        ? getLocalBounds().reduced(2, 0)
        : getLocalBounds().reduced(0, 2));
}

void DockSplitter::mouseEnter(const juce::MouseEvent&) { repaint(); }
void DockSplitter::mouseExit(const juce::MouseEvent&)  { repaint(); }

void DockSplitter::mouseDown(const juce::MouseEvent&)
{
    repaint();
    if (onDragStart) onDragStart();
}

void DockSplitter::mouseUp(const juce::MouseEvent& e)
{
    repaint();
    auto source = e.source; // the event is const; a copy is the same mouse
    source.forceMouseCursorUpdate(); // the drag let go of the cursor: back to whatever is under the mouse
}

void DockSplitter::mouseDrag(const juce::MouseEvent& e)
{
    // The resize cursor stays for the whole drag, even when the mouse runs ahead of the thin splitter.
    auto source = e.source;
    source.showMouseCursor(getMouseCursor());

    auto delta = orientation == SplitterOrientation::Vertical
        ? e.getDistanceFromDragStartX()
        : e.getDistanceFromDragStartY();

    if (onDragDelta) onDragDelta(delta);
}

} // namespace CreationDock
