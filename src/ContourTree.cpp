#include "stdafx.h"
#include "ContourTree.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include "mmcore/param/IntParam.h"

using namespace megamol;

/*
 * mmvis_static::ContourTree::ContourTree
 */
mmvis_static::ContourTree::ContourTree()
	: stdplugin::datatools::AbstractParticleManipulator("outData", "indata") {
}


/*
 * mmvis_static::ContourTree::~ContourTree
 */
mmvis_static::ContourTree::~ContourTree(void) {
	this->Release();
}
