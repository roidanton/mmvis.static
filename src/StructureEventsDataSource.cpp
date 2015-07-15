/**
 * StructureEventsDataSource.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "mmcore/param/FilePathParam.h"
#include "StructureEventsDataSource.h"
#include "StructureEventsDataCall.h"
#include "vislib/sys/Log.h"
#include "vislib/sys/FastFile.h"
#include "vislib/sys/SystemInformation.h"

using namespace megamol;
using namespace megamol::core;

/**
 * mmvis_static::StructureEventsDataSource::StructureEventsDataSource
 */
mmvis_static::StructureEventsDataSource::StructureEventsDataSource(void) : core::Module(),
//mmvis_static::StructureEventsDataSource::StructureEventsDataSource(void) : core::view::AnimDataModule(),
	filename("filename", "The path to the MMSE file to load."),
	getDataSlot("getdata", "Slot to request data from this data source."),
	file(NULL), bbox(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f),
	clipbox(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f),
	sedcHash(0), headerSize(0), eventCount(0), maxTime(0), frameCount(1), reReadData(false) {

	this->filename.SetParameter(new param::FilePathParam("eventsFromMPDC.mmse"));
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
	this->bbox.Set(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f);
	this->clipbox = this->bbox;

	// Change hash to flag that sedc data has changed.
	this->sedcHash = this->sedcHash != 1 ? 1 : 2;

	///
	/// Check for existing file.
	///
	if (this->file == NULL) {
		this->file = new vislib::sys::FastFile();
	}
	else {
		this->file->Close();
	}
	ASSERT(this->filename.Param<param::FilePathParam>() != NULL);
	
	///
	/// Open file.
	///
	if (!this->file->Open(this->filename.Param<param::FilePathParam>()->Value(), File::READ_ONLY, File::SHARE_READ, File::OPEN_ONLY)) {
		Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, "Unable to open MMSE-File \"%s\".", vislib::StringA(
			this->filename.Param<param::FilePathParam>()->Value()).PeekBuffer());

		SAFE_DELETE(this->file);
		//this->setFrameCount(1);
		//this->initFrameCache(1);

		return true;
	}

	///
	/// Error macros.
	///
#define _ERROR_OUT(MSG) Log::DefaultLog.WriteMsg(Log::LEVEL_ERROR, MSG); \
        SAFE_DELETE(this->file); \
		this->frameCount = 1; \
        this->bbox.Set(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f); \
        this->clipbox = this->bbox; \
        return true;
#define _ASSERT_READFILE(BUFFER, BUFFERSIZE) if (this->file->Read((BUFFER), (BUFFERSIZE)) != (BUFFERSIZE)) { \
        _ERROR_OUT("Unable to read MMSE file header"); \
	    }
	
	this->headerSize = 0;

	///
	/// Check file type by id.
	///
	char magicid[4];
	_ASSERT_READFILE(magicid, 4);
	if (::memcmp(magicid, "MMSE", 4) != 0) { // with 4 it works, the two bytes afterwards are not NULL when 6, didnt found the error in writer, so changed to 4 in both
		_ERROR_OUT("MMSE file header id wrong");
	}
	this->headerSize += 4;

	///
	/// Get extends and mmse properties.
	/// file->Read() automatically moves the file pointer.
	///
	float box[6];
	_ASSERT_READFILE(box, 4 * 6);
	this->bbox.Set(box[0], box[1], box[2], box[3], box[4], box[5]);
	this->headerSize += 4 * 6;

	_ASSERT_READFILE(box, 4 * 6);
	this->clipbox.Set(box[0], box[1], box[2], box[3], box[4], box[5]);
	this->headerSize += 4 * 6;

	uint64_t count[1]; // Beautiful workaround.
	_ASSERT_READFILE(count, 8);
	this->eventCount = count[0];
	this->headerSize += 8;

	float time[1]; // Beautiful workaround.
	_ASSERT_READFILE(time, 4);
	this->maxTime = time[0];
	this->frameCount = static_cast<int>(time[0]) == 0 ? 1 : static_cast<int>(time[0]);
	this->headerSize += 4;

	/// Flag that the data has to be renewed.
	this->reReadData = true;

#undef _ASSERT_READFILE_EXTEND
#undef _ASSERT_READFILE
#undef _ERROR_OUT

	return true;
}


/**
 * mmvis_static::StructureEventsDataSource::getDataCallback
 */
bool mmvis_static::StructureEventsDataSource::getDataCallback(Call& caller) {
	StructureEventsDataCall *outSedc = dynamic_cast<StructureEventsDataCall*>(&caller);
	if (outSedc == NULL)
		return false;

	//call->SetUnlocker(new Unlocker());
	outSedc->SetDataHash(this->sedcHash);

	if (!this->reReadData) // Will cause erroneous data if file is moved in memory.
		return true;

	if (!SetData(*outSedc))
		return false;

	return true;
}


/**
 * mmvis_static::StructureEventsDataSource::getExtentCallback
 */
bool mmvis_static::StructureEventsDataSource::getExtentCallback(Call& caller) {
	StructureEventsDataCall *outSedc = dynamic_cast<StructureEventsDataCall*>(&caller);
	if (outSedc == NULL)
		return false;

	outSedc->SetFrameCount(this->frameCount);
	outSedc->AccessBoundingBoxes().Clear();
	outSedc->AccessBoundingBoxes().SetObjectSpaceBBox(this->bbox);
	outSedc->AccessBoundingBoxes().SetObjectSpaceClipBox(this->clipbox);
	outSedc->SetDataHash(this->sedcHash);
	return true;
}


bool mmvis_static::StructureEventsDataSource::SetData(StructureEventsDataCall& data) {
	//This function gets called very often!

#define _ASSERT_READFILE(BUFFER, BUFFERSIZE) if (this->file->Read((BUFFER), (BUFFERSIZE)) != (BUFFERSIZE)) { \
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR, "Unable to read MMSE file data"); \
		return false; \
		}

	//if (this->sedcHash == 0) // filenameChanged not exectuted.
	//	filenameChanged(this->filename);

	StructureEvents* events = &data.getEvents();

	size_t bufferSize = events->getStride() * this->eventCount;

	if (this->headerSize == 0) {
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR, "MMSE file header not set");
		return false;
	}

	//vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO, "MMSE Source: HeaderSize %d bufferSize %d.", this->headerSize, bufferSize);

	this->file->Seek(this->headerSize);
	this->eventData.EnforceSize(bufferSize);
	_ASSERT_READFILE(this->eventData, bufferSize);

	if (this->eventData.IsEmpty())
		return false;

	const float* location = this->eventData.As<float>();
	const float* time = this->eventData.AsAt<float>(12);
	const StructureEvents::EventType* type = this->eventData.AsAt<StructureEvents::EventType>(16);

	// Debug.
	/*
	int stride = 20 / 4;
	for (int eventnumber = 0; eventnumber < this->eventCount; ++eventnumber){
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO, "MMSE Source: Event %d (frames %d, events %d) (%f, %f, %f), %f, %d.", eventnumber, this->frameCount, this->eventCount,
			location[(stride * eventnumber)], location[(stride * eventnumber) + 1], location[(stride * eventnumber) + 2], time[(stride * eventnumber)], type[(stride * eventnumber)]);
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO, "MMSE Source: Event %d (%p, %p, %p), %p, %p.", eventnumber,
			&location[(stride * eventnumber)], &location[(stride * eventnumber) + 1], &location[(stride * eventnumber) + 2], &time[(stride * eventnumber)], &type[(stride * eventnumber)]);
	}
	*/

	events->setEvents(location,
		time,
		type,
		this->maxTime,
		this->eventCount);

	// Flag to renew data.
	this->reReadData = false;

	return true;
}