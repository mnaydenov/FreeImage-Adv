// ==========================================================
// JPEG Loader and writer
// Based on code developed by The Independent JPEG Group
//
// Design and implementation by
// - Floris van den Berg (flvdberg@wxs.nl)
// - Jan L. Nauta (jln@magentammt.com)
// - Markus Loibl (markus.loibl@epost.de)
// - Karl-Heinz Bussian (khbussian@moss.de)
// - Herv� Drolon (drolon@infonie.fr)
// - Jascha Wetzel (jascha@mainia.de)
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

#ifdef _MSC_VER 
#pragma warning (disable : 4786) // identifier was truncated to 'number' characters
#endif

extern "C" {
#define XMD_H
#undef FAR
#include <setjmp.h>

#include "../LibJPEG/jinclude.h"
#include "../LibJPEG/jpeglib.h"
#include "../LibJPEG/jerror.h"
}

#include "FreeImage.h"
#include "Utilities.h"

#include "../Metadata/FreeImageTag.h"


// ==========================================================
// Plugin Interface
// ==========================================================

static int s_format_id;

// ----------------------------------------------------------
//   Constant declarations
// ----------------------------------------------------------

#define INPUT_BUF_SIZE  4096	// choose an efficiently fread'able size 
#define OUTPUT_BUF_SIZE 4096    // choose an efficiently fwrite'able size

#define EXIF_MARKER		(JPEG_APP0+1)	// EXIF marker / Adobe XMP marker
#define ICC_MARKER		(JPEG_APP0+2)	// ICC profile marker
#define IPTC_MARKER		(JPEG_APP0+13)	// IPTC marker / BIM marker 

#define ICC_HEADER_SIZE 14				// size of non-profile data in APP2
#define MAX_BYTES_IN_MARKER 65533L		// maximum data length of a JPEG marker
#define MAX_DATA_BYTES_IN_MARKER 65519L	// maximum data length of a JPEG APP2 marker

// ----------------------------------------------------------
//   Typedef declarations
// ----------------------------------------------------------

typedef struct tagErrorManager {
	/// "public" fields
	struct jpeg_error_mgr pub;
	/// for return to caller
	jmp_buf setjmp_buffer;
} ErrorManager;

typedef struct tagSourceManager {
	/// public fields
	struct jpeg_source_mgr pub;
	/// source stream
	fi_handle infile;
	FreeImageIO *m_io;
	/// start of buffer
	JOCTET * buffer;
	/// have we gotten any data yet ?
	boolean start_of_file;
} SourceManager;

typedef struct tagDestinationManager {
	/// public fields
	struct jpeg_destination_mgr pub;
	/// destination stream
	fi_handle outfile;
	FreeImageIO *m_io;
	/// start of buffer
	JOCTET * buffer;
} DestinationManager;

typedef SourceManager*		freeimage_src_ptr;
typedef DestinationManager* freeimage_dst_ptr;
typedef ErrorManager*		freeimage_error_ptr;

// ----------------------------------------------------------
//   Error handling
// ----------------------------------------------------------

/**
	Receives control for a fatal error.  Information sufficient to
	generate the error message has been stored in cinfo->err; call
	output_message to display it.  Control must NOT return to the caller;
	generally this routine will exit() or longjmp() somewhere.
*/
METHODDEF(void)
jpeg_error_exit (j_common_ptr cinfo) {
	// always display the message
	(*cinfo->err->output_message)(cinfo);

	// allow JPEG with a premature end of file
	if((cinfo)->err->msg_parm.i[0] != 13) {
	
		// let the memory manager delete any temp files before we die
		jpeg_destroy(cinfo);
		
		throw s_format_id;
	}
}

/**
	Actual output of any JPEG message.  Note that this method does not know
	how to generate a message, only where to send it.
*/
METHODDEF(void)
jpeg_output_message (j_common_ptr cinfo) {
	char buffer[JMSG_LENGTH_MAX];

	// create the message
	(*cinfo->err->format_message)(cinfo, buffer);
	// send it to user's message proc
	FreeImage_OutputMessageProc(s_format_id, buffer);
}

// ----------------------------------------------------------
//   Destination manager
// ----------------------------------------------------------

/**
	Initialize destination.  This is called by jpeg_start_compress()
	before any data is actually written. It must initialize
	next_output_byte and free_in_buffer. free_in_buffer must be
	initialized to a positive value.
*/
METHODDEF(void)
init_destination (j_compress_ptr cinfo) {
	freeimage_dst_ptr dest = (freeimage_dst_ptr) cinfo->dest;

	dest->buffer = (JOCTET *)
	  (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				  OUTPUT_BUF_SIZE * SIZEOF(JOCTET));

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

/**
	This is called whenever the buffer has filled (free_in_buffer
	reaches zero). In typical applications, it should write out the
	*entire* buffer (use the saved start address and buffer length;
	ignore the current state of next_output_byte and free_in_buffer).
	Then reset the pointer & count to the start of the buffer, and
	return TRUE indicating that the buffer has been dumped.
	free_in_buffer must be set to a positive value when TRUE is
	returned.  A FALSE return should only be used when I/O suspension is
	desired.
*/
METHODDEF(boolean)
empty_output_buffer (j_compress_ptr cinfo) {
	freeimage_dst_ptr dest = (freeimage_dst_ptr) cinfo->dest;

	if (dest->m_io->write_proc(dest->buffer, 1, OUTPUT_BUF_SIZE, dest->outfile) != OUTPUT_BUF_SIZE)
		throw(cinfo, JERR_FILE_WRITE);

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

	return TRUE;
}

/**
	Terminate destination --- called by jpeg_finish_compress() after all
	data has been written.  In most applications, this must flush any
	data remaining in the buffer.  Use either next_output_byte or
	free_in_buffer to determine how much data is in the buffer.
*/
METHODDEF(void)
term_destination (j_compress_ptr cinfo) {
	freeimage_dst_ptr dest = (freeimage_dst_ptr) cinfo->dest;

	size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

	// write any data remaining in the buffer

	if (datacount > 0) {
		if (dest->m_io->write_proc(dest->buffer, 1, datacount, dest->outfile) != datacount)
		  throw(cinfo, JERR_FILE_WRITE);
	}
}

// ----------------------------------------------------------
//   Source manager
// ----------------------------------------------------------

/**
	Initialize source.  This is called by jpeg_read_header() before any
	data is actually read. Unlike init_destination(), it may leave
	bytes_in_buffer set to 0 (in which case a fill_input_buffer() call
	will occur immediately).
*/
METHODDEF(void)
init_source (j_decompress_ptr cinfo) {
	freeimage_src_ptr src = (freeimage_src_ptr) cinfo->src;

	/* We reset the empty-input-file flag for each image,
 	 * but we don't clear the input buffer.
	 * This is correct behavior for reading a series of images from one source.
	*/

	src->start_of_file = TRUE;
}

/**
	This is called whenever bytes_in_buffer has reached zero and more
	data is wanted.  In typical applications, it should read fresh data
	into the buffer (ignoring the current state of next_input_byte and
	bytes_in_buffer), reset the pointer & count to the start of the
	buffer, and return TRUE indicating that the buffer has been reloaded.
	It is not necessary to fill the buffer entirely, only to obtain at
	least one more byte.  bytes_in_buffer MUST be set to a positive value
	if TRUE is returned.  A FALSE return should only be used when I/O
	suspension is desired.
*/
METHODDEF(boolean)
fill_input_buffer (j_decompress_ptr cinfo) {
	freeimage_src_ptr src = (freeimage_src_ptr) cinfo->src;

	size_t nbytes = src->m_io->read_proc(src->buffer, 1, INPUT_BUF_SIZE, src->infile);

	if (nbytes <= 0) {
		if (src->start_of_file)	/* Treat empty input file as fatal error */
			throw(cinfo, JERR_INPUT_EMPTY);

		WARNMS(cinfo, JWRN_JPEG_EOF);

		/* Insert a fake EOI marker */

		src->buffer[0] = (JOCTET) 0xFF;
		src->buffer[1] = (JOCTET) JPEG_EOI;

		nbytes = 2;
	}

	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;
	src->start_of_file = FALSE;

	return TRUE;
}

/**
	Skip num_bytes worth of data.  The buffer pointer and count should
	be advanced over num_bytes input bytes, refilling the buffer as
	needed. This is used to skip over a potentially large amount of
	uninteresting data (such as an APPn marker). In some applications
	it may be possible to optimize away the reading of the skipped data,
	but it's not clear that being smart is worth much trouble; large
	skips are uncommon.  bytes_in_buffer may be zero on return.
	A zero or negative skip count should be treated as a no-op.
*/
METHODDEF(void)
skip_input_data (j_decompress_ptr cinfo, long num_bytes) {
	freeimage_src_ptr src = (freeimage_src_ptr) cinfo->src;

	/* Just a dumb implementation for now.  Could use fseek() except
     * it doesn't work on pipes.  Not clear that being smart is worth
	 * any trouble anyway --- large skips are infrequent.
	*/

	if (num_bytes > 0) {
		while (num_bytes > (long) src->pub.bytes_in_buffer) {
		  num_bytes -= (long) src->pub.bytes_in_buffer;

		  (void) fill_input_buffer(cinfo);

		  /* note we assume that fill_input_buffer will never return FALSE,
		   * so suspension need not be handled.
		   */
		}

		src->pub.next_input_byte += (size_t) num_bytes;
		src->pub.bytes_in_buffer -= (size_t) num_bytes;
	}
}

/**
	Terminate source --- called by jpeg_finish_decompress
	after all data has been read.  Often a no-op.

	NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
	application must deal with any cleanup that should happen even
	for error exit.
*/
METHODDEF(void)
term_source (j_decompress_ptr cinfo) {
  // no work necessary here
}

// ----------------------------------------------------------
//   Source manager & Destination manager setup
// ----------------------------------------------------------

/**
	Prepare for input from a stdio stream.
	The caller must have already opened the stream, and is responsible
	for closing it after finishing decompression.
*/
GLOBAL(void)
jpeg_freeimage_src (j_decompress_ptr cinfo, fi_handle infile, FreeImageIO *io) {
	freeimage_src_ptr src;

	// allocate memory for the buffer. is released automatically in the end

	if (cinfo->src == NULL) {
		cinfo->src = (struct jpeg_source_mgr *) (*cinfo->mem->alloc_small)
			((j_common_ptr) cinfo, JPOOL_PERMANENT, SIZEOF(SourceManager));

		src = (freeimage_src_ptr) cinfo->src;

		src->buffer = (JOCTET *) (*cinfo->mem->alloc_small)
			((j_common_ptr) cinfo, JPOOL_PERMANENT, INPUT_BUF_SIZE * SIZEOF(JOCTET));
	}

	// initialize the jpeg pointer struct with pointers to functions

	src = (freeimage_src_ptr) cinfo->src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart; // use default method 
	src->pub.term_source = term_source;
	src->infile = infile;
	src->m_io = io;
	src->pub.bytes_in_buffer = 0;		// forces fill_input_buffer on first read 
	src->pub.next_input_byte = NULL;	// until buffer loaded 
}

/**
	Prepare for output to a stdio stream.
	The caller must have already opened the stream, and is responsible
	for closing it after finishing compression.
*/
GLOBAL(void)
jpeg_freeimage_dst (j_compress_ptr cinfo, fi_handle outfile, FreeImageIO *io) {
	freeimage_dst_ptr dest;

	if (cinfo->dest == NULL) {
		cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)
			((j_common_ptr) cinfo, JPOOL_PERMANENT, SIZEOF(DestinationManager));
	}

	dest = (freeimage_dst_ptr) cinfo->dest;
	dest->pub.init_destination = init_destination;
	dest->pub.empty_output_buffer = empty_output_buffer;
	dest->pub.term_destination = term_destination;
	dest->outfile = outfile;
	dest->m_io = io;
}

// ----------------------------------------------------------
//   Special markers read functions
// ----------------------------------------------------------

/**
	Read JPEG_COM marker (comment)
*/
static BOOL 
jpeg_read_comment(FIBITMAP *dib, const BYTE *dataptr, unsigned int datalen) {
	size_t length = datalen;
	BYTE *profile = (BYTE*)dataptr;

	// read the comment
	char *value = (char*)malloc((length + 1) * sizeof(char));
	if(value == NULL) return FALSE;
	memcpy(value, profile, length);
	value[length] = '\0';

	// create a tag
	FITAG *tag = FreeImage_CreateTag();
	if(tag) {
		unsigned count = length + 1;	// includes the null value

		FreeImage_SetTagID(tag, JPEG_COM);
		FreeImage_SetTagKey(tag, "Comment");
		FreeImage_SetTagLength(tag, count);
		FreeImage_SetTagCount(tag, count);
		FreeImage_SetTagType(tag, FIDT_ASCII);
		FreeImage_SetTagValue(tag, value);
		
		// store the tag
		FreeImage_SetMetadata(FIMD_COMMENTS, dib, FreeImage_GetTagKey(tag), tag);

		// destroy the tag
		FreeImage_DeleteTag(tag);
	}

	free(value);

	return TRUE;
}

/** 
	Read JPEG_APP2 marker (ICC profile)
*/

/**
Handy subroutine to test whether a saved marker is an ICC profile marker.
*/
static BOOL 
marker_is_icc(jpeg_saved_marker_ptr marker) {
    // marker identifying string "ICC_PROFILE" (null-terminated)
	const BYTE icc_signature[12] = { 0x49, 0x43, 0x43, 0x5F, 0x50, 0x52, 0x4F, 0x46, 0x49, 0x4C, 0x45, 0x00 };

	if(marker->marker == ICC_MARKER) {
		// verify the identifying string
		if(marker->data_length >= ICC_HEADER_SIZE) {
			if(memcmp(icc_signature, marker->data, sizeof(icc_signature)) == 0) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
  See if there was an ICC profile in the JPEG file being read;
  if so, reassemble and return the profile data.

  TRUE is returned if an ICC profile was found, FALSE if not.
  If TRUE is returned, *icc_data_ptr is set to point to the
  returned data, and *icc_data_len is set to its length.
  
  IMPORTANT: the data at **icc_data_ptr has been allocated with malloc()
  and must be freed by the caller with free() when the caller no longer
  needs it.  (Alternatively, we could write this routine to use the
  IJG library's memory allocator, so that the data would be freed implicitly
  at jpeg_finish_decompress() time.  But it seems likely that many apps
  will prefer to have the data stick around after decompression finishes.)
  
  NOTE: if the file contains invalid ICC APP2 markers, we just silently
  return FALSE.  You might want to issue an error message instead.
*/
static BOOL 
jpeg_read_icc_profile(j_decompress_ptr cinfo, JOCTET **icc_data_ptr, unsigned *icc_data_len) {
	jpeg_saved_marker_ptr marker;
	int num_markers = 0;
	int seq_no;
	JOCTET *icc_data;
	unsigned total_length;

	const int MAX_SEQ_NO = 255;			// sufficient since marker numbers are bytes
	BYTE marker_present[MAX_SEQ_NO+1];	// 1 if marker found
	unsigned data_length[MAX_SEQ_NO+1];	// size of profile data in marker
	unsigned data_offset[MAX_SEQ_NO+1];	// offset for data in marker
	
	*icc_data_ptr = NULL;		// avoid confusion if FALSE return
	*icc_data_len = 0;
	
	/**
	this first pass over the saved markers discovers whether there are
	any ICC markers and verifies the consistency of the marker numbering.
	*/
	
	memset(marker_present, 0, (MAX_SEQ_NO + 1));
	
	for(marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
		if (marker_is_icc(marker)) {
			if (num_markers == 0) {
				// number of markers
				num_markers = GETJOCTET(marker->data[13]);
			}
			else if (num_markers != GETJOCTET(marker->data[13])) {
				return FALSE;		// inconsistent num_markers fields 
			}
			// sequence number
			seq_no = GETJOCTET(marker->data[12]);
			if (seq_no <= 0 || seq_no > num_markers) {
				return FALSE;		// bogus sequence number 
			}
			if (marker_present[seq_no]) {
				return FALSE;		// duplicate sequence numbers 
			}
			marker_present[seq_no] = 1;
			data_length[seq_no] = marker->data_length - ICC_HEADER_SIZE;
		}
	}
	
	if (num_markers == 0)
		return FALSE;
		
	/**
	check for missing markers, count total space needed,
	compute offset of each marker's part of the data.
	*/
	
	total_length = 0;
	for(seq_no = 1; seq_no <= num_markers; seq_no++) {
		if (marker_present[seq_no] == 0) {
			return FALSE;		// missing sequence number
		}
		data_offset[seq_no] = total_length;
		total_length += data_length[seq_no];
	}
	
	if (total_length <= 0)
		return FALSE;		// found only empty markers ?
	
	// allocate space for assembled data 
	icc_data = (JOCTET *) malloc(total_length * sizeof(JOCTET));
	if (icc_data == NULL)
		return FALSE;		// out of memory
	
	// and fill it in
	for (marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
		if (marker_is_icc(marker)) {
			JOCTET FAR *src_ptr;
			JOCTET *dst_ptr;
			unsigned length;
			seq_no = GETJOCTET(marker->data[12]);
			dst_ptr = icc_data + data_offset[seq_no];
			src_ptr = marker->data + ICC_HEADER_SIZE;
			length = data_length[seq_no];
			while (length--) {
				*dst_ptr++ = *src_ptr++;
			}
		}
	}
	
	*icc_data_ptr = icc_data;
	*icc_data_len = total_length;
	
	return TRUE;
}

/**
	Read JPEG_APPD marker (IPTC or Adobe Photoshop profile)
*/
BOOL 
jpeg_read_iptc_profile(FIBITMAP *dib, const BYTE *dataptr, unsigned int datalen) {
	return read_iptc_profile(dib, dataptr, datalen);
}

/**
	Read JPEG_APP1 marker (XMP profile)
	@param dib Input FIBITMAP
	@param dataptr Pointer to the APP1 marker
	@param datalen APP1 marker length
	@return Returns TRUE if successful, FALSE otherwise
*/
static BOOL  
jpeg_read_xmp_profile(FIBITMAP *dib, const BYTE *dataptr, unsigned int datalen) {
	// marker identifying string for XMP (null terminated)
	char *xmp_signature = "http://ns.adobe.com/xap/1.0/";

	size_t length = datalen;
	BYTE *profile = (BYTE*)dataptr;

	// verify the identifying string

	if(memcmp(xmp_signature, profile, strlen(xmp_signature)) == 0) {
		// XMP profile

		size_t offset = strlen(xmp_signature) + 1;
		profile += offset;
		length  -= offset;

		// create a tag
		FITAG *tag = FreeImage_CreateTag();
		if(tag) {
			FreeImage_SetTagID(tag, JPEG_APP0+1);	// 0xFFE1
			FreeImage_SetTagKey(tag, g_TagLib_XMPFieldName);
			FreeImage_SetTagLength(tag, length);
			FreeImage_SetTagCount(tag, length);
			FreeImage_SetTagType(tag, FIDT_ASCII);
			FreeImage_SetTagValue(tag, profile);
			
			// store the tag
			FreeImage_SetMetadata(FIMD_XMP, dib, FreeImage_GetTagKey(tag), tag);

			// destroy the tag
			FreeImage_DeleteTag(tag);
		}

		return TRUE;
	}

	return FALSE;
}

/**
	Read JPEG special markers
*/
static BOOL 
read_markers(j_decompress_ptr cinfo, FIBITMAP *dib) {
	jpeg_saved_marker_ptr marker;

	for(marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
		switch(marker->marker) {
			case JPEG_COM:
				// JPEG comment
				jpeg_read_comment(dib, marker->data, marker->data_length);
				break;
			case EXIF_MARKER:
				// Exif or Adobe XMP profile
				jpeg_read_exif_profile(dib, marker->data, marker->data_length);
				jpeg_read_xmp_profile(dib, marker->data, marker->data_length);
				break;
			case IPTC_MARKER:
				// IPTC/NAA or Adobe Photoshop profile
				jpeg_read_iptc_profile(dib, marker->data, marker->data_length);
				break;
		}
	}

	// ICC profile
	BYTE *icc_profile = NULL;
	unsigned icc_length = 0;

	if( jpeg_read_icc_profile(cinfo, &icc_profile, &icc_length) ) {
		// copy ICC profile data
		FreeImage_CreateICCProfile(dib, icc_profile, icc_length);
		// clean up
		free(icc_profile);
	}

	return TRUE;
}

// ----------------------------------------------------------
//   Special markers write functions
// ----------------------------------------------------------

/**
	Write JPEG_COM marker (comment)
*/
static BOOL 
jpeg_write_comment(j_compress_ptr cinfo, FIBITMAP *dib) {
	FITAG *tag = NULL;

	// write user comment as a JPEG_COM marker
	FreeImage_GetMetadata(FIMD_COMMENTS, dib, "Comment", &tag);
	if(tag) {
		const char *tag_value = (char*)FreeImage_GetTagValue(tag);

		if(NULL != tag_value) {
			for(long i = 0; i < (long)strlen(tag_value); i+= MAX_BYTES_IN_MARKER) {
				jpeg_write_marker(cinfo, JPEG_COM, (BYTE*)tag_value + i, MIN((long)strlen(tag_value + i), MAX_BYTES_IN_MARKER));
			}
			return TRUE;
		}
	}
	return FALSE;
}

/** 
	Write JPEG_APP2 marker (ICC profile)
*/
static BOOL 
jpeg_write_icc_profile(j_compress_ptr cinfo, FIBITMAP *dib) {
    // marker identifying string "ICC_PROFILE" (null-terminated)
	BYTE icc_signature[12] = { 0x49, 0x43, 0x43, 0x5F, 0x50, 0x52, 0x4F, 0x46, 0x49, 0x4C, 0x45, 0x00 };

	FIICCPROFILE *iccProfile = FreeImage_GetICCProfile(dib);

	if (iccProfile->size && iccProfile->data) {
		// ICC_HEADER_SIZE: ICC signature is 'ICC_PROFILE' + 2 bytes

		BYTE *profile = (BYTE*)malloc((iccProfile->size + ICC_HEADER_SIZE) * sizeof(BYTE));
		if(profile == NULL) return FALSE;
		memcpy(profile, icc_signature, 12);

		for(long i = 0; i < (long)iccProfile->size; i += MAX_DATA_BYTES_IN_MARKER) {
			unsigned length = MIN((long)(iccProfile->size - i), MAX_DATA_BYTES_IN_MARKER);
			// sequence number
			profile[12] = (BYTE) ((i / MAX_DATA_BYTES_IN_MARKER) + 1);
			// number of markers
			profile[13] = (BYTE) (iccProfile->size / MAX_DATA_BYTES_IN_MARKER + 1);

			memcpy(profile + ICC_HEADER_SIZE, (BYTE*)iccProfile->data + i, length);
			jpeg_write_marker(cinfo, ICC_MARKER, profile, (length + ICC_HEADER_SIZE));
        }

		free(profile);

		return TRUE;		
	}
	
	return FALSE;
}

/** 
	Write JPEG_APPD marker (IPTC or Adobe Photoshop profile)
	@return Returns TRUE if successful, FALSE otherwise
*/
static BOOL  
jpeg_write_iptc_profile(j_compress_ptr cinfo, FIBITMAP *dib) {
	//const char *ps_header = "Photoshop 3.0\x08BIM\x04\x04\x0\x0\x0\x0";
	const unsigned tag_length = 26;

	if(FreeImage_GetMetadataCount(FIMD_IPTC, dib)) {
		BYTE *profile = NULL;
		unsigned profile_size = 0;

		// create a binary profile
		if(write_iptc_profile(dib, &profile, &profile_size)) {

			// write the profile
			for(long i = 0; i < (long)profile_size; i += 65517L) {
				unsigned length = MIN((long)profile_size - i, 65517L);
				unsigned roundup = length & 0x01;	// needed for Photoshop
				BYTE *iptc_profile = (BYTE*)malloc(length + roundup + tag_length);
				if(iptc_profile == NULL) break;
				// Photoshop identification string
				memcpy(&iptc_profile[0], "Photoshop 3.0\x0", 14);
				// 8BIM segment type
				memcpy(&iptc_profile[14], "8BIM\x04\x04\x0\x0\x0\x0", 10);
				// segment size
				iptc_profile[24] = (BYTE)(length >> 8);
				iptc_profile[25] = (BYTE)(length & 0xFF);
				// segment data
				memcpy(&iptc_profile[tag_length], &profile[i], length);
				if(roundup)
					iptc_profile[length + tag_length] = 0;
				jpeg_write_marker(cinfo, IPTC_MARKER, iptc_profile, length + roundup + tag_length);
				free(iptc_profile);
			}

			// release profile
			free(profile);

			return TRUE;
		}
	}

	return FALSE;
}

/** 
	Write JPEG_APP1 marker (XMP profile)
	@return Returns TRUE if successful, FALSE otherwise
*/
static BOOL  
jpeg_write_xmp_profile(j_compress_ptr cinfo, FIBITMAP *dib) {
	// marker identifying string for XMP (null terminated)
	char *xmp_signature = "http://ns.adobe.com/xap/1.0/";

	FITAG *tag_xmp = NULL;
	FreeImage_GetMetadata(FIMD_XMP, dib, g_TagLib_XMPFieldName, &tag_xmp);

	if(tag_xmp) {
		const BYTE *tag_value = (BYTE*)FreeImage_GetTagValue(tag_xmp);

		if(NULL != tag_value) {
			// XMP signature is 29 bytes long
			unsigned xmp_header_size = strlen(xmp_signature) + 1;

			DWORD tag_length = FreeImage_GetTagLength(tag_xmp);

			BYTE *profile = (BYTE*)malloc((tag_length + xmp_header_size) * sizeof(BYTE));
			if(profile == NULL) return FALSE;
			memcpy(profile, xmp_signature, xmp_header_size);

			for(DWORD i = 0; i < tag_length; i += 65504L) {
				unsigned length = MIN((long)(tag_length - i), 65504L);
				
				memcpy(profile + xmp_header_size, tag_value + i, length);
				jpeg_write_marker(cinfo, EXIF_MARKER, profile, (length + xmp_header_size));
			}

			free(profile);

			return TRUE;	
		}
	}

	return FALSE;
}

/**
	Write JPEG special markers
*/
static BOOL 
write_markers(j_compress_ptr cinfo, FIBITMAP *dib) {

	// write user comment as a JPEG_COM marker
	jpeg_write_comment(cinfo, dib);

	// write ICC profile
	jpeg_write_icc_profile(cinfo, dib);

	// write IPTC profile
	jpeg_write_iptc_profile(cinfo, dib);

	// write Adobe XMP profile
	jpeg_write_xmp_profile(cinfo, dib);

	return TRUE;
}

// ------------------------------------------------------------
//   Keep original size info when using scale option on loading
// ------------------------------------------------------------
static void 
store_size_info(FIBITMAP *dib, JDIMENSION width, JDIMENSION height) {
	char buffer[256];
	// create a tag
	FITAG *tag = FreeImage_CreateTag();
	if(tag) {
		int length = 0;
		// set the original width
		sprintf(buffer, "%d", (int)width);
		length = strlen(buffer) + 1;	// include the NULL/0 value
		FreeImage_SetTagKey(tag, "OriginalJPEGWidth");
		FreeImage_SetTagLength(tag, length);
		FreeImage_SetTagCount(tag, length);
		FreeImage_SetTagType(tag, FIDT_ASCII);
		FreeImage_SetTagValue(tag, buffer);
		FreeImage_SetMetadata(FIMD_COMMENTS, dib, FreeImage_GetTagKey(tag), tag);
		// set the original height
		sprintf(buffer, "%d", (int)height);
		length = strlen(buffer) + 1;	// include the NULL/0 value
		FreeImage_SetTagKey(tag, "OriginalJPEGHeight");
		FreeImage_SetTagLength(tag, length);
		FreeImage_SetTagCount(tag, length);
		FreeImage_SetTagType(tag, FIDT_ASCII);
		FreeImage_SetTagValue(tag, buffer);
		FreeImage_SetMetadata(FIMD_COMMENTS, dib, FreeImage_GetTagKey(tag), tag);
		// destroy the tag
		FreeImage_DeleteTag(tag);
	}
}

// ==========================================================
// Plugin Implementation
// ==========================================================

static const char * DLL_CALLCONV
Format() {
	return "JPEG";
}

static const char * DLL_CALLCONV
Description() {
	return "JPEG - JFIF Compliant";
}

static const char * DLL_CALLCONV
Extension() {
	return "jpg,jif,jpeg,jpe";
}

static const char * DLL_CALLCONV
RegExpr() {
	return "^\377\330\377";
}

static const char * DLL_CALLCONV
MimeType() {
	return "image/jpeg";
}

static BOOL DLL_CALLCONV
Validate(FreeImageIO *io, fi_handle handle) {
	BYTE jpeg_signature[] = { 0xFF, 0xD8 };
	BYTE signature[2] = { 0, 0 };

	io->read_proc(signature, 1, sizeof(jpeg_signature), handle);

	return (memcmp(jpeg_signature, signature, sizeof(jpeg_signature)) == 0);
}

static BOOL DLL_CALLCONV
SupportsExportDepth(int depth) {
	return (
			(depth == 8) ||
			(depth == 24)
		);
}

static BOOL DLL_CALLCONV 
SupportsExportType(FREE_IMAGE_TYPE type) {
	return (type == FIT_BITMAP) ? TRUE : FALSE;
}

static BOOL DLL_CALLCONV
SupportsICCProfiles() {
	return TRUE;
}

// ----------------------------------------------------------

static FIBITMAP * DLL_CALLCONV
Load(FreeImageIO *io, fi_handle handle, int page, int flags, void *data) {
	if (handle) {
		FIBITMAP *dib = NULL;

		try {
			// set up the jpeglib structures

			struct jpeg_decompress_struct cinfo;
			struct jpeg_error_mgr jerr;

			// step 1: allocate and initialize JPEG decompression object

			cinfo.err = jpeg_std_error(&jerr);

			jerr.error_exit     = jpeg_error_exit;
			jerr.output_message = jpeg_output_message;

			jpeg_create_decompress(&cinfo);

			// step 2a: specify data source (eg, a handle)

			jpeg_freeimage_src(&cinfo, handle, io);

			// step 2b: save special markers for later reading
			
			jpeg_save_markers(&cinfo, JPEG_COM, 0xFFFF);
			for(int m = 0; m < 16; m++) {
				jpeg_save_markers(&cinfo, JPEG_APP0 + m, 0xFFFF);
			}

			// step 3: read handle parameters with jpeg_read_header()

			jpeg_read_header(&cinfo, TRUE);

			// step 4: set parameters for decompression

			unsigned int scale_denom = 1;		// fraction by which to scale image
			int	requested_size = flags >> 16;	// requested user size in pixels
			if(requested_size > 0) {
				// the JPEG codec can perform x2, x4 or x8 scaling on loading
				// try to find the more appropriate scaling according to user's need
				double scale = MAX((double)cinfo.image_width, (double)cinfo.image_height) / (double)requested_size;
				if(scale >= 8) {
					scale_denom = 8;
				} else if(scale >= 4) {
					scale_denom = 4;
				} else if(scale >= 2) {
					scale_denom = 2;
				}
			}
			cinfo.scale_denom = scale_denom;

			if ((flags & JPEG_ACCURATE) != JPEG_ACCURATE) {
				cinfo.dct_method          = JDCT_IFAST;
				cinfo.do_fancy_upsampling = FALSE;
			}

			// step 5a: start decompressor and calculate output width and height

			jpeg_start_decompress(&cinfo);

			// step 5b: allocate dib and init header

			if((cinfo.num_components == 4) && (cinfo.out_color_space == JCS_CMYK)) {
				// CMYK image
				if((flags & JPEG_CMYK) == JPEG_CMYK) {
					// load as CMYK
					dib = FreeImage_Allocate(cinfo.output_width, cinfo.output_height, 32, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);
					if(!dib) return NULL;
					FreeImage_GetICCProfile(dib)->flags |= FIICC_COLOR_IS_CMYK;
				} else {
					// load as CMYK and convert to RGB
					dib = FreeImage_Allocate(cinfo.output_width, cinfo.output_height, 24, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);
					if(!dib) return NULL;
				}
			} else {
				// RGB or greyscale image
				dib = FreeImage_Allocate(cinfo.output_width, cinfo.output_height, 8 * cinfo.num_components, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);
				if(!dib) return NULL;

				if (cinfo.num_components == 1) {
					// build a greyscale palette
					RGBQUAD *colors = FreeImage_GetPalette(dib);

					for (int i = 0; i < 256; i++) {
						colors[i].rgbRed   = (BYTE)i;
						colors[i].rgbGreen = (BYTE)i;
						colors[i].rgbBlue  = (BYTE)i;
					}
				}
			}
			if(scale_denom != 1) {
				// store original size info if a scaling was requested
				store_size_info(dib, cinfo.image_width, cinfo.image_height);
			}

			// step 5c: handle metrices

			BITMAPINFOHEADER *pInfoHeader = FreeImage_GetInfoHeader(dib);

			if (cinfo.density_unit == 1) {
				// dots/inch

				pInfoHeader->biXPelsPerMeter = (int) (((float)cinfo.X_density) / 0.0254000 + 0.5);
				pInfoHeader->biYPelsPerMeter = (int) (((float)cinfo.Y_density) / 0.0254000 + 0.5);
			} else if (cinfo.density_unit == 2) {
				// dots/cm

				pInfoHeader->biXPelsPerMeter = cinfo.X_density * 100;
				pInfoHeader->biYPelsPerMeter = cinfo.Y_density * 100;
			}

			// step 6a: while (scan lines remain to be read) jpeg_read_scanlines(...);

			if((cinfo.out_color_space == JCS_CMYK) && ((flags & JPEG_CMYK) != JPEG_CMYK)) {
				// convert from CMYK to RGB

				JSAMPARRAY buffer;		// output row buffer
				unsigned row_stride;	// physical row width in output buffer

				// JSAMPLEs per row in output buffer
				row_stride = cinfo.output_width * cinfo.output_components;
				// make a one-row-high sample array that will go away when done with image
				buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

				while (cinfo.output_scanline < cinfo.output_height) {
					JSAMPROW src = buffer[0];
					JSAMPROW dst = FreeImage_GetScanLine(dib, cinfo.output_height - cinfo.output_scanline - 1);

					jpeg_read_scanlines(&cinfo, buffer, 1);

					for(unsigned x = 0; x < FreeImage_GetWidth(dib); x++) {
						WORD K = (WORD)src[3];
						dst[FI_RGBA_RED]   = (BYTE)((K * src[0]) / 255);
						dst[FI_RGBA_GREEN] = (BYTE)((K * src[1]) / 255);
						dst[FI_RGBA_BLUE]  = (BYTE)((K * src[2]) / 255);
						src += 4;
						dst += 3;
					}
				}
			} else {
				// normal case (RGB or greyscale image)

				while (cinfo.output_scanline < cinfo.output_height) {
					JSAMPROW dst = FreeImage_GetScanLine(dib, cinfo.output_height - cinfo.output_scanline - 1);

					jpeg_read_scanlines(&cinfo, &dst, 1);
				}

				// step 6b: swap red and blue components (see LibJPEG/jmorecfg.h: #define RGB_RED, ...)
				// The default behavior of the JPEG library is kept "as is" because LibTIFF uses 
				// LibJPEG "as is".

#ifndef FREEIMAGE_BIGENDIAN
				if(cinfo.num_components == 3) {
					for(unsigned y = 0; y < FreeImage_GetHeight(dib); y++) {
						BYTE *target = FreeImage_GetScanLine(dib, y);
						for(unsigned x = 0; x < FreeImage_GetWidth(dib); x++) {
							INPLACESWAP(target[0], target[2]);
							target += 3;
						}
					}
				}
#endif
			}

			// step 7: read special markers

			read_markers(&cinfo, dib);

			// step 8: finish decompression

			jpeg_finish_decompress(&cinfo);

			// step 9: release JPEG decompression object

			jpeg_destroy_decompress(&cinfo);

			// everything went well. return the loaded dib

			return (FIBITMAP *)dib;
		} catch (...) {
			if(NULL != dib) {
				FreeImage_Unload(dib);
			}
		}
	}

	return NULL;
}

static BOOL DLL_CALLCONV
Save(FreeImageIO *io, FIBITMAP *dib, fi_handle handle, int page, int flags, void *data) {
	if ((dib) && (handle)) {
		try {
			// Check dib format

			const char *sError = "only 24-bit highcolor or 8-bit greyscale/palette bitmaps can be saved as JPEG";

			FREE_IMAGE_COLOR_TYPE color_type = FreeImage_GetColorType(dib);
			WORD bpp = (WORD)FreeImage_GetBPP(dib);

			if ((bpp != 24) && (bpp != 8))
				throw sError;

			if(bpp == 8) {
				// allow grey, reverse grey and palette 
				if ((color_type != FIC_MINISBLACK) && (color_type != FIC_MINISWHITE) && (color_type != FIC_PALETTE))
					throw sError;
			}


			struct jpeg_compress_struct cinfo;
			struct jpeg_error_mgr jerr;

			// Step 1: allocate and initialize JPEG compression object

			cinfo.err = jpeg_std_error(&jerr);

			jerr.error_exit     = jpeg_error_exit;
			jerr.output_message = jpeg_output_message;

			// Now we can initialize the JPEG compression object

			jpeg_create_compress(&cinfo);

			// Step 2: specify data destination (eg, a file)

			jpeg_freeimage_dst(&cinfo, handle, io);

			// Step 3: set parameters for compression 

			cinfo.image_width = FreeImage_GetWidth(dib);
			cinfo.image_height = FreeImage_GetHeight(dib);

			switch(color_type) {
				case FIC_MINISBLACK :
				case FIC_MINISWHITE :
					cinfo.in_color_space = JCS_GRAYSCALE;
					cinfo.input_components = 1;
					break;

				default :
					cinfo.in_color_space = JCS_RGB;
					cinfo.input_components = 3;
					break;
			}

			jpeg_set_defaults(&cinfo);

		    // progressive-JPEG support
			if((flags & JPEG_PROGRESSIVE) == JPEG_PROGRESSIVE) {
				jpeg_simple_progression(&cinfo);
			}

			// Set JFIF density parameters from the DIB data

			BITMAPINFOHEADER *pInfoHeader = FreeImage_GetInfoHeader(dib);
			cinfo.X_density = (UINT16) (0.5 + 0.0254 * pInfoHeader->biXPelsPerMeter);
			cinfo.Y_density = (UINT16) (0.5 + 0.0254 * pInfoHeader->biYPelsPerMeter);
			cinfo.density_unit = 1;	// dots / inch

			// Step 4: set quality
			// the first 7 bits are reserved for low level quality settings
			// the other bits are high level (i.e. enum-ish)

			int quality;

			if ((flags & JPEG_QUALITYBAD) == JPEG_QUALITYBAD) {
				quality = 10;
			} else if ((flags & JPEG_QUALITYAVERAGE) == JPEG_QUALITYAVERAGE) {
				quality = 25;
			} else if ((flags & JPEG_QUALITYNORMAL) == JPEG_QUALITYNORMAL) {
				quality = 50;
			} else if ((flags & JPEG_QUALITYGOOD) == JPEG_QUALITYGOOD) {
				quality = 75;
			} else 	if ((flags & JPEG_QUALITYSUPERB) == JPEG_QUALITYSUPERB) {
				quality = 100;
			} else {
				if ((flags & 0x7F) == 0) {
					quality = 75;
				} else {
					quality = flags & 0x7F;
				}
			}

			jpeg_set_quality(&cinfo, quality, TRUE); /* limit to baseline-JPEG values */

			// Step 5: Start compressor 

			jpeg_start_compress(&cinfo, TRUE);

			// Step 6: Write special markers

			write_markers(&cinfo, dib);

			// Step 7: while (scan lines remain to be written) 

			if(color_type == FIC_RGB) {
				// 24-bit RGB image : need to swap red and blue channels
				unsigned pitch = FreeImage_GetPitch(dib);
				BYTE *target = (BYTE*)malloc(pitch * sizeof(BYTE));
				if (target == NULL) 
					throw "no memory to allocate intermediate scanline buffer";

				while (cinfo.next_scanline < cinfo.image_height) {
					// get a copy of the scanline
					memcpy(target, FreeImage_GetScanLine(dib, FreeImage_GetHeight(dib) - cinfo.next_scanline - 1), pitch);
#ifndef FREEIMAGE_BIGENDIAN
					// swap R and B channels
					BYTE *target_p = target;
					for(unsigned x = 0; x < cinfo.image_width; x++) {
						INPLACESWAP(target_p[0], target_p[2]);
						target_p += 3;
					}
#endif
					// write the scanline
					jpeg_write_scanlines(&cinfo, &target, 1);
				}
				free(target);
			}
			else if(color_type == FIC_MINISBLACK) {
				// 8-bit standard greyscale images
				while (cinfo.next_scanline < cinfo.image_height) {
					JSAMPROW b = FreeImage_GetScanLine(dib, FreeImage_GetHeight(dib) - cinfo.next_scanline - 1);

					jpeg_write_scanlines(&cinfo, &b, 1);
				}
			}
			else if(color_type == FIC_PALETTE) {
				// 8-bit palettized images are converted to 24-bit images
				RGBQUAD *palette = FreeImage_GetPalette(dib);
				BYTE *target = (BYTE*)malloc(cinfo.image_width * 3);
				if (target == NULL)
					throw "no memory to allocate intermediate scanline buffer";

				while (cinfo.next_scanline < cinfo.image_height) {
					BYTE *source = FreeImage_GetScanLine(dib, FreeImage_GetHeight(dib) - cinfo.next_scanline - 1);
					FreeImage_ConvertLine8To24(target, source, cinfo.image_width, palette);

#ifndef FREEIMAGE_BIGENDIAN
					// swap R and B channels
					BYTE *target_p = target;
					for(unsigned x = 0; x < cinfo.image_width; x++) {
						INPLACESWAP(target_p[0], target_p[2]);
						target_p += 3;
					}
#endif


					jpeg_write_scanlines(&cinfo, &target, 1);
				}

				free(target);
			}
			else if(color_type == FIC_MINISWHITE) {
				// reverse 8-bit greyscale image, so reverse grey value on the fly
				unsigned i;
				BYTE reverse[256];
				BYTE *target = (BYTE *)malloc(cinfo.image_width);
				if (target == NULL)
					throw "no memory to allocate intermediate scanline buffer";

				for(i = 0; i < 256; i++) {
					reverse[i] = (BYTE)(255 - i);
				}

				while(cinfo.next_scanline < cinfo.image_height) {
					BYTE *source = FreeImage_GetScanLine(dib, FreeImage_GetHeight(dib) - cinfo.next_scanline - 1);
					for(i = 0; i < cinfo.image_width; i++) {
						target[i] = reverse[ source[i] ];
					}
					jpeg_write_scanlines(&cinfo, &target, 1);
				}

				free(target);
			}

			// Step 8: Finish compression 

			jpeg_finish_compress(&cinfo);

			// Step 9: release JPEG compression object 

			jpeg_destroy_compress(&cinfo);

			return TRUE;

		} catch (const char *text) {
			FreeImage_OutputMessageProc(s_format_id, text);
			return FALSE;
		} catch (FREE_IMAGE_FORMAT) {
			return FALSE;
		}
	}

	return FALSE;
}

// ==========================================================
//   Init
// ==========================================================

void DLL_CALLCONV
InitJPEG(Plugin *plugin, int format_id) {
	s_format_id = format_id;

	plugin->format_proc = Format;
	plugin->description_proc = Description;
	plugin->extension_proc = Extension;
	plugin->regexpr_proc = RegExpr;
	plugin->open_proc = NULL;
	plugin->close_proc = NULL;
	plugin->pagecount_proc = NULL;
	plugin->pagecapability_proc = NULL;
	plugin->load_proc = Load;
	plugin->save_proc = Save;
	plugin->validate_proc = Validate;
	plugin->mime_proc = MimeType;
	plugin->supports_export_bpp_proc = SupportsExportDepth;
	plugin->supports_export_type_proc = SupportsExportType;
	plugin->supports_icc_profiles_proc = SupportsICCProfiles;
}
