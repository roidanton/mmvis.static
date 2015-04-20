/**
 * StructureEventsCalculator.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "StructureEventsCalculator.h"
#include "StructureEventsDataCall.h"

using namespace megamol;
using namespace megamol::core;

/*
 * mmvis_static::StructureEventsCalculator::StructureEventsCalculator
 */
mmvis_static::StructureEventsCalculator::StructureEventsCalculator() : Module(),
	getDataSlot("getdata", "Connects to the data source"),
	sendDataSlot("senddata", "Slot to request data from this calculation.") {

	this->getDataSlot.SetCompatibleCall<core::moldyn::MultiParticleDataCallDescription>();
	this->MakeSlotAvailable(&this->getDataSlot);

	this->sendDataSlot.SetCallback("StructureEventsDataCall", "GetData", &StructureEventsCalculator::getDataCallback);
	this->MakeSlotAvailable(&this->sendDataSlot);

}

/*
 * mmvis_static::StructureEventsCalculator::~StructureEventsCalculator
 */
mmvis_static::StructureEventsCalculator::~StructureEventsCalculator(void) {
}

/*
 * mmvis_static::StructureEventsCalculator::create
 */
bool mmvis_static::StructureEventsCalculator::create(void) {
	// intentionally empty
	return true;
}


/*
 * mmvis_static::StructureEventsCalculator::release
 */
void mmvis_static::StructureEventsCalculator::release(void) {
}


/*
 * mmvis_static::StructureEventsCalculator::getDataCallback
 */
bool mmvis_static::StructureEventsCalculator::getDataCallback(Call& caller) {
	StructureEventsDataCall *c = dynamic_cast<StructureEventsDataCall*>(&caller);
	if (c == NULL) return false;

	// TODO

	return true;
}