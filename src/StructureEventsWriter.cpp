/**
 * StructureEventsWriter.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Copyright (C) 2015 by Richard Hähne, TU Dresden
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "StructureEventsWriter.h"
#include "mmcore/param/FilePathParam.h"
#include "StructureEventsDataCall.h"

using namespace megamol;
using namespace megamol::core;

/*
 * mmvis_static::StructureEventsWriter::StructureEventsWriter
 */
mmvis_static::StructureEventsWriter::StructureEventsWriter() : AbstractDataWriter(),
	filenameSlot("filename", "The path to the MMSE file to be written"),
	dataSlot("data", "The slot requesting the data to be written") {

	this->filenameSlot << new param::FilePathParam("");
	this->MakeSlotAvailable(&this->filenameSlot);

	this->dataSlot.SetCompatibleCall<StructureEventsDataCallDescription>();
	this->MakeSlotAvailable(&this->dataSlot);
}


/*
 * mmvis_static::StructureEventsWriter::~StructureEventsWriter
 */
mmvis_static::StructureEventsWriter::~StructureEventsWriter(void) {
	this->Release();
}


/*
 * mmvis_static::StructureEventsWriter::create
 */
bool mmvis_static::StructureEventsWriter::create(void) {
	// intentionally empty
	return true;
}


/*
 * mmvis_static::StructureEventsWriter::release
 */
void mmvis_static::StructureEventsWriter::release(void) {
}


/*
 * mmvis_static::StructureEventsWriter::run
 */
bool mmvis_static::StructureEventsWriter::run(void) {
	// TODO
	return true;
}


/*
 * mmvis_static::StructureEventsWriter::getCapabilities
 */
bool mmvis_static::StructureEventsWriter::getCapabilities(DataWriterCtrlCall& call) {
	call.SetAbortable(false);
	return true;
}