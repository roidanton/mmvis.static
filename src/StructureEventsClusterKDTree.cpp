/**
 * StructureEventsClusterKDTree.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Copyright (C) 2015 by Richard Hähne, TU Dresden
 * Alle Rechte vorbehalten.
 *
 * Obsolete module, existed for testing purposes. Can be removed safely.
 */

#include "stdafx.h"
#include "StructureEventsClusterKDTree.h"
#include "mmcore/param/IntParam.h"
#include "ANN/ANN.h"
#include <chrono>
#include <random>

using namespace megamol;
using namespace megamol::core;

/**
 * mmvis_static::StructureEventsClusterKDTree::StructureEventsClusterKDTree
 */
mmvis_static::StructureEventsClusterKDTree::StructureEventsClusterKDTree() : Module(),
	inDataSlot("in data", "Connects to the data source. Expects signed distance particles"),
	outDataSlot("out data", "Slot to request data from this calculation."),
	dataHash(0), frameId(0), newColors() {

	this->inDataSlot.SetCompatibleCall<core::moldyn::MultiParticleDataCallDescription>();
	this->MakeSlotAvailable(&this->inDataSlot);

	this->outDataSlot.SetCallback("MultiParticleDataCall", "GetData", &StructureEventsClusterKDTree::getDataCallback);
	this->outDataSlot.SetCallback("MultiParticleDataCall", "GetExtent", &StructureEventsClusterKDTree::getExtentCallback);
	this->MakeSlotAvailable(&this->outDataSlot);
}


/**
 * mmvis_static::StructureEventsClusterKDTree::~StructureEventsClusterKDTree
 */
mmvis_static::StructureEventsClusterKDTree::~StructureEventsClusterKDTree(void) {
}


/**
 * mmvis_static::StructureEventsClusterKDTree::create
 */
bool mmvis_static::StructureEventsClusterKDTree::create(void) {
	// intentionally empty
	return true;
}


/**
 * mmvis_static::StructureEventsClusterKDTree::release
 */
void mmvis_static::StructureEventsClusterKDTree::release(void) {
}


/**
 * mmvis_static::StructureEventsClusterKDTree::getDataCallback
 */
bool mmvis_static::StructureEventsClusterKDTree::getDataCallback(Call& caller) {
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
 * mmvis_static::StructureEventsClusterKDTree::getExtentCallback
 */
bool mmvis_static::StructureEventsClusterKDTree::getExtentCallback(Call& caller) {
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
 * mmvis_static::StructureEventsClusterKDTree::manipulateData
 */
bool mmvis_static::StructureEventsClusterKDTree::manipulateData (
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
 * mmvis_static::StructureEventsClusterKDTree::manipulateExtent
 */
bool mmvis_static::StructureEventsClusterKDTree::manipulateExtent(
	megamol::core::moldyn::MultiParticleDataCall& outData,
	megamol::core::moldyn::MultiParticleDataCall& inData) {

	outData = inData;
	inData.SetUnlocker(nullptr, false);
	return true;
}


/**
 * mmvis_static::StructureEventsClusterKDTree::setData
 */
void mmvis_static::StructureEventsClusterKDTree::setData(megamol::core::moldyn::MultiParticleDataCall& data) {
	using megamol::core::moldyn::MultiParticleDataCall;

	float globalRadius;
	float lastParticleRadius;
	uint8_t globalColor[4];
	float globalColorIndexMin, globalColorIndexMax;

	size_t globalGaslessParticleIndex = 0;

	double debugNeighboursTotalDistance = 0;

	///
	/// Count particles to determine list sizes.
	///

	size_t globalParticleCnt = 0;
	unsigned int plc = data.GetParticleListCount();
	for (unsigned int pli = 0; pli < plc; pli++) {
		auto& pl = data.AccessParticles(pli);
		if (pl.GetColourDataType() != MultiParticleDataCall::Particles::COLDATA_FLOAT_I)
			continue;
		if ((pl.GetVertexDataType() != MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZ)
			&& (pl.GetVertexDataType() != MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR)) {
			continue;
		}
		globalParticleCnt += static_cast<size_t>(pl.GetCount());
	}

	ANNpoint dataPtsData = new ANNcoord[3 * globalParticleCnt]; // Bigger than needed.
	this->newColors.resize(globalParticleCnt);
	//std::vector<size_t> posparts;
	//posparts.reserve(globalParticleCnt);

	///
	/// Build tree.
	///
	auto time_buildTree = std::chrono::system_clock::now();

	for (unsigned int particleListIndex = 0; particleListIndex < data.GetParticleListCount(); particleListIndex++) {

		///
		/// Get meta information of data.
		///

		MultiParticleDataCall::Particles& particles = data.AccessParticles(particleListIndex);

		// Check for existing data.
		if (particles.GetCount() == 0) {
			printf("Particlelist %d skipped, no vertex data.\n", particleListIndex); // Debug.
			continue; // Skip this particle list.
		}

		// Check for correct vertex type.
		if (particles.GetVertexDataType() != MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZ
			&& particles.GetVertexDataType() != MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR) {
			printf("Particlelist %d skipped, vertex are not floats.\n", particleListIndex); // Debug.
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
		bool hasRadius = false;
		switch (particles.GetVertexDataType()) { // VERTDATA_NONE tested above.
		case MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR:
			hasRadius = true;
			vertexStride = std::max<unsigned int>(vertexStride, 16);
			break;
		case MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZ:
			vertexStride = std::max<unsigned int>(vertexStride, 12);
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
		/// Write data into local container.
		///

		for (uint64_t particleIndex = 0; particleIndex < particles.GetCount(); particleIndex++, vertexPtr += vertexStride, colourPtr += colourStride) {

			// Skip gas particles.
			const float *colourPtrf = reinterpret_cast<const float*>(colourPtr);
			if (*colourPtrf < -0.001f)
				continue;

			// Add particle to list.
			const float *vertexPtrf = reinterpret_cast<const float*>(vertexPtr);
			
			dataPtsData[globalGaslessParticleIndex * 3 + 0] = static_cast<ANNcoord>(vertexPtrf[0]);
			dataPtsData[globalGaslessParticleIndex * 3 + 1] = static_cast<ANNcoord>(vertexPtrf[1]);
			dataPtsData[globalGaslessParticleIndex * 3 + 2] = static_cast<ANNcoord>(vertexPtrf[2]);
			/*
			dataPtsData[(globalParticleCnt + particleIndex) * 3 + 0] = static_cast<ANNcoord>(vertexPtrf[0]);
			dataPtsData[(globalParticleCnt + particleIndex) * 3 + 1] = static_cast<ANNcoord>(vertexPtrf[1]);
			dataPtsData[(globalParticleCnt + particleIndex) * 3 + 2] = static_cast<ANNcoord>(vertexPtrf[2]);
			*/
			lastParticleRadius = hasRadius ? vertexPtrf[3] : globalRadius;
			globalGaslessParticleIndex++;
		}
		globalParticleCnt += static_cast<size_t>(particles.GetCount());
	}

	// Create k-d-Tree.
	ANNpointArray annPts = new ANNpoint[globalGaslessParticleIndex];
	for (size_t i = 0; i < globalGaslessParticleIndex; ++i) {
		annPts[i] = dataPtsData + (i * 3);
	}
	ANNkd_tree* tree = new ANNkd_tree(annPts, static_cast<int>(globalGaslessParticleIndex), 3);

	{ // Time measurement.
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_buildTree);
		printf("Calculator: Created tree with particle gasless count of %d after %lld ms\n", globalGaslessParticleIndex, duration.count());
	}

	///
	/// Calculate cluster.
	///
	auto time_calc = std::chrono::system_clock::now();

	globalParticleCnt = 0; // Reset.
	
	for (unsigned int particleListIndex = 0; particleListIndex < data.GetParticleListCount(); particleListIndex++) {

		///
		/// Get meta information of data.
		///

		MultiParticleDataCall::Particles& particles = data.AccessParticles(particleListIndex);

		// Check for existing data.
		if (particles.GetCount() == 0) {
			printf("Particlelist %d skipped, no vertex data.\n", particleListIndex); // Debug.
			continue; // Skip this particle list.
		}

		// Check for correct vertex type.
		if (particles.GetVertexDataType() != MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZ
			&& particles.GetVertexDataType() != MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR) {
			printf("Particlelist %d skipped, vertex are not floats.\n", particleListIndex); // Debug.
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
		bool hasRadius = false;
		switch (particles.GetVertexDataType()) { // VERTDATA_NONE tested above.
		case MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR:
			hasRadius = true;
			vertexStride = std::max<unsigned int>(vertexStride, 16);
			break;
		case MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZ:
			vertexStride = std::max<unsigned int>(vertexStride, 12);
			break;
		default: throw std::exception();
		}

		// Color.
		unsigned int colourStride = particles.GetColourDataStride(); // Distance from one Color to the next == VertexStride.
		colourStride = std::max<unsigned int>(colourStride, 4); // MultiParticleDataCall::Particles::COLDATA_FLOAT_I.

		///
		/// Write data into local container.
		///

		for (uint64_t particleIndex = 0; particleIndex < particles.GetCount(); particleIndex++, vertexPtr += vertexStride, colourPtr += colourStride) {

			// Skip gas particles.
			const float *colourPtrf = reinterpret_cast<const float*>(colourPtr);
			if (*colourPtrf < -0.001f) {
				this->newColors[(globalParticleCnt + particleIndex)] = 0.0f;
				continue;
			}
				
			// Calculate depth
			const float *vertexPtrf = reinterpret_cast<const float*>(vertexPtr);

			ANNcoord q[3];
			ANNidxArray   nn_idx = 0;
			ANNdistArray  dd = 0;
			int k_max = 10;

			nn_idx = new ANNidx[k_max];
			dd = new ANNdist[k_max];

			ANNdist sqrRadius = static_cast<ANNdist>(vertexPtrf[3])*static_cast<ANNdist>(vertexPtrf[3]);

			q[0] = static_cast<ANNcoord>(vertexPtrf[0]);
			q[1] = static_cast<ANNcoord>(vertexPtrf[1]);
			q[2] = static_cast<ANNcoord>(vertexPtrf[2]);

			//tree->annkSearch(q, k_max, nn_idx, dd);
			tree->annkFRSearch(q, sqrRadius, k_max, nn_idx, dd);


			//printf("Particle %d neighbour %d distance %f.\n", globalParticleCnt + particleIndex, nn_idx[2], dd[2]);
			for (int i = 0; i < k_max; ++i) {
				debugNeighboursTotalDistance += dd[i];
			}

			std::random_device rd;
			std::mt19937_64 mt(rd());
			std::uniform_real_distribution<float> distribution(0, 1);
			this->newColors[(globalParticleCnt + particleIndex)] = distribution(mt); //static_cast<float> (nd);

			delete tree;
			delete[] nn_idx;
			delete[] dd;
		}
		globalParticleCnt += static_cast<size_t>(particles.GetCount());
	}

	// Debug
	{
		auto duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - time_calc);
		printf("Calculator: Cluster calculation after %lld s with total neighbours %f.\n", duration.count(), debugNeighboursTotalDistance);
	}
	// Find neighbours.

	// Cluster creation.
	//createClustersSignedDistanceOnly();

	// Set alternative coloring.
	//setSignedDistanceColor(signedDistanceMin, signedDistanceMax);
	//setClusterColor();

	///
	/// Fill MultiParticleDataCall::Particles with data of the local container.
	///

	unsigned int particleStride = sizeof(Particle);

	this->particles.SetCount(globalParticleCnt);
	this->particles.SetGlobalRadius(globalRadius);
	this->particles.SetGlobalColour(globalColor[0], globalColor[1], globalColor[2]);
	this->particles.SetColourMapIndexValues(globalColorIndexMin, globalColorIndexMax);

	auto& pl = data.AccessParticles(0); // Dirty output.
	this->particles.SetVertexData(MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR, pl.GetVertexData(), pl.GetVertexDataStride());
	this->particles.SetColourData(MultiParticleDataCall::Particles::COLDATA_FLOAT_I, this->newColors.data(), sizeof(float));
	
	// Don't forget!
	delete tree;
	delete[] dataPtsData;
}

void mmvis_static::StructureEventsClusterKDTree::setSignedDistanceColor(float min, float max) {
	for (auto & element : this->particleList) {

		float borderTolerance = 6 * element.radius;

		// Border between liquid and gas.
		if (element.signedDistance >= 0 && element.signedDistance <= borderTolerance) {
			element.r = 0.f;
			element.g = .44f;
			element.b = .73f;
		}

		// Liquid.
		if (element.signedDistance > borderTolerance) {
			element.r = element.signedDistance / max;
			element.g = .44f;
			element.b = .73f;
		}

		// Gas.
		if (element.signedDistance < 0) {
			element.r = .98f;
			element.g = .78f;
			element.b = 0.f;
			//element.g = 1 - element.signedDistance / min; element.r = element.b = 0.f;
		}
	}
}

void mmvis_static::StructureEventsClusterKDTree::findNeighboursBySignedDistance() {
	uint64_t counter = 0;
	for (auto & particle : this->particleList) {
		if (particle.signedDistance < 0)
			continue; // Skip gas.

		float signedDistanceRange = particle.radius * 3;

		if (counter %10000 == 0)
			printf("Particle %d: Distance %f +/- %f. Position (%f, %f, %f). \n", particle.id, particle.signedDistance, signedDistanceRange, particle.x, particle.y, particle.z);

		for (auto & neighbour : this->particleList) {
			if (neighbour.signedDistance <= particle.signedDistance - signedDistanceRange)
				break; // Leave loop since list is sorted by Signed Distance and lower particle are not of interest.

			if (neighbour.signedDistance >= particle.signedDistance + signedDistanceRange)
				continue; // Skip particles above.

			if (neighbour.signedDistance < 0)
				continue; // Skip gas.

			if (neighbour.id == particle.id)
				continue; // Skip same particle.

			float criticalDistance = particle.radius * 3;

			if (fabs(neighbour.x - particle.x) > criticalDistance)
				continue; // Skip particle with bigger x distance.

			if (fabs(neighbour.y - particle.y) > criticalDistance)
				continue; // Skip particle with bigger y distance.

			if (fabs(neighbour.z - particle.z) > criticalDistance)
				continue; // Skip particle with bigger z distance.

			//float distanceSquare = powf(neighbour.x - particle.x, 2) + powf(neighbour.y - particle.y, 2) + powf(neighbour.z - particle.z, 2);
			//float radiusSquare = powf(particle.radius * 3, 2);

			//printf("Neighbour %d checked for particle %d with square distance %f at square radius %f.\n", neighbour.id, particle.id, distanceSquare, radiusSquare);

			//if (distanceSquare <= radiusSquare) {
				particle.neighbourIDs.push_back(neighbour.id);
				particle.neighbourPtrs.push_back(&neighbour);
				if (counter % 10000 == 0)
					printf("Neighbour %d added to particle %d with distance %f, %f, %f.\n", neighbour.id, particle.id, fabs(neighbour.x - particle.x), fabs(neighbour.y - particle.y), fabs(neighbour.z - particle.z));
					//printf("Neighbour %d added to particle %d with square distance %f at square radius %f.\n", neighbour.id, particle.id, distanceSquare, radiusSquare);
			//}
		}
		counter++;
	}
}

void mmvis_static::StructureEventsClusterKDTree::findNeighboursByKDTree() {
	// Grid oder kd Baum.
}

void mmvis_static::StructureEventsClusterKDTree::createClustersFastDepth() {


}
