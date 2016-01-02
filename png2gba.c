/* png2gba.c
 * this program converts PNG images into C header files storing
 * arrays of data as required for programming the GBA */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <argp.h>

#define PNG_DEBUG 3
#include <png.h>

/* the GBA palette size is always 256 colors */
#define PALETTE_SIZE 256

/* the GBA always uses 8x8 tiles */
#define TILE_SIZE 8

/* the max 8-bit or 16-bit values on a row
 * this only affects aesthetics by keeping the files from exceeding a width
 * of 80 characters */
#define MAX_ROW8 12
#define MAX_ROW16 9

/* the info for the command line parameters */
const char* argp_program_version = "png2gba 1.0";
const char* argp_program_bug_address = "<finlayson@umw.edu>";
const char doc [] = "PNG to GBA image conversion utility";
const char args_doc[] = "FILE";

/* the command line options for the compiler */
const struct argp_option options[] = {
    {"output", 'o', "file", 0, "Specify output file", 0}, 
    {"colorkey", 'c', "color", 0, "Specify the transparent color (#rrggbb)", 0}, 
    {"palette", 'p', NULL, 0, "Use a palette in the produced image", 0},
    {"tileize", 't', NULL, 0, "Output the image as consecutive 8x8 tiles", 0},
    {NULL, 0, NULL, 0, NULL, 0}
};

/* used by main to communicate with parse_opt */
struct arguments {
    int palette;
    int tileize;
    char* colorkey;
    char* output_file_name;
    char* input_file_name;
};

/* the function which parses command line options */
error_t parse_opt (int key, char* arg, struct argp_state* state) {

    /* get the input argument from argp_parse */
    struct arguments *arguments = state->input;

    /* switch on the command line option that was passed in */
    switch (key) {
        case 'p':
            /* set the palette option */
            arguments->palette = 1;
            break;

        case 't':
            /* set the tileize option */
            arguments->tileize = 1;

        case 'o':
            /* the output file name is set */
            arguments->output_file_name = arg;
            break;

        case 'c':
            /* the colorkey is set */
            arguments->colorkey = arg;
            break;

            /* we got a file name */
        case ARGP_KEY_ARG:
            if (state->arg_num > 1) {
                /* too many arguments */
                fprintf(stderr, "Error: Only one file name can be given!\n");
                argp_usage(state);
            }

            /* save it as an argument */
            arguments->input_file_name = arg;
            break;

            /* we hit the end of the arguments */
        case ARGP_KEY_END:
            if (state->arg_num < 1) {
                /* not enough arguments */
                fprintf(stderr, "Error: Must pass an input file name!\n");
                argp_usage(state);
            }
            break;

            /* some other kind of thing happended */
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/* the parameters to the argp library containing our program details */
struct argp info = {options, parse_opt, args_doc, doc, NULL, NULL, NULL};

/* a PNG image we load */
struct Image {
    int w, h, channels;
    png_byte color_type;
    png_byte bit_depth;
    png_bytep* rows;
};

/* load the png image from a file */
struct Image* read_png(FILE* in) {
    /* read the PNG signature */
    unsigned char header[8];
    fread(header, 1, 8, in);
    if (png_sig_cmp(header, 0, 8)) {
        fprintf(stderr, "Error: This does not seem to be a valid PNG file!\n");
        exit(-1);
    }

    /* setup structs for reading */
    png_structp png_reader = png_create_read_struct(PNG_LIBPNG_VER_STRING,
            NULL, NULL, NULL);
    if (!png_reader) {
        fprintf(stderr, "Error: Could not read PNG file!\n");
        exit(-1);
    }
    png_infop png_info = png_create_info_struct(png_reader);
    if (!png_info) {
        fprintf(stderr, "Error: Could not read PNG file!\n");
        exit(-1);
    }
    if (setjmp(png_jmpbuf(png_reader))) {
        fprintf(stderr, "Error: Could not read PNG file!\n");
        exit(-1);
    }

    /* allocate an image */
    struct Image* image = malloc(sizeof(struct Image));

    /* read in the header information */
    png_init_io(png_reader, in);
    png_set_sig_bytes(png_reader, 8);
    png_read_info(png_reader, png_info);
    image->w = png_get_image_width(png_reader, png_info);
    image->h = png_get_image_height(png_reader, png_info);
    image->color_type = png_get_color_type(png_reader, png_info);
    image->bit_depth = png_get_bit_depth(png_reader, png_info);
    png_set_interlace_handling(png_reader);
    png_read_update_info(png_reader, png_info);

    /* read the actual file */
    if (setjmp(png_jmpbuf(png_reader))) {
        fprintf(stderr, "Error: Could not read PNG file!\n");
        exit(-1);
    }
    image->rows = (png_bytep*) malloc(sizeof(png_bytep) * image->h);
    int r;
    for (r = 0; r < image->h; r++) {
        image->rows[r] = malloc(png_get_rowbytes(png_reader,png_info));
    }
    png_read_image(png_reader, image->rows);

    /* check format */
    if (png_get_color_type(png_reader, png_info) == PNG_COLOR_TYPE_RGB) {
        image->channels = 3;
    } else if (png_get_color_type(png_reader,png_info)==PNG_COLOR_TYPE_RGBA) {
        fprintf(stderr, "Warning: PNG alpha channel is ignored!\n");
        image->channels = 4;
    } else {
        fprintf(stderr, "Error: PNG file is not in the RGB or RGBA format!\n");
        exit(-1);
    }

    /* close the input file and return */
    fclose(in);
    return image;
}

/* inserts a color into a palette and returns the index, or return
 * the existing index if the color is already there */
unsigned char insert_palette(unsigned short color,
        unsigned short* color_palette, unsigned char* palette_size) {

    /* loop through the palette */
    unsigned char i;
    for (i = 0; i < *palette_size; i++) {
        /* if this is it, return it */
        if (color_palette[i] == color) {
            return i;
        }
    }

    /* if the palette is full, we're in trouble */
    if (*palette_size == (PALETTE_SIZE - 1)) {
        fprintf(stderr, "Error: Too many colors in image for a palette!\n");
        exit(-1);
    }

    /* it was not found, so add it */
    color_palette[*palette_size] = color;

    /* increment palette size */
    (*palette_size)++;

    /* return the index */
    return (*palette_size) - 1;
}

/* returns the next pixel from the image, based on whether we
 * are tile-izing or not, returns NULL when we have done them all */
png_byte* next_byte(struct Image* image, int tileize) {
    /* keeps track of where we are in the "global" image */
    static int r = 0;
    static int c = 0;

    /* keeps track of where we are relative to one tile (0-7) */
    static int tr = 0;
    static int tc = 0;

    /* if we have gone through it all */
    if (r == image->h) {
        return NULL;
    }

    /* get the pixel next */
    png_byte* row = image->rows[r];
    png_byte* ptr = &(row[c * image->channels]);

    /* increment things based on if we are tileizing or not */
    if (!tileize) {
        /* just go sequentially, wrapping to the next row at the end of a column */
        c++;
        if (c >= image->w) {
            r++;
            c = 0;
        }
    } else {
        /* increment the column */
        c++;
        tc++;

        /* if we hit the end of a tile row */
        if (tc >= 8) {
            /* go to the next one */
            r++;
            tr++;
            c -= 8;
            tc = 0;

            /* if we hit the end of the tile altogether */
            if (tr >= 8) {
                r -= 8;
                tr = 0;
                c += 8;
            }

            /* if we are now at the end of the actual row, go to next one */
            if (c >= image->w) {
                tc = 0;
                tr = 0;
                c = 0;
                r += 8;
            }
        }
    }

    /* and return the pixel we found previously */
    return ptr;
}

unsigned short hex24_to_15(char* hex24) {
    /* skip the # sign */
    hex24++;

    /* break off the pieces */
    char rs[3], gs[3], bs[3];
    rs[0] = *hex24++;
    rs[1] = *hex24++;
    rs[2] = '\0';
    gs[0] = *hex24++;
    gs[1] = *hex24++;
    gs[2] = '\0';
    bs[0] = *hex24++;
    bs[1] = *hex24++;
    bs[2] = '\0';

    /* convert from hex string to int */
    int r = strtol(rs, NULL, 16);
    int g = strtol(gs, NULL, 16);
    int b = strtol(bs, NULL, 16);

    /* build the full 15 bit short */
    unsigned short color = (b >> 3) << 10;
    color += (g >> 3) << 5;
    color += (r >> 3);

    return color;
}

/* perform the actual conversion from png to gba formats */
void png2gba(FILE* in, FILE* out, char* name, int palette,
        int tileize, char* colorkey) {

    /* load the image */
    struct Image* image = read_png(in);

    /* write preamble stuff */
    fprintf(out, "/* %s.h\n * generated by png2gba program */\n\n", name);
    fprintf(out, "#define %s_width %d\n", name, image->w);
    fprintf(out, "#define %s_height %d\n\n", name, image->h);
    if (palette) {
        fprintf(out, "const unsigned char %s_data [] = {\n", name);
    } else {
        fprintf(out, "const unsigned short %s_data [] = {\n", name); 
    }

    /* the palette stores up to PALETTE_SIZE colors */
    unsigned short color_palette[PALETTE_SIZE];

    /* palette sub 0 is reserved for transparent color */
    unsigned char palette_size = 1;

    /* clear the palette */
    memset(color_palette, 0, PALETTE_SIZE * sizeof(unsigned short));

    /* insert the transparent color */
    unsigned short ckey = hex24_to_15(colorkey);
    color_palette[0] = ckey; 

    /* loop through the pixel data */
    unsigned char red, green, blue;
    int colors_this_line = 0;
    png_byte* ptr;

    while ((ptr = next_byte(image, tileize))) {
        red = ptr[0];
        green = ptr[1];
        blue = ptr[2];

        /* convert to 16-bit color */
        unsigned short color = (blue >> 3) << 10;
        color += (green >> 3) << 5;
        color += (red >> 3);

        /* print leading space if first of line */
        if (colors_this_line == 0) {
            fprintf(out, "    ");
        }

        /* print color directly, or palette index */
        if (!palette) { 
            fprintf(out, "0x%04X", color);
        } else {
            unsigned char index = insert_palette(color, color_palette,
                    &palette_size);
            fprintf(out, "0x%02X", index);
        }

        fprintf(out, ", ");

        /* increment colors on line unless too many */
        colors_this_line++;
        if ((palette && colors_this_line >= MAX_ROW8) ||
                (!palette && colors_this_line >= MAX_ROW16)) {
            fprintf(out, "\n");
            colors_this_line = 0;
        } 
    }

    /* write postamble stuff */
    fprintf(out, "\n};\n\n");

    /* write the palette if needed */
    if (palette) {
        int colors_this_line = 0;
        fprintf(out, "const unsigned short %s_palette [] = {\n", name); 
        int i;
        for (i = 0; i < PALETTE_SIZE; i++) {
            if (colors_this_line == 0) {
                fprintf(out, "    "); 
            }
            fprintf(out, "0x%04x", color_palette[i]);
            if (i != (PALETTE_SIZE - 1)) {
                fprintf(out, ", ");
            }
            colors_this_line++;
            if (colors_this_line > 8) {
                fprintf(out, "\n");
                colors_this_line = 0;
            }
        }
        fprintf(out, "\n};\n\n");
    }

    /* close up, we're done */
    fclose(out);
}


int main(int argc, char** argv) {
    /* set up the arguments structure */
    struct arguments args;

    /* the default values */
    args.output_file_name = NULL;
    args.input_file_name = NULL;
    args.colorkey = "#ff00ff";
    args.palette = 0;
    args.tileize = 0;

    /* parse command line */
    argp_parse(&info, argc, argv, 0, 0, &args);

    /* the image name without the extension */
    char* name = strdup(args.input_file_name);
    char* extension = strstr(name, ".png");
    if (!extension) {
        fprintf(stderr, "Error: File name should end in .png!\n");
        exit(-1);
    }
    *extension = '\0';

    /* set output file if name given */
    FILE* output;
    /* if none specified use input name with .h */
    char output_name[sizeof(char) * (strlen(name) + 3)];
    sprintf(output_name, "%s.h", name);
    output = fopen(output_name, "w");

    /* set input file to what was passed in */
    FILE* input = fopen(args.input_file_name, "rb");
    if (!input) {
        fprintf(stderr, "Error: Can not open %s for reading!\n",
                args.input_file_name);
        return -1;
    }

    /* do the conversion on these files */
    png2gba(input, output, name, args.palette, args.tileize, args.colorkey);

    return 0;
}

