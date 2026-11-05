# UI refinement

## 1.1: The Header: project / track / sequence information
Instead of a single line with all of the info I want to separate the header (this applies to all views)

- top line: Project info - name, opened yaml file
- 2nd row: Sequence:
    - Sequence name
    - Sequence lenght
    - total sequnces in project
- 3rd row: Track
    - track index (Like Track 1 / 3)
    - track name
    - track length

Create a new Color Pair - let's call it "faint gray", over the default background to render all of this - I want the attention to be on the actual piano roll.

## 1.2: Multi track view.
If the terminal has sufficient rows to show 1/2 octave of midi - then the piano roll view should be in Multi Track mode.

Basically we will show 2 tracks in the screen (or 3 or 4 if we have sufficient real estate - as mentioned the criteria is - can I fit half an octave per Track)

Each track window has it's own Track header (so it's detached from the header)

I can move between track window by using Ctrl + up / down arrow

When a tab loses focus (I moved to the other), I can select which Track to display in that track window using the normal 1 / 2 / 3 / 4 selector. The tab that is out of focus should be colored in "faint gray".