/*
//  source code for the ImageLib BMP functions
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

#include "BMGDLL.h"
#include "BMGUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

const unsigned short BMP_ID = 0x4D42;

/*
    ReadBMP - reads the image data from a BMP files and stores it in a
              BMGImageStruct.

    Inputs:
        filename    - the name of the file to be opened

    Outputs:
        img         - the BMGImageStruct containing the image data

    Returns:
        BMGError - if the file could not be read or a resource error occurred
        BMG_OK   - if the file was read and the data was stored in img

    Limitations:
        will not read BMP files using BI_RLE8, BI_RLE4, or BI_BITFIELDS
*/
BMGError ReadBMP( const char *filename,
              struct BMGImageStruct *img )
{
    FILE *file;
    jmp_buf err_jmp;
    int error;
	BMGError tmp;
    unsigned char *p, *q; /*, *q_end; */
/*    unsigned int cnt; */
    int i;
/*    int EOBMP; */

    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;
/*
    DWORD mask[3];
*/

    unsigned int DIBScanWidth;
    unsigned int bit_size, rawbit_size;
    unsigned char *rawbits = NULL;

	SetLastBMGError( BMG_OK );

    /* error handler */
    error = setjmp( err_jmp );
    if ( error != 0 )
    {
        if ( file != NULL )
            fclose( file );
        if ( rawbits != NULL )
            free( rawbits );
		if( img != NULL)
			FreeBMGImage( img );
		SetLastBMGError( (BMGError)error );
        return (BMGError)error;
    }

    if ( img == NULL )
        longjmp( err_jmp, (int)errInvalidBMGImage );

    file = fopen( filename, "rb" );
    if  ( file == NULL )
        longjmp( err_jmp, (int)errFileOpen );

        /* read the file header */
    if ( fread( (void *)&bmfh, sizeof(BITMAPFILEHEADER), 1, file ) != 1 )
        longjmp( err_jmp, (int)errFileRead );

    /* confirm that this is a BMP file */
    if ( bmfh.bfType != BMP_ID )
        longjmp( err_jmp, (int)errUnsupportedFileFormat );

    /* read the bitmap info header */
    if ( fread( (void *)&bmih, sizeof(BITMAPINFOHEADER), 1, file ) != 1 )
        longjmp( err_jmp, (int)errFileRead );

    /* abort if this is an unsupported format */
    if ( bmih.biCompression != BI_RGB )
            longjmp( err_jmp, (int)errUnsupportedFileFormat );

    img->bits_per_pixel = (unsigned char)bmih.biBitCount;
    img->width  = bmih.biWidth;
    img->height = bmih.biHeight;
    if ( img->bits_per_pixel <= 8 )
    {
        img->palette_size = (unsigned short)bmih.biClrUsed;
        img->bytes_per_palette_entry = 4U;
    }

	tmp = AllocateBMGImage( img );
    if ( tmp != BMG_OK )
        longjmp( err_jmp, (int)tmp );

    /* read palette if necessary */
    if ( img->bits_per_pixel <= 8 )
    {
        if ( fread( (void *)img->palette, sizeof(RGBQUAD), img->palette_size,
                file ) != (unsigned int)img->palette_size )
        {
            longjmp( err_jmp, (int)errFileRead );
        }
    }

    /* dimensions */
    DIBScanWidth = ( img->bits_per_pixel * img->width + 7 ) / 8;
    if ( DIBScanWidth %4 )
        DIBScanWidth += 4 - DIBScanWidth % 4;

    bit_size = img->scan_width * img->height;

    /* allocate memory for the raw bits */
    if ( bmih.biCompression != BI_RGB )
        rawbit_size = bmfh.bfSize - bmfh.bfOffBits;
    else
        rawbit_size = DIBScanWidth * img->height;

    rawbits = (unsigned char *)calloc( rawbit_size, 1 );
    if ( rawbits == NULL )
        longjmp( err_jmp, (int)errMemoryAllocation );

    if ( fread( (void *)rawbits, sizeof(unsigned char), rawbit_size, file )
                   != rawbit_size )
    {
        longjmp( err_jmp, (int)errFileRead );
    }

    if ( bmih.biCompression == BI_RGB )
    {
        p = rawbits;
        for ( q = img->bits; q < img->bits + bit_size;
                         q += img->scan_width, p += DIBScanWidth )
        {
            memcpy( (void *)q, (void *)p, img->scan_width );
        }
    }

    /* swap rows if necessary */
    if ( bmih.biHeight < 0 )
    {
        for ( i = 0; i < (int)(img->height) / 2; i++ )
        {
            p = img->bits + i * img->scan_width;
            q = img->bits + ((img->height) - i - 1 ) * img->scan_width;
            memcpy( (void *)rawbits, (void *)p, img->scan_width );
            memcpy( (void *)p, (void *)q, img->scan_width );
            memcpy( (void *)q, (void *)rawbits, img->scan_width );
        }
    }

    fclose( file );
    free( rawbits );
    return BMG_OK;
}

/*
    ReadBMPInfo - reads the header data from a BMP files and stores it in a
              BMGImageStruct.

    Inputs:
        filename    - the name of the file to be opened

    Outputs:
        img         - the BMGImageStruct containing the image data

    Returns:
        BMGError - if the file could not be read or a resource error occurred
        BMG_OK   - if the file was read and the data was stored in img

*/
BMGError ReadBMPInfo( const char *filename,
              struct BMGImageStruct *img )
{
    FILE *file;
    jmp_buf err_jmp;
    int error;

    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;

	SetLastBMGError( BMG_OK );

    /* error handler */
    error = setjmp( err_jmp );
    if ( error != 0 )
    {
        if ( file != NULL )
            fclose( file );
		if ( img != NULL)
			FreeBMGImage( img );
		SetLastBMGError( (BMGError)error );
        return (BMGError)error;
    }

    if ( img == NULL )
        longjmp( err_jmp, (int)errInvalidBMGImage );

    file = fopen( filename, "rb" );
    if  ( file == NULL )
        longjmp( err_jmp, (int)errFileOpen );

        /* read the file header */
    if ( fread( (void *)&bmfh, sizeof(BITMAPFILEHEADER), 1, file ) != 1 )
        longjmp( err_jmp, (int)errFileRead );

    /* confirm that this is a BMP file */
    if ( bmfh.bfType != BMP_ID )
        longjmp( err_jmp, (int)errUnsupportedFileFormat );

    /* read the bitmap info header */
    if ( fread( (void *)&bmih, sizeof(BITMAPINFOHEADER), 1, file ) != 1 )
        longjmp( err_jmp, (int)errFileRead );

    /* abort if this is an unsupported format */
    if ( bmih.biCompression != BI_RGB )
            longjmp( err_jmp, (int)errUnsupportedFileFormat );

    img->bits_per_pixel = (unsigned char)bmih.biBitCount;
    img->width  = bmih.biWidth;
    img->height = bmih.biHeight;
    if ( img->bits_per_pixel <= 8 )
    {
        img->palette_size = (unsigned short)bmih.biClrUsed;
        img->bytes_per_palette_entry = 4U;
    }

    fclose( file );
    return BMG_OK;
}

/*
    WriteBMP - writes the contents of an BMGImageStruct to a bmp file.

    Inputs:
        filename    - the name of the file to be opened
        img         - the BMGImageStruct containing the image data

    Returns:
        BMGError - if a write error or a resource error occurred
        BMG_OK   - if the data was successfilly stored in filename

    Limitations:
        will not write BMP files using BI_RLE8, BI_RLE4, or BI_BITFIELDS
*/
BMGError WriteBMP( const char *filename,
                   struct BMGImageStruct img )
{
    FILE *file;
    jmp_buf err_jmp;
    int error;

    unsigned char *bits = NULL;
    unsigned int DIBScanWidth;
    unsigned int BitsPerPixel;
    unsigned int bit_size; /*, new_bit_size; */
/*    unsigned int rawbit_size; */
    unsigned char *p, *q, *r, *t;
/*    unsigned int cnt;  */
    unsigned char *pColor = NULL;

    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;

	SetLastBMGError( BMG_OK );

    /* error handler */
    error = setjmp( err_jmp );
    if ( error != 0 )
    {
        if ( file != NULL )
            fclose( file );
        if ( bits != NULL )
            free( bits );
        if ( pColor != NULL )
            free( pColor );
		SetLastBMGError( (BMGError)error );
        return (BMGError)error;
    }

    if ( img.bits == NULL )
        longjmp( err_jmp, (int)errInvalidBMGImage );

    file = fopen( filename, "wb" );
    if ( file == NULL )
        longjmp( err_jmp, (int)errFileOpen );

    /* abort if we do not support the data */
    if ( img.palette != NULL && img.bytes_per_palette_entry < 3 )
        longjmp( err_jmp, (int)errInvalidBMGImage );

    /* calculate dimensions */
    BitsPerPixel = img.bits_per_pixel < 32 ? img.bits_per_pixel : 24U;
    DIBScanWidth = ( BitsPerPixel * img.width + 7 ) / 8;
    if ( DIBScanWidth % 4 )
        DIBScanWidth += 4 - DIBScanWidth % 4;
    bit_size = DIBScanWidth * img.height;
/*    rawbit_size = BITScanWidth * img.height; */

    /* allocate memory for bit array - assume that compression will
    // actually compress the bitmap */
    bits = (unsigned char *)calloc( bit_size, 1 );
    if ( bits == NULL )
        longjmp( err_jmp, (int)errMemoryAllocation );

    /* some initialization */
    memset( (void *)&bmih, 0, sizeof(BITMAPINFOHEADER) );
    bmih.biSize = sizeof(BITMAPINFOHEADER);
    bmih.biWidth = img.width;
    bmih.biHeight = img.height;
    bmih.biPlanes = 1;
    /* 32-bit images will be stored as 24-bit images to save space.  The BMP
       format does not use the high word and I do not want to store alpha
       components in an image format that does not recognize it */
    bmih.biBitCount = BitsPerPixel;
    bmih.biCompression = BI_RGB; // assumed
    bmih.biSizeImage = bit_size; // assumed
    bmih.biClrUsed = img.palette == NULL ? 0 : img.palette_size;
    bmih.biClrImportant = img.palette == NULL ? 0 : img.palette_size;

    /* if we are not compressed then copy the raw bits to bits */
    if ( bmih.biCompression == BI_RGB )
    {
        p = img.bits;
        /* simple memcpy's for images containing < 32-bits per pixel */
        if ( img.bits_per_pixel < 32 )
        {
            for ( q = bits; q < bits + bit_size; q += DIBScanWidth,
                                                 p += img.scan_width )
            {
                memcpy( (void *)q, (void *)p, img.scan_width );
            }
        }
        /* store 32-bit images as 24-bit images to save space. alpha terms
           are lost */
        else
        {
            DIBScanWidth = 3 * img.width;
            if ( DIBScanWidth % 4 )
                DIBScanWidth += 4 - DIBScanWidth % 4;

            for ( q = bits; q < bits + bit_size; q += DIBScanWidth,
                                                 p += img.scan_width )
            {
                t = p;
                for ( r = q; r < q + DIBScanWidth; r += 3, t += 4 )
                    memcpy( (void *)r, (void *)t, 3 );
            }
        }
    }

    /* create the palette if necessary */
    if ( img.palette != NULL )
    {
        pColor = (unsigned char *)calloc( img.palette_size, sizeof(RGBQUAD) );
        if ( pColor == NULL )
            longjmp( err_jmp, (int)errMemoryAllocation );

        if ( img.bytes_per_palette_entry == 3 )
        {
            p = img.palette;
            for ( q = pColor + 1; q < pColor +img.palette_size*sizeof(RGBQUAD);
                            q += sizeof(RGBQUAD), p += 3 )
            {
                memcpy( (void *)pColor, (void *)p, 3 );
            }
        }
        else /* img.bytes_per_palette_entry == 4 */
        {
            memcpy( (void *)pColor, (void *)img.palette,
                img.palette_size * sizeof(RGBQUAD) );
        }
    }

    /* now that we know how big everything is let's write the file */
    memset( (void *)&bmfh, 0, sizeof(BITMAPFILEHEADER) );
    bmfh.bfType = BMP_ID;
    bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) +
                     img.palette_size * sizeof(RGBQUAD);
    bmfh.bfSize = bmfh.bfOffBits + bit_size;

    if ( fwrite( (void *)&bmfh, sizeof(BITMAPFILEHEADER), 1, file ) != 1 )
        longjmp( err_jmp, (int)errFileWrite );

    if ( fwrite( (void *)&bmih, sizeof(BITMAPINFOHEADER), 1, file ) != 1 )
        longjmp( err_jmp, (int)errFileWrite );

    if ( pColor != NULL )
    {
        if ( fwrite( (void *)pColor, sizeof(RGBQUAD), img.palette_size, file )
                              != (unsigned int)img.palette_size )
        {
            longjmp( err_jmp, (int)errFileWrite );
        }
    }

    if ( fwrite( (void *)bits, sizeof(unsigned char), bit_size, file )
                    != bit_size )
    {
        longjmp( err_jmp, (int)errFileWrite );
    }

    fclose( file );
    free( bits );
    if ( pColor != NULL )
        free( pColor );
    return BMG_OK;
}