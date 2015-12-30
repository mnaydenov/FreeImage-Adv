// ==========================================================
// FreeImage Plugin Interface
//
// Design and implementation by
// - Floris van den Berg (flvdberg@wxs.nl)
// - Rui Lopes (ruiglopes@yahoo.com)
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

#ifndef FREEIMAGE_PLUGIN_H
#define FREEIMAGE_PLUGIN_H

#ifndef FREEIMAGE_THREADS_H
#error "FreeImageThreads.h must be included first"
#endif

/**
Plugin Node.
This class is used to store all information about a plugin.
*/
class PluginNode {
public:
	/** FREE_IMAGE_FORMAT attached to this plugin */
	int id;
	/** Handle to a user plugin DLL (NULL for standard plugins) */
	void *instance;
	/** The actual plugin, holding the function pointers */
	Plugin *plugin;
	/** Enable/Disable switch */
	bool isEnabled;

	/** Unique format string for the plugin */
	const char *format;
	/** Description string for the plugin */
	const char *description;
	/** Comma separated list of file extensions indicating what files this plugin can open */
	const char *extension;
	/** optional regular expression to help	software identifying a bitmap type */
	const char *regexpr;

public:
	PluginNode() : id(0), instance(NULL), plugin(NULL), isEnabled(false), format(NULL), description(NULL), extension(NULL), regexpr(NULL) {
	}
	~PluginNode() {
	}
};

// --------------------------------------------------------------------------

/**
helper used to keep a Most Recently Used list of plugins
*/
typedef struct tagFIFItem {
	FREE_IMAGE_FORMAT fif;	//! FreeImage format
	size_t weight;			//! weight used to measure how many times this FreeImage format was found
} FIFItem;

/**
helper used to define a Most Recently Used list of FIF plugins
*/
typedef std::vector<FIFItem> MRUList;

/**
Internal Plugin List.
This class is used to manage all FIF plugins.
It is declared as static inside Plugin.cpp and 
initialized / destroyed using FreeImage_Initialise / FreeImage_DeInitialise functions.

The class uses a MRU list (impklemented as a priority queue) to dynamically sort plugins from the most recently used to the least recently used. 
This way, when using FreeImage_GetFileTypeFromHandle to scan the signature of a file, there are a lot of chances
that the right plugin is quickly found, whatever the order plugins were registered inside FreeImage_Initialise.
*/
class PluginList {
public:
	//! helper for map<FREE_IMAGE_FORMAT, PluginNode*>
	typedef std::map<int, PluginNode*> PluginMap;
	//! iterator helper for map<FREE_IMAGE_FORMAT, PluginNode*>
	typedef std::map<int, PluginNode*>::iterator PluginMapIterator;

private:
	//! list of FIF plugins
	PluginMap _plugin_map;

	//! priority queue of Most Recently Used FIF plugins
	MRUList _mru_list;

	//! prevent concurrent access to the plugin list
	FreeImage::Mutex _mutex;

public:
	//! Default constructor
	PluginList();

	//! Destructor
	~PluginList();

	//! lock access to the plugin list
	void lock();

	//! unlock access to the plugin list
	void unlock();

	/**
	Add a new FIF to the library
	@param proc Plugin Init procedure 
	@param instance Plugin instance (for external loaded libraries)
	@param format FreeImage format
	@param description FreeImage description
	@param extension FreeImage extension
	@param regexpr FreeImage regexp
	@return Returns a new FIF to be added to the list of supported FIF
	*/
	FREE_IMAGE_FORMAT addNode(FI_InitProc proc, void *instance = NULL, const char *format = 0, const char *description = 0, const char *extension = 0, const char *regexpr = 0);
	/**
	@param format A filename
	@return Returns the corresponding FIF plugin
	*/
	PluginNode *findNodeFromFormat(const char *format);
	/**
	@param mime A MIME type
	@return Returns the corresponding FIF plugin
	*/
	PluginNode *findNodeFromMime(const char *mime);
	/**
	@param node_id A FREE_IMAGE_FORMAT id
	@return Returns the corresponding FIF plugin
	*/
	PluginNode *findNodeFromFIF(int node_id);

	/**
	@return Returns the number of registered plugins
	*/
	size_t size() const;
	/**
	@return Returns true if no plugin is available
	*/
	bool isEmpty() const;

	/**
	Update a FREE_IMAGE_FORMAT in the MRU list
	@param fif Registered FREE_IMAGE_FORMAT
	@see FreeImage_GetFileTypeFromHandle
	*/
	void updateMRUList(FREE_IMAGE_FORMAT fif);

	/**
	@return Returns a refence to the MRU list
	*/
	MRUList& getMRUList();
};

// ==========================================================
//   Plugin Initialisation Callback
// ==========================================================

void DLL_CALLCONV FreeImage_OutputMessage(int fif, const char *message, ...);

// =====================================================================
// Reimplementation of stricmp (it is not supported on some systems)
// =====================================================================

int FreeImage_stricmp(const char *s1, const char *s2);

// ==========================================================
//   Internal functions
// ==========================================================

extern "C" {
BOOL DLL_CALLCONV FreeImage_Validate(FREE_IMAGE_FORMAT fif, FreeImageIO *io, fi_handle handle);
void * DLL_CALLCONV FreeImage_Open(const PluginNode *node, FreeImageIO *io, fi_handle handle, BOOL open_for_reading);
void DLL_CALLCONV FreeImage_Close(const PluginNode *node, FreeImageIO *io, fi_handle handle, void *data);
PluginList * DLL_CALLCONV FreeImage_GetPluginList();
}

// ==========================================================
//   Internal plugins
// ==========================================================

void DLL_CALLCONV InitBMP(Plugin *plugin, int format_id);
void DLL_CALLCONV InitCUT(Plugin *plugin, int format_id);
void DLL_CALLCONV InitICO(Plugin *plugin, int format_id);
void DLL_CALLCONV InitIFF(Plugin *plugin, int format_id);
void DLL_CALLCONV InitJPEG(Plugin *plugin, int format_id);
void DLL_CALLCONV InitKOALA(Plugin *plugin, int format_id);
void DLL_CALLCONV InitLBM(Plugin *plugin, int format_id);
void DLL_CALLCONV InitMNG(Plugin *plugin, int format_id);
void DLL_CALLCONV InitPCD(Plugin *plugin, int format_id);
void DLL_CALLCONV InitPCX(Plugin *plugin, int format_id);
void DLL_CALLCONV InitPNG(Plugin *plugin, int format_id);
void DLL_CALLCONV InitPNM(Plugin *plugin, int format_id);
void DLL_CALLCONV InitPSD(Plugin *plugin, int format_id);
void DLL_CALLCONV InitRAS(Plugin *plugin, int format_id);
void DLL_CALLCONV InitTARGA(Plugin *plugin, int format_id);
void DLL_CALLCONV InitTIFF(Plugin *plugin, int format_id);
void DLL_CALLCONV InitWBMP(Plugin *plugin, int format_id);
void DLL_CALLCONV InitXBM(Plugin *plugin, int format_id);
void DLL_CALLCONV InitXPM(Plugin *plugin, int format_id);
void DLL_CALLCONV InitDDS(Plugin *plugin, int format_id);
void DLL_CALLCONV InitGIF(Plugin *plugin, int format_id);
void DLL_CALLCONV InitHDR(Plugin *plugin, int format_id);
void DLL_CALLCONV InitG3(Plugin *plugin, int format_id);
void DLL_CALLCONV InitSGI(Plugin *plugin, int format_id);
void DLL_CALLCONV InitEXR(Plugin *plugin, int format_id);
void DLL_CALLCONV InitJ2K(Plugin *plugin, int format_id);
void DLL_CALLCONV InitJP2(Plugin *plugin, int format_id);
void DLL_CALLCONV InitPFM(Plugin *plugin, int format_id);
void DLL_CALLCONV InitPICT(Plugin *plugin, int format_id);
void DLL_CALLCONV InitRAW(Plugin *plugin, int format_id);
void DLL_CALLCONV InitJNG(Plugin *plugin, int format_id);
void DLL_CALLCONV InitWEBP(Plugin *plugin, int format_id);
void DLL_CALLCONV InitJXR(Plugin *plugin, int format_id);

#endif // FREEIMAGE_PLUGIN_H
