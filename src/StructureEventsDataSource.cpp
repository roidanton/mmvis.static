/**
 * StructureEventsDataSource.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "StructureEventsDataSource.h"
#include "mmcore/param/FilePathParam.h"
#include "StructureEventsDataCall.h"

using namespace megamol;
using namespace megamol::core;

/*
 * mmvis_static::StructureEventsDataSource::StructureEventsDataSource
 */
mmvis_static::StructureEventsDataSource::StructureEventsDataSource() : Module(),
	filename("filename", "The path to the MMSED file to load."),
	getDataSlot("getdata", "Slot to request data from this data source.") {

	this->filename.SetParameter(new param::FilePathParam(""));
	this->filename.SetUpdateCallback(&StructureEventsDataSource::filenameChanged);
	this->MakeSlotAvailable(&this->filename);

	this->getDataSlot.SetCallback("StructureEventsDataCall", "GetData", &StructureEventsDataSource::getDataCallback);
	this->getDataSlot.SetCallback("StructureEventsDataCall", "GetExtent", &StructureEventsDataSource::getExtentCallback);
	this->MakeSlotAvailable(&this->getDataSlot);
}

/*
 * mmvis_static::StructureEventsDataSource::~StructureEventsDataSource
 */
mmvis_static::StructureEventsDataSource::~StructureEventsDataSource(void) {
}

/*
 * mmvis_static::StructureEventsDataSource::create
 */
bool mmvis_static::StructureEventsDataSource::create(void) {
	// intentionally empty
	return true;
}


/*
 * mmvis_static::StructureEventsDataSource::release
 */
void mmvis_static::StructureEventsDataSource::release(void) {
}


/*
 * mmvis_static::StructureEventsDataSource::filenameChanged
 */
bool mmvis_static::StructureEventsDataSource::filenameChanged(param::ParamSlot& slot) {
	//TODO
	return true;
}


/*
 * mmvis_static::StructureEventsDataSource::getDataCallback
 */
bool mmvis_static::StructureEventsDataSource::getDataCallback(Call& caller) {
	StructureEventsDataCall *c = dynamic_cast<StructureEventsDataCall*>(&caller);
	if (c == NULL) return false;

	// TODO

	return true;
}


/*
 * mmvis_static::StructureEventsDataSource::getExtentCallback
 */
bool mmvis_static::StructureEventsDataSource::getExtentCallback(Call& caller) {
	StructureEventsDataCall *c = dynamic_cast<StructureEventsDataCall*>(&caller);
	if (c == NULL) return false;

	// TODO

	return true;
}