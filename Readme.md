# MetalScroll

A Visual Studio 2008 add-in which replaces the editor scrollbar with a graphic representation of the code.

For more info and full Features list refer to:
[Original Project's page](https://code.google.com/archive/p/metalscroll/).

## Building

See building.txt  
Needs Visual Studio 2008 SP1 SDK [Download](https://www.microsoft.com/en-us/download/details.aspx?id=21827).

- Visual Studio 2010 SP1 SDK [Download](https://www.microsoft.com/en-us/download/details.aspx?id=21835)

## Crystal Hammer's modifications

Link to original issue/topic [here](https://code.google.com/archive/p/metalscroll/issues/9).

* Created left and right margins only for markers.
* Markers are now smaller and don't overlap.
  * On left: edited code lines (saved, unsaved) and breakpoints (red).
  * On right: bookmarks (skyblue) and find results (orange).
* Bookmarks have 4 levels.
  * When put together markers change size and color, visible on the screenshot below.
  * This is good when you have plenty of them and don't want them to look the same.
* Options dialog is opened with RMB, this way is more convenient, it doesn't change position.
* More syntax colors (numbers, strings, etc.) for scrollbar and preview code area.
  * All colors can be customized in options dialog (reworked).
  * Preview is now more like the code.
* Find results markers are drawn as an overlay, so when the scrollbar gets resized they are still the same
  * Added also options to change their sizes (separate for right margin) and bookmarks size.
  * This can be seen on the second screen with 14k lines of code.
* Added option SplitterH for masking the dirty area, seems working.
* Added font size for preview, it changes after restarting. Resigned from bold fonts which consumed one char when longer.
