/**
 * StructureEventsClusterVisualization.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Copyright (C) 2015 by Richard Hähne, TU Dresden
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "StructureEventsClusterVisualization.h"
#include "mmcore/param/IntParam.h"
#include <random>

using namespace megamol;
using namespace megamol::core;

/**
 * mmvis_static::StructureEventsClusterVisualization::StructureEventsClusterVisualization
 */
mmvis_static::StructureEventsClusterVisualization::StructureEventsClusterVisualization() : Module(),
	inDataSlot("in data", "Connects to the data source. Expects signed distance particles"),
	outDataSlot("out data", "Slot to request data from this calculation."),
	dataHash(0), frameId(0) {

	this->inDataSlot.SetCompatibleCall<core::moldyn::MultiParticleDataCallDescription>();
	this->MakeSlotAvailable(&this->inDataSlot);

	this->outDataSlot.SetCallback("MultiParticleDataCall", "GetData", &StructureEventsClusterVisualization::getDataCallback);
	this->outDataSlot.SetCallback("MultiParticleDataCall", "GetExtent", &StructureEventsClusterVisualization::getExtentCallback);
	this->MakeSlotAvailable(&this->outDataSlot);
}


/**
 * mmvis_static::StructureEventsClusterVisualization::~StructureEventsClusterVisualization
 */
mmvis_static::StructureEventsClusterVisualization::~StructureEventsClusterVisualization(void) {
}


/**
 * mmvis_static::StructureEventsClusterVisualization::create
 */
bool mmvis_static::StructureEventsClusterVisualization::create(void) {
	// intentionally empty
	return true;
}


/**
 * mmvis_static::StructureEventsClusterVisualization::release
 */
void mmvis_static::StructureEventsClusterVisualization::release(void) {
}


/**
 * mmvis_static::StructureEventsClusterVisualization::getDataCallback
 */
bool mmvis_static::StructureEventsClusterVisualization::getDataCallback(Call& caller) {
	using megamol::core::moldyn::MultiParticleDataCall;

	MultiParticleDataCall *outMpdc = dynamic_cast<MultiParticleDataCall*>(&caller);
	if (outMpdc == NULL) return false;

	MultiParticleDataCall *inMpdc = this->inDataSlot.CallAs<MultiParticleDataCall>();
	if (inMpdc == NULL) return false;

	*inMpdc = *outMpdc; // Get the correct request time.
	if (!(*inMpdc)(0)) return false;

	if (!this->manipulateData(*outMpdc, *inMpdc)) {
		inMpdc->Unlock();
		return false;
	}

	inMpdc->Unlock();

	return true;
}


/**
 * mmvis_static::StructureEventsClusterVisualization::getExtentCallback
 */
bool mmvis_static::StructureEventsClusterVisualization::getExtentCallback(Call& caller) {
	using megamol::core::moldyn::MultiParticleDataCall;

	MultiParticleDataCall *outMpdc = dynamic_cast<MultiParticleDataCall*>(&caller);
	if (outMpdc == NULL) return false;

	MultiParticleDataCall *inMpdc = this->inDataSlot.CallAs<MultiParticleDataCall>();
	if (inMpdc == NULL) return false;

	*inMpdc = *outMpdc; // Get the correct request time.
	if (!(*inMpdc)(1)) return false;

	if (!this->manipulateExtent(*outMpdc, *inMpdc)) {
		inMpdc->Unlock();
		return false;
	}

	inMpdc->Unlock();

	return true;
}


/**
 * mmvis_static::StructureEventsClusterVisualization::manipulateData
 */
bool mmvis_static::StructureEventsClusterVisualization::manipulateData (
	megamol::core::moldyn::MultiParticleDataCall& outData,
	megamol::core::moldyn::MultiParticleDataCall& inData) {

	//printf("Calculator: FrameIDs in: %d, out: %d, stored: %d.\n", inData.FrameID(), outData.FrameID(), this->frameId); // Debug.

	// Only calculate when inData has changed frame or hash (data has been manipulated).
	if ((this->frameId != inData.FrameID()) || (this->dataHash != inData.DataHash()) || (inData.DataHash() == 0)) {
		this->frameId = inData.FrameID();
		this->dataHash = inData.DataHash();
		this->setData(inData);
	}

	// From datatools::ParticleListMergeModule:

	inData.Unlock();

	// Output the data.
	outData.SetDataHash(this->dataHash);
	outData.SetFrameID(this->frameId);
	outData.SetParticleListCount(1);
	outData.AccessParticles(0) = this->particles;
	outData.SetUnlocker(nullptr); // HAZARD: we could have one ...

	return true;
}


/**
 * mmvis_static::StructureEventsClusterVisualization::manipulateExtent
 */
bool mmvis_static::StructureEventsClusterVisualization::manipulateExtent(
	megamol::core::moldyn::MultiParticleDataCall& outData,
	megamol::core::moldyn::MultiParticleDataCall& inData) {

	outData = inData;
	inData.SetUnlocker(nullptr, false);
	return true;
}


/**
 * mmvis_static::StructureEventsClusterVisualization::setData
 */
void mmvis_static::StructureEventsClusterVisualization::setData(megamol::core::moldyn::MultiParticleDataCall& inData) {
	using megamol::core::moldyn::MultiParticleDataCall;

	uint64_t globalParticleIndex = 0;
	float globalRadius;
	uint8_t globalColor[4];
	float globalColorIndexMin, globalColorIndexMax;

	for (unsigned int particleListIndex = 0; particleListIndex < inData.GetParticleListCount(); particleListIndex++) {

		///
		/// Get meta information of inData.
		///

		MultiParticleDataCall::Particles& particles = inData.AccessParticles(particleListIndex);

		// Check for existing data.
		if (particles.GetVertexDataType() == MultiParticleDataCall::Particles::VERTDATA_NONE
				|| particles.GetCount() == 0) {
			printf("Particlelist %d skipped, no vertex data.\n", particleListIndex); // Debug.
			continue; // Skip this particle list.
		}

		// Check for correct color type.
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

		// Particlelist globals. Gets overwritten with each ParticleList. Doesn't matter here anyways.
		globalRadius = particles.GetGlobalRadius();
		::memcpy(globalColor, particles.GetGlobalColour(), 4);
		globalColorIndexMin = particles.GetMinColourIndexValue();
		globalColorIndexMax = particles.GetMaxColourIndexValue();

		///
		/// Write inData into local container.
		///

		//ASSERT((hasRadius && vertexIsFloat) || !hasRadius); // radius is only used with floats
		for (uint64_t particleIndex = 0; particleIndex < particles.GetCount(); particleIndex++, vertexPtr += vertexStride, colourPtr += colourStride) {

			Particle particle;

			// Vertex.
			if (vertexIsFloat) { // Performance is lower with if-else in loop, however readability is higher.
				const float *vertexPtrf = reinterpret_cast<const float*>(vertexPtr);
				particle.x = vertexPtrf[0];
				particle.y = vertexPtrf[1];
				particle.z = vertexPtrf[2];
				particle.radius = hasRadius ? vertexPtrf[3] : globalRadius;
			}
			else {
				const uint16_t *vertexPtr16 = reinterpret_cast<const uint16_t*>(vertexPtr);
				Particle particle;
				particle.x = static_cast<float>(vertexPtr16[0]);
				particle.y = static_cast<float>(vertexPtr16[1]);
				particle.z = static_cast<float>(vertexPtr16[2]);
				particle.radius = static_cast<float>(vertexPtr16[3]);
			}

			// Colour/Signed Distance.
			const float *colourPtrf = reinterpret_cast<const float*>(colourPtr);
			particle.signedDistance = *colourPtrf;

			// Set random color.
			std::random_device rd;
			std::mt19937_64 mt(rd());
			std::uniform_real_distribution<float> distribution(0, 1);
			particle.r = distribution(mt);
			particle.g = distribution(mt);
			particle.b = distribution(mt);

			// Set particle ID.
			globalParticleIndex++;
			particle.id = globalParticleIndex;

			// Add particle to list.
			particleList.push_back(particle);
		}
	}

	///
	/// Set MultiParticleDataCall::Particles with data of the local container.
	///

	unsigned int particleStride = sizeof(Particle);

	// Debug.
	printf("Calculator: ParticleStride: %d, ParticleCount: %d, RandomParticleColor: (%f, %f, %f)\n",
		particleStride, globalParticleIndex, particleList[10].r, particleList[10].g, particleList[10].b);

	this->particles.SetCount(globalParticleIndex);
	this->particles.SetGlobalRadius(globalRadius);
	this->particles.SetGlobalColour(globalColor[0], globalColor[1], globalColor[2]);
	this->particles.SetColourMapIndexValues(globalColorIndexMin, globalColorIndexMax);

	this->particles.SetVertexData(MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR, &this->particleList[0], particleStride);
	this->particles.SetColourData(MultiParticleDataCall::Particles::COLDATA_FLOAT_RGB, &this->particleList[0].r, particleStride);
	
	// Don't forget!
	particleList.clear();
}
