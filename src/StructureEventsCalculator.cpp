/**
 * StructureEventsCalculator.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Alle Rechte vorbehalten.
 *
 * ToDo:
 * - CountourTree
 * - MergeTree
 * - Threshold
 */

#include "stdafx.h"
#include "StructureEventsCalculator.h"
#include "mmcore/param/IntParam.h"

using namespace megamol;
using namespace megamol::core;

/**
 * mmvis_static::StructureEventsCalculator::StructureEventsCalculator
 */
mmvis_static::StructureEventsCalculator::StructureEventsCalculator() : Module(),
	inDataSlot("in data", "Connects to the data source. Expects colored particles"),
	outDataSlot("out data", "Slot to request data from this calculation."),
	thresholdSlot("Threshold Value", "The threshold for the contour tree.") {

	this->inDataSlot.SetCompatibleCall<core::moldyn::MultiParticleDataCallDescription>();
	this->MakeSlotAvailable(&this->inDataSlot);

	this->outDataSlot.SetCallback("StructureEventsDataCall", "GetData", &StructureEventsCalculator::getDataCallback);
	this->outDataSlot.SetCallback("StructureEventsDataCall", "GetExtent", &StructureEventsCalculator::getExtentCallback);
	this->MakeSlotAvailable(&this->outDataSlot);

	this->thresholdSlot.SetParameter(new core::param::IntParam(5, 1));
	this->MakeSlotAvailable(&this->thresholdSlot);
}


/**
 * mmvis_static::StructureEventsCalculator::~StructureEventsCalculator
 */
mmvis_static::StructureEventsCalculator::~StructureEventsCalculator(void) {
}


/**
 * mmvis_static::StructureEventsCalculator::create
 */
bool mmvis_static::StructureEventsCalculator::create(void) {
	// intentionally empty
	return true;
}


/**
 * mmvis_static::StructureEventsCalculator::release
 */
void mmvis_static::StructureEventsCalculator::release(void) {
}


/**
 * mmvis_static::StructureEventsCalculator::getDataCallback
 */
bool mmvis_static::StructureEventsCalculator::getDataCallback(Call& callee) {
	using megamol::core::moldyn::MultiParticleDataCall;

	StructureEventsDataCall *outSedc = dynamic_cast<StructureEventsDataCall*>(&callee);
	if (outSedc == NULL) return false;

	MultiParticleDataCall *inMpdc = this->inDataSlot.CallAs<MultiParticleDataCall>();
	if (inMpdc == NULL) return false;

	// We have to go through all frames. Normally this is controlled by View3D or
	// the writer (look into writer for the code). StructureEventsDataCall shouldn't contain frames!

	//*inMpdc = *outSedc; // to get the correct request time
	if (!(*inMpdc)(0)) return false;

	if (!this->manipulateData(*outSedc, *inMpdc)) {
		inMpdc->Unlock();
		return false;
	}

	inMpdc->Unlock();

	return true;
}


/**
 * datatools::AbstractParticleManipulator::getExtentCallback
 */
bool mmvis_static::StructureEventsCalculator::getExtentCallback(Call& callee) {
	using mmvis_static::StructureEventsDataCall;
	using megamol::core::moldyn::MultiParticleDataCall;

	StructureEventsDataCall *outSedc = dynamic_cast<StructureEventsDataCall*>(&callee);
	if (outSedc == NULL) return false;

	MultiParticleDataCall *inMpdc = this->inDataSlot.CallAs<MultiParticleDataCall>();
	if (inMpdc == NULL) return false;

	//*inMpdc = *outSedc; // to get the correct request time
	if (!(*inMpdc)(1)) return false;

	if (!this->manipulateExtent(*outSedc, *inMpdc)) {
		inMpdc->Unlock();
		return false;
	}

	inMpdc->Unlock();

	// Aus DataGridder, alternative Methode, müsste in manipulateExtend:
	/*
	MultiParticleDataCall *mpdc = this->inDataSlot.CallAs<MultiParticleDataCall>();
	if (mpdc != NULL) {
		*static_cast<AbstractGetData3DCall*>(mpdc) = *outSedc;
		if ((*mpdc)(1)) {
			*static_cast<AbstractGetData3DCall*>(outSedc) = *mpdc;
			return true;
		}
	}*/

	return true;
}


/**
 * mmvis_static::StructureEventsCalculator::manipulateData
 */
bool mmvis_static::StructureEventsCalculator::manipulateData (
	mmvis_static::StructureEventsDataCall& outData,
	megamol::core::moldyn::MultiParticleDataCall& inData) {

	int threshold = this->thresholdSlot.Param<core::param::IntParam>()->Value();

	getMPDCFrame(inData, 10);

	// ContourTree TODO
	// ToDo: Get data for each frame. Is the complete MMPLD in memory? Nope and never should (big data!)
	//for (uint32_t frame = 0; frame < inData.FrameCount; frame++) {
	//}

	// MergeTree TODO
	// getEvents with Threshold TODO

	// Test.
	const float *location = reinterpret_cast<const float*>(&particleList[0].position);
	const float *time = reinterpret_cast<const float*>(&particleList[0].position);
	//const uint8_t *type = reinterpret_cast<const uint8_t*>(&particleList[0].position);
	const StructureEvents::EventType type = StructureEvents::EventType::BIRTH;

	// Send data to the call.
	StructureEvents events = outData.getEvents();
	events.setEvents(location, time, &type, particleList.size());

	// Debug.
	printf("Calculator: Location: %f, Listsize: %d, Max Listsize: %lu\n", *location, particleList.size(), particleList.max_size());

	inData.SetUnlocker(nullptr, false);
	return true;
}


/**
 * mmvis_static::StructureEventsCalculator::manipulateExtent
 */
bool mmvis_static::StructureEventsCalculator::manipulateExtent(
	mmvis_static::StructureEventsDataCall& outData,
	megamol::core::moldyn::MultiParticleDataCall& inData) {

	// TODO
	//outData = inData;
	inData.SetUnlocker(nullptr, false);
	return true;
}


/**
 * mmvis_static::StructureEventsCalculator::getMPDCData
 */
void mmvis_static::StructureEventsCalculator::getMPDCFrame(megamol::core::moldyn::MultiParticleDataCall& inData, uint32_t frameID) {
	// ToDo: Add return value to reflect success?
	using megamol::core::moldyn::MultiParticleDataCall;

	inData.SetFrameID(frameID, false); // Don't force data.

	// ParticleIndex.
	uint64_t particleIndex = 0;

	// This takes currently several seconds for 2Mio particles.
	for (unsigned int particleListIndex = 0; particleListIndex < inData.GetParticleListCount(); particleListIndex++) {
		MultiParticleDataCall::Particles& particles = inData.AccessParticles(particleListIndex);

		if (particles.GetVertexDataType() == MultiParticleDataCall::Particles::VERTDATA_NONE
				|| particles.GetCount() == 0) {
			printf("Particlelist %d skipped, no vertex data.\n", particleListIndex); // Debug.
			continue; // Skip this particle list.
		}

		if (particles.GetColourDataType() != MultiParticleDataCall::Particles::COLDATA_FLOAT_I) {
			printf("Particlelist %d skipped, COLDATA_FLOAT_I expected.\n", particleListIndex); // Debug.
			continue; // Skip this particle list.
		}
		// Get pointer at vertex data.
		const uint8_t *vertexPtr = static_cast<const uint8_t*>(particles.GetVertexData()); // ParticleRelaxationModule.
		//const void *vertexPtr = particles.GetVertexData(); // Particle Thinner.

		// Get pointer at color data. The signed distance is stored here.
		const uint8_t *colourPtr = static_cast<const uint8_t*>(particles.GetColourData());

		// Get single vertices by heeding dataType and Stride.
		// Vertex.
		unsigned int vertexStride = particles.GetVertexDataStride(); // Distance from one Vertex to the next.
		bool vertexIsFloat = true;
		bool hasRadius = false;
		switch (particles.GetVertexDataType()) { // VERTDATA_NONE tested above.
		case MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR:
			hasRadius = true;
			vertexStride = std::max<unsigned int>(vertexStride, 16);
			break;
		case MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZ:
			vertexStride = std::max<unsigned int>(vertexStride, 12);
			break;
		case MultiParticleDataCall::Particles::VERTDATA_SHORT_XYZ:
			vertexIsFloat = false;
			vertexStride = std::max<unsigned int>(vertexStride, 6);
			break;
		default: throw std::exception();
		}

		// Color.
		unsigned int colourStride = particles.GetColourDataStride(); // Distance from one Color to the next == VertexStride.
		colourStride = std::max<unsigned int>(colourStride, 4); // MultiParticleDataCall::Particles::COLDATA_FLOAT_I.

		float globalRadius = particles.GetGlobalRadius();


		//ASSERT((hasRadius && vertexIsFloat) || !hasRadius); // radius is only used with floats
		for (uint64_t particleIndex = 0; particleIndex < particles.GetCount(); particleIndex++, vertexPtr += vertexStride, colourPtr += colourStride) {

			Particle particle;

			// Vertex.
			if (vertexIsFloat) { // Performance is lower with if-else in loop, however readability is higher.
				const float *vertexPtrf = reinterpret_cast<const float*>(vertexPtr);
				particle.position = {
					vertexPtrf[0],
					vertexPtrf[1],
					vertexPtrf[2] };
				particle.radius = hasRadius ? vertexPtrf[3] : globalRadius;
			}
			else {
				const uint16_t *vertexPtr16 = reinterpret_cast<const uint16_t*>(vertexPtr);
				Particle particle;
				particle.position = {
					static_cast<float>(vertexPtr16[0]),
					static_cast<float>(vertexPtr16[1]),
					static_cast<float>(vertexPtr16[2]) };
				particle.radius = static_cast<float>(vertexPtr16[3]);
			}

			// Colour/Signed Distance.
			const float *colourPtrf = reinterpret_cast<const float*>(colourPtr);
			particle.signedDistance = *colourPtrf;

			// Add particle to list with particleIndex as ID and key.
			particleIndex++;
			particleList[particleIndex] = particle;

			// Debug.
			//printf("Particle %d color: (%f, %f, %f)\n",
			//	particleID,
			//	particleList[particleID].signedDistance.x,
			//	particleList[particleID].signedDistance.y,
			//	particleList[particleID].signedDistance.z);
		}
	}
}