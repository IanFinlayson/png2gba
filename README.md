# png2gba

A utility to convert PNG images into C arrays as required for GBA programming.
It supports RGB and RGBA PNG images (though it ignores the alpha channel if
present), which are the most common PNG file types.  The utility supports 16-bit
raw images (the default) or 8-bit palletized images (with the -p option).  It
also supports "tileized" images with the -t option.  This writes the image tile
by tile as when using a tile mode.

Requires a C compiler and dependencies: libpng, argp.

# To compile on Ubuntu Linux:
1. Install libpng: `sudo apt install clang libpng-dev exuberant-ctags`
2. `make`

argp doesn't need to be installed on Linux.

# To compile on OSX El Capitan:
1. Make sure you've got XCode and homebrew installed
2. Install argp: `brew install argp-standalone`
3. Install libpng: `brew install libpng`
4. Export the installed include and lib directories:
    *  `export CFLAGS="$CFLAGS -I/usr/local/Cellar/argp-standalone/1.3/include/ -I/usr/local/Cellar/libpng/1.6.35/include/"`
    * `export LDFLAGS="$LDFLAGS -L/usr/local/Cellar/argp-standalone/1.3/lib/ -largp -L/usr/local/Cellar/libpng/1.6.35/lib/ -lpng"`
4. `make`

Don't forget to adjust revision numbers.
