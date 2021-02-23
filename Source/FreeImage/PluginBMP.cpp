// ==========================================================
// BMP Loader and Writer
//
// Design and implementation by
// - Floris van den Berg (flvdberg@wxs.nl)
// - Markus Loibl (markus.loibl@epost.de)
// - Martin Weber (martweb@gmx.net)
// - Herv� Drolon (drolon@infonie.fr)
// - Michal Novotny (michal@etc.cz)
// - Mihail Naydenov (mnaydenov@users.sourceforge.net)
//
// This file is part of FreeImage 3
//
// COVERED CODE IS PROVIDED UNDER THIS LICENSE ON AN "AS IS" BASIS, WITHOUT WARRANTY
// OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, WITHOUT LIMITATION, WARRANTIES
// THAT THE COVERED CODE IS FREE OF DEFECTS, MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE
// OR NON-INFRINGING. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE COVERED
// CODE IS WITH YOU. SHOULD ANY COVERED CODE PROVE DEFECTIVE IN ANY RESPECT, YOU (NOT
// THE INITIAL DEVELOPER OR ANY OTHER CONTRIBUTOR) ASSUME THE COST OF ANY NECESSARY
// SERVICING, REPAIR OR CORRECTION. THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL
// PART OF THIS LICENSE. NO USE OF ANY COVERED CODE IS AUTHORIZED HEREUNDER EXCEPT UNDER
// THIS DISCLAIMER.
//
// Use at your own risk!
// ==========================================================

#include "FreeImage.h"
#include "Utilities.h"

// ----------------------------------------------------------
//   Constants + headers
// ----------------------------------------------------------

static const BYTE RLE_COMMAND     = 0;
static const BYTE RLE_ENDOFLINE   = 0;
static const BYTE RLE_ENDOFBITMAP = 1;
static const BYTE RLE_DELTA       = 2;

static const BYTE BI_RGB            = 0;	// compression: none
static const BYTE BI_RLE8           = 1;	// compression: RLE 8-bit/pixel
static const BYTE BI_RLE4           = 2;	// compression: RLE 4-bit/pixel
static const BYTE BI_BITFIELDS      = 3;	// compression: Bit field or Huffman 1D compression for BITMAPCOREHEADER2
static const BYTE BI_JPEG           = 4;	// compression: JPEG or RLE-24 compression for BITMAPCOREHEADER2
static const BYTE BI_PNG            = 5;	// compression: PNG
static const BYTE BI_ALPHABITFIELDS = 6;	// compression: Bit field (this value is valid in Windows CE .NET 4.0 and later)

// ----------------------------------------------------------

#ifdef _WIN32
#pragma pack(push, 1)
#else
#pragma pack(1)
#endif

typedef struct tagBITMAPCOREHEADER {
  DWORD   bcSize;
  WORD    bcWidth;
  WORD    bcHeight;
  WORD    bcPlanes;
  WORD    bcBitCnt;
} BITMAPCOREHEADER, *PBITMAPCOREHEADER; 

typedef struct tagBITMAPINFOOS2_1X_HEADER {
  DWORD  biSize;
  WORD   biWidth;
  WORD   biHeight; 
  WORD   biPlanes; 
  WORD   biBitCount;
} BITMAPINFOOS2_1X_HEADER, *PBITMAPINFOOS2_1X_HEADER; 

typedef struct tagBITMAPFILEHEADER {
  WORD    bfType;		//! The file type
  DWORD   bfSize;		//! The size, in bytes, of the bitmap file
  WORD    bfReserved1;	//! Reserved; must be zero
  WORD    bfReserved2;	//! Reserved; must be zero
  DWORD   bfOffBits;	//! The offset, in bytes, from the beginning of the BITMAPFILEHEADER structure to the bitmap bits
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;

#ifdef _WIN32
#pragma pack(pop)
#else
#pragma pack()
#endif

// ==========================================================
// Plugin Interface
// ==========================================================

static int s_format_id;

// ==========================================================
// Internal functions
// ==========================================================

#ifdef FREEIMAGE_BIGENDIAN
static void
SwapInfoHeader(BITMAPINFOHEADER *header) {
	SwapLong(&header->biSize);
	SwapLong((DWORD *)&header->biWidth);
	SwapLong((DWORD *)&header->biHeight);
	SwapShort(&header->biPlanes);
	SwapShort(&header->biBitCount);
	SwapLong(&header->biCompression);
	SwapLong(&header->biSizeImage);
	SwapLong((DWORD *)&header->biXPelsPerMeter);
	SwapLong((DWORD *)&header->biYPelsPerMeter);
	SwapLong(&header->biClrUsed);
	SwapLong(&header->biClrImportant);
}

static void
SwapCoreHeader(BITMAPCOREHEADER *header) {
	SwapLong(&header->bcSize);
	SwapShort(&header->bcWidth);
	SwapShort(&header->bcHeight);
	SwapShort(&header->bcPlanes);
	SwapShort(&header->bcBitCnt);
}

static void
SwapOS21XHeader(BITMAPINFOOS2_1X_HEADER *header) {
	SwapLong(&header->biSize);
	SwapShort(&header->biWidth);
	SwapShort(&header->biHeight);
	SwapShort(&header->biPlanes);
	SwapShort(&header->biBitCount);
}

static void
SwapFileHeader(BITMAPFILEHEADER *header) {
	SwapShort(&header->bfType);
  	SwapLong(&header->bfSize);
  	SwapShort(&header->bfReserved1);
  	SwapShort(&header->bfReserved2);
	SwapLong(&header->bfOffBits);
}
#endif

// --------------------------------------------------------------------------

/**
Check if a BITMAPINFOHEADER is valid
@return Returns TRUE if successful, returns FALSE otherwise
*/
static BOOL
CheckBitmapInfoHeader(BITMAPINFOHEADER *bih) {
	if (bih->biSize != sizeof(BITMAPINFOHEADER)) {
		// The size, in bytes, of the image.This may be set to zero for BI_RGB bitmaps.
		// If biCompression is BI_JPEG or BI_PNG, biSizeImage indicates the size of the JPEG or PNG image buffer, respectively.
		if ((bih->biSize == 0) && (bih->biCompression != BI_RGB)) {
			return FALSE;
		}
		else if ((bih->biCompression == BI_JPEG) || (bih->biCompression == BI_PNG)) {
			// JPEG or PNG is not yet supported
			return FALSE;
		}
		else {
			return FALSE;
		}
	}
	if (bih->biWidth < 0) {
		return FALSE;
	}
	if (bih->biHeight < 0) {
		// If biHeight is negative, indicating a top-down DIB, biCompression must be either BI_RGB or BI_BITFIELDS. 
		// Top-down DIBs cannot be compressed.
		// If biCompression is BI_JPEG or BI_PNG, the biHeight member specifies the height of the decompressed JPEG or PNG image file, respectively.
		if ((bih->biCompression != BI_RGB) && (bih->biCompression != BI_BITFIELDS)) {
			return FALSE;
		}
	}
	if (bih->biPlanes != 1) {
		//  The number of planes for the target device. This value must be set to 1.
		return FALSE;
	}
	switch (bih->biBitCount) {
		case 0:
			// The number of bits-per-pixel is specified or is implied by the JPEG or PNG format.
			// JPEG or PNG is not yet supported			
			return FALSE;
			break;
		case 1:
		case 4:
		case 8:
		case 16:
		case 24:
		case 32:
			break;
		default:
			// Unsupported bitdepth
			return FALSE;
	}
	switch (bih->biCompression) {
		case BI_RGB:
		case BI_RLE8:
		case BI_RLE4:
		case BI_BITFIELDS:
			break;
		case BI_JPEG:
		case BI_PNG:
		default:
			return FALSE;
	}

	return TRUE;
}

// --------------------------------------------------------------------------

/**
Load uncompressed image pixels for 1-, 4-, 8-, 16-, 24- and 32-bit dib
@param io FreeImage IO
@param handle FreeImage IO handle
@param dib Image to be loaded 
@param height Image height
@param pitch Image pitch
@param bit_count Image bit-depth (1-, 4-, 8-, 16-, 24- or 32-bit)
@return Returns the final loaded image
*/
static FIBITMAP* 
LoadPixelData(FreeImageIO *io, fi_handle handle, FIBITMAP* dib, int height, unsigned pitch, unsigned bit_count, FIProgress& progress) throw() {
	BOOL ok = TRUE;
	unsigned count = 0;

	// Load pixel data
	// NB: height can be < 0 for BMP data
	if (height > 0) {
		count = io->read_proc((void *)FreeImage_GetBits(dib), height * pitch, 1, handle);
		if(count != 1) {
			ok = FALSE;
		}
	} else {
		int positiveHeight = abs(height);
		FIProgress::Step step = progress.getStepProgress(positiveHeight, 1);
		for (int c = 0; c < positiveHeight; ++c) {
			count = io->read_proc((void *)FreeImage_GetScanLine(dib, positiveHeight - c - 1), pitch, 1, handle);
			if(count != 1) {
				ok = FALSE;
				break;
			}
			if(! step.progress()) {
				FreeImage_Unload(dib);
				return NULL;
			}
		}
	}

	// swap as needed
#ifdef FREEIMAGE_BIGENDIAN
	if (bit_count == 16) {
		for(unsigned y = 0; y < FreeImage_GetHeight(dib); y++) {
			WORD *pixel = (WORD *)FreeImage_GetScanLine(dib, y);
			for(unsigned x = 0; x < FreeImage_GetWidth(dib); x++) {
				SwapShort(pixel);
				pixel++;
			}
		}
	}
#endif

#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_RGB
	if (bit_count == 24 || bit_count == 32) {
		for(unsigned y = 0; y < FreeImage_GetHeight(dib); y++) {
			BYTE *pixel = FreeImage_GetScanLine(dib, y);
			for(unsigned x = 0; x < FreeImage_GetWidth(dib); x++) {
				INPLACESWAP(pixel[0], pixel[2]);
				pixel += (bit_count >> 3);
			}
		}
	}
#endif

	if(! ok) {
		FreeImage_OutputMessageProc(s_format_id, "Failed to read image data");
	}

	return dib;
}

/**
Load image pixels for 4-bit RLE compressed dib
@param io FreeImage IO
@param handle FreeImage IO handle
@param width Image width
@param height Image height
@param dib 4-bit image to be loaded 
@return Returns the final loaded image
*/
static FIBITMAP* 
LoadPixelDataRLE4(FreeImageIO *io, fi_handle handle, int width, int height, unsigned bit_count, unique_dib& dib_storage, FIProgress& progress) {
	static const char* errmsg = "Error encountered while decoding RLE4 BMP data";

	if(4 != bit_count) {
		throw errmsg;
	}

	int status_byte = 0;
	BYTE second_byte = 0;
	int bits = 0;

	height = abs(height);

	FIProgress::Step step = progress.getStepProgress(height, .99);

	BYTE *const pixels = (BYTE*)malloc(width * height * sizeof(BYTE));
	if (!pixels) {
		throw FI_MSG_ERROR_MEMORY;
	}
	unique_mem pixels_storage(pixels);

	memset(pixels, 0, width * height * sizeof(BYTE));

	BYTE *q = pixels;
	BYTE *const end = pixels + height * width;

	for (int scanline = 0; scanline < height; ) {
		assert(! (q < pixels));

		if (q >= end) {
			if(! step.progress()) {
				dib_storage.reset();
				return NULL;
			}
			break;
		}

		if(io->read_proc(&status_byte, sizeof(BYTE), 1, handle) != 1) {
			throw errmsg;
		}
		if (status_byte != 0)	{
			status_byte = (int)MIN((size_t)status_byte, (size_t)(end - q));
			// Encoded mode
			if(io->read_proc(&second_byte, sizeof(BYTE), 1, handle) != 1) {
				throw errmsg;
			}
			for (int i = 0; i < status_byte; i++)	{
				*q++ = (BYTE)((i & 0x01) ? (second_byte & 0x0f) : ((second_byte >> 4) & 0x0f));
			}
			bits += status_byte;
		}
		else {
			// Escape mode
			if(io->read_proc(&status_byte, sizeof(BYTE), 1, handle) != 1) {
				throw errmsg;
			}
			switch (status_byte) {
				case RLE_ENDOFLINE:
				{
					// End of line
					if(! step.progress()) {
						dib_storage.reset();
						return NULL;
					}
					bits = 0;
					scanline++;
					q = pixels + scanline * width;
				}
				break;

				case RLE_ENDOFBITMAP:
					// End of bitmap
					q = end;
					break;

				case RLE_DELTA:
				{
					// read the delta values
					BYTE delta_x = 0;
					BYTE delta_y = 0;

					if(io->read_proc(&delta_x, sizeof(BYTE), 1, handle) != 1) {
						throw errmsg;
					}
					if(io->read_proc(&delta_y, sizeof(BYTE), 1, handle) != 1) {
						throw errmsg;
					}

					// apply them
					bits += delta_x;
					scanline += delta_y;
					q = pixels + scanline*width+bits;

					while(delta_y) {

						if(! step.progress()) {
							dib_storage.reset();
							return NULL;
						}

						delta_y--;
					}
				}
				break;

				default:
				{
					// Absolute mode
					status_byte = (int)MIN((size_t)status_byte, (size_t)(end - q));
					for (int i = 0; i < status_byte; i++) {
						if ((i & 0x01) == 0) {
							if(io->read_proc(&second_byte, sizeof(BYTE), 1, handle) != 1) {
								throw errmsg;
							}
						}
						*q++ = (BYTE)((i & 0x01) ? (second_byte & 0x0f) : ((second_byte >> 4) & 0x0f));
					}
					bits += status_byte;
					// Read pad byte
					if (((status_byte & 0x03) == 1) || ((status_byte & 0x03) == 2)) {
						BYTE padding = 0;
						if(io->read_proc(&padding, sizeof(BYTE), 1, handle) != 1) {
							throw errmsg;
						}
					}
				}
				break;
			}
		}
	}
		
	{
		// Convert to 4-bit
		for(int y = 0; y < height; y++) {
			const BYTE *src = (BYTE*)pixels + y * width;
			BYTE *dst = FreeImage_GetScanLine(dib_storage.get(), y);

			BOOL hinibble = TRUE;

			for (int cols = 0; cols < width; cols++){
				if (hinibble) {
					dst[cols >> 1] = (src[cols] << 4);
				} else {
					dst[cols >> 1] |= src[cols];
				}

				hinibble = !hinibble;
			}
		}
	}

	return dib_storage.release();
}

/**
Load image pixels for 8-bit RLE compressed dib
@param io FreeImage IO
@param handle FreeImage IO handle
@param width Image width
@param height Image height
@param dib 8-bit image to be loaded 
@return Returns the final loaded image
*/
static FIBITMAP* 
LoadPixelDataRLE8(FreeImageIO *io, fi_handle handle, int width, int height, unsigned bit_count, unique_dib& dib_storage, FIProgress& progress) {
	static const char* errmsg = "Error encountered while decoding RLE8 BMP data";

	if(8 != bit_count) {
		throw errmsg;
	}

	FIBITMAP* dib = dib_storage.get();

	BYTE status_byte = 0;
	BYTE second_byte = 0;
	int scanline = 0;
	int bits = 0;
	int count = 0;
	BYTE delta_x = 0;
	BYTE delta_y = 0;

	height = abs(height);

	FIProgress::Step step = progress.getStepProgress(height, 1);
	
	while(scanline < height) {

		if (io->read_proc(&status_byte, sizeof(BYTE), 1, handle) != 1) {
			throw errmsg;
		}

		if (status_byte == RLE_COMMAND) {
			if (io->read_proc(&status_byte, sizeof(BYTE), 1, handle) != 1) {
				throw errmsg;
			}

			switch (status_byte) {
				case RLE_ENDOFLINE:
					bits = 0;
					scanline++;
					if(! step.progress()) {
						dib_storage.reset();
						return NULL;
					}
					break;

				case RLE_ENDOFBITMAP:
					return dib_storage.release();

				case RLE_DELTA:
					// read the delta values
					delta_x = 0;
					delta_y = 0;
					if (io->read_proc(&delta_x, sizeof(BYTE), 1, handle) != 1) {
						throw errmsg;
					}
					if (io->read_proc(&delta_y, sizeof(BYTE), 1, handle) != 1) {
						throw errmsg;
					}
					// apply them
					bits += delta_x;
					scanline += delta_y;

					while(delta_y) {

						if(! step.progress()) {
							dib_storage.reset();
							return NULL;
						}

						delta_y--;
					}
					break;

				default:
					// absolute mode
					count = MIN((int)status_byte, width - bits);
					if (count < 0) {
						throw errmsg;
					}
					BYTE *sline = FreeImage_GetScanLine(dib, scanline);
					if (io->read_proc((void *)(sline + bits), sizeof(BYTE) * count, 1, handle) != 1) {
						throw errmsg;
					}
					// align run length to even number of bytes
					if ((status_byte & 1) == 1) {
						if (io->read_proc(&second_byte, sizeof(BYTE), 1, handle) != 1) {
							throw errmsg;
						}
					}
					bits += status_byte;				
					break;

			} // switch (status_byte)
		}
		else {
			count = MIN((int)status_byte, width - bits);
			if (count < 0) {
				throw errmsg;
			}
			BYTE *sline = FreeImage_GetScanLine(dib, scanline);
			if (io->read_proc(&second_byte, sizeof(BYTE), 1, handle) != 1) {
				throw errmsg;
			}
			for (int i = 0; i < count; i++) {
				*(sline + bits) = second_byte;
				bits++;
			}
		}
	}
	
	return dib_storage.release();
}

// --------------------------------------------------------------------------

static FIBITMAP *
LoadWindowsBMP(FreeImageIO *io, fi_handle handle, int flags, unsigned bitmap_bits_offset, int type, FIProgress& progress) {

	BOOL header_only = (flags & FIF_LOAD_NOPIXELS) == FIF_LOAD_NOPIXELS;

	// load the info header
	BITMAPINFOHEADER bih;
	memset(&bih, 0, sizeof(BITMAPINFOHEADER));
	if (io->read_proc(&bih, sizeof(BITMAPINFOHEADER), 1, handle) != 1) {
		throw FI_MSG_ERROR_INVALID_FORMAT;
	}

#ifdef FREEIMAGE_BIGENDIAN
	SwapInfoHeader(&bih);
#endif

	if (CheckBitmapInfoHeader(&bih) == FALSE) {
		throw FI_MSG_ERROR_INVALID_FORMAT;
	}

	// keep some general information about the bitmap

	unsigned used_colors	= bih.biClrUsed;
	int width				= bih.biWidth;
	int height				= bih.biHeight;		// WARNING: height can be < 0 => check each call using 'height' as a parameter
	unsigned bit_count		= bih.biBitCount;
	unsigned compression	= bih.biCompression;
	unsigned pitch			= CalculatePitch(CalculateLine(width, bit_count));

	switch (bit_count) {
		case 1 :
		case 4 :
		case 8 :
		{
			if ((used_colors == 0) || (used_colors > CalculateUsedPaletteEntries(bit_count))) {
				used_colors = CalculateUsedPaletteEntries(bit_count);
			}
				
			// allocate enough memory to hold the bitmap (header, palette, pixels) and read the palette

			FIBITMAP *dib = FreeImage_AllocateHeader(header_only, width, height, bit_count);
			if (dib == NULL) {
				throw FI_MSG_ERROR_DIB_MEMORY;
			}

			// set resolution information
			FreeImage_SetDotsPerMeterX(dib, bih.biXPelsPerMeter);
			FreeImage_SetDotsPerMeterY(dib, bih.biYPelsPerMeter);

			// seek to the end of the header (depending on the BMP header version)
			// type == sizeof(BITMAPVxINFOHEADER)
			switch(type) {
				case 40:	// sizeof(BITMAPINFOHEADER) - all Windows versions since Windows 3.0
					break;
				case 52:	// sizeof(BITMAPV2INFOHEADER) (undocumented)
				case 56:	// sizeof(BITMAPV3INFOHEADER) (undocumented)
				case 108:	// sizeof(BITMAPV4HEADER) - all Windows versions since Windows 95/NT4 (not supported)
				case 124:	// sizeof(BITMAPV5HEADER) - Windows 98/2000 and newer (not supported)
					io->seek_proc(handle, (long)(type - sizeof(BITMAPINFOHEADER)), SEEK_CUR);
					break;
			}
				
			// load the palette
			io->read_proc(FreeImage_GetPalette(dib), used_colors * sizeof(RGBQUAD), 1, handle);

#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_RGB
			RGBQUAD *pal = FreeImage_GetPalette(dib);
			for(unsigned i = 0; i < used_colors; i++) {
				INPLACESWAP(pal[i].rgbRed, pal[i].rgbBlue);
			}
#endif

			if(header_only) {
				// header only mode
				return dib;
			}

			// seek to the actual pixel data.
			// this is needed because sometimes the palette is larger than the entries it contains predicts
			io->seek_proc(handle, bitmap_bits_offset, SEEK_SET);

			// read the pixel data

			unique_dib dib_storage(dib);

			switch (compression) {
				case BI_RGB :
					return LoadPixelData(io, handle, dib_storage.release(), height, pitch, bit_count, progress);

				case BI_RLE4 :
					return LoadPixelDataRLE4(io, handle, width, height, bit_count, dib_storage, progress);

				case BI_RLE8 :
					return LoadPixelDataRLE8(io, handle, width, height, bit_count, dib_storage, progress);

				default :
					throw FI_MSG_ERROR_UNSUPPORTED_COMPRESSION;
			}
		}
		break; // 1-, 4-, 8-bit

		case 16 :
		{
			int use_bitfields = 0;
			if (bih.biCompression == BI_BITFIELDS) {
				use_bitfields = 3;
			}
			else if (bih.biCompression == BI_ALPHABITFIELDS) {
				use_bitfields = 4;
			}
			else if (type == 52) {
				use_bitfields = 3;
			}
			else if (type >= 56) {
				use_bitfields = 4;
			}
				
			FIBITMAP* dib;
			if (use_bitfields > 0) {
 				DWORD bitfields[4];
				io->read_proc(bitfields, use_bitfields * sizeof(DWORD), 1, handle);
				dib = FreeImage_AllocateHeader(header_only, width, height, bit_count, bitfields[0], bitfields[1], bitfields[2]);
			} else {
				dib = FreeImage_AllocateHeader(header_only, width, height, bit_count, FI16_555_RED_MASK, FI16_555_GREEN_MASK, FI16_555_BLUE_MASK);
			}

			if (dib == NULL) {
				throw FI_MSG_ERROR_DIB_MEMORY;						
			}

			// set resolution information
			FreeImage_SetDotsPerMeterX(dib, bih.biXPelsPerMeter);
			FreeImage_SetDotsPerMeterY(dib, bih.biYPelsPerMeter);

			if(header_only) {
				// header only mode
				return dib;
			}
				
			// seek to the actual pixel data
			io->seek_proc(handle, bitmap_bits_offset, SEEK_SET);

			// load pixel data and swap as needed if OS is Big Endian
			return LoadPixelData(io, handle, dib, height, pitch, bit_count, progress);
		}
		break; // 16-bit RGB

		case 24 :
		case 32 :
		{
			int use_bitfields = 0;
			if (bih.biCompression == BI_BITFIELDS) {
				use_bitfields = 3;
			}
			else if (bih.biCompression == BI_ALPHABITFIELDS) {
				use_bitfields = 4;
			}
			else if (type == 52) {
				use_bitfields = 3;
			}
			else if (type >= 56) {
				use_bitfields = 4;
			}

			FIBITMAP* dib;
 			if (use_bitfields > 0) {
				DWORD bitfields[4];
				io->read_proc(bitfields, use_bitfields * sizeof(DWORD), 1, handle);
				dib = FreeImage_AllocateHeader(header_only, width, height, bit_count, bitfields[0], bitfields[1], bitfields[2]);
			} else {
				dib = FreeImage_AllocateHeader(header_only, width, height, bit_count, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);
			}

			if (dib == NULL) {
				throw FI_MSG_ERROR_DIB_MEMORY;
			}

			// set resolution information
			FreeImage_SetDotsPerMeterX(dib, bih.biXPelsPerMeter);
			FreeImage_SetDotsPerMeterY(dib, bih.biYPelsPerMeter);

			if(header_only) {
				// header only mode
				return dib;
			}

			// Skip over the optional palette 
			// A 24 or 32 bit DIB may contain a palette for faster color reduction
			// i.e. you can have (FreeImage_GetColorsUsed(dib) > 0)

			// seek to the actual pixel data
			io->seek_proc(handle, bitmap_bits_offset, SEEK_SET);

			// read in the bitmap bits
			// load pixel data and swap as needed if OS is Big Endian
			LoadPixelData(io, handle, dib, height, pitch, bit_count, progress);

			// check if the bitmap contains transparency, if so enable it in the header

			FreeImage_SetTransparent(dib, (FreeImage_GetColorType(dib) == FIC_RGBALPHA));

			return dib;
		}
		break; // 24-, 32-bit
	}

	return NULL;
}

// --------------------------------------------------------------------------

static FIBITMAP *
LoadOS22XBMP(FreeImageIO *io, fi_handle handle, int flags, unsigned bitmap_bits_offset, FIProgress& progress) {

	BOOL header_only = (flags & FIF_LOAD_NOPIXELS) == FIF_LOAD_NOPIXELS;

	// load the info header
	BITMAPINFOHEADER bih;
	memset(&bih, 0, sizeof(BITMAPINFOHEADER));
	if (io->read_proc(&bih, sizeof(BITMAPINFOHEADER), 1, handle) != 1) {
		throw FI_MSG_ERROR_INVALID_FORMAT;
	}

#ifdef FREEIMAGE_BIGENDIAN
	SwapInfoHeader(&bih);
#endif

	if (CheckBitmapInfoHeader(&bih) == FALSE) {
		throw FI_MSG_ERROR_INVALID_FORMAT;
	}

	// keep some general information about the bitmap

	unsigned used_colors	= bih.biClrUsed;
	int width				= bih.biWidth;
	int height				= bih.biHeight;		// WARNING: height can be < 0 => check each read_proc using 'height' as a parameter
	unsigned bit_count		= bih.biBitCount;
	unsigned compression	= bih.biCompression;
	unsigned pitch			= CalculatePitch(CalculateLine(width, bit_count));
		
	switch (bit_count) {
		case 1 :
		case 4 :
		case 8 :
		{
			if ((used_colors == 0) || (used_colors > CalculateUsedPaletteEntries(bit_count)))
				used_colors = CalculateUsedPaletteEntries(bit_count);
					
			// allocate enough memory to hold the bitmap (header, palette, pixels) and read the palette

			FIBITMAP* dib = FreeImage_AllocateHeader(header_only, width, height, bit_count);

			if (dib == NULL) {
				throw FI_MSG_ERROR_DIB_MEMORY;
			}

			// set resolution information
			FreeImage_SetDotsPerMeterX(dib, bih.biXPelsPerMeter);
			FreeImage_SetDotsPerMeterY(dib, bih.biYPelsPerMeter);
				
			// load the palette
			// note that it may contain RGB or RGBA values : we will calculate this
			unsigned pal_size = (bitmap_bits_offset - sizeof(BITMAPFILEHEADER) - bih.biSize) / used_colors; 

			io->seek_proc(handle, sizeof(BITMAPFILEHEADER) + bih.biSize, SEEK_SET);

			RGBQUAD *pal = FreeImage_GetPalette(dib);

			if(pal_size == 4) {
				for (unsigned count = 0; count < used_colors; count++) {
					FILE_BGRA bgra;

					io->read_proc(&bgra, sizeof(FILE_BGRA), 1, handle);
						
					pal[count].rgbRed	= bgra.r;
					pal[count].rgbGreen = bgra.g;
					pal[count].rgbBlue	= bgra.b;
				} 
			} else if(pal_size == 3) {
				for (unsigned count = 0; count < used_colors; count++) {
					FILE_BGR bgr;

					io->read_proc(&bgr, sizeof(FILE_BGR), 1, handle);
						
					pal[count].rgbRed	= bgr.r;
					pal[count].rgbGreen = bgr.g;
					pal[count].rgbBlue	= bgr.b;
				} 
			}
				
			if(header_only) {
				// header only mode
				return dib;
			}

			// seek to the actual pixel data.
			// this is needed because sometimes the palette is larger than the entries it contains predicts

			if (bitmap_bits_offset > (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (used_colors * 3))) {
				io->seek_proc(handle, bitmap_bits_offset, SEEK_SET);
			}

			// read the pixel data

			unique_dib dib_storage(dib);

			switch (compression) {
				case BI_RGB :
					// load pixel data 
					return LoadPixelData(io, handle, dib_storage.release(), height, pitch, bit_count, progress);

				case BI_RLE4 :
					return LoadPixelDataRLE4(io, handle, width, height, bit_count, dib_storage, progress);

				case BI_RLE8 :
					return LoadPixelDataRLE8(io, handle, width, height, bit_count, dib_storage, progress);

				default :		
					throw FI_MSG_ERROR_UNSUPPORTED_COMPRESSION;
			}	
		}

		case 16 :
		{
			FIBITMAP* dib;
			if (bih.biCompression == BI_BITFIELDS) {
				DWORD bitfields[3];

				io->read_proc(bitfields, 3 * sizeof(DWORD), 1, handle);

				dib = FreeImage_AllocateHeader(header_only, width, height, bit_count, bitfields[0], bitfields[1], bitfields[2]);
			} else {
				dib = FreeImage_AllocateHeader(header_only, width, height, bit_count, FI16_555_RED_MASK, FI16_555_GREEN_MASK, FI16_555_BLUE_MASK);
			}

			if (dib == NULL) {
				throw FI_MSG_ERROR_DIB_MEMORY;
			}

			// set resolution information
			FreeImage_SetDotsPerMeterX(dib, bih.biXPelsPerMeter);
			FreeImage_SetDotsPerMeterY(dib, bih.biYPelsPerMeter);

			if(header_only) {
				// header only mode
				return dib;
			}

			if (bitmap_bits_offset > (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (used_colors * 3))) {
				io->seek_proc(handle, bitmap_bits_offset, SEEK_SET);
			}

			// load pixel data and swap as needed if OS is Big Endian
			return LoadPixelData(io, handle, dib, height, pitch, bit_count, progress);
		}

		case 24 :
		case 32 :
		{
			FIBITMAP* dib = FreeImage_AllocateHeader(header_only, width, height, bit_count, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);

			if (dib == NULL) {
				throw FI_MSG_ERROR_DIB_MEMORY;
			}
				
			// set resolution information
			FreeImage_SetDotsPerMeterX(dib, bih.biXPelsPerMeter);
			FreeImage_SetDotsPerMeterY(dib, bih.biYPelsPerMeter);

			if(header_only) {
				// header only mode
				return dib;
			}

			// Skip over the optional palette 
			// A 24 or 32 bit DIB may contain a palette for faster color reduction

			if (bitmap_bits_offset > (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (used_colors * 3))) {
				io->seek_proc(handle, bitmap_bits_offset, SEEK_SET);
			}
				
			// read in the bitmap bits
			// load pixel data and swap as needed if OS is Big Endian
			LoadPixelData(io, handle, dib, height, pitch, bit_count, progress);

			// check if the bitmap contains transparency, if so enable it in the header

			FreeImage_SetTransparent(dib, (FreeImage_GetColorType(dib) == FIC_RGBALPHA));

			return dib;
		}
	}

	return NULL;
}

// --------------------------------------------------------------------------

static FIBITMAP *
LoadOS21XBMP(FreeImageIO *io, fi_handle handle, int flags, unsigned bitmap_bits_offset, FIProgress& progress) {

	BOOL header_only = (flags & FIF_LOAD_NOPIXELS) == FIF_LOAD_NOPIXELS;

	// load the info header
	BITMAPINFOOS2_1X_HEADER bios2_1x;
	memset(&bios2_1x, 0, sizeof(BITMAPINFOOS2_1X_HEADER));
	if (io->read_proc(&bios2_1x, sizeof(BITMAPINFOOS2_1X_HEADER), 1, handle) != 1) {
		throw FI_MSG_ERROR_INVALID_FORMAT;
	}

#ifdef FREEIMAGE_BIGENDIAN
	SwapOS21XHeader(&bios2_1x);
#endif
	// keep some general information about the bitmap

	unsigned width		= bios2_1x.biWidth;
	unsigned height		= bios2_1x.biHeight;	// WARNING: height can be < 0 => check each read_proc using 'height' as a parameter
	unsigned bit_count	= bios2_1x.biBitCount;
	unsigned pitch		= CalculatePitch(CalculateLine(width, bit_count));
		
	switch (bit_count) {
		case 1 :
		case 4 :
		case 8 :
		{
			const unsigned used_colors = CalculateUsedPaletteEntries(bit_count);
				
			// allocate enough memory to hold the bitmap (header, palette, pixels) and read the palette

			FIBITMAP *dib = FreeImage_AllocateHeader(header_only, width, height, bit_count);

			if (dib == NULL) {
				throw FI_MSG_ERROR_DIB_MEMORY;
			}

			// set resolution information to default values (72 dpi in english units)
			FreeImage_SetDotsPerMeterX(dib, 2835);
			FreeImage_SetDotsPerMeterY(dib, 2835);
				
			// load the palette

			RGBQUAD *pal = FreeImage_GetPalette(dib);

			for (unsigned count = 0; count < used_colors; count++) {
				FILE_BGR bgr;

				io->read_proc(&bgr, sizeof(FILE_BGR), 1, handle);
					
				pal[count].rgbRed	= bgr.r;
				pal[count].rgbGreen = bgr.g;
				pal[count].rgbBlue	= bgr.b;
			}
				
			if(header_only) {
				// header only mode
				return dib;
			}

			// Skip over the optional palette 
			// A 24 or 32 bit DIB may contain a palette for faster color reduction

			io->seek_proc(handle, bitmap_bits_offset, SEEK_SET);
				
			// read the pixel data

			// load pixel data
			return LoadPixelData(io, handle, dib, height, pitch, bit_count, progress);
		}

		case 16 :
		{
			FIBITMAP* dib = FreeImage_AllocateHeader(header_only, width, height, bit_count, FI16_555_RED_MASK, FI16_555_GREEN_MASK, FI16_555_BLUE_MASK);

			if (dib == NULL) {
				throw FI_MSG_ERROR_DIB_MEMORY;						
			}

			// set resolution information to default values (72 dpi in english units)
			FreeImage_SetDotsPerMeterX(dib, 2835);
			FreeImage_SetDotsPerMeterY(dib, 2835);

			if(header_only) {
				// header only mode
				return dib;
			}

			// load pixel data and swap as needed if OS is Big Endian
			return LoadPixelData(io, handle, dib, height, pitch, bit_count, progress);
		}

		case 24 :
		case 32 :
		{
			FIBITMAP* dib = FreeImage_AllocateHeader(header_only, width, height, bit_count, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);

			if (dib == NULL) {
				throw FI_MSG_ERROR_DIB_MEMORY;						
			}

			// set resolution information to default values (72 dpi in english units)
			FreeImage_SetDotsPerMeterX(dib, 2835);
			FreeImage_SetDotsPerMeterY(dib, 2835);

			if(header_only) {
				// header only mode
				return dib;
			}

			// Skip over the optional palette 
			// A 24 or 32 bit DIB may contain a palette for faster color reduction

			// load pixel data and swap as needed if OS is Big Endian
			LoadPixelData(io, handle, dib, height, pitch, bit_count, progress);

			// check if the bitmap contains transparency, if so enable it in the header

			FreeImage_SetTransparent(dib, (FreeImage_GetColorType(dib) == FIC_RGBALPHA));

			return dib;
		}
	}

	return NULL;
}

// ==========================================================
// Plugin Implementation
// ==========================================================

static const char * DLL_CALLCONV
Format() {
	return "BMP";
}

static const char * DLL_CALLCONV
Description() {
	return "Windows or OS/2 Bitmap";
}

static const char * DLL_CALLCONV
Extension() {
	return "bmp";
}

static const char * DLL_CALLCONV
RegExpr() {
	return "^BM";
}

static const char * DLL_CALLCONV
MimeType() {
	return "image/bmp";
}

static BOOL DLL_CALLCONV
Validate(FreeImageIO *io, fi_handle handle) {
	BYTE bmp_signature1[] = { 0x42, 0x4D };
	BYTE bmp_signature2[] = { 0x42, 0x41 };
	BYTE signature[2] = { 0, 0 };

	io->read_proc(signature, 1, sizeof(bmp_signature1), handle);

	if (memcmp(bmp_signature1, signature, sizeof(bmp_signature1)) == 0)
		return TRUE;

	if (memcmp(bmp_signature2, signature, sizeof(bmp_signature2)) == 0)
		return TRUE;

	return FALSE;
}

static BOOL DLL_CALLCONV
SupportsExportDepth(int depth) {
	return (
			(depth == 1) ||
			(depth == 4) ||
			(depth == 8) ||
			(depth == 16) ||
			(depth == 24) ||
			(depth == 32)
		);
}

static BOOL DLL_CALLCONV 
SupportsExportType(FREE_IMAGE_TYPE type) {
	return (type == FIT_BITMAP) ? TRUE : FALSE;
}

static BOOL DLL_CALLCONV
SupportsNoPixels() {
	return TRUE;
}

// ----------------------------------------------------------

static FIBITMAP * DLL_CALLCONV
LoadAdv(FreeImageIO *io, fi_handle handle, int page, const FreeImageLoadArgs* args, void *data) {
	if(handle == NULL) {
		return NULL;
	}

	try {
		const int flags = args->flags;
		FIProgress progress(args->cbOption, args->cb, FI_OP_LOAD, s_format_id);
		if(progress.isCanceled()) {
			return NULL;
		}
		BITMAPFILEHEADER bitmapfileheader;
		DWORD type = 0;

		// we use this offset value to make seemingly absolute seeks relative in the file		
		long offset_in_file = io->tell_proc(handle);

		// read the fileheader
		memset(&bitmapfileheader, 0, sizeof(BITMAPFILEHEADER));
		if (io->read_proc(&bitmapfileheader, sizeof(BITMAPFILEHEADER), 1, handle) != 1) {
			return NULL;
		}

#ifdef FREEIMAGE_BIGENDIAN
		SwapFileHeader(&bitmapfileheader);
#endif

		// check the signature
		if((bitmapfileheader.bfType != 0x4D42) && (bitmapfileheader.bfType != 0x4142)) {
			FreeImage_OutputMessageProc(s_format_id, FI_MSG_ERROR_MAGIC_NUMBER);
			return NULL;
		}

		// read the first byte of the infoheader
		io->read_proc(&type, sizeof(DWORD), 1, handle);
		io->seek_proc(handle, 0 - (long)sizeof(DWORD), SEEK_CUR);

#ifdef FREEIMAGE_BIGENDIAN
		SwapLong(&type);
#endif

		// call the appropriate load function for the found bitmap type

		switch(type) {
			case 12:
				// OS/2 and also all Windows versions since Windows 3.0
				return LoadOS21XBMP(io, handle, flags, offset_in_file + bitmapfileheader.bfOffBits, progress);

			case 64:
				// OS/2
				return LoadOS22XBMP(io, handle, flags, offset_in_file + bitmapfileheader.bfOffBits, progress);

			case 40:	// BITMAPINFOHEADER - all Windows versions since Windows 3.0
			case 52:	// BITMAPV2INFOHEADER (undocumented, partially supported)
			case 56:	// BITMAPV3INFOHEADER (undocumented, partially supported)
			case 108:	// BITMAPV4HEADER - all Windows versions since Windows 95/NT4 (partially supported)
			case 124:	// BITMAPV5HEADER - Windows 98/2000 and newer (partially supported)
				return LoadWindowsBMP(io, handle, flags, offset_in_file + bitmapfileheader.bfOffBits, type, progress);

			default:
				FreeImage_OutputMessageProc(s_format_id, "Unknown bmp subtype with id %d", type);
				return NULL;
		}

	} catch(const char *message) {	

		FreeImage_OutputMessageProc(s_format_id, message);
	  return NULL;
	}

}

// ----------------------------------------------------------

/**
Encode a 8-bit source buffer into a 8-bit target buffer using a RLE compression algorithm. 
The size of the target buffer must be equal to the size of the source buffer. 
On return, the function will return the real size of the target buffer, which should be less that or equal to the source buffer size. 
@param target 8-bit Target buffer
@param source 8-bit Source buffer
@param size Source/Target input buffer size
@return Returns the target buffer size
*/
static int
RLEEncodeLine(BYTE *target, BYTE *source, int size) {
	BYTE buffer[256];
	int buffer_size = 0;
	int target_pos = 0;

	for (int i = 0; i < size; ++i) {
		if ((i < size - 1) && (source[i] == source[i + 1])) {
			// find a solid block of same bytes

			int j = i + 1;
			int jmax = 254 + i;

			while ((j < size - 1) && (j < jmax) && (source[j] == source[j + 1]))
				++j;

			// if the block is larger than 3 bytes, use it
			// else put the data into the larger pool

			if (((j - i) + 1) > 3) {
				// don't forget to write what we already have in the buffer

				switch(buffer_size) {
					case 0 :
						break;

					case RLE_DELTA :
						target[target_pos++] = 1;
						target[target_pos++] = buffer[0];
						target[target_pos++] = 1;
						target[target_pos++] = buffer[1];
						break;

					case RLE_ENDOFBITMAP :
						target[target_pos++] = (BYTE)buffer_size;
						target[target_pos++] = buffer[0];
						break;

					default :
						target[target_pos++] = RLE_COMMAND;
						target[target_pos++] = (BYTE)buffer_size;
						memcpy(target + target_pos, buffer, buffer_size);

						// prepare for next run
						
						target_pos += buffer_size;

						if ((buffer_size & 1) == 1)
							target_pos++;

						break;
				}

				// write the continuous data

				target[target_pos++] = (BYTE)((j - i) + 1);
				target[target_pos++] = source[i];

				buffer_size = 0;
			} else {
				for (int k = 0; k < (j - i) + 1; ++k) {
					buffer[buffer_size++] = source[i + k];

					if (buffer_size == 254) {
						// write what we have

						target[target_pos++] = RLE_COMMAND;
						target[target_pos++] = (BYTE)buffer_size;
						memcpy(target + target_pos, buffer, buffer_size);

						// prepare for next run

						target_pos += buffer_size;
						buffer_size = 0;
					}
				}
			}

			i = j;
		} else {
			buffer[buffer_size++] = source[i];
		}

		// write the buffer if it's full

		if (buffer_size == 254) {
			target[target_pos++] = RLE_COMMAND;
			target[target_pos++] = (BYTE)buffer_size;
			memcpy(target + target_pos, buffer, buffer_size);

			// prepare for next run

			target_pos += buffer_size;
			buffer_size = 0;
		}
	}

	// write the last bytes

	switch(buffer_size) {
		case 0 :
			break;

		case RLE_DELTA :
			target[target_pos++] = 1;
			target[target_pos++] = buffer[0];
			target[target_pos++] = 1;
			target[target_pos++] = buffer[1];
			break;

		case RLE_ENDOFBITMAP :
			target[target_pos++] = (BYTE)buffer_size;
			target[target_pos++] = buffer[0];
			break;

		default :
			target[target_pos++] = RLE_COMMAND;
			target[target_pos++] = (BYTE)buffer_size;
			memcpy(target + target_pos, buffer, buffer_size);

			// prepare for next run
			
			target_pos += buffer_size;

			if ((buffer_size & 1) == 1)
				target_pos++;

			break;			
	}

	// write the END_OF_LINE marker

	target[target_pos++] = RLE_COMMAND;
	target[target_pos++] = RLE_ENDOFLINE;

	// return the written size

	return target_pos;
}

static BOOL DLL_CALLCONV
Save(FreeImageIO *io, FIBITMAP *dib, fi_handle handle, int page, int flags, void *data) {
	if ((dib != NULL) && (handle != NULL)) {
		// write the file header

		const unsigned dst_width = FreeImage_GetWidth(dib);
		const unsigned dst_height = FreeImage_GetHeight(dib);

		// note that the dib may have been created using FreeImage_CreateView
		// we need to recalculate the dst pitch here
		const unsigned dst_bpp = FreeImage_GetBPP(dib);
		const unsigned dst_pitch = CalculatePitch(CalculateLine(dst_width, dst_bpp));

		BITMAPFILEHEADER bitmapfileheader;
		bitmapfileheader.bfType = 0x4D42;
		bitmapfileheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + FreeImage_GetColorsUsed(dib) * sizeof(RGBQUAD);
		bitmapfileheader.bfSize = bitmapfileheader.bfOffBits + dst_height * dst_pitch;
		bitmapfileheader.bfReserved1 = 0;
		bitmapfileheader.bfReserved2 = 0;

		// take care of the bit fields data of any

		bool bit_fields = (dst_bpp == 16) ? true : false;

		if (bit_fields) {
			bitmapfileheader.bfSize += 3 * sizeof(DWORD);
			bitmapfileheader.bfOffBits += 3 * sizeof(DWORD);
		}

#ifdef FREEIMAGE_BIGENDIAN
		SwapFileHeader(&bitmapfileheader);
#endif
		if (io->write_proc(&bitmapfileheader, sizeof(BITMAPFILEHEADER), 1, handle) != 1) {
			return FALSE;
		}

		// update the bitmap info header

		BITMAPINFOHEADER bih;
		memcpy(&bih, FreeImage_GetInfoHeader(dib), sizeof(BITMAPINFOHEADER));

		if (bit_fields) {
			bih.biCompression = BI_BITFIELDS;
		}
		else if ((bih.biBitCount == 8) && ((flags & BMP_SAVE_RLE) == BMP_SAVE_RLE)) {
			bih.biCompression = BI_RLE8;
		}
		else {
			bih.biCompression = BI_RGB;
		}

		// write the bitmap info header

#ifdef FREEIMAGE_BIGENDIAN
		SwapInfoHeader(&bih);
#endif
		if (io->write_proc(&bih, sizeof(BITMAPINFOHEADER), 1, handle) != 1) {
			return FALSE;
		}

		// write the bit fields when we are dealing with a 16 bit BMP

		if (bit_fields) {
			DWORD d;

			d = FreeImage_GetRedMask(dib);

			if (io->write_proc(&d, sizeof(DWORD), 1, handle) != 1) {
				return FALSE;
			}

			d = FreeImage_GetGreenMask(dib);

			if (io->write_proc(&d, sizeof(DWORD), 1, handle) != 1) {
				return FALSE;
			}

			d = FreeImage_GetBlueMask(dib);

			if (io->write_proc(&d, sizeof(DWORD), 1, handle) != 1) {
				return FALSE;
			}
		}

		// write the palette

		if (FreeImage_GetPalette(dib) != NULL) {
			RGBQUAD *pal = FreeImage_GetPalette(dib);
			FILE_BGRA bgra;
			for(unsigned i = 0; i < FreeImage_GetColorsUsed(dib); i++ ) {
				bgra.b = pal[i].rgbBlue;
				bgra.g = pal[i].rgbGreen;
				bgra.r = pal[i].rgbRed;
				bgra.a = pal[i].rgbReserved;
				if (io->write_proc(&bgra, sizeof(FILE_BGRA), 1, handle) != 1) {
					return FALSE;
				}
			}
		}

		// write the bitmap data... if RLE compression is enable, use it

		if ((dst_bpp == 8) && ((flags & BMP_SAVE_RLE) == BMP_SAVE_RLE)) {
			BYTE *buffer = (BYTE*)malloc(dst_pitch * 2 * sizeof(BYTE));

			for (unsigned i = 0; i < dst_height; ++i) {
				int size = RLEEncodeLine(buffer, FreeImage_GetScanLine(dib, i), FreeImage_GetLine(dib));

				if (io->write_proc(buffer, size, 1, handle) != 1) {
					free(buffer);
					return FALSE;
				}
			}

			buffer[0] = RLE_COMMAND;
			buffer[1] = RLE_ENDOFBITMAP;

			if (io->write_proc(buffer, 2, 1, handle) != 1) {
				free(buffer);
				return FALSE;
			}

			free(buffer);

#ifdef FREEIMAGE_BIGENDIAN
		} else if (dst_bpp == 16) {
			int padding = dst_pitch - dst_width * sizeof(WORD);
			WORD pad = 0;
			WORD pixel;
			for(unsigned y = 0; y < dst_height; y++) {
				BYTE *line = FreeImage_GetScanLine(dib, y);
				for(unsigned x = 0; x < dst_width; x++) {
					pixel = ((WORD *)line)[x];
					SwapShort(&pixel);
					if (io->write_proc(&pixel, sizeof(WORD), 1, handle) != 1) {
						return FALSE;
					}
				}
				if(padding != 0) {
					if(io->write_proc(&pad, padding, 1, handle) != 1) {
						return FALSE;				
					}
				}
			}
#endif

#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_RGB
		} else if (dst_bpp == 24) {
			int padding = dst_pitch - dst_width * sizeof(FILE_BGR);
			DWORD pad = 0;
			FILE_BGR bgr;
			for(unsigned y = 0; y < dst_height; y++) {
				BYTE *line = FreeImage_GetScanLine(dib, y);
				for(unsigned x = 0; x < dst_width; x++) {
					RGBTRIPLE *triple = ((RGBTRIPLE *)line)+x;
					bgr.b = triple->rgbtBlue;
					bgr.g = triple->rgbtGreen;
					bgr.r = triple->rgbtRed;
					if (io->write_proc(&bgr, sizeof(FILE_BGR), 1, handle) != 1) {
						return FALSE;
					}
				}
				if(padding != 0) {
					if(io->write_proc(&pad, padding, 1, handle) != 1) {
						return FALSE;					
					}
				}
			}
		} else if (dst_bpp == 32) {
			FILE_BGRA bgra;
			for(unsigned y = 0; y < dst_height; y++) {
				BYTE *line = FreeImage_GetScanLine(dib, y);
				for(unsigned x = 0; x < dst_width; x++) {
					RGBQUAD *quad = ((RGBQUAD *)line)+x;
					bgra.b = quad->rgbBlue;
					bgra.g = quad->rgbGreen;
					bgra.r = quad->rgbRed;
					bgra.a = quad->rgbReserved;
					if (io->write_proc(&bgra, sizeof(FILE_BGRA), 1, handle) != 1) {
						return FALSE;
					}
				}
			}
#endif
		} 
		else if (FreeImage_GetPitch(dib) == dst_pitch) {
			return (io->write_proc(FreeImage_GetBits(dib), dst_height * dst_pitch, 1, handle) != 1) ? FALSE : TRUE;
		}
		else {
			for (unsigned y = 0; y < dst_height; y++) {
				BYTE *line = (BYTE*)FreeImage_GetScanLine(dib, y);
				
				if (io->write_proc(line, dst_pitch, 1, handle) != 1) {
					return FALSE;
				}
			}
		}

		return TRUE;

	} else {
		return FALSE;
	}
}

// ==========================================================
//   Init
// ==========================================================

void DLL_CALLCONV
InitBMP(Plugin *plugin, int format_id) {
	s_format_id = format_id;

	plugin->format_proc = Format;
	plugin->description_proc = Description;
	plugin->extension_proc = Extension;
	plugin->regexpr_proc = RegExpr;
	plugin->open_proc = NULL;
	plugin->close_proc = NULL;
	plugin->pagecount_proc = NULL;
	plugin->pagecapability_proc = NULL;
	plugin->load_proc = NULL;
	plugin->loadAdv_proc = LoadAdv;
	plugin->save_proc = Save;
	plugin->validate_proc = Validate;
	plugin->mime_proc = MimeType;
	plugin->supports_export_bpp_proc = SupportsExportDepth;
	plugin->supports_export_type_proc = SupportsExportType;
	plugin->supports_icc_profiles_proc = NULL;	// not implemented yet;
	plugin->supports_no_pixels_proc = SupportsNoPixels;
}
