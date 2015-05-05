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
#include "vislib/sys/Log.h"
#include "vislib/sys/MemmappedFile.h"
#include "vislib/sys/SystemInformation.h"

using namespace megamol;
using namespace megamol::core;

/**
 * mmvis_static::StructureEventsDataSource::StructureEventsDataSource
 */
mmvis_static::StructureEventsDataSource::StructureEventsDataSource() : core::Module(),
//mmvis_static::StructureEventsDataSource::StructureEventsDataSource() : core::view::AnimDataModule(),
	filename("filename", "The path to the MMSED file to load."),
	getDataSlot("getdata", "Slot to request data from this data source."),
	file(NULL), bbox(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f),
	clipbox(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f) {

	this->filename.SetParameter(new param::FilePathParam(""));
	this->filename.SetUpdateCallback(&StructureEventsDataSource::filenameChanged);
	this->MakeSlotAvailable(&this->filename);

	this->getDataSlot.SetCallback("StructureEventsDataCall", "GetData", &StructureEventsDataSource::getDataCallback);
	this->getDataSlot.SetCallback("StructureEventsDataCall", "GetExtent", &StructureEventsDataSource::getExtentCallback);
	this->MakeSlotAvailable(&this->getDataSlot);
}

/**
 * mmvis_static::StructureEventsDataSource::~StructureEventsDataSource
 */
mmvis_static::StructureEventsDataSource::~StructureEventsDataSource(void) {
}


/**
 * mmvis_static::StructureEventsDataSource::create
 */
bool mmvis_static::StructureEventsDataSource::create(void) {
	// intentionally empty
	return true;
}


/**
 * mmvis_static::StructureEventsDataSource::release
 */
void mmvis_static::StructureEventsDataSource::release(void) {
}


/**
 * mmvis_static::StructureEventsDataSource::filenameChanged
 */
bool mmvis_static::StructureEventsDataSource::filenameChanged(param::ParamSlot& slot) {
	using vislib::sys::Log;
	using vislib::sys::File;

	// Check for existing file.
	if (this->file == NULL) {
		this->file = new vislib::sys::MemmappedFile();
	}
	else {
		this->file->Close();
	}
	ASSERT(this->filename.Param<param::FilePathParam>() != NULL);

	// Try to open file.
	if (!this->file->Open(this->filename.Param<param::FilePathParam>()->Value(), File::READ_ONLY, File::SHARE_READ, File::OPEN_ONLY)) {
		//this->GetCoreInstance()->Log().WriteMsg(Log::LEVEL_ERROR, "Unable to open MMSE-File \"%s\".", vislib::StringA(
		//	this->filename.Param<param::FilePathParam>()->Value()).PeekBuffer());

		SAFE_DELETE(this->file);
		//this->setFrameCount(1);
		//this->initFrameCache(1);

		return true;
	}

	// Error macros.

#define _ERROR_OUT(MSG) Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, MSG); \
        SAFE_DELETE(this->file); \
        this->bbox.Set(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f); \
        this->clipbox = this->bbox; \
        return true;
#define _ASSERT_READFILE(BUFFER, BUFFERSIZE) if (this->file->Read((BUFFER), (BUFFERSIZE)) != (BUFFERSIZE)) { \
        _ERROR_OUT("Unable to read MMSE file header"); \
	    }

	// Bounding box.
	float box[6];
	_ASSERT_READFILE(box, 4 * 6);
	this->bbox.Set(box[0], box[1], box[2], box[3], box[4], box[5]);
	_ASSERT_READFILE(box, 4 * 6);
	this->clipbox.Set(box[0], box[1], box[2], box[3], box[4], box[5]);

#undef _ASSERT_READFILE
#undef _ERROR_OUT

	return true;
}


/**
 * mmvis_static::StructureEventsDataSource::getDataCallback
 */
bool mmvis_static::StructureEventsDataSource::getDataCallback(Call& caller) {
	StructureEventsDataCall *call = dynamic_cast<StructureEventsDataCall*>(&caller);
	if (call == NULL) return false;

	// TODO
	if (call != NULL) {
		/*
		f = dynamic_cast<Frame *>(this->requestLockedFrame(c2->FrameID()));
		if (f == NULL) return false;
		call->SetUnlocker(new Unlocker(*f));
		call->SetFrameID(f->FrameNumber());
		call->SetDataHash(this->data_hash);
		f->SetData(*call);
		*/
	}

	return true;
}


/**
 * mmvis_static::StructureEventsDataSource::getExtentCallback
 */
bool mmvis_static::StructureEventsDataSource::getExtentCallback(Call& caller) {
	StructureEventsDataCall *call = dynamic_cast<StructureEventsDataCall*>(&caller);
	if (call == NULL) return false;

	// TODO
	if (call != NULL) {
		//call->SetFrameCount(this->FrameCount());
		call->AccessBoundingBoxes().Clear();
		call->AccessBoundingBoxes().SetObjectSpaceBBox(this->bbox);
		call->AccessBoundingBoxes().SetObjectSpaceClipBox(this->clipbox);
		//call->SetDataHash(this->data_hash);
		return true;
	}

	return false;
}