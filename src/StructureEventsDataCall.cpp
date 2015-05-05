/**
 * StructureEventsDataCall.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "StructureEventsDataCall.h"

using namespace megamol;

/**
 * mmvis_static::StructureEvents::StructureEvents
 */
mmvis_static::StructureEvents::StructureEvents(void) :
		locationPtr(NULL),
		timePtr(NULL),
		typePtr(NULL),
		stride(0),
		count(0),
		maxTime(0) {
	printf("Structure Events alive!\n");
}


/**
 * mmvis_static::StructureEvents::~StructureEvents
 */
mmvis_static::StructureEvents::~StructureEvents(void) {
}


/**
 * mmvis_static::StructureEvent::StructureEvent
 */
mmvis_static::StructureEvents::StructureEvents(const mmvis_static::StructureEvents& src) {
	*this = src;
}


/**
 * mmvis_static::StructureEvent::operator=
 */
mmvis_static::StructureEvents&
mmvis_static::StructureEvents::operator=(
	const mmvis_static::StructureEvents& rhs) {
	this->agglomeration = rhs.agglomeration;
	this->count = rhs.count;
	this->locationPtr = rhs.locationPtr;
	this->maxTime = rhs.maxTime;
	this->stride = rhs.stride;
	this->timePtr = rhs.timePtr;
	this->typePtr = rhs.typePtr;
	return *this;
}


/**
 * mmvis_static::StructureEventsDataCall::StructureEventsDataCall
 */
mmvis_static::StructureEventsDataCall::StructureEventsDataCall() : AbstractGetData3DCall(), events() {
	printf("Data call alive!\n\n\n");
}


/**
 * mmvis_static::StructureEventsDataCall::~StructureEventsDataCall
 */
mmvis_static::StructureEventsDataCall::~StructureEventsDataCall(void) {
	this->Unlock();
	// Delete events here if they are at free store (new keyword).
}


/**
 * moldyn::MultiParticleDataCall::operator=
 */
mmvis_static::StructureEventsDataCall& mmvis_static::StructureEventsDataCall::operator=(
	const mmvis_static::StructureEventsDataCall& rhs) {
	AbstractGetData3DCall::operator =(rhs);
	return *this;
}
