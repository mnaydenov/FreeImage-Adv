// ==========================================================
// FreeImage Plugin Interface
//
// Design and implementation by
// - Floris van den Berg (flvdberg@wxs.nl)
// - Rui Lopes (ruiglopes@yahoo.com)
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

#include "FreeImage.h"
#include "Utilities.h"

/**
Plugin Node.
This structure is used to store all information about a plugin.
*/
typedef struct tagPluginNode {
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
} PluginNode;

/**
Internal Plugin List.
This class is used to manage all FIF plugins.
This class is declared as static inside the library initialization function. 
It is thus a singleton, internal to the library.
*/
class PluginList {
public:
	//! helper for map<FREE_IMAGE_FORMAT, PluginNode*>
	typedef std::map<int, PluginNode*> PluginMap;
	//! iterator helper for map<FREE_IMAGE_FORMAT, PluginNode*>
	typedef std::map<int, PluginNode*>::iterator PluginMapIterator;

private:
	//! list of FIF plugins
	PluginMap m_plugin_map;
	//! number of FIF plugins
	int m_node_count;

public:
	//! Default constructor
	PluginList();
	//! Destructor
	~PluginList();

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
void * DLL_CALLCONV FreeImage_Open(PluginNode *node, FreeImageIO *io, fi_handle handle, BOOL open_for_reading);
void DLL_CALLCONV FreeImage_Close(PluginNode *node, FreeImageIO *io, fi_handle handle, void *data);
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
