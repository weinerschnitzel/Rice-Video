/*
//  source code for the ImageLib PNG functions
//
//  Copyright (C) 2001 M. Scott Heiman
//  All Rights Reserved
//
// You may use the software for any purpose you see fit.  You may modify
// it, incorporate it in a commercial application, use it for school,
// even turn it in as homework.  You must keep the Copyright in the
// header and source files.  This software is not in the "Public Domain".
// You may use this software at your own risk.  I have made a reasonable
// effort to verify that this software works in the manner I expect it to;
// however,...
//
// THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS" AND
// WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE, INCLUDING
// WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR FITNESS FOR A
// PARTICULAR PURPOSE. IN NO EVENT SHALL MICHAEL S. HEIMAN BE LIABLE TO
// YOU OR ANYONE ELSE FOR ANY DIRECT, SPECIAL, INCIDENTAL, INDIRECT OR
// CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING
// WITHOUT LIMITATION, LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE,
// OR THE CLAIMS OF THIRD PARTIES, WHETHER OR NOT MICHAEL S. HEIMAN HAS
// BEEN ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
// POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include "pngrw.h"
#include "BMGUtils.h"
#include "png.h"

#ifndef png_jmpbuf
#  define png_jmpbuf(png_ptr) ((png_ptr)->jmpbuf)
#endif
/*
    ReadPNG - Reads the contents of a PNG file and stores the contents into
              BMGImageStruct

    Inputs:
        filename    - the name of the file to be opened

    Outputs:
        img         - the BMGImageStruct containing the image data

    Returns:
        BMGError - if the file could not be read or a resource error occurred
        BMG_OK   - if the file was read and the data was stored in img

    Limitations:
        None.

    Comments:
        2-bit images are converted to 4-bit images.
        16-bit images are converted to 8-bit images.
        gray scale images with alpha components are converted to 32-bit images
*/
BMGError ReadPNG( const char *filename,
        struct BMGImageStruct * volatile img )
{
    jmp_buf             err_jmp;
    int                 error;

    FILE * volatile		file = NULL;
	int	bit_depth, color_type, interlace_type, compression_type, filter_type,
		row_bytes;
    unsigned char       signature[8];
    png_structp volatile png_ptr = NULL;
    png_infop   volatile info_ptr = NULL;
    unsigned long       Width, Height;

    unsigned char      *bits;
    unsigned char** volatile rows = NULL;

	BMGError tmp;

    /* error handler */
    error = setjmp( err_jmp );
    if ( error != 0 )
    {
        if (info_ptr != NULL)
            png_destroy_read_struct((png_structp *) &png_ptr, (png_infop *) &info_ptr, NULL);
        else if (png_ptr != NULL)
            png_destroy_read_struct((png_structp *) &png_ptr, NULL, NULL);
        if ( rows )
        {
            if ( rows[0] )
                free( rows[0] );
            free( rows );
        }
		if (img)
			FreeBMGImage( img );
		if (file)
			fclose( file );
        return (BMGError)error;
    }

    if ( img == NULL )
        longjmp ( err_jmp, (int)errInvalidBMGImage );

    file = fopen( filename, "rb" );
    if ( !file || fread( signature, 1, 8, file ) != 8)
        longjmp ( err_jmp, (int)errFileOpen );

    /* check the signature */
    if (png_sig_cmp( signature, 0, 8 ) != 0)
        longjmp( err_jmp, (int)errUnsupportedFileFormat );

    /* create a pointer to the png read structure */
    png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
    if ( !png_ptr )
        longjmp( err_jmp, (int)errMemoryAllocation );

    /* create a pointer to the png info structure */
    info_ptr = png_create_info_struct( png_ptr );
    if ( !info_ptr )
        longjmp( err_jmp, (int)errMemoryAllocation );

    /* bamboozle the PNG longjmp buffer */
    /*generic PNG error handler*/
    error = setjmp( png_jmpbuf( png_ptr ) );
    if ( error > 0 )
        longjmp( err_jmp, error );

    /* attach file buffer to the png read pointer */
    png_init_io( png_ptr, file );

    /*let the read functions know that we have already read the 1st 8 bytes */
    png_set_sig_bytes( png_ptr, 8 );

    /* read all PNG data up to the image data */
    png_read_info( png_ptr, info_ptr );

    /* extract the data we need to form the HBITMAP from the PNG header */
	png_get_IHDR(png_ptr, info_ptr, &Width, &Height, &bit_depth, &color_type,
                    &interlace_type, &compression_type, &filter_type );

    img->width =  (unsigned int) Width;
    img->height = (unsigned int) Height;

    img->bits_per_pixel = (unsigned char)32;
	img->scan_width = Width * 4;

	/* strip if color channel is larger than 8 bits */
	if (bit_depth > 8)
	{
		png_set_strip_16(png_ptr);
		bit_depth = 8;
	}

   /* These are not really required per Rice format spec,
    * but is done just in case someone uses them.
    * convert palette color to rgb color */
	if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
		color_type = PNG_COLOR_TYPE_RGB;
    }

	/* expand 1,2,4 bit gray scale to 8 bit gray scale */
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);

    /* convert gray scale or gray scale + alpha to rgb color */
	if (color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
		color_type = PNG_COLOR_TYPE_RGB;
    }

    /* add alpha channel if any */
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(png_ptr);
		color_type = PNG_COLOR_TYPE_RGB_ALPHA;
	}

	/* convert rgb to rgba */
	if (color_type == PNG_COLOR_TYPE_RGB) {
		png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
		color_type = PNG_COLOR_TYPE_RGB_ALPHA;
	}

	/* punt invalid formats */
	if (color_type != PNG_COLOR_TYPE_RGB_ALPHA) {
		longjmp(err_jmp, (int)errIncorrectFormat);
	}

	/* convert rgba to bgra */
    png_set_bgr(png_ptr);

	img->palette_size = (unsigned short)0;
	img->bytes_per_palette_entry = 4U;

	tmp = AllocateBMGImage( img );
    if ( tmp != BMG_OK )
        longjmp( err_jmp, (int)tmp );

	/* Update info structure */
    png_read_update_info( png_ptr, info_ptr );

    /* create buffer to read data to */
    rows = (unsigned char **)malloc(Height*sizeof(unsigned char *));
    if ( !rows )
        longjmp( err_jmp, (int)errMemoryAllocation );

	row_bytes = png_get_rowbytes(png_ptr, info_ptr);

    rows[0] = (unsigned char *)malloc( Height*row_bytes*sizeof(char));

    if ( !rows[0] )
        longjmp( err_jmp, (int)errMemoryAllocation );

    for (int i = 1; i < (int)Height; i++ )
		rows[i] = rows[i - 1] + row_bytes;

    /* read the entire image into rows */
    png_read_image( png_ptr, rows );

    bits = img->bits + (Height - 1) * img->scan_width;
    for (int i = 0; i < (int)Height; i++ )
    {
		memcpy(bits, rows[i], 4*Width);
        bits -= img->scan_width;
    }

	free( rows[0] );
    free( rows );
    png_read_end( png_ptr, info_ptr );
    png_destroy_read_struct((png_structp *) &png_ptr, (png_infop *) &info_ptr, NULL);
    fclose( file );

    return BMG_OK;
}

/*
    ReadPNGInfo - Reads the contents of a PNG header and stores the contents into
              BMGImageStruct

    Inputs:
        filename    - the name of the file to be opened

    Outputs:
        img         - the BMGImageStruct containing the image data

    Returns:
        BMGError - if the file could not be read or a resource error occurred
        BMG_OK   - if the file was read and the data was stored in img

*/
BMGError ReadPNGInfo( const char *filename,
        struct BMGImageStruct * volatile img )
{
    jmp_buf             err_jmp;
    int                 error;

    FILE * volatile     file = NULL;
    int                 BitDepth;
    int                 ColorType;
    int                 InterlaceType;
    unsigned char       signature[8];
    png_structp volatile png_ptr = NULL;
    png_infop   volatile info_ptr = NULL;
    png_uint_32         Width, Height;

    /* error handler */
    error = setjmp( err_jmp );
    if (error != 0)
    {
		if (info_ptr != NULL)
            png_destroy_read_struct((png_structp *) &png_ptr, (png_infop *) &info_ptr, NULL);
        else if (png_ptr != NULL)
            png_destroy_read_struct((png_structp *) &png_ptr, NULL, NULL);
        if (img)
            FreeBMGImage(img);
        if (file)
            fclose(file);
        return (BMGError) error;
    }

    if ( img == NULL )
        longjmp ( err_jmp, (int)errInvalidBMGImage );

    file = fopen( filename, "rb" );
    if ( !file || fread( signature, 1, 8, file ) != 8)
        longjmp ( err_jmp, (int)errFileOpen );

    /* check the signature */
    if ( png_sig_cmp( signature, 0, 8 ) != 0 )
        longjmp( err_jmp, (int)errUnsupportedFileFormat );

    /* create a pointer to the png read structure */
    png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
    if ( !png_ptr )
        longjmp( err_jmp, (int)errMemoryAllocation );

    /* bamboozle the PNG longjmp buffer */
    /*generic PNG error handler*/

    error = setjmp( png_jmpbuf( png_ptr ) );
    if ( error > 0 )
        longjmp( err_jmp, error );

    /* set function pointers in the PNG library, for read callbacks */
   png_init_io( png_ptr, file );

    /*let the read functions know that we have already read the 1st 8 bytes */
    png_set_sig_bytes( png_ptr, 8 );

    /* read all PNG data up to the image data */
    png_read_info( png_ptr, info_ptr );

    /* extract the data we need to form the HBITMAP from the PNG header */
    png_get_IHDR( png_ptr, info_ptr, &Width, &Height, &BitDepth, &ColorType,
        &InterlaceType, NULL, NULL);

    img->width = (unsigned int) Width;
    img->height = (unsigned int) Height;

    img->bits_per_pixel = (unsigned char)32;
    img->scan_width = Width * 4;

    img->palette_size = (unsigned short)0;
    img->bytes_per_palette_entry = 4U;
    img->bits = NULL;

    png_destroy_read_struct((png_structp *) &png_ptr, (png_infop *) &info_ptr, NULL);
    fclose( file );

    return BMG_OK;
}

/*
    WritePNG - writes the contents of a BMGImageStruct to a JPEG file.

    Inputs:
        filename    - the name of the file to be opened
        img         - the BMGImageStruct containing the image data

    Returns:
        0 - if the file could not be written or a resource error occurred
        1 - if the file was written

    Comments:
		16-BPP BMG Images are converted to 24-BPP images

    Limitations:
        Color Type is limited to PNG_COLOR_TYPE_GRAY, PNG_COLOR_TYPE_RGB_ALPHA,
        PNG_COLOR_TYPE_RGB, & PNG_COLOR_TYPE_PALETTE;
*/
BMGError WritePNG( const char *filename,
                   struct BMGImageStruct img )
{
    jmp_buf             err_jmp;
    int                 error;

    int                 BitDepth;
    int                 ColorType;
    png_structp         png_ptr = NULL;
    png_infop           info_ptr = NULL;
    png_colorp          PNGPalette = NULL;
    int                 GrayScale;

    unsigned char      *bits, *p, *q;
    unsigned char     **rows = NULL;
    int                 NumColors = 0;

    int                DIBScanWidth;
    int                HasPalette;
    FILE              *outfile = NULL;
    int                i;
	BMGError           tmp;

    /* error handler */
    error = setjmp( err_jmp );
    if ( error != 0 )
    {
        if ( png_ptr != NULL )
            png_destroy_write_struct( &png_ptr, NULL );
        if ( rows )
        {
            if ( rows[0] )
            {
                free( rows[0] );
            }
            free( rows );
        }
        if ( PNGPalette )
            free( PNGPalette );
		if ( outfile)
			fclose( outfile );
        return (BMGError)error;
    }

    /* open the file */
    if ((outfile = fopen(filename, "wb")) == NULL)
        longjmp( err_jmp, (int)errFileOpen );

	/* 16 BPP DIBS do not have palettes.  libPNG expects 16 BPP images to have
	   a palette.  To correct this situation we must convert 16 BPP images to
	   24 BPP images before saving the data to the file */
	if ( img.bits_per_pixel == 16 )
	{
		tmp = Convert16to24( &img ); 
		if (  tmp != BMG_OK )
	        longjmp( err_jmp, (int)tmp );
	}

    HasPalette = img.bits_per_pixel <= 8;
    if ( HasPalette )
    {
        NumColors = img.palette_size;
        /* if this is a grayscale image then set the flag and delete the palette*/
        i = 0;
        bits = img.palette;
        while ( i < NumColors && bits[0] == bits[1] && bits[0] == bits[2] )
        {
            i++;
            bits += img.bytes_per_palette_entry;
        }
        GrayScale = i == NumColors;
    }
    else
        GrayScale = 0;

    /* dimensions */
    DIBScanWidth = ( img.width * img.bits_per_pixel + 7 ) / 8;

    /* create the png pointer */
    png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if ( !png_ptr )
        longjmp( err_jmp, (int)errMemoryAllocation );

    /* create the info pointer */
    info_ptr = png_create_info_struct( png_ptr );
    if ( !info_ptr )
        longjmp( err_jmp, (int)errMemoryAllocation );

    /* bamboozle the png error handler */
	/* error will always == 1 which equals errLib */
//    error = png_setjmp(png_ptr);
    error = setjmp( png_jmpbuf( png_ptr ) );
	if ( error > 0 )
        longjmp( err_jmp, error );

    /* setup the output control */
    png_init_io( png_ptr, outfile );

    /* prepare variables needed to create PNG header */
    BitDepth = img.bits_per_pixel < 8 ? img.bits_per_pixel : 8;

    /* determine color type */
    if ( GrayScale )
        ColorType = PNG_COLOR_TYPE_GRAY;
    else if ( img.bits_per_pixel == 32 )
        ColorType = PNG_COLOR_TYPE_RGB_ALPHA;
    else if ( img.bits_per_pixel == 24 )
        ColorType = PNG_COLOR_TYPE_RGB;
    else
        ColorType = PNG_COLOR_TYPE_PALETTE;

    /* create the PNG header */
    png_set_IHDR( png_ptr, info_ptr, img.width, img.height, BitDepth, ColorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE );

    /* store the palette information if there is any */
    if ( img.palette != NULL && !GrayScale )
    {
        PNGPalette = (png_colorp)png_malloc( png_ptr,
                                             NumColors*sizeof(png_color));
        if ( PNGPalette )
        {
            bits = img.palette;
            for ( i = 0; i < NumColors; i++, bits += img.bytes_per_palette_entry )
            {
                PNGPalette[i].red   = bits[2];
                PNGPalette[i].green = bits[1];
                PNGPalette[i].blue  = bits[0];
            }
            png_set_PLTE( png_ptr, info_ptr, PNGPalette, NumColors );
        }
        else
            longjmp( err_jmp, (int)errMemoryAllocation );
    }

    /* write the file header information */
    png_write_info( png_ptr, info_ptr );

    /* create array to store data in */
    rows = (unsigned char **)malloc(sizeof(unsigned char*));
    if ( !rows )
        longjmp( err_jmp, (int)errMemoryAllocation );
    rows[0] = (unsigned char *)malloc( DIBScanWidth * sizeof(unsigned char));
    if ( !rows[0] )
        longjmp( err_jmp, (int)errMemoryAllocation );

    /* point to the bottom row of the DIB data.  DIBs are stored bottom-to-top,
        PNGs are stored top-to-bottom. */
    bits = img.bits + (img.height - 1) * img.scan_width;

    /* store bits */
    for ( i = 0; i < (int)img.height; i++ )
    {
        switch ( img.bits_per_pixel )
        {
            case 1:
            case 4:
            case 8:
                memcpy( (void *)rows[0], (void *)bits, DIBScanWidth );
                break;
            case 24:
                q = bits;
                for ( p = rows[0]; p < rows[0] + DIBScanWidth; p += 3, q += 3 )
                {
                    p[0] = q[2];
                    p[1] = q[1];
                    p[2] = q[0];
                }
                break;
            case 32:
                q = bits;
                for ( p = rows[0]; p < rows[0] + DIBScanWidth; p += 4, q += 4 )
                {
                    p[3] = q[3];
                    p[0] = q[2];
                    p[1] = q[1];
                    p[2] = q[0];
                }
                break;
        }

        png_write_rows( png_ptr, rows, 1 );
        bits -= img.scan_width;
    }

    /* finish writing the rest of the file */
    png_write_end( png_ptr, info_ptr );

    /* clean up and exit */
    if ( PNGPalette )
        free( PNGPalette );
    free( rows[0] );
    free( rows );
    png_destroy_write_struct( &png_ptr, NULL );
    fclose( outfile );

    return BMG_OK;
}