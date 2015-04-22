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
 * mmvis_static::StructureEvent::StructureEvent
 */
mmvis_static::StructureEvents::StructureEvents(const mmvis_static::StructureEvents& src) {
	*this = src;
}


/*
 * mmvis_static::StructureEventsDataCall::StructureEventsDataCall
 */
mmvis_static::StructureEventsDataCall::StructureEventsDataCall() : AbstractGetData3DCall() {
}


/*
 * mmvis_static::StructureEventsDataCall::~StructureEventsDataCall
 */
mmvis_static::StructureEventsDataCall::~StructureEventsDataCall(void) {
}


/*
 * moldyn::MultiParticleDataCall::operator=
 */
mmvis_static::StructureEventsDataCall& mmvis_static::StructureEventsDataCall::operator=(
	const mmvis_static::StructureEventsDataCall& rhs) {
	AbstractGetData3DCall::operator =(rhs);
	return *this;
}
