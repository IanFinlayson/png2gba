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


/* the info for the command line parameters */
const char* argp_program_version = "png2gba 1.0";
const char* argp_program_bug_address = "<finlayson@umw.edu>";
const char doc [] = "PNG to GBA image conversion utility";
const char args_doc[] = "FILE";

/* the command line options for the compiler */
const struct argp_option options[] = {
    {"output", 'o', "file", 0, "Specify output file", 0},
    {NULL, 0, NULL, 0, NULL, 0}
};

/* used by main to communicate with parse_opt */
struct arguments {
    char* output_file_name;
    char* input_file_name;
};

/* the function which parses command line options */
error_t parse_opt (int key, char* arg, struct argp_state* state) {

    /* get the input argument from argp_parse */
    struct arguments *arguments = state->input;

    /* switch on the command line option that was passed in */
    switch (key) {
        case 'o':
            /* the output file name is set */
            arguments->output_file_name = arg;
            break;

            /* we got a file name */
        case ARGP_KEY_ARG:
            if (state->arg_num > 1) {
                /* too many arguments */
                fprintf(stderr, "Only one file name can be passed as input!\n");
                argp_usage(state);
            }

            /* save it as an argument */
            arguments->input_file_name = arg;
            break;

            /* we hit the end of the arguments */
        case ARGP_KEY_END:
            if (state->arg_num < 1) {
                /* not enough arguments */
                fprintf(stderr, "Must pass an input file name!\n");
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
    int w, h;
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
        fprintf(stderr, "This does not seem to be a valid PNG file!\n");
        exit(-1);
    }

    /* setup structs for reading */
    png_structp png_reader = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_reader) {
        fprintf(stderr, "Could not read PNG file!\n");
        exit(-1);
    }
    png_infop png_info = png_create_info_struct(png_reader);
    if (!png_info) {
        fprintf(stderr, "Could not read PNG file!\n");
        exit(-1);
    }
    if (setjmp(png_jmpbuf(png_reader))) {
        fprintf(stderr, "Could not read PNG file!\n");
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
        fprintf(stderr, "Could not read PNG file!\n");
        exit(-1);
    }
    image->rows = (png_bytep*) malloc(sizeof(png_bytep) * image->h);
    int row;
    for (row = 0; row < image->h; row++) {
        image->rows[row] = (png_byte*) malloc(png_get_rowbytes(png_reader,png_info));
    }
    png_read_image(png_reader, image->rows);

    /* close the input file and return */
    fclose(in);
    return image;
}

/* perform the actual conversion from png to gba formats */
void png2gba(FILE* in, FILE* out) {
    /* load the image */
    struct Image* image = read_png(in);









}

int main(int argc, char** argv) {
    /* set up the arguments structure */
    struct arguments args;

    /* the default values */
    args.output_file_name = NULL;
    args.input_file_name = NULL;

    /* parse command line */
    argp_parse(&info, argc, argv, 0, 0, &args);

    /* set output file based on stage and file name given */
    FILE* output;
    if (args.output_file_name != NULL) {
        /* if a name is specified, use that */
        output = fopen(args.output_file_name, "w");
    }
    else {
        /* if none specified use stdout */
        output = stdout;
    }

    /* set input file to what was passed in */
    FILE* input = fopen(args.input_file_name, "rb");
    if (!input) {
        fprintf(stderr, "Error, can not open %s for reading!\n", args.input_file_name);
        return -1;
    }

    /* do the conversion on these files */
    png2gba(input, output);

    return 0;
}




