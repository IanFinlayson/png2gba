# png2gba

A utility to convert PNG images into C arrays as required for GBA programming.
It supports RGB and RGBA PNG images (though it ignores the alpha channel if
present), which are the most common PNG file types.  The utility supports 16-bit
raw images (the default) or 8-bit palletized images (with the -p option).  It
also supports "tileized" images with the -t option.  This writes the image tile
by tile as when using a tile mode.

Requires a C compiler and the libpng development library.

