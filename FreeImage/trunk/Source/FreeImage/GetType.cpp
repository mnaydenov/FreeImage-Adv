// ==========================================================
// GetType
//
// Design and implementation by
// - Floris van den Berg (flvdberg@wxs.nl)
// - Herve Drolon (drolon@infonie.fr)
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

#include "FreeImageIO.h"
#include "Utilities.h"
#include "FreeImageThreads.h"
#include "Plugin.h"
#include "FreeImage.h"

// ----------------------------------------------------------

FREE_IMAGE_FORMAT DLL_CALLCONV
FreeImage_GetFileTypeFromHandle(FreeImageIO *io, fi_handle handle, int size) {
	FREE_IMAGE_FORMAT returned_fif = FIF_UNKNOWN;

	if (handle != NULL) {
		PluginList * list = FreeImage_GetPluginList();

		// get exclusive access
		list->lock();

		const MRUList& mruList = list->getMRUList();
		const size_t mruListSize = mruList.size();

		// scan the plugins from the Most Recently Used to the Least Recently Used
		for (size_t k = 0; k < mruListSize; k++) {
			FREE_IMAGE_FORMAT fif = mruList[k].fif;
			if (FreeImage_Validate(fif, io, handle)) {
				if (fif == FIF_TIFF) {
					// many camera raw files use a TIFF signature ...
					// ... try to revalidate against FIF_RAW (even if it breaks the code genericity)
					if (FreeImage_Validate(FIF_RAW, io, handle)) {
						fif = FIF_RAW;
					}
				}

				// update the MRU list
				list->updateMRUList(fif);

				returned_fif = fif;

				break;
			}
		}

		// release exclusive access
		list->unlock();
	}

	return returned_fif;
}

FREE_IMAGE_FORMAT DLL_CALLCONV
FreeImage_GetFileType(const char *filename, int size) {
	FreeImageIO io;
	SetDefaultIO(&io);
	
	FILE *handle = fopen(filename, "rb");

	if (handle != NULL) {
		FREE_IMAGE_FORMAT format = FreeImage_GetFileTypeFromHandle(&io, (fi_handle)handle, size);

		fclose(handle);

		return format;
	}

	return FIF_UNKNOWN;
}

FREE_IMAGE_FORMAT DLL_CALLCONV 
FreeImage_GetFileTypeU(const wchar_t *filename, int size) {
#ifdef _WIN32	
	FreeImageIO io;
	SetDefaultIO(&io);
	FILE *handle = _wfopen(filename, L"rb");

	if (handle != NULL) {
		FREE_IMAGE_FORMAT format = FreeImage_GetFileTypeFromHandle(&io, (fi_handle)handle, size);

		fclose(handle);

		return format;
	}
#endif
	return FIF_UNKNOWN;
}

