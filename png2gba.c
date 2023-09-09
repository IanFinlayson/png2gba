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
const char* argp_program_bug_address = "<ifinlay@umw.edu>";
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
            break;

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
            //Disabled the error check since batch processing allows multiple files as input
            //argc counts the amount of parameters given and can normally never be exceeded
            if (state->arg_num > (uint)state->argc) {
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
        //Reset all parameters to reuse them
        r = 0;
        c = 0;
        tr = 0;
        tc = 0;
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

char* get_output_name(char* output_file_name_option, char* input_name){
    char* output_name;
    /* if none specified use input name with .h */
    if (output_file_name_option) {
        output_name = output_file_name_option;
    } else {
        output_name = malloc(sizeof(char) * (strlen(input_name) + 3));
        sprintf(output_name, "%s.h", input_name);
    }
    /* if the name contains directories, like ../test1 or
     * data/images/bg1 or something, get just the base name */
    while (strstr(output_name, "/")) {
        output_name = strstr(output_name, "/") + 1;
    }
    return output_name;
}

/* perform the actual conversion from png to gba formats */
void png2gba(FILE* in, FILE* out, FILE* palette_out, char* name, struct arguments args, int amount_of_files_to_be_processed) {
    
    int palette, tileize;
    palette = args.palette;
    tileize = args.tileize; 
    char* colorkey = args.colorkey;
    char* output_option = args.output_file_name;

    /* load the image */
    struct Image* image = read_png(in);

    char palette_option[10];
    char* index_2d_array_option = "";
    static int amount_of_files_processed = 1;
    char beginning_paragraphs[10] = {"{"};
    char ending_paragraphs[15] = {"};"};
    static char* previous_output_file_name = "";
    char* palette_header1 = ""; 
    char* palette_header2 = ""; 
    char* palette_header3 = ""; 
    char* palette_header4 = ""; 
    char* palette_header5 = "";
    char* include_header = "";
    char* include_guard = "";

    char* output_file_name = get_output_name(args.output_file_name, name);

    include_guard = malloc(strlen(output_file_name)-strlen(".h")+1);
    //Remove ".h" extension from file name
    strncpy(include_guard, output_file_name, strlen(output_file_name)-strlen(".h"));
    include_guard[strlen(output_file_name)-strlen(".h")] = '\0';
    
    for(size_t i=0; i<strlen(include_guard);i++){
        include_guard[i] = toupper((unsigned char)include_guard[i]);
    }

    /* if the name contains directories, like ../test1 or
     * data/images/bg1 or something, get just the base name */
    while (strstr(name, "/")) {
        name = strstr(name, "/") + 1;
    }

    if(amount_of_files_to_be_processed > 1 && output_option){
        index_2d_array_option = malloc(strlen("[%d]")+sizeof(amount_of_files_to_be_processed)+1);
        sprintf(index_2d_array_option, "[%d]", amount_of_files_to_be_processed);
        strcpy(beginning_paragraphs, "{{");
        if(amount_of_files_processed == amount_of_files_to_be_processed){
            //Last file of the batch
            strcpy(ending_paragraphs, "}};\n\n#endif");
        }else{
            strcpy(ending_paragraphs, "}");
        }
    }else{
        strcpy(ending_paragraphs, "};\n\n#endif");
    }

    if(palette){
        include_header = malloc(strlen("#include \"palette_%s\"\n\n")+strlen(output_file_name)+1);
        sprintf(include_header, "#include \"palette_%s\"\n\n", output_file_name);
    }else{
        include_header = malloc(strlen("")+1);
        strcpy(include_header, "");
    }

    if(strcmp(output_file_name, previous_output_file_name)){
        //First file of the batch
        /* write preamble stuff */
        fprintf(out, "/* %s\n * generated by png2gba program */\n\n", output_file_name);
        //Add include guard for outdated and new compilers
        fprintf(out, "#pragma once\n#ifndef _%s_H_\n#define _%s_H_\n\n", include_guard, include_guard);
        fprintf(out, "%s", include_header);
        fprintf(out, "#define %s_width %d\n", name, image->w);
        fprintf(out, "#define %s_height %d\n\n", name, image->h);
        if(output_option){
            fprintf(out, "#define %s_entries %d\n\n", name, amount_of_files_to_be_processed);
            palette_header4 = malloc(strlen("#define %s_palette_entries %d\n\n")+strlen(name)+sizeof(amount_of_files_to_be_processed)+1);
            sprintf(palette_header4, "#define %s_palette_entries %d\n\n", name, amount_of_files_to_be_processed);
        }else{
            fprintf(out, "#define %s_entries %d\n\n", name, 1);
            palette_header4 = malloc(strlen("#define %s_palette_entries %d\n\n")+strlen(name)+sizeof(1)+1);
            sprintf(palette_header4, "#define %s_palette_entries %d\n\n", name, 1);
        }

        palette_header1 = malloc(strlen("/* palette_%s\n * generated by png2gba program */\n\n")+strlen(output_file_name)+1);
        sprintf(palette_header1, "/* palette_%s\n * generated by png2gba program */\n\n", output_file_name);
        palette_header2 = malloc(strlen("//This palette file belongs to the file %s.h\n")+strlen(name)+1);
        sprintf(palette_header2, "//This palette file belongs to the file %s.h\n", name);
        //Add include guard for outdated and new compilers
        palette_header3 = malloc(strlen("#pragma once\n#ifndef _PALETTE_%s_H_\n#define _PALETTE_%s_H_\n\n#include \"%s\"\n\n")+strlen(include_guard)+strlen(include_guard)+strlen(output_file_name)+1);
        sprintf(palette_header3, "#pragma once\n#ifndef _PALETTE_%s_H_\n#define _PALETTE_%s_H_\n\n#include \"%s\"\n\n", include_guard, include_guard, output_file_name);
        
        if (palette) {
            sprintf(palette_option, "char");
        } else {
            sprintf(palette_option, "short");
        }
        fprintf(out, "const unsigned %s %s_data %s[%d] = %s\n", palette_option, name, index_2d_array_option, image->w*image->h, beginning_paragraphs);
        
        palette_header5 = malloc(strlen("const unsigned short %s_palette %s[%d] = %s\n")+strlen(name)+strlen(index_2d_array_option)+sizeof(PALETTE_SIZE)+strlen(beginning_paragraphs)+1);
        sprintf(palette_header5, "const unsigned short %s_palette %s[%d] = %s\n", name, index_2d_array_option, PALETTE_SIZE, beginning_paragraphs);
    }else{
        palette_header1 = malloc(strlen("")+1);
        strcpy(palette_header1, "");
        palette_header2 = malloc(strlen("")+1);
        strcpy(palette_header2, "");
        palette_header3 = malloc(strlen("")+1);
        strcpy(palette_header3, "");
        palette_header4 = malloc(strlen("")+1);
        strcpy(palette_header4, "");
        palette_header5 = malloc(strlen(",{\n")+1);
        strcpy(palette_header5, ",{\n");
        fprintf(out, ",{\n");
    }
    
    previous_output_file_name = malloc(strlen(output_file_name)+1);
    strcpy(previous_output_file_name, output_file_name);

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
    fprintf(out, "\n%s", ending_paragraphs);

    /* write the palette if needed */
    if (palette) {
        int colors_this_line = 0;
        fprintf(palette_out, "%s",palette_header1);
        fprintf(palette_out, "%s",palette_header2);
        fprintf(palette_out, "%s",palette_header3);
        fprintf(palette_out, "%s",palette_header4);
        fprintf(palette_out, "%s",palette_header5); 
        int i;
        for (i = 0; i < PALETTE_SIZE; i++) {
            if (colors_this_line == 0) {
                fprintf(palette_out, "    "); 
            }
            fprintf(palette_out, "0x%04x", color_palette[i]);
            if (i != (PALETTE_SIZE - 1)) {
                fprintf(palette_out, ", ");
            }
            colors_this_line++;
            if (colors_this_line > 8) {
                fprintf(palette_out, "\n");
                colors_this_line = 0;
            }
        }
        fprintf(palette_out, "\n%s", ending_paragraphs);
    }
    amount_of_files_processed++;
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

    //The count parameters used, command-program itself takes up one space so start counting starting from 1
    //Some parameters take two spaces, one for the option and another for the option input
    int command_offset = 1 + args.palette + args.tileize + (strcmp(args.colorkey, "#ff00ff") != 0) + ((args.output_file_name != NULL)*2);

    /* set output file if name given */
    FILE* output;
    FILE* palette_output;
    char* output_name;
    char* palette_output_name;
    //Initialize name with a value as a fall back option
    char* name = strdup(args.input_file_name);

    FILE* input;

    for(int i=command_offset;i<argc;i++){
        //Copy the first argument of the regex given to the commandline
        strcpy(name, argv[i]);
        /* the image name without the extension */
        char* extension = strstr(name, ".png");
        if (!extension) {
            fprintf(stderr, "Error: File name should end in .png!\n");
            exit(-1);
        }
        *extension = '\0';

        char *file_operand_option = "w";;
        
        if(args.output_file_name && i > command_offset){
            file_operand_option = "a";
        }
        output_name = get_output_name(args.output_file_name, name);

        /* set input file to what was passed in */
        input = fopen(argv[i], "rb");
        if (!input) {
            fprintf(stderr, "Error: Can not open %s for reading!\n",
                    args.input_file_name);
            return -1;
        }

        if(args.palette){
            palette_output_name = malloc(sizeof(char) * (strlen(output_name) + strlen("palette_") + 1));
            sprintf(palette_output_name, "palette_%s", output_name);
            palette_output = fopen(palette_output_name, file_operand_option);
        }
        output = fopen(output_name, file_operand_option);
        /* do the conversion on these files */
        png2gba(input, output, palette_output, name, args, (argc-command_offset));
        /* close up, we're done */
        fclose(output);
        if(args.palette){
           fclose(palette_output);
        }
    }

    return 0;
}

