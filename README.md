# SlicerSequenceLightbox

Extension for [3D Slicer](https://www.slicer.org) that displays several items of a sequence at
once. The lightbox view shows a grid of sequence items side by side, the timeline view shows a
strip of the items around the current item. The tiles follow a slice view or a 3D view, and they
can be interacted with like regular views. Nothing is added to the scene: no extra volumes,
models, display nodes or view nodes have to be created.

The extension requires the Sequences module of Slicer and a Slicer build that positions 2D
markups and interaction handles relative to the viewport of the view (Slicer 5.9 or later).

## Usage

- Load or create a sequence (for example a volume sequence from DICOM or a recording) and make
  sure that it has a sequence browser node.
- Open the "Sequence Lightbox" module (in the "Sequences" category), choose the sequence
  browser, choose a layout and click "Show". The same layouts are available in the layout menu
  of the toolbar.

The following layouts are available, each for a slice view and for a 3D view:

- Sequence lightbox: the view and a grid of sequence items next to it. The view that is
  displayed is the reference view of the lightbox (the view that the tiles follow): choose
  another slice view or a 3D view in the module panel or in the controller bar of the lightbox,
  and the layout shows that view instead.
- Sequence timeline: the view on top and a strip of sequence items along the bottom. The strip
  is centered on the current item, so the previous and the next items are always visible, and
  the current item stays in the middle during playback.
- Sequence lightbox only: just the grid of sequence items, without the view that it follows.
  The tiles can be interacted with like regular views (see below), so the view itself is not
  needed.

Each tile of the grid shows the same content as the reference view (a slice view or a 3D view),
except that it displays a different sequence item. Navigating the reference view - changing the
slice position, zoom, window/level, or moving the 3D camera - updates all tiles, and any node
that is visible in the reference view (volumes, models, segmentations, markups, volume
rendering, ...) is shown in the tiles as well.

The tiles can be interacted with like regular views: the slice can be scrolled (mouse wheel),
panned (middle button) and zoomed (right button), the 3D camera can be rotated, and markups can
be placed and edited in any tile. Changes of the view are applied to the reference view, so all
tiles follow. Changes of a sequence item (for example a markup placed in a tile that shows the
sequence's markups) are stored in that item of the sequence, whether or not "Save changes" is
enabled for the sequence; if the tile shows the current item then the proxy node is updated as
well, so the regular views show the change immediately. If the sequence has no item for the
index value of the tile (for example a markups sequence that was only recorded for some of the
time points), the tile shows the previous item in the default "Previous item" missing item mode
of the sequence (the *Missing item* column of the synchronized nodes table in the Sequences
module), and nothing in the other modes; editing such a tile adds the missing item to the
sequence instead of modifying the previous one. Choose the "Empty" missing item mode to place
markups independently at each time point. Right-clicking a markup in a tile shows its context
menu (after making the item of the tile the current item, so that the actions of the menu apply
to that item). Interaction can be disabled with the "Interactive tiles" setting in the
controller bar of the lightbox; clicking a tile then only selects its item.

The grid is configured in the controller bar of the lightbox view:

- Rows, columns: size of the grid. "Show all items" chooses them so that every item of the
  sequence is displayed, with tiles as large as the view allows.
- Tile spacing: width of the gap between the tiles.
- Displayed items: which items are shown in the tiles.
  - "Consecutive items from the selected item" starts the grid at the item that is currently
    selected in the browser, so the grid scrolls during playback.
  - "Consecutive items around the selected item" shows the selected item in the center tile,
    with the previous items before it and the next items after it. This is the mode of the
    timeline layouts.
  - "Consecutive items from a fixed item" always starts the grid at a chosen item. The grid can
    be scrolled by one row using Shift + mouse wheel over the lightbox (or the mouse wheel alone
    if the tiles are not interactive).
  - "Custom list of items" displays exactly the items that are listed, in the given order. Enter
    them as a comma separated list of item numbers, using ranges where convenient, for example
    `0, 3, 7-9`. Item numbers start at 0. Use this to compare arbitrary time points.
- Item step: difference between the item numbers of consecutive tiles (for example, a step of 2
  displays every second item). It is not used by the custom list of items.
- Show item labels: display the index value of the item in each tile.
- Highlight selected item: draw a border around the tile that shows the currently selected item.
- Interactive tiles: allow interacting with the tiles as described above.

Clicking a tile (without dragging, and without placing a markup) makes the item of that tile the
current item of the browser node, which updates the proxy nodes and therefore all the regular
views as well. Shift + mouse wheel over the lightbox steps through the items of the browser node
(or scrolls the grid, if a fixed first item is displayed). The tile under the mouse pointer is
marked by a thin frame. The keyboard can be used while the mouse pointer is over the lightbox:
Shift + Left/Right arrow keys select the previous/next item, Shift + Page Up/Page Down move by
one row of tiles, Shift + Home/End select the first/last item (if a fixed first item is
displayed then Shift + Page Up/Page Down and Shift + Home/End scroll the grid instead). Without
Shift, the keys act on the tile under the pointer as in a regular view (for example the arrow
keys scroll the slice of a slice tile).

## Design

- `vtkMRMLSequenceLightboxViewNode` (MRML): view node of the lightbox. It references the
  sequence browser node and the reference view, and stores the grid settings. The layout
  manager creates one lightbox view node per lightbox layout (`SequenceLightbox1`,
  `SequenceLightbox3D`, `SequenceTimeline1`, `SequenceTimeline3D`).
- `vtkSequenceLightboxSceneMirror` (Logic): private scene of a tile. It copies the nodes that
  are displayed in the reference view into a scene of its own (with the same node IDs) and
  replaces the proxy nodes of the browser by the data nodes of the item of the tile. Changes
  made in the tile are written back to the sequence item.
- `vtkMRMLSequenceLightboxDisplayableManager` (MRMLDM): displayable manager of the lightbox
  view. It creates a renderer and a mirror scene for each tile, displays the mirror scene with
  the standard slice or 3D displayable managers, keeps the tiles synchronized with the scene
  and forwards the interaction events to the tile under the pointer.
- `qMRMLSequenceLightboxView`, `qMRMLSequenceLightboxWidget`,
  `qMRMLSequenceLightboxViewControllerWidget` and `qMRMLLayoutSequenceLightboxViewFactory`
  (Widgets): the view widget, its controller bar and the factory that creates the views for the
  layout manager.
- `qSlicerSequenceLightboxModule`: registers the view factory, the displayable manager and the
  layouts (layout IDs `vtkMRMLLayoutNode::SlicerLayoutUserView + 601` to `+ 606`), adds the
  layouts to the layout menu and keeps the lightbox view nodes configured when nodes are added
  or a scene is loaded or closed.

## Building

Build the extension against a Slicer build tree, as any other extension:

```
cmake -DSlicer_DIR=/path/to/Slicer-build /path/to/SlicerSequenceLightbox
cmake --build . --config RelWithDebInfo
```

The tests (two C++ tests and the `SequenceLightboxSelfTest` Python test, which can also be run
from the application) are run with `ctest`.

## License

This extension is distributed under the [MIT License](LICENSE.txt).
