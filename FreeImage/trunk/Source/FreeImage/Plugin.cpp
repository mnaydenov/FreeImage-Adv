// =====================================================================
// FreeImage Plugin Interface
//
// Design and implementation by
// - Floris van den Berg (floris@geekhq.nl)
// - Rui Lopes (ruiglopes@yahoo.com)
// - Detlev Vendt (detlev.vendt@brillit.de)
// - Petr Pytelka (pyta@lightcomp.com)
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
// =====================================================================

#ifdef _MSC_VER 
#pragma warning (disable : 4786) // identifier was truncated to 'number' characters
#endif

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <ctype.h>
#endif // _WIN32

#include "FreeImage.h"
#include "Utilities.h"
#include "FreeImageIO.h"
#include "FreeImageThreads.h"
#include "Plugin.h"

#include "../Metadata/FreeImageTag.h"

// =====================================================================

using namespace std;

// =====================================================================
// Plugin search list
// =====================================================================

//! Where to look for external plugins
const char * s_search_list[] = {
	"",
	"plugins\\",
};

static int s_search_list_size = sizeof(s_search_list) / sizeof(char *);

//! Internal plugin list
static PluginList *s_plugins = NULL;

//! Internal reference counter
static int s_plugin_reference_count = 0;


// =====================================================================
// Reimplementation of stricmp (it is not supported on some systems)
// =====================================================================

int
FreeImage_stricmp(const char *s1, const char *s2) {
	int c1, c2;

	do {
		c1 = tolower(*s1++);
		c2 = tolower(*s2++);
	} while (c1 && c1 == c2);

	return c1 - c2;
}

// =====================================================================
//  Implementation of PluginList
// =====================================================================

PluginList::PluginList() {
}

void PluginList::lock() {
	_mutex.lock();
}

void PluginList::unlock() {
	_mutex.unlock();
}

FREE_IMAGE_FORMAT
PluginList::addNode(FI_InitProc init_proc, void *instance, const char *format, const char *description, const char *extension, const char *regexpr) {
	PluginNode *node = NULL;
	Plugin *plugin = NULL;

	// prevent concurrent access
	FreeImage::ScopedMutex scopedMutex(_mutex);

	try {
		if(init_proc == NULL) {
			throw "Invalid Init plugin procedure";
		}
		// the new FIF to be handled
		node = new(std::nothrow) PluginNode;
		// the way to load this FIF
		plugin = new(std::nothrow) Plugin;

		if(!node || !plugin) {
			throw FI_MSG_ERROR_MEMORY;
		}

		memset(plugin, 0, sizeof(Plugin));

		// fill-in the plugin structure
		// note we have memset to 0, so all unset pointers should be NULL

		init_proc(plugin, (int)_plugin_map.size());

		// get the format string (two possible ways)

		const char *the_format = NULL;

		if (format != NULL) {
			the_format = format;
		} else if (NULL != plugin->format_proc) {
			the_format = plugin->format_proc();
		}

		// add the node if it wasn't there already

		if (the_format != NULL) {
			node->id = (int)_plugin_map.size();
			node->instance = instance;
			node->plugin = plugin;
			node->format = the_format;
			node->description = description;
			node->extension = extension;
			node->regexpr = regexpr;
			node->isEnabled = true;

			_plugin_map[node->id] = node;

			// update the MRU list
			{
				FIFItem item;
				item.fif = (FREE_IMAGE_FORMAT)node->id;
				item.weight = 0;
				_mru_list.push_back(item);
			}

			return (FREE_IMAGE_FORMAT)node->id;
		}

		// something went wrong while allocating the plugin... cleanup
		throw FI_MSG_ERROR_MEMORY;

	} catch(const char *text) {
		delete plugin;
		delete node;

		if(text != NULL) {
			FreeImage_OutputMessageProc(FIF_UNKNOWN, text);
		} else {
			FreeImage_OutputMessageProc(FIF_UNKNOWN, FI_MSG_ERROR_MEMORY);
		}
		return FIF_UNKNOWN;
	}
}

PluginNode *
PluginList::findNodeFromFormat(const char *format) {
	for (PluginMapIterator i = _plugin_map.begin(); i != _plugin_map.end(); ++i) {
		PluginNode *node = (*i).second;
		if (node && node->isEnabled) {
			const char *the_format = (NULL != node->format) ? node->format : node->plugin->format_proc();
			if (FreeImage_stricmp(the_format, format) == 0) {
				return node;
			}
		}
	}

	return NULL;
}

PluginNode *
PluginList::findNodeFromMime(const char *mime) {
	for (PluginMapIterator i = _plugin_map.begin(); i != _plugin_map.end(); ++i) {
		PluginNode *node = (*i).second;
		if (node && node->isEnabled) {
			const char *the_mime = (NULL != node->plugin->mime_proc) ? node->plugin->mime_proc() : "";
			if ((the_mime != NULL) && (strcmp(the_mime, mime) == 0)) {
				return node;
			}
		}
	}

	return NULL;
}

PluginNode *
PluginList::findNodeFromFIF(int node_id) {
	PluginMapIterator i = _plugin_map.find(node_id);

	if (i != _plugin_map.end()) {
		PluginNode *node = (*i).second;
		return node;
	}

	return NULL;
}

size_t
PluginList::size() const {
	return _plugin_map.size();
}

bool
PluginList::isEmpty() const {
	return _plugin_map.empty();
}

PluginList::~PluginList() {
	for (PluginMapIterator i = _plugin_map.begin(); i != _plugin_map.end(); ++i) {
		PluginNode *node = (*i).second;
#ifdef _WIN32
		if (node->instance != NULL) {
			FreeLibrary((HINSTANCE)node->instance);
		}
#endif
		free(node->plugin);
		delete node;
	}
}

void 
PluginList::updateMRUList(FREE_IMAGE_FORMAT fif) {
	size_t index = 0;
	bool bFound = false;
	
	const size_t mruListSize = _mru_list.size();

	// get the FIF index
	for (size_t k = 0; k < mruListSize; k++) {
		if (_mru_list[k].fif == fif) {
			index = k;
			bFound = true;
			break;
		}
	}

	if (bFound) {
		// update the FIF weight
		_mru_list[index].weight += 1;

		// comparison with the members of the queue, slide larger values up
		for (size_t k = 0; k < index; k++) {
			// compare weights
			if (_mru_list[k].weight < _mru_list[index].weight) {
				// swap items
				FIFItem item = _mru_list[k];
				_mru_list[k] = _mru_list[index];
				_mru_list[index] = item;
				break;
			}
		}
	}
}

MRUList& 
PluginList::getMRUList() {
	return _mru_list;
}

// =====================================================================
// Retrieve a pointer to the plugin list container
// =====================================================================

PluginList * DLL_CALLCONV
FreeImage_GetPluginList() {
	return s_plugins;
}

// =====================================================================
// Plugin System Initialization
// =====================================================================

void DLL_CALLCONV
FreeImage_Initialise(BOOL load_local_plugins_only) {

	if (s_plugin_reference_count++ == 0) {
		
		/*
		Note: initialize all singletons here 
		in order to avoid race conditions with multi-threading
		*/

		// initialise the TagLib singleton
		TagLib& s = TagLib::instance();

		// internal plugin initialization

		s_plugins = new(std::nothrow) PluginList;

		if (s_plugins) {
			/* NOTE : 
			The order used to initialize internal plugins below MUST BE the same order 
			as the one used to define the FREE_IMAGE_FORMAT enum. 
			*/
			s_plugins->addNode(InitBMP);
			s_plugins->addNode(InitICO);
			s_plugins->addNode(InitJPEG);
			s_plugins->addNode(InitJNG);
			s_plugins->addNode(InitKOALA);
			s_plugins->addNode(InitIFF);
			s_plugins->addNode(InitMNG);
			s_plugins->addNode(InitPNM, NULL, "PBM", "Portable Bitmap (ASCII)", "pbm", "^P1");
			s_plugins->addNode(InitPNM, NULL, "PBMRAW", "Portable Bitmap (RAW)", "pbm", "^P4");
			s_plugins->addNode(InitPCD);
			s_plugins->addNode(InitPCX);
			s_plugins->addNode(InitPNM, NULL, "PGM", "Portable Greymap (ASCII)", "pgm", "^P2");
			s_plugins->addNode(InitPNM, NULL, "PGMRAW", "Portable Greymap (RAW)", "pgm", "^P5");
			s_plugins->addNode(InitPNG);
			s_plugins->addNode(InitPNM, NULL, "PPM", "Portable Pixelmap (ASCII)", "ppm", "^P3");
			s_plugins->addNode(InitPNM, NULL, "PPMRAW", "Portable Pixelmap (RAW)", "ppm", "^P6");
			s_plugins->addNode(InitRAS);
			s_plugins->addNode(InitTARGA);
			s_plugins->addNode(InitTIFF);
			s_plugins->addNode(InitWBMP);
			s_plugins->addNode(InitPSD);
			s_plugins->addNode(InitCUT);
			s_plugins->addNode(InitXBM);
			s_plugins->addNode(InitXPM);
			s_plugins->addNode(InitDDS);
	        s_plugins->addNode(InitGIF);
	        s_plugins->addNode(InitHDR);
			s_plugins->addNode(InitG3);
			s_plugins->addNode(InitSGI);
			s_plugins->addNode(InitEXR);
			s_plugins->addNode(InitJ2K);
			s_plugins->addNode(InitJP2);
			s_plugins->addNode(InitPFM);
			s_plugins->addNode(InitPICT);
			s_plugins->addNode(InitRAW);
			s_plugins->addNode(InitWEBP);
			s_plugins->addNode(InitJXR);	// unsupported by MS Visual Studio 2003 !!!
			
			// external plugin initialization

#ifdef _WIN32
			if (!load_local_plugins_only) {
				int count = 0;
				char buffer[MAX_PATH + 200];
				wchar_t current_dir[2 * _MAX_PATH], module[2 * _MAX_PATH];
				BOOL bOk = FALSE;

				// store the current directory. then set the directory to the application location

				if (GetCurrentDirectoryW(2 * _MAX_PATH, current_dir) != 0) {
					if (GetModuleFileNameW(NULL, module, 2 * _MAX_PATH) != 0) {
						wchar_t *last_point = wcsrchr(module, L'\\');

						if (last_point) {
							*last_point = L'\0';

							bOk = SetCurrentDirectoryW(module);
						}
					}
				}

				// search for plugins

				while (count < s_search_list_size) {
					_finddata_t find_data;
					long find_handle;

					strcpy(buffer, s_search_list[count]);
					strcat(buffer, "*.fip");

					if ((find_handle = (long)_findfirst(buffer, &find_data)) != -1L) {
						do {
							strcpy(buffer, s_search_list[count]);
							strncat(buffer, find_data.name, MAX_PATH + 200);

							HINSTANCE instance = LoadLibrary(buffer);

							if (instance != NULL) {
								FARPROC proc_address = GetProcAddress(instance, "_Init@8");

								if (proc_address != NULL) {
									s_plugins->addNode((FI_InitProc)proc_address, (void *)instance);
								} else {
									FreeLibrary(instance);
								}
							}
						} while (_findnext(find_handle, &find_data) != -1L);

						_findclose(find_handle);
					}

					count++;
				}

				// restore the current directory

				if (bOk) {
					SetCurrentDirectoryW(current_dir);
				}
			}
#endif // _WIN32
		}
	}
}

void DLL_CALLCONV
FreeImage_DeInitialise() {
	--s_plugin_reference_count;

	if (s_plugin_reference_count == 0) {
		delete s_plugins;
	}
}

// =====================================================================
// Open and close a bitmap
// =====================================================================

void * DLL_CALLCONV
FreeImage_Open(const PluginNode *node, FreeImageIO *io, fi_handle handle, BOOL open_for_reading) {
	if(NULL != node->plugin->open_proc) {
       return node->plugin->open_proc(io, handle, open_for_reading);
	}

	return NULL;
}

void DLL_CALLCONV
FreeImage_Close(const PluginNode *node, FreeImageIO *io, fi_handle handle, void *data) {
	if(NULL != node->plugin->close_proc) {
		node->plugin->close_proc(io, handle, data);
	}
}

// =====================================================================
// Plugin System Load/Save Functions
// =====================================================================

FIBITMAP * DLL_CALLCONV
FreeImage_LoadFromHandle(FREE_IMAGE_FORMAT fif, FreeImageIO *io, fi_handle handle, int flags) {
	if ((fif >= 0) && (fif < FreeImage_GetFIFCount())) {
		const PluginNode *node = s_plugins->findNodeFromFIF(fif);
		
		if (node != NULL) {
			if(NULL != node->plugin->load_proc) {
				void *data = FreeImage_Open(node, io, handle, TRUE);
					
				FIBITMAP *bitmap = node->plugin->load_proc(io, handle, -1, flags, data);
					
				FreeImage_Close(node, io, handle, data);
					
				return bitmap;
			}
		}
	}

	return NULL;
}

FIBITMAP * DLL_CALLCONV
FreeImage_Load(FREE_IMAGE_FORMAT fif, const char *filename, int flags) {
	FreeImageIO io;
	SetDefaultIO(&io);
	
	FILE *handle = fopen(filename, "rb");

	if (handle) {
		FIBITMAP *bitmap = FreeImage_LoadFromHandle(fif, &io, (fi_handle)handle, flags);

		fclose(handle);

		return bitmap;
	} else {
		FreeImage_OutputMessageProc((int)fif, "FreeImage_Load: failed to open file %s", filename);
	}

	return NULL;
}

FIBITMAP * DLL_CALLCONV
FreeImage_LoadU(FREE_IMAGE_FORMAT fif, const wchar_t *filename, int flags) {
	FreeImageIO io;
	SetDefaultIO(&io);
#ifdef _WIN32	
	FILE *handle = _wfopen(filename, L"rb");

	if (handle) {
		FIBITMAP *bitmap = FreeImage_LoadFromHandle(fif, &io, (fi_handle)handle, flags);

		fclose(handle);

		return bitmap;
	} else {
		FreeImage_OutputMessageProc((int)fif, "FreeImage_LoadU: failed to open input file");
	}
#endif
	return NULL;
}

BOOL DLL_CALLCONV
FreeImage_SaveToHandle(FREE_IMAGE_FORMAT fif, FIBITMAP *dib, FreeImageIO *io, fi_handle handle, int flags) {
	// cannot save "header only" formats
	if(FreeImage_HasPixels(dib) == FALSE) {
		FreeImage_OutputMessageProc((int)fif, "FreeImage_SaveToHandle: cannot save \"header only\" formats");
		return FALSE;
	}

	if ((fif >= 0) && (fif < FreeImage_GetFIFCount())) {
		const PluginNode *node = s_plugins->findNodeFromFIF(fif);
		
		if (node) {
			if(NULL != node->plugin->save_proc) {
				void *data = FreeImage_Open(node, io, handle, FALSE);
					
				BOOL result = node->plugin->save_proc(io, dib, handle, -1, flags, data);
					
				FreeImage_Close(node, io, handle, data);
					
				return result;
			}
		}
	}

	return FALSE;
}


BOOL DLL_CALLCONV
FreeImage_Save(FREE_IMAGE_FORMAT fif, FIBITMAP *dib, const char *filename, int flags) {
	FreeImageIO io;
	SetDefaultIO(&io);
	
	FILE *handle = fopen(filename, "w+b");
	
	if (handle) {
		BOOL success = FreeImage_SaveToHandle(fif, dib, &io, (fi_handle)handle, flags);

		fclose(handle);

		return success;
	} else {
		FreeImage_OutputMessageProc((int)fif, "FreeImage_Save: failed to open file %s", filename);
	}

	return FALSE;
}

BOOL DLL_CALLCONV
FreeImage_SaveU(FREE_IMAGE_FORMAT fif, FIBITMAP *dib, const wchar_t *filename, int flags) {
	FreeImageIO io;
	SetDefaultIO(&io);
#ifdef _WIN32	
	FILE *handle = _wfopen(filename, L"w+b");
	
	if (handle) {
		BOOL success = FreeImage_SaveToHandle(fif, dib, &io, (fi_handle)handle, flags);

		fclose(handle);

		return success;
	} else {
		FreeImage_OutputMessageProc((int)fif, "FreeImage_SaveU: failed to open output file");
	}
#endif
	return FALSE;
}

// =====================================================================
// Plugin construction + enable/disable functions
// =====================================================================

FREE_IMAGE_FORMAT DLL_CALLCONV
FreeImage_RegisterLocalPlugin(FI_InitProc proc_address, const char *format, const char *description, const char *extension, const char *regexpr) {
	return s_plugins->addNode(proc_address, NULL, format, description, extension, regexpr);
}

#ifdef _WIN32
FREE_IMAGE_FORMAT DLL_CALLCONV
FreeImage_RegisterExternalPlugin(const char *path, const char *format, const char *description, const char *extension, const char *regexpr) {
	if (path != NULL) {
		HINSTANCE instance = LoadLibrary(path);

		if (instance != NULL) {
			FARPROC proc_address = GetProcAddress(instance, "_Init@8");

			FREE_IMAGE_FORMAT result = s_plugins->addNode((FI_InitProc)proc_address, (void *)instance, format, description, extension, regexpr);

			if (result == FIF_UNKNOWN)
				FreeLibrary(instance);

			return result;
		}
	}

	return FIF_UNKNOWN;
}
#endif // _WIN32

int DLL_CALLCONV
FreeImage_SetPluginEnabled(FREE_IMAGE_FORMAT fif, BOOL enable) {
	if (s_plugins != NULL) {
		PluginNode *node = s_plugins->findNodeFromFIF(fif);

		if (node != NULL) {
			BOOL previous_state = node->isEnabled;

			node->isEnabled = enable ? true : false;

			return previous_state;
		}
	}

	return -1;
}

int DLL_CALLCONV
FreeImage_IsPluginEnabled(FREE_IMAGE_FORMAT fif) {
	if (s_plugins != NULL) {
		PluginNode *node = s_plugins->findNodeFromFIF(fif);

		return (node != NULL) ? node->isEnabled : FALSE;
	}
	
	return -1;
}

// =====================================================================
// Plugin Access Functions
// =====================================================================

int DLL_CALLCONV
FreeImage_GetFIFCount() {
	return (s_plugins != NULL) ? (int)s_plugins->size() : 0;
}

FREE_IMAGE_FORMAT DLL_CALLCONV
FreeImage_GetFIFFromFormat(const char *format) {
	if (s_plugins != NULL) {
		const PluginNode *node = s_plugins->findNodeFromFormat(format);
		return (node != NULL) ? (FREE_IMAGE_FORMAT)node->id : FIF_UNKNOWN;
	}

	return FIF_UNKNOWN;
}

FREE_IMAGE_FORMAT DLL_CALLCONV
FreeImage_GetFIFFromMime(const char *mime) {
	if (s_plugins != NULL) {
		const PluginNode *node = s_plugins->findNodeFromMime(mime);
		return (node != NULL) ? (FREE_IMAGE_FORMAT)node->id : FIF_UNKNOWN;
	}

	return FIF_UNKNOWN;
}

const char * DLL_CALLCONV
FreeImage_GetFormatFromFIF(FREE_IMAGE_FORMAT fif) {
	if (s_plugins != NULL) {
		PluginNode *node = s_plugins->findNodeFromFIF(fif);
		return (node != NULL) ? (node->format != NULL) ? node->format : node->plugin->format_proc() : NULL;
	}
	return NULL;
}

const char * DLL_CALLCONV 
FreeImage_GetFIFMimeType(FREE_IMAGE_FORMAT fif) {
	if (s_plugins != NULL) {
		PluginNode *node = s_plugins->findNodeFromFIF(fif);
		return (node != NULL) ? (node->plugin != NULL) ? ( node->plugin->mime_proc != NULL )? node->plugin->mime_proc() : NULL : NULL : NULL;
	}
	return NULL;
}

const char * DLL_CALLCONV
FreeImage_GetFIFExtensionList(FREE_IMAGE_FORMAT fif) {
	if (s_plugins != NULL) {
		PluginNode *node = s_plugins->findNodeFromFIF(fif);

		return (node != NULL) ? (node->extension != NULL) ? node->extension : (node->plugin->extension_proc != NULL) ? node->plugin->extension_proc() : NULL : NULL;
	}

	return NULL;
}

const char * DLL_CALLCONV
FreeImage_GetFIFDescription(FREE_IMAGE_FORMAT fif) {
	if (s_plugins != NULL) {
		PluginNode *node = s_plugins->findNodeFromFIF(fif);

		return (node != NULL) ? (node->description != NULL) ? node->description : (node->plugin->description_proc != NULL) ? node->plugin->description_proc() : NULL : NULL;
	}

	return NULL;
}

const char * DLL_CALLCONV
FreeImage_GetFIFRegExpr(FREE_IMAGE_FORMAT fif) {
	if (s_plugins != NULL) {
		PluginNode *node = s_plugins->findNodeFromFIF(fif);

		return (node != NULL) ? (node->regexpr != NULL) ? node->regexpr : (node->plugin->regexpr_proc != NULL) ? node->plugin->regexpr_proc() : NULL : NULL;
	}

	return NULL;
}

BOOL DLL_CALLCONV
FreeImage_FIFSupportsReading(FREE_IMAGE_FORMAT fif) {
	if (s_plugins != NULL) {
		PluginNode *node = s_plugins->findNodeFromFIF(fif);

		return (node != NULL) ? node->plugin->load_proc != NULL : FALSE;
	}

	return FALSE;
}

BOOL DLL_CALLCONV
FreeImage_FIFSupportsWriting(FREE_IMAGE_FORMAT fif) {
	if (s_plugins != NULL) {
		PluginNode *node = s_plugins->findNodeFromFIF(fif);

		return (node != NULL) ? node->plugin->save_proc != NULL : FALSE ;
	}

	return FALSE;
}

BOOL DLL_CALLCONV
FreeImage_FIFSupportsExportBPP(FREE_IMAGE_FORMAT fif, int depth) {
	if (s_plugins != NULL) {
		PluginNode *node = s_plugins->findNodeFromFIF(fif);

		return (node != NULL) ? 
			(node->plugin->supports_export_bpp_proc != NULL) ? 
				node->plugin->supports_export_bpp_proc(depth) : FALSE : FALSE;
	}

	return FALSE;
}

BOOL DLL_CALLCONV
FreeImage_FIFSupportsExportType(FREE_IMAGE_FORMAT fif, FREE_IMAGE_TYPE type) {
	if (s_plugins != NULL) {
		PluginNode *node = s_plugins->findNodeFromFIF(fif);

		return (node != NULL) ? 
			(node->plugin->supports_export_type_proc != NULL) ? 
				node->plugin->supports_export_type_proc(type) : FALSE : FALSE;
	}

	return FALSE;
}

BOOL DLL_CALLCONV
FreeImage_FIFSupportsICCProfiles(FREE_IMAGE_FORMAT fif) {
	if (s_plugins != NULL) {
		PluginNode *node = s_plugins->findNodeFromFIF(fif);

		return (node != NULL) ? 
			(node->plugin->supports_icc_profiles_proc != NULL) ? 
				node->plugin->supports_icc_profiles_proc() : FALSE : FALSE;
	}

	return FALSE;
}

BOOL DLL_CALLCONV
FreeImage_FIFSupportsNoPixels(FREE_IMAGE_FORMAT fif) {
	if (s_plugins != NULL) {
		PluginNode *node = s_plugins->findNodeFromFIF(fif);

		return (node != NULL) ? 
			(node->plugin->supports_no_pixels_proc != NULL) ? 
				node->plugin->supports_no_pixels_proc() : FALSE : FALSE;
	}

	return FALSE;
}

FREE_IMAGE_FORMAT DLL_CALLCONV
FreeImage_GetFIFFromFilename(const char *filename) {
	if (filename != NULL) {
		const char *extension;

		// get the proper extension if we received a filename

		char *place = strrchr((char *)filename, '.');	
		extension = (place != NULL) ? ++place : filename;

		// look for the extension in the plugin table

		for (int i = 0; i < FreeImage_GetFIFCount(); ++i) {

			if (s_plugins->findNodeFromFIF(i)->isEnabled) {

				// compare the format id with the extension

				if (FreeImage_stricmp(FreeImage_GetFormatFromFIF((FREE_IMAGE_FORMAT)i), extension) == 0) {
					return (FREE_IMAGE_FORMAT)i;
				} else {
					// make a copy of the extension list and split it

					char *copy = (char *)malloc(strlen(FreeImage_GetFIFExtensionList((FREE_IMAGE_FORMAT)i)) + 1);
					memset(copy, 0, strlen(FreeImage_GetFIFExtensionList((FREE_IMAGE_FORMAT)i)) + 1);
					memcpy(copy, FreeImage_GetFIFExtensionList((FREE_IMAGE_FORMAT)i), strlen(FreeImage_GetFIFExtensionList((FREE_IMAGE_FORMAT)i)));

					// get the first token

					char *token = strtok(copy, ",");

					while (token != NULL) {
						if (FreeImage_stricmp(token, extension) == 0) {
							free(copy);

								return (FREE_IMAGE_FORMAT)i;
						}

						token = strtok(NULL, ",");
					}

					// free the copy of the extension list

					free(copy);
				}	
			}
		}
	}

	return FIF_UNKNOWN;
}

FREE_IMAGE_FORMAT DLL_CALLCONV 
FreeImage_GetFIFFromFilenameU(const wchar_t *filename) {
#ifdef _WIN32	
	if (filename == NULL) return FIF_UNKNOWN;
    	
	// get the proper extension if we received a filename
	wchar_t *place = wcsrchr((wchar_t *)filename, '.');	
	if (place == NULL) return FIF_UNKNOWN;
	// convert to single character - no national chars in extensions
	char *extension = (char *)malloc(wcslen(place)+1);
	unsigned int i=0;
	for(; i < wcslen(place); i++) // convert 16-bit to 8-bit
		extension[i] = (char)(place[i] & 0x00FF);
	// set terminating 0
	extension[i]=0;
	FREE_IMAGE_FORMAT fRet = FreeImage_GetFIFFromFilename(extension);
	free(extension);

	return fRet;
#else
	return FIF_UNKNOWN;
#endif // _WIN32
}

BOOL DLL_CALLCONV
FreeImage_Validate(FREE_IMAGE_FORMAT fif, FreeImageIO *io, fi_handle handle) {
	if (s_plugins != NULL) {
		BOOL validated = FALSE;

		PluginNode *node = s_plugins->findNodeFromFIF(fif);

		if (node && node->isEnabled) {
			if (NULL != node->plugin->validate_proc) {
				long tell = io->tell_proc(handle);

				validated = node->plugin->validate_proc(io, handle);

				io->seek_proc(handle, tell, SEEK_SET);
			}
		}

		return validated;
	}

	return FALSE;
}
