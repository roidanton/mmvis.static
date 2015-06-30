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
#include "mmcore/param/BoolParam.h"
#include "ANN/ANN.h"
#include "vislib/math/Vector.h"
#include <chrono>
//#include <ctime>
#include <random>
#include <string>

using namespace megamol;
using namespace megamol::core;

/**
 * mmvis_static::StructureEventsClusterVisualization::StructureEventsClusterVisualization
 */
mmvis_static::StructureEventsClusterVisualization::StructureEventsClusterVisualization() : Module(),
	inDataSlot("in data", "Connects to the data source. Expects signed distance particles"),
	outDataSlot("out data", "Slot to request data from this calculation."),
	activateCalculationSlot("activateCalculation", "Triggers the calculation"),
	dataHash(0), frameId(0), treeSize(0) {

	this->inDataSlot.SetCompatibleCall<core::moldyn::MultiParticleDataCallDescription>();
	this->MakeSlotAvailable(&this->inDataSlot);

	this->outDataSlot.SetCallback("MultiParticleDataCall", "GetData", &StructureEventsClusterVisualization::getDataCallback);
	this->outDataSlot.SetCallback("MultiParticleDataCall", "GetExtent", &StructureEventsClusterVisualization::getExtentCallback);
	this->MakeSlotAvailable(&this->outDataSlot);

	this->activateCalculationSlot << new param::BoolParam(false);
	this->MakeSlotAvailable(&this->activateCalculationSlot);
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

	if (this->activateCalculationSlot.Param<param::BoolParam>()->Value()) {
		// Only calculate when inData has changed frame or hash (data has been manipulated).
		if ((this->frameId != inData.FrameID()) || (this->dataHash != inData.DataHash()) || (inData.DataHash() == 0)) {
			this->frameId = inData.FrameID();
			this->dataHash = inData.DataHash();
			this->setData(inData);
		}
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
void mmvis_static::StructureEventsClusterVisualization::setData(megamol::core::moldyn::MultiParticleDataCall& data) {
	using megamol::core::moldyn::MultiParticleDataCall;

	uint64_t globalParticleIndex = 0;
	float globalRadius;
	uint8_t globalColor[4];
	float globalColorIndexMin, globalColorIndexMax;

	//float signedDistanceMin = 0, signedDistanceMax = 0;

	///
	/// Copy old lists.
	///
	if (this->particleList.size() > 0) {
		this->previousParticleList = this->particleList;
		// Don't forget!
		this->particleList.clear();
	}
	if (this->clusterList.size() > 0) {
		this->previousClusterList = this->clusterList;
		// Don't forget!
		this->clusterList.clear();
	}

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
	particleList.reserve(globalParticleCnt);

	///
	/// Build list.
	///
	auto time_buildList = std::chrono::system_clock::now();

	for (unsigned int particleListIndex = 0; particleListIndex < data.GetParticleListCount(); particleListIndex++) {

		///
		/// Get meta information of data.
		///

		MultiParticleDataCall::Particles& particles = data.AccessParticles(particleListIndex);

		// Check for existing data.
		
		//if (particles.GetVertexDataType() == MultiParticleDataCall::Particles::VERTDATA_NONE
		//		|| particles.GetCount() == 0) {
		//	printf("Particlelist %d skipped, no vertex data.\n", particleListIndex); // Debug.
		//	continue; // Skip this particle list.
		//}
		

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
		//bool vertexIsFloat = true;
		bool hasRadius = false;
		switch (particles.GetVertexDataType()) { // VERTDATA_NONE tested above.
		case MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR:
			hasRadius = true;
			vertexStride = std::max<unsigned int>(vertexStride, 16);
			break;
		case MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZ:
			vertexStride = std::max<unsigned int>(vertexStride, 12);
			break;
		//case MultiParticleDataCall::Particles::VERTDATA_SHORT_XYZ:
		//	vertexIsFloat = false;
		//	vertexStride = std::max<unsigned int>(vertexStride, 6);
		//	break;
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

			Particle particle;

			// Vertex.
			//if (vertexIsFloat) { // Performance is lower with if-else in loop, however readability is higher.
				const float *vertexPtrf = reinterpret_cast<const float*>(vertexPtr);
				particle.x = vertexPtrf[0];
				particle.y = vertexPtrf[1];
				particle.z = vertexPtrf[2];
				particle.radius = hasRadius ? vertexPtrf[3] : globalRadius;
			//}
			//else {
			//	const uint16_t *vertexPtr16 = reinterpret_cast<const uint16_t*>(vertexPtr);
			//	Particle particle;
			//	particle.x = static_cast<float>(vertexPtr16[0]);
			//	particle.y = static_cast<float>(vertexPtr16[1]);
			//	particle.z = static_cast<float>(vertexPtr16[2]);
			//	particle.radius = static_cast<float>(vertexPtr16[3]);
			//}

			// Colour/Signed Distance.
			const float *colourPtrf = reinterpret_cast<const float*>(colourPtr);
			particle.signedDistance = *colourPtrf;


			// For testing if sorting works.
			//if (particle.signedDistance > signedDistanceMax)
			//	signedDistanceMax = particle.signedDistance;
			//if (particle.signedDistance < signedDistanceMin)
			//	signedDistanceMin = particle.signedDistance;

			// Set particle ID.
			particle.id = globalParticleIndex + particleIndex;

			// Add particle to list.
			particleList.push_back(particle);
		}
		globalParticleIndex += static_cast<size_t>(particles.GetCount());
		
	}
	
	this->logFile.open("SEClusterVis.log", std::ios_base::app | std::ios_base::out);
	this->debugFile.open("SEClusterVisDebug.log");
	
	{ // Time measurement. 4s
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_buildList);
			printf("Calculator: Created list with size %d after %lld ms\n", particleList.size(), duration.count());
	}
	
	findNeighboursWithKDTree();

	createClustersFastDepth();

	mergeSmallClusters(10);
	
	
	//setDummyLists();

	if (this->previousClusterList.size() > 0 && this->previousParticleList.size() > 0) {
		compareClusters();
		setClusterColor(false);
	}
	else {
		setClusterColor(true);
	}

	///
	/// Fill MultiParticleDataCall::Particles with data of the local container.
	///

	unsigned int particleStride = sizeof(Particle);

	// Debug.
	printf("Calculator: ParticleStride: %d, ParticleCount: %d, RandomParticleColor: (%f, %f, %f)\n",
		particleStride, globalParticleIndex, particleList[0].r, particleList[0].g, particleList[0].b);
	// For testing if sorting works.
	//printf("Calculator: ParticleList SignedDistance max: %f, min: %f\n",
	//	this->particleList.front().signedDistance, this->particleList.back().signedDistance);
	//printf("Calculator: Particle SignedDistance max: %f, min: %f\n",
	//	signedDistanceMax, signedDistanceMin);

	this->particles.SetCount(globalParticleIndex);
	this->particles.SetGlobalRadius(globalRadius);
	this->particles.SetGlobalColour(globalColor[0], globalColor[1], globalColor[2]);
	this->particles.SetColourMapIndexValues(globalColorIndexMin, globalColorIndexMax);

	this->particles.SetVertexData(MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR, &this->particleList[0], particleStride);
	this->particles.SetColourData(MultiParticleDataCall::Particles::COLDATA_FLOAT_RGB, &this->particleList[0].r, particleStride);
	
	// Data structure sizes.
	int unitConversion = 1024;
	size_t particleBytes = this->particleList.size() * sizeof(Particle) / unitConversion;
	size_t previousParticleBytes = this->previousParticleList.size() * sizeof(Particle) / unitConversion;
	size_t clusterBytes = this->clusterList.size() * sizeof(Cluster) / unitConversion;

	this->logFile << " - Sizes p/pp/cl/kd (kiB): ";
	this->logFile << particleBytes << "/" << previousParticleBytes << "/" << clusterBytes << "/" << this->treeSize / unitConversion;

	this->logFile << " - Frame " << this->frameId << "\n"; // For MMPLDs with single frame it is 0 of course. Alternatively data.FrameID() (returns same).
	this->debugFile.close();
	this->logFile.close();
}

void mmvis_static::StructureEventsClusterVisualization::findNeighboursWithKDTree() {

	///
	/// Create k-d-Tree.
	///
	/// nn_idx return value of search functions matches position in ANNpointArray,
	/// which matches the particle ID in particleList as long as particleList
	/// is not resorted!
	///
	auto time_buildTree = std::chrono::system_clock::now();

	ANNpointArray annPts = new ANNpoint[this->particleList.size()];
	uint64_t annPtsCounter = 0;
	for (auto & particle : this->particleList) {
		ANNpoint q = new ANNcoord[3];
		q[0] = static_cast<ANNcoord>(particle.x);
		q[1] = static_cast<ANNcoord>(particle.y);
		q[2] = static_cast<ANNcoord>(particle.z);
		annPts[annPtsCounter] = q;
		annPtsCounter++;
	}
	ANNkd_tree* tree = new ANNkd_tree(annPts, static_cast<int>(annPtsCounter), 3);

	this->treeSize = tree->nPoints() * sizeof(ANNcoord) * 3; // One ANNPoint consists of 3 ANNcoords here.

	{ // Time measurement. 7s
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_buildTree);
		printf("Calculator: Created k-d-tree after %lld ms\n", duration.count());
	}

	///
	/// Find and store neighbours.
	///
	auto time_findNeighbours = std::chrono::system_clock::now();

	uint64_t debugSkippedNeighbours = 0;
	uint64_t debugAddedNeighbours = 0;

	/// Minimal maxNeighbours for radius so no particle in radius gets excluded (determined by experiments):
	/// radiusModifier, maxNeighbours
	/// 4, 35
	/// 5, 60
	/// 6, 100
	/// 7, 155
	/// 10, 425
	/// 20, 3270
	bool debugSkipParticles = false;
	bool useFRSearch = true;
	int radiusModifier = 4;
	int maxNeighbours = 35;

	ANNdist sqrRadius = powf(radiusModifier * particleList[0].radius, 2);

	printf("Radius: %d (%.2f). Max neighbours: %d\n", radiusModifier, sqrRadius, maxNeighbours);	// Debug.
	if (useFRSearch)
		this->logFile << radiusModifier << "*radius, ";
	this->logFile << maxNeighbours << " max neighbours ";

	/// http://stackoverflow.com/questions/17848521/using-openmp-with-c11-range-based-for-loops
	for (auto & particle : this->particleList) {
		
		// Skip particles for faster testing.
		if (debugSkipParticles && particle.id > 1000)
			break;
			
		ANNpoint q = new ANNcoord[3];
		q[0] = static_cast<ANNcoord>(particle.x);
		q[1] = static_cast<ANNcoord>(particle.y);
		q[2] = static_cast<ANNcoord>(particle.z);

		ANNidxArray   nn_idx = 0;
		ANNdistArray  dd = 0;

		nn_idx = new ANNidx[maxNeighbours];
		dd = new ANNdist[maxNeighbours];
		
		if (useFRSearch)
			tree->annkFRSearch(
				q,				// the query point
				sqrRadius,		// squared radius of query ball
				maxNeighbours,	// number of neighbors to return
				nn_idx,			// nearest neighbor array (modified)
				dd				// dist to near neighbors as squared distance (modified)
				);				// error bound (optional).
		else
			tree->annkSearch(
				q,				// the query point
				maxNeighbours,	// number of neighbors to return
				nn_idx,			// nearest neighbor array (modified)
				dd				// dist to near neighbors as squared distance (modified)
				);				// error bound (optional).
			
		for (size_t i = 0; i < maxNeighbours; ++i) {
			if (nn_idx[i] == ANN_NULL_IDX) {
				debugSkippedNeighbours++;
				continue;
			}
			if (dd[i] < 0.001f) // Exclude self to catch ANN_ALLOW_SELF_MATCH = true.
				continue;

			debugAddedNeighbours++;
			//particle.neighbours.push_back(_getParticle(nn_idx[i])); // Up to 500ms/Particle + high memory consumption!
			//particle.neighbours.push_back(this->particleList[nn_idx[i]]); // Takes a lot of memory and time! At maximum maxNeighbours = 3 and 2*r works on test machine.

			// Requires untouched particleList but needs a lot less memory!
			//particle.neighbourPtrs.push_back(&this->particleList[nn_idx[i]]);
			particle.neighbourIDs.push_back(nn_idx[i]);
		}
		if (debugAddedNeighbours % 100000 == 0)
			printf("SECalc progress: Neighbours: %d added, %d skipped.\n", debugAddedNeighbours, debugSkippedNeighbours); // Debug.
	}

	{ // Time measurement depends heavily on maxNeighbours and sqrRadius (when using annkFRSearch) as well as on save method in particleList.
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_findNeighbours);
		printf("Calculator: Neighbours set in %lld ms with %d added and %d skipped.\n", duration.count(), debugAddedNeighbours, debugSkippedNeighbours);

		// Log.
		this->logFile << "(added/skipped " << debugAddedNeighbours << "/" << debugSkippedNeighbours << ") (" << duration.count() << " ms); ";
	}

	///
	/// Test.
	///
	uint64_t id = 1000;
	if (this->particleList[id].neighbourIDs.size() > 0) {
		//Particle nearestNeighbour = *this->particleList[id].neighbourPtrs[0];
		Particle nearestNeighbour = this->particleList[this->particleList[id].neighbourIDs[0]];

		printf("Calculator: Particle %d at (%2f, %2f, %2f) nearest neighbour %d at (%2f, %2f, %2f).\n",
			particleList[id].id, particleList[id].x, particleList[id].y, particleList[id].z,
			nearestNeighbour.id, nearestNeighbour.x, nearestNeighbour.y, nearestNeighbour.z);
	}
}


void mmvis_static::StructureEventsClusterVisualization::createClustersFastDepth() {
	auto time_createCluster = std::chrono::system_clock::now();

	size_t debugNoNeighbourCounter = 0;
	size_t debugUsedExistingClusterCounter = 0;

	/// Reallocation of clusterList (due to growth) makes pointer invalid. Either fix by:
	/// 1) give clusterList big size, so list doesnt resize.
	/// 2) change references to ids, clusterList mustn't be resorted but may be reallocated in memory.
	/// 3) use id's in struct Cluster (+ vector may be resorted, - access very slow)

	// Avoid reallocation of clusterList, method (1) - waste of a lot of memory or fails for too many clusters!
	this->clusterList.reserve(this->particleList.size() / 4);

	/// http://stackoverflow.com/questions/17848521/using-openmp-with-c11-range-based-for-loops
	/// OpenMP doesn't like continue, break.
	// #pragma omp parallel for
	// for (auto part = this->particleList.begin(); part < this->particleList.end(); part++) {
	//	auto particle = *part;
	for (auto & particle : this->particleList) {
		if (particle.signedDistance < 0)
			continue; // Skip gas.

		//if (particle.neighbourPtrs.size() == 0) {
		if (particle.neighbourIDs.size() == 0) {
			debugNoNeighbourCounter++;
			continue; // Skip particles without neighbours.
		}

		//if (particle.clusterPtr)
		if (particle.clusterID != -1)
			continue; // Skip particles that already belong to a cluster.

		///
		/// Find deepest neighbour.
		///
		auto time_addParticlePath = std::chrono::system_clock::now();

		// Container for deepest neighbours.
		std::vector<uint64_t> parsedParticleIDs;

		Particle deepestNeighbour = particle; // Initial condition.

		for (;;) {
			float signedDistance = 0; // For comparison.
			Particle currentParticle = deepestNeighbour; // Set last deepest particle as new current.
			//for (int i = 0; i < currentParticle.neighbourPtrs.size(); ++i) {
			//	if (currentParticle.neighbourPtrs[i]->signedDistance > signedDistance) {
			//		signedDistance = currentParticle.neighbourPtrs[i]->signedDistance;
			//		deepestNeighbour = *currentParticle.neighbourPtrs[i];
			//	}
			//}
			for (int i = 0; i < currentParticle.neighbourIDs.size(); ++i) {
				if (this->particleList[currentParticle.neighbourIDs[i]].signedDistance > signedDistance) {
					signedDistance = this->particleList[currentParticle.neighbourIDs[i]].signedDistance;
					deepestNeighbour = this->particleList[currentParticle.neighbourIDs[i]];
				}
			}

			// Add current deepest neighbour to the list containing all the parsed particles.
			parsedParticleIDs.push_back(deepestNeighbour.id);
			
			if (currentParticle.signedDistance >= deepestNeighbour.signedDistance) { // Current particle is local maximum.

				// Find cluster in list.
				uint64_t rootParticleID = currentParticle.id;
				std::vector<Cluster>::iterator clusterListIterator = std::find_if(this->clusterList.begin(), this->clusterList.end(), [rootParticleID](const Cluster& c) -> bool {
					return rootParticleID == c.rootParticleID;
				});

				Cluster* clusterPtr;
				
				//this->debugFile << "Particle " << particle.id; // Debugging black particles (clusterList got reallocated during creation).

				// Create new cluster.
				if (clusterListIterator == this->clusterList.end()) { // Iterator at the end of the list means std::find didnt find match.
					Cluster cluster;
					cluster.rootParticleID = currentParticle.id;
					cluster.id = static_cast<int> (this->clusterList.size());
					this->clusterList.push_back(cluster);
					clusterPtr = &clusterList.back();
					// this->debugFile << " created cluster at "; // Debugging black particles (clusterList got reallocated during creation).
				}
				// Point to existing cluster. 
				else {
					debugUsedExistingClusterCounter++;
					clusterPtr = &(*clusterListIterator); // Since iterator is never at v.end() here, it can be dereferenced.
					//this->debugFile << " added to cluster at "; // Debugging black particles (clusterList got reallocated during creation).
				}
				
				// Add particle to cluster.
				//particle.clusterPtr = clusterPtr;
				particle.clusterID = clusterPtr->id;
				(*clusterPtr).numberOfParticles++;
				
				// Add all found deepest neighbours to the cluster.
				for (auto &particleID : parsedParticleIDs) {
					//this->particleList[particleID].clusterPtr = clusterPtr;
					this->particleList[particleID].clusterID = clusterPtr->id; // Requires untouched (i.e. sorting forbidden) particleList!
					(*clusterPtr).numberOfParticles++;
				}
				
				//this->debugFile << clusterPtr << " with root " << (*clusterPtr).rootParticleID << ", parsed particles " << parsedParticleIDs.size() << " , number of particles " << (*clusterPtr).numberOfParticles << ".\n"; // Debugging black particles (clusterList got reallocated during creation).

				// Debug/progress, to not loose patience when waiting for results.
				if (particle.id % 100000 == 0) {
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_addParticlePath);
					auto durTotal = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_createCluster);
					printf("SECalc progress: Cluster for p %d (%lld ms, total %lld ms), pathlength %d.\nCluster elements: %d. Total number of clusters: %d\n",
						particle.id, duration.count(), durTotal.count(), parsedParticleIDs.size(), (*clusterPtr).numberOfParticles, this->clusterList.size());
				}
				break;
			}
			//printf("Particle %d with deepest neighbour %d.\n", currentParticle.id, deepestNeighbour.id); // Debug.
		}
	}

	// Time measurement and Debug.
	uint64_t minCluster = std::min_element(
		this->clusterList.begin(), this->clusterList.end(), [](const Cluster& lhs, const Cluster& rhs) {
		return lhs.numberOfParticles < rhs.numberOfParticles;
	})->numberOfParticles;
	uint64_t maxCluster = std::max_element(
		this->clusterList.begin(), this->clusterList.end(), [](const Cluster& lhs, const Cluster& rhs) {
		return lhs.numberOfParticles < rhs.numberOfParticles;
	})->numberOfParticles;

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_createCluster);
	printf("%d (%d/%d) clusters created in %lld ms. %d used existing clusters. %d liquid particles w/o neighbour.\n",
		this->clusterList.size(), minCluster, maxCluster, duration.count(), debugUsedExistingClusterCounter, debugNoNeighbourCounter);
		
	// Log.
	this->logFile << this->clusterList.size() << " cl (min/max " << minCluster << "/" << maxCluster << ") (" << duration.count() << " ms); ";
	this->logFile << debugUsedExistingClusterCounter << " p used existing cl; ";
	this->logFile << debugNoNeighbourCounter << " liquid p w/o neighbour; ";
}

void mmvis_static::StructureEventsClusterVisualization::mergeSmallClusters(int minClusterSize) {
	auto time_mergeClusters = std::chrono::system_clock::now();

	int mergedParticles = 0;

	std::vector<Cluster*> neighbourClusters;

	for (auto particle : this->particleList) {
		if (particle.signedDistance < 0)
			continue; // Skip gas.

		//if (particle.neighbourPtrs.size() == 0)
		if (particle.neighbourIDs.size() == 0)
			continue; // Skip particles without neighbours.

		//if (!particle.clusterPtr)
		if (particle.clusterID == -1)
			continue; // Skip particles w/o pointers, mandatory for test runs.

		//if (particle.clusterPtr->numberOfParticles >= minClusterSize)
		if (this->clusterList[particle.clusterID].numberOfParticles >= minClusterSize)
			continue; // Skip particles of bigger clusters.

		// Add clusters in range which are bigger than minClusterSize.
		//for (auto neighbour : particle.neighbourPtrs) {
		for (auto neighbourID : particle.neighbourIDs) {
			Particle* neighbour = &this->particleList[neighbourID];

			//if (!neighbour->clusterPtr)
			if (neighbour->clusterID == -1)
				continue; // Skip particles w/o pointers, mandatory for test runs.

			//if (neighbour->clusterPtr->rootParticleID != particle.clusterPtr->rootParticleID
			//	&& neighbour->clusterPtr->numberOfParticles >= minClusterSize)
			//	neighbourClusters.push_back(neighbour->clusterPtr);
			if (this->clusterList[neighbour->clusterID].rootParticleID != this->clusterList[particle.clusterID].rootParticleID
				&& this->clusterList[neighbour->clusterID].numberOfParticles >= minClusterSize)
				neighbourClusters.push_back(&this->clusterList[neighbour->clusterID]);
		}

		// If no clusters in range, check neighbours of neighbours.
		if (neighbourClusters.size() == 0) {

			//#pragma omp parallel for
			for (auto neighbourIT = particle.neighbourIDs.begin(); neighbourIT < particle.neighbourIDs.end(); ++neighbourIT) {
				Particle* neighbour = &this->particleList[*neighbourIT];
			//for (auto neighbour : particle.neighbourPtrs) {
			//for (auto neighbourID : particle.neighbourIDs) {
			//	Particle* neighbour = &this->particleList[neighbourID];

				// Check neighbours of neighbours.
				//for (auto secondaryNeighbour : neighbour->neighbourPtrs) {
				for (auto secondaryNeighbourID : neighbour->neighbourIDs) {
					Particle* secondaryNeighbour = &this->particleList[secondaryNeighbourID];

					//if (!secondaryNeighbour->clusterPtr)
					if (secondaryNeighbour->clusterID == -1)
						continue; // Skip particles w/o pointers, mandatory for test runs.

					//if (secondaryNeighbour->clusterPtr->rootParticleID != particle.clusterPtr->rootParticleID
					//	&& secondaryNeighbour->clusterPtr->numberOfParticles >= minClusterSize)
					//	neighbourClusters.push_back(secondaryNeighbour->clusterPtr);
					if (this->clusterList[secondaryNeighbour->clusterID].rootParticleID != this->clusterList[particle.clusterID].rootParticleID
						&& this->clusterList[secondaryNeighbour->clusterID].numberOfParticles >= minClusterSize)
						neighbourClusters.push_back(&this->clusterList[secondaryNeighbour->clusterID]);
				}
			}
		}
			
		// If still no other clusters are in range it is assumed there are no connected other clusters, so this particle stays in the small cluster.
		if (neighbourClusters.size() == 0)
			continue;

		///
		/// Determine new cluster.
		///
		
		//this->debugFile << "Angle: ";

		Cluster* newClusterPtr;
		double M_PI = 3.14159265358979323846;
		double smallestAngle = 2*M_PI;

		// Direction of particle to its root.
		Particle particleClusterRoot = this->particleList[this->clusterList[particle.clusterID].rootParticleID];
		vislib::math::Vector<float, 3> dirParticle;
		dirParticle.SetX(particle.x - particleClusterRoot.x);
		dirParticle.SetY(particle.y - particleClusterRoot.y);
		dirParticle.SetZ(particle.z - particleClusterRoot.z);

		// Direction of particle to its neighbour clusters roots.
		for (auto clusterPtr : neighbourClusters) {
			Particle clusterRoot = this->particleList[clusterPtr->rootParticleID]; // Requires untouched (i.e. sorting forbidden) particleList!
			vislib::math::Vector<float, 3> dirNeighbourCluster;
			dirNeighbourCluster.SetX(particle.x - clusterRoot.x);
			dirNeighbourCluster.SetY(particle.y - clusterRoot.y);
			dirNeighbourCluster.SetZ(particle.z - clusterRoot.z);

			// Get angle.
			double angle = dirParticle.Angle(dirNeighbourCluster);

			
			if (angle > M_PI)
				angle -= 2 * M_PI;

			// Smallest angle.
			if (angle < smallestAngle) {
				newClusterPtr = clusterPtr; // Requires untouched (i.e. sorting forbidden) particleList!
				smallestAngle = angle;
			}
			//this->debugFile << angle << ", ";
		}

		//this->debugFile << "\nSmallest Angle: " << smallestAngle << ".\n";

		// Eventually set new cluster.
		this->clusterList[particle.clusterID].numberOfParticles--;
		particle.clusterID = newClusterPtr->id;
		newClusterPtr->numberOfParticles++;

		mergedParticles++;
	}


	///
	/// No deletion of clusters here to not fuck up
	/// referencing by vector indices. Instead clusters
	/// should be ignored by the compareCluster function.
	///

	// Count min clusters for log.
	int minClusters = 0;
	for (auto & cluster : this->clusterList) {
		if (cluster.numberOfParticles == 0) {
			minClusters++;
		}
	}

	// Time measurement depends heavily on maxNeighbours and sqrRadius (when using annkFRSearch) as well as on save method in particleList.
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_mergeClusters);
	printf("Calculator: %d particles merged (%lld ms).\n", mergedParticles, duration.count());

	// Log.
	this->logFile << mergedParticles << " p merged, " << minClusters << " min cl removed (" << duration.count() << " ms), ";

	///
	/// Method 1: Search for nearest cluster (= root particle)
	/// Bad since connected components would be needed, too see if cluster is adjacent.
	/// 
	/*
	int rootParticleIndex = 0;
	ANNpoint rootPts = new ANNcoord[3 * this->clusterList.size()]; // Bigger than needed.
	
	std::vector<Cluster> minClusters;
	minClusters.reserve(this->clusterList.size()); // Bigger than needed to avoid reallocation.

	for (auto cluster : this->clusterList) {
		if (cluster.numberOfParticles < minClusterSize) {
			minClusters.push_back(cluster);
			continue;
		}

		// Add root particles of remaining clusters to ptsList for kdTree.
		Particle root = this->particleList[cluster.rootParticleID]; // Requires untouched (i.e. sorting forbidden) particleList!
		rootPts[rootParticleIndex * 3 + 0] = static_cast<ANNcoord>(root.x);
		rootPts[rootParticleIndex * 3 + 1] = static_cast<ANNcoord>(root.y);
		rootPts[rootParticleIndex * 3 + 2] = static_cast<ANNcoord>(root.z);
	}

	// Build kdTree with root particles from remaining clusters.
	ANNpointArray rootPtsArray = new ANNpoint[rootParticleIndex];
	for (size_t i = 0; i < rootParticleIndex; ++i) {
		rootPtsArray[i] = rootPts + (i * 3);
	}
	ANNkd_tree* tree = new ANNkd_tree(rootPtsArray, static_cast<int>(rootParticleIndex), 3);

	// Get nearest and connected remaining cluster for minCluster with kdTree.
	for (auto cluster : minClusters) {
		int maxNeighbours = 5;

		Particle root = this->particleList[cluster.rootParticleID]; // Requires untouched (i.e. sorting forbidden) particleList!

		ANNpoint q = new ANNcoord[3];
		q[0] = static_cast<ANNcoord>(root.x);
		q[1] = static_cast<ANNcoord>(root.y);
		q[2] = static_cast<ANNcoord>(root.z);

		ANNidxArray   nn_idx = 0;
		ANNdistArray  dd = 0;

		nn_idx = new ANNidx[maxNeighbours];
		dd = new ANNdist[maxNeighbours];

		tree->annkSearch(
			q,				// the query point
			maxNeighbours,	// number of neighbors to return
			nn_idx,			// nearest neighbor array (modified)
			dd				// dist to near neighbors as squared distance (modified)
			);				// error bound (optional).

		for (size_t i = 0; i < maxNeighbours; ++i) {
			if (nn_idx[i] == ANN_NULL_IDX) {
				continue;
			}
			if (dd[i] < 0.001f) // Exclude self to catch ANN_ALLOW_SELF_MATCH = true.
				continue;

			// Check connected component.
			if (connected) {
				// Merge clusters by altering particle pointers.
				break;
			}
		}

	}
	*/
}


void mmvis_static::StructureEventsClusterVisualization::compareClusters() {
	std::ofstream compareAllFile;
	std::ofstream compareSummaryFile;
	std::string filenameEnd = " f" + std::to_string(this->frameId) + " p" + std::to_string(this->particleList.size());
	filenameEnd += ".log";
	std::string filename = "SEClusterVisCompareAll" + filenameEnd;
	compareAllFile.open(filename.c_str());
	filename = "SEClusterVisCompareSummary" + filenameEnd;
	compareSummaryFile.open(filename.c_str());

	if (this->previousParticleList.size() == 0 || this->previousClusterList.size() == 0) {
		printf("Quit compareClusters. Nothing to compare!\n");
		return;
	}

	auto time_compareClusters = std::chrono::system_clock::now();

	///
	/// Create a compareMatrix to see how many particles the clusters have in common.
	/// The inner vector contains the columns, the outer vector contains the rows.
	/// The columns represents the previous clusters. Column id == cluster id of previous clusterList.
	/// The rows represents the current clusters. Row id == cluster id of current clusterList.
	/// Access by clusterCompareMatrix[row][column].
	///

	std::vector<std::vector<int>> clusterCompareMatrix;
	clusterCompareMatrix.resize(this->clusterList.size(), std::vector<int>(this->previousClusterList.size(), 0));

	//#pragma omp parallel for
	for (int pid = 0; pid < this->particleList.size(); ++pid) {
		// Possible race condition!
		if (this->particleList[pid].clusterID != -1 && this->previousParticleList[pid].clusterID != -1) // Skip gas.
			clusterCompareMatrix[this->particleList[pid].clusterID][this->previousParticleList[pid].clusterID]++;
	}

	///
	/// Detecting the type of event.
	/// First checking all new clusters for each previous cluster,
	/// then vice versa.
	///

	std::vector<ConnectedClusters> connectedClusterListForward;
	std::vector<ConnectedClusters> connectedClusterListBackwards;

	///
	/// Forward check.
	///
	for (int pcid = 0; pcid < this->previousClusterList.size(); ++pcid) {
		
		if (this->previousClusterList[pcid].numberOfParticles == 0)
			continue; // Skip empty (merged) clusters.

		// Debug.
		ConnectedClusters connectedClusters;
		connectedClusters.cluster = this->previousClusterList[pcid];

		for (int cid = 0; cid < this->clusterList.size(); ++cid) {

			if (this->clusterList[cid].numberOfParticles == 0)
				continue; // Skip empty (merged) clusters.

			int numberOfCommonParticles = clusterCompareMatrix[cid][pcid];

			// Debug.
			if (numberOfCommonParticles > 0) {
				connectedClusters.addConnected(this->clusterList[cid], numberOfCommonParticles);
			}
		}

		// Debug.
		compareAllFile << "Previous cluster " << pcid;
		compareAllFile << ", " << connectedClusters.cluster.numberOfParticles << " particles";
		compareAllFile << ", " << connectedClusters.getTotalCommonParticles() << " common particles (" << connectedClusters.getTotalCommonPercentage() << "%)";
		compareAllFile << ", " << connectedClusters.getLocalTotalRatio() << " % max to total ratio";
		compareAllFile << ", common particles min/max (" << connectedClusters.getMinCommonParticles() << ", " << connectedClusters.getMaxCommonParticles() << ")";
		compareAllFile << ", percentage min/max (" << connectedClusters.getMinCommonPercentage() << "%, " << connectedClusters.getMaxCommonPercentage() << "%)";
		compareAllFile << ", " << connectedClusters.getNumberOfConnected() << " connected clusters";
		compareAllFile << "\n";

		connectedClusters.sortConnected();

		for (int i = 0; i < connectedClusters.getNumberOfConnected(); ++i) {
			ConnectedClusters::ConnectedCluster cc = connectedClusters.getConnected(i);
			compareAllFile << "Cluster " << cc.cluster.id << " (size " << cc.cluster.numberOfParticles << "): " << cc.commonParticles << " common";
			compareAllFile << ", ratio this/global (" << cc.getCommonPercentage() << "%, " << cc.getClusterCommonPercentage(connectedClusters.cluster) << "%)";
			compareAllFile << "\n";
		}
		compareAllFile << "\n";
		connectedClusterListForward.push_back(connectedClusters);
	}


	///
	/// Backwards check.
	///
	for (int cid = 0; cid < this->clusterList.size(); ++cid) {

		if (this->clusterList[cid].numberOfParticles == 0)
			continue; // Skip empty (merged) clusters.

		// Debug.
		ConnectedClusters connectedClusters;
		connectedClusters.cluster = this->clusterList[cid];

		for (int pcid = 0; pcid < this->previousClusterList.size(); ++pcid) {

			if (this->previousClusterList[pcid].numberOfParticles == 0)
				continue; // Skip empty (merged) clusters.

			int numberOfCommonParticles = clusterCompareMatrix[cid][pcid];

			// Debug.
			if (numberOfCommonParticles > 0) {
				connectedClusters.addConnected(this->previousClusterList[pcid], numberOfCommonParticles);
			}
		}

		// Debug.
		compareAllFile << "Cluster " << cid;
		compareAllFile << ", " << connectedClusters.cluster.numberOfParticles << " particles";
		compareAllFile << ", " << connectedClusters.getTotalCommonParticles() << " common particles (" << connectedClusters.getTotalCommonPercentage() << "%)";
		compareAllFile << ", " << connectedClusters.getLocalTotalRatio() << " % max to total ratio";
		compareAllFile << ", common particles min/max (" << connectedClusters.getMinCommonParticles() << ", " << connectedClusters.getMaxCommonParticles() << ")";
		compareAllFile << ", percentage min/max (" << connectedClusters.getMinCommonPercentage() << "%, " << connectedClusters.getMaxCommonPercentage() << "%)";
		compareAllFile << ", " << connectedClusters.getNumberOfConnected() << " connected clusters";
		compareAllFile << "\n";

		connectedClusters.sortConnected();

		for (int i = 0; i < connectedClusters.getNumberOfConnected(); ++i) {
			ConnectedClusters::ConnectedCluster cc = connectedClusters.getConnected(i);
			compareAllFile << "Previous cluster " << cc.cluster.id << " (size " << cc.cluster.numberOfParticles << "): " << cc.commonParticles << " common";
			compareAllFile << ", ratio this/global (" << cc.getCommonPercentage() << "%, " << cc.getClusterCommonPercentage(connectedClusters.cluster) << "%)";
			compareAllFile << "\n";
		}
		compareAllFile << "\n";

		connectedClusterListBackwards.push_back(connectedClusters);
	}

	///
	/// Summary evaluation: Max percentage.
	///
	auto maxPercentageFwdIT = std::max_element(connectedClusterListForward.begin(), connectedClusterListForward.end(), [](const ConnectedClusters& lhs, const ConnectedClusters& rhs) {
		return lhs.getTotalCommonPercentage() < rhs.getTotalCommonPercentage();
	});
	compareSummaryFile << "Forward: " << maxPercentageFwdIT->cluster.id << " maximal percentage " << maxPercentageFwdIT->getTotalCommonPercentage() << "% and LocalTotalRatio " << maxPercentageFwdIT->getLocalTotalRatio() << "%\n";
	
	auto maxPercentageBwIT = std::max_element(connectedClusterListBackwards.begin(), connectedClusterListBackwards.end(), [](const ConnectedClusters& lhs, const ConnectedClusters& rhs) {
		return lhs.getTotalCommonPercentage() < rhs.getTotalCommonPercentage();
	});
	compareSummaryFile << "Backwards: " << maxPercentageBwIT->cluster.id << " maximal percentage " << maxPercentageBwIT->getTotalCommonPercentage() << "% and LocalTotalRatio " << maxPercentageBwIT->getLocalTotalRatio() << "%\n";

	///
	/// Summary evaluation: Most common/uncommon clusters, critical values have to be evaluated depending on:
	/// - how many clusters are close to MinPercentage
	/// - how many clusters are close to MaxPercentage
	/// - how many clusters are close to median.
	///
	/// Number of clusters with TotalCommonPercentage > 70%, 50%, 30%, ... . Fwd: Detect split, bw: Detect merge.
	/// Number of clusters with TotalCommonPercentage < 5%, 15%, 25%. Fwd: Detect birth, bw: Detect death.
	/// Specific numbers have to be tested!
	///

	///
	/// Summary evaluation: Number of biggest partner clusters:
	/// Ratio clusterCommonPercentage / TotalCommonPercentage > 25%.
	///



	///
	/// Naive coloring by comparing clusterIDs.
	///
	#pragma omp parallel for
	for (int i = 0; i < this->clusterList.size(); ++i) {
		if (i < this->previousClusterList.size()) {
			clusterList[i].r = previousClusterList[i].r;
			clusterList[i].g = previousClusterList[i].g;
			clusterList[i].b = previousClusterList[i].b;
		}
		else {
			// Set random color.
			std::random_device rd;
			std::mt19937_64 mt(rd());
			std::uniform_real_distribution<float> distribution(0, 1);
			clusterList[i].r = distribution(mt);
			clusterList[i].g = distribution(mt);
			clusterList[i].b = distribution(mt);
		}
	}

	// Time measurement.
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_compareClusters);
	printf("Calculator: Compared clusters (%lld ms).\n", duration.count());

	compareAllFile.close();
	compareSummaryFile.close();
}


void mmvis_static::StructureEventsClusterVisualization::setClusterColor(bool renewClusterColors) {
	auto time_setClusterColor = std::chrono::system_clock::now();

	size_t debugBlackParticles = 0;

	///
	/// Colourize cluster.
	///
	if (renewClusterColors) {
		for (auto & cluster : this->clusterList) {
			// Set random color.
			std::random_device rd;
			std::mt19937_64 mt(rd());
			std::uniform_real_distribution<float> distribution(0, 1);
			cluster.r = distribution(mt);
			cluster.g = distribution(mt);
			cluster.b = distribution(mt);
			if (cluster.rootParticleID < 0) { // Debug.
				printf("Cl: %d.\n", cluster.rootParticleID);
			}
		}
	}

	///
	/// Colourize particles.
	///
	for (auto & particle : this->particleList) {

		// Gas. Orange.
		if (particle.clusterID == -1) {
			particle.r = .98f;
			particle.g = .78f;
			particle.b = 0.f;
			continue;
		}

		// Cluster.
		particle.r = this->clusterList[particle.clusterID].r;
		particle.g = this->clusterList[particle.clusterID].g;
		particle.b = this->clusterList[particle.clusterID].b;

		/*
		if (particle.r < 0) { // Debug wrong clusterPtr, points to nothing: ints = -572662307; floats = -1998397155538108400.000000 for all -> points to reallocated address!
			debugBlackParticles++;
			if (particle.id % 100 == 0)
				printf("Part: %d, %d (%f, %f, %f).\n", this->clusterList[particle.clusterID].rootParticleID, this->clusterList[particle.clusterID].numberOfParticles, particle.r, particle.g, particle.b);
		}
		*/
	}

	// Time measurement.
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_setClusterColor);
	printf("Calculator: Colorized %d clusters in %lld ms with %d black particles.\n", this->clusterList.size(), duration.count(), debugBlackParticles);
	this->logFile << debugBlackParticles << " black p.";
}


void mmvis_static::StructureEventsClusterVisualization::setSignedDistanceColor(float min, float max) {
	for (auto & element : this->particleList) {

		float borderTolerance = 6 * element.radius;

		// Border between liquid and gas. Blue.
		if (element.signedDistance >= 0 && element.signedDistance <= borderTolerance) {
			element.r = 0.f;
			element.g = .44f;
			element.b = .73f;
		}

		// Liquid. Blue.
		if (element.signedDistance > borderTolerance) {
			element.r = element.signedDistance / max;
			element.g = .44f;
			element.b = .73f;
		}

		// Gas. Orange.
		if (element.signedDistance < 0) {
			element.r = .98f;
			element.g = .78f;
			element.b = 0.f;
			//element.g = 1 - element.signedDistance / min; element.r = element.b = 0.f;
		}
	}
}


void mmvis_static::StructureEventsClusterVisualization::sortBySignedDistance() {
	///
	/// Sort list from greater to lower signedDistance.
	///
	auto time_sortList = std::chrono::system_clock::now();

	std::sort(this->particleList.begin(), this->particleList.end(), [](const Particle& lhs, const Particle& rhs) {
		return lhs.signedDistance > rhs.signedDistance;
	});

	// Time measurement. 77s!
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_sortList);
	printf("Calculator: Sorted particle list after %lld ms\n", duration.count());
}


void mmvis_static::StructureEventsClusterVisualization::setDummyLists() {
	int particleAmount = 1000;
	int clusterAmount = 10;
	
	this->particleList.resize(particleAmount);
	this->previousParticleList.resize(particleAmount);
	this->clusterList.resize(clusterAmount);
	this->previousClusterList.resize(clusterAmount);

	uint64_t numberOfParticlesInCluster = 0;

	#pragma omp parallel for
	for (int i = 0; i < clusterAmount; ++i) {
		this->clusterList[i].id = i;
		this->previousClusterList[i].id = i;

		// Set number of particles.
		uint64_t maxParticles = (particleAmount - numberOfParticlesInCluster) / clusterAmount;
		std::random_device rd;
		std::mt19937_64 mt(rd());
		std::uniform_int_distribution<uint64_t> distribution(1, maxParticles);
		clusterList[i].numberOfParticles = distribution(mt);
		numberOfParticlesInCluster += clusterList[i].numberOfParticles;

		uint64_t streuung = this->clusterList[i].numberOfParticles / 10;
		std::uniform_int_distribution<uint64_t> distribution2(this->clusterList[i].numberOfParticles - streuung, this->clusterList[i].numberOfParticles + streuung);
		this->previousClusterList[i].numberOfParticles = distribution2(mt);
	}

	#pragma omp parallel for
	for (int i = 0; i < particleAmount; ++i) {
		this->particleList[i].id = i;
		this->previousParticleList[i].id = i;

		// Set cluster id.
		std::random_device rd;
		std::mt19937_64 mt(rd());
		std::uniform_int_distribution<int> distribution(0, clusterAmount - 1);
		this->particleList[i].clusterID = distribution(mt);

		int streuung = 1;
		std::uniform_int_distribution<int> distribution2(std::max(0, this->particleList[i].clusterID - streuung), std::min(9, this->particleList[i].clusterID + streuung));
		this->previousParticleList[i].clusterID = distribution2(mt);
	}
}


void mmvis_static::StructureEventsClusterVisualization::findNeighboursBySignedDistance() {
	uint64_t counter = 0;
	for (auto & particle : this->particleList) {
		if (particle.signedDistance < 0)
			continue; // Skip gas.

		float signedDistanceRange = particle.radius * 3;

		if (counter % 10000 == 0)
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

			float criticalDistance = particle.radius * 2;

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
			//particle.neighbourPtrs.push_back(&_getParticle(neighbour.id));
			particle.neighbourIDs.push_back(neighbour.id);
			if (counter % 10000 == 0)
				printf("SECalc progress: Neighbour %d added to particle %d with distance %f, %f, %f.\n", neighbour.id, particle.id, fabs(neighbour.x - particle.x), fabs(neighbour.y - particle.y), fabs(neighbour.z - particle.z));
			//printf("Neighbour %d added to particle %d with square distance %f at square radius %f.\n", neighbour.id, particle.id, distanceSquare, radiusSquare);
			//}
		}
		counter++;
	}
}


void mmvis_static::StructureEventsClusterVisualization::createClustersSignedDistanceOnly() {

	uint64_t clusterID = 0;
	uint64_t listIteration = 0;

	// Naive cluster detection.
	for (auto & particle : this->particleList) {
		// Gas particle doesnt need to be in a cluster.
		if (particle.signedDistance < 0)
			continue;

		// Create new cluster if there is none.
		if (clusterList.size() == 0) {
			Cluster cluster;
			//cluster.id = clusterID;
			clusterID++;
			cluster.rootParticleID = particle.id;
			clusterList.push_back(cluster);
			particle.clusterID = static_cast<int> (clusterList.size());
			//particle.clusterPtr = &clusterList.back();
			continue;
		}

		// Put the particle in the first cluster in range.
		for (auto cluster : this->clusterList) {
			// Find particle in list
			uint64_t rootParticleID = cluster.rootParticleID;
			std::vector<Particle>::iterator particleListIterator = std::find_if(this->particleList.begin(), this->particleList.end(), [rootParticleID](const Particle& p) -> bool {
				return true; // return p.id == rootParticleID;
			});
			if (isInSameComponent(*particleListIterator, particle)) {
				//printf("Particle %d added to cluster %d\n", particle.id, cluster.id);
				//particle.clusterPtr = &cluster;
				particle.clusterID = cluster.id;
				break;
			}
		}

		// Create a new cluster since the particle is not in range of others.
		//if (!particle.clusterPtr) {
		if (particle.clusterID == -1) {
			Cluster cluster;
			//cluster.id = clusterID;
			clusterID++;
			cluster.rootParticleID = particle.id;
			clusterList.push_back(cluster);
			particle.clusterID = static_cast<int> (clusterList.size());
			//particle.clusterPtr = &clusterList.back();
		}
		// Debug.
		if (clusterID % 1000 == 0)
			printf("Cluster ID: %d\n", clusterID);
		if (listIteration % 10000 == 0)
			printf("List Iteration ID: %d\n", listIteration);
		listIteration++;
	}
}


bool mmvis_static::StructureEventsClusterVisualization::isInSameComponent(
	const mmvis_static::StructureEventsClusterVisualization::Particle &referenceParticle,
	const mmvis_static::StructureEventsClusterVisualization::Particle &particle) const {

	// Avoid sqrt since calculation cost is very high.
	float physicalDistance = powf(referenceParticle.x - particle.x, 2) + powf(referenceParticle.y - particle.y, 2) + powf(referenceParticle.z - particle.z, 2);
	float depthDistance = referenceParticle.signedDistance - particle.signedDistance;

	return physicalDistance <= powf(depthDistance * (2 * particle.radius) + 2 * particle.radius, 2);
}


mmvis_static::StructureEventsClusterVisualization::Particle
mmvis_static::StructureEventsClusterVisualization::_getParticle (const uint64_t particleID) const {
	auto time_getParticle = std::chrono::system_clock::now();

	for (auto & particle : this->particleList) {
		if (particle.id == particleID) {

			// Time measurement.
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_getParticle);
			printf("Calculator: Particle found in %lld ms\n", duration.count());

			return particle;
		}
	}
	// Missing return statement!
}
/*
mmvis_static::StructureEventsClusterVisualization::Cluster*
mmvis_static::StructureEventsClusterVisualization::_getCluster(const uint64_t rootParticleID) const {
	auto time_getCluster = std::chrono::system_clock::now();

	for (auto &cluster : this->clusterList) {
		if (cluster.rootParticleID == rootParticleID) {

			// Time measurement.
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_getCluster);
			printf("Calculator: Particle found in %lld ms\n", duration.count());

			return *cluster;
		}
	}
	return nullptr;
}*/