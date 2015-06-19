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
#include "ANN/ANN.h"
#include <chrono>
#include <ctime>
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
void mmvis_static::StructureEventsClusterVisualization::setData(megamol::core::moldyn::MultiParticleDataCall& data) {
	using megamol::core::moldyn::MultiParticleDataCall;

	uint64_t globalParticleIndex = 0;
	float globalRadius;
	uint8_t globalColor[4];
	float globalColorIndexMin, globalColorIndexMax;

	//float signedDistanceMin = 0, signedDistanceMax = 0;

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
		/*
		if (particles.GetVertexDataType() == MultiParticleDataCall::Particles::VERTDATA_NONE
				|| particles.GetCount() == 0) {
			printf("Particlelist %d skipped, no vertex data.\n", particleListIndex); // Debug.
			continue; // Skip this particle list.
		}
		*/

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
		/*
		case MultiParticleDataCall::Particles::VERTDATA_SHORT_XYZ:
			vertexIsFloat = false;
			vertexStride = std::max<unsigned int>(vertexStride, 6);
			break;
			*/
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
			/*}
			else {
				const uint16_t *vertexPtr16 = reinterpret_cast<const uint16_t*>(vertexPtr);
				Particle particle;
				particle.x = static_cast<float>(vertexPtr16[0]);
				particle.y = static_cast<float>(vertexPtr16[1]);
				particle.z = static_cast<float>(vertexPtr16[2]);
				particle.radius = static_cast<float>(vertexPtr16[3]);
			}*/

			// Colour/Signed Distance.
			const float *colourPtrf = reinterpret_cast<const float*>(colourPtr);
			particle.signedDistance = *colourPtrf;


			// For testing if sorting works.
			/*
			if (particle.signedDistance > signedDistanceMax)
				signedDistanceMax = particle.signedDistance;
			if (particle.signedDistance < signedDistanceMin)
				signedDistanceMin = particle.signedDistance;
			*/

			// Set particle ID.
			particle.id = globalParticleIndex + particleIndex;

			// Add particle to list.
			particleList.push_back(particle);
		}
		globalParticleIndex += static_cast<size_t>(particles.GetCount());
	}
	//std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	//std::ctime(&t);

	logFile.open("SEClusterVis.log", std::ios_base::app | std::ios_base::out);

	{ // Time measurement. 4s
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_buildList);
			printf("Calculator: Created list with size %d after %lld ms\n", particleList.size(), duration.count());
	}

	findNeighboursWithKDTree();

	createClustersFastDepth();

	setClusterColor();

	///
	/// Sort list from greater to lower signedDistance.
	///
	/*
	auto time_sortList = std::chrono::system_clock::now();

	std::sort(this->particleList.begin(), this->particleList.end(), [](const Particle& lhs, const Particle& rhs) {
		return lhs.signedDistance > rhs.signedDistance;
	});
	{ // Time measurement. 77s!
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_sortList);
		printf("Calculator: Sorted particle list after %lld ms\n", duration.count());
	}
	*/
	

	// Find neighbours.
	//findNeighboursBySignedDistance();

	// Cluster creation.
	//createClustersSignedDistanceOnly();

	// Set alternative coloring.
	//setSignedDistanceColor(signedDistanceMin, signedDistanceMax);

	///
	/// Fill MultiParticleDataCall::Particles with data of the local container.
	///

	unsigned int particleStride = sizeof(Particle);

	// Debug.
	printf("Calculator: ParticleStride: %d, ParticleCount: %d, RandomParticleColor: (%f, %f, %f)\n",
		particleStride, globalParticleIndex, particleList[0].r, particleList[0].g, particleList[0].b);
	// For testing if sorting works.
	/*
	printf("Calculator: ParticleList SignedDistance max: %f, min: %f\n",
		this->particleList.front().signedDistance, this->particleList.back().signedDistance);
	printf("Calculator: Particle SignedDistance max: %f, min: %f\n",
		signedDistanceMax, signedDistanceMin);
	*/

	this->particles.SetCount(globalParticleIndex);
	this->particles.SetGlobalRadius(globalRadius);
	this->particles.SetGlobalColour(globalColor[0], globalColor[1], globalColor[2]);
	this->particles.SetColourMapIndexValues(globalColorIndexMin, globalColorIndexMax);

	this->particles.SetVertexData(MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR, &this->particleList[0], particleStride);
	this->particles.SetColourData(MultiParticleDataCall::Particles::COLDATA_FLOAT_RGB, &this->particleList[0].r, particleStride);
	
	// Don't forget!
	particleList.clear();

	logFile.close();
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

	///
	/// Frame 10:
	/// sqrRadius, maxNeighbours = 10 (enough, still a lot of skipped neighbours), 12 for >4*R
	/// 2*R, 156733 Cluster, 1639033 Particles w/o neighbour
	/// 3*R, 16051 Cluster, 43 Particles w/o neighbour, 380s (time cluster algo)
	/// 3.5*R, 14709 Cluster, 8 Particles w/o neighbour, 191772 ms (time cluster algo), smallest cluster: 2, biggest cluster: 112362
	/// 4*R, 14437 Cluster, 4 Particles w/o neighbour, 188043 ms (time cluster algo), smallest cluster: 2, biggest cluster: 111897
	/// ... see log file
	///
	/// 4*r, 35
	/// 5*r, 60

	int maxNeighbours = 35;
	int radiusModifier = 4;
	ANNdist sqrRadius = powf(radiusModifier * particleList[0].radius, 2);

	printf("Radius: %f. Max neighbours: %d\n", sqrRadius, maxNeighbours);	// Debug.
	this->logFile << radiusModifier << "*radius, " << maxNeighbours << " max neighbours ";

	for (auto & particle : this->particleList) {
		/*
		// Skip particles for faster testing.
		if (particle.id > 1000)
			break;
			*/
		ANNpoint q = new ANNcoord[3];
		q[0] = static_cast<ANNcoord>(particle.x);
		q[1] = static_cast<ANNcoord>(particle.y);
		q[2] = static_cast<ANNcoord>(particle.z);

		ANNidxArray   nn_idx = 0;
		ANNdistArray  dd = 0;

		nn_idx = new ANNidx[maxNeighbours];
		dd = new ANNdist[maxNeighbours];
		
		tree->annkFRSearch(
			q,				// the query point
			sqrRadius,		// squared radius of query ball
			maxNeighbours,	// number of neighbors to return
			nn_idx,			// nearest neighbor array (modified)
			dd				// dist to near neighbors as squared distance (modified)
			);				// error bound (optional).
		/*
		tree->annkSearch(
			q,				// the query point
			maxNeighbours,	// number of neighbors to return
			nn_idx,			// nearest neighbor array (modified)
			dd				// dist to near neighbors as squared distance (modified)
			);				// error bound (optional).
			*/
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
			particle.neighbourPtrs.push_back(&this->particleList[nn_idx[i]]);
		}
		if (debugAddedNeighbours % 100000 == 0)
			printf("Calculator progress: Neighbours: %d added, %d skipped.\n", debugAddedNeighbours, debugSkippedNeighbours); // Debug.
	}

	{ // Time measurement depends heavily on maxNeighbours and sqrRadius (when using annkFRSearch) as well as on save method in particleList.
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_findNeighbours);
		printf("Calculator: Neighbours set in %lld ms with %d added and %d skipped.\n", duration.count(), debugAddedNeighbours, debugSkippedNeighbours);

		// Log.
		this->logFile << "(added/skipped " << debugAddedNeighbours << "/" << debugSkippedNeighbours << " in " << duration.count() << " ms); ";
	}

	///
	/// Test.
	///
	uint64_t id = 1000;
	if (this->particleList[id].neighbourPtrs.size() > 0) {
		Particle nearestNeighbour = *this->particleList[id].neighbourPtrs[0];

		printf("Calculator: Particle %d at (%2f, %2f, %2f) nearest neighbour %d at (%2f, %2f, %2f).\n",
			particleList[id].id, particleList[id].x, particleList[id].y, particleList[id].z,
			nearestNeighbour.id, nearestNeighbour.x, nearestNeighbour.y, nearestNeighbour.z);
	}
}

void mmvis_static::StructureEventsClusterVisualization::findNeighboursBySignedDistance() {
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
				particle.neighbourPtrs.push_back(&_getParticle(neighbour.id));
				//particle.neighbourIDs.push_back(neighbour.id);
				if (counter % 10000 == 0)
					printf("Calculator progress: Neighbour %d added to particle %d with distance %f, %f, %f.\n", neighbour.id, particle.id, fabs(neighbour.x - particle.x), fabs(neighbour.y - particle.y), fabs(neighbour.z - particle.z));
					//printf("Neighbour %d added to particle %d with square distance %f at square radius %f.\n", neighbour.id, particle.id, distanceSquare, radiusSquare);
			//}
		}
		counter++;
	}
}

void mmvis_static::StructureEventsClusterVisualization::createClustersFastDepth() {
	auto time_createCluster = std::chrono::system_clock::now();

	size_t debugNoNeighbourCounter = 0;

	for (auto & particle : this->particleList) {
		if (particle.signedDistance < 0)
			continue; // Skip gas.

		if (particle.neighbourPtrs.size() == 0) {
			debugNoNeighbourCounter++;
			continue; // Skip particles without neighbours.
		}

		if (particle.clusterPtr)
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
			for (int i = 0; i < currentParticle.neighbourPtrs.size(); ++i) {
				if (currentParticle.neighbourPtrs[i]->signedDistance > signedDistance) {
					signedDistance = currentParticle.neighbourPtrs[i]->signedDistance;
					deepestNeighbour = *currentParticle.neighbourPtrs[i];
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

				// Create new cluster.
				if (clusterListIterator == this->clusterList.end()) { // Iterator at the end of the list means std::find didnt find match.
					Cluster cluster;
					cluster.rootParticleID = currentParticle.id;
					//cluster.numberOfParticles++;
					this->clusterList.push_back(cluster);
					clusterPtr = &clusterList.back();
				}
				// Point to existing cluster.
				else {
					//(*clusterListIterator).numberOfParticles++;
					clusterPtr = &(*clusterListIterator);
				}

				// Add particle to cluster.
				particle.clusterPtr = clusterPtr;
				(*clusterPtr).numberOfParticles++;

				// Add all found deepest neighbours to the cluster.
				for (auto &particleID : parsedParticleIDs) {
					this->particleList[particleID].clusterPtr = clusterPtr; // Requires ontouched (i.e. sorting forbidden) particleList!
					(*clusterPtr).numberOfParticles++;
				}

				// Debug/progress, to not loose patience when waiting for results.
				if (particle.id % 100000 == 0) {
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_addParticlePath);
					printf("Calculator progress: Cluster for particle %d in %lld ms with pathlength %d.\n Cluster elements: %d. Total number of clusters: %d\n",
						particle.id, duration.count(), parsedParticleIDs.size(), (*clusterPtr).numberOfParticles, this->clusterList.size());
				}
				break;
			}
			//printf("Particle %d with deepest neighbour %d.\n", currentParticle.id, deepestNeighbour.id);
		}
	}

	// Time measurement. 
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_createCluster);
	printf("Calculator: Clusters created in %lld ms with %d clusters and %d liquid particles without neighbour.\n", duration.count(), this->clusterList.size(), debugNoNeighbourCounter);
	// Debug.
	uint64_t minCluster = std::min_element(
		this->clusterList.begin(), this->clusterList.end(), [](const Cluster& lhs, const Cluster& rhs) {
			return lhs.numberOfParticles < rhs.numberOfParticles;
		})->numberOfParticles;
	uint64_t maxCluster = std::max_element(
		this->clusterList.begin(), this->clusterList.end(), [](const Cluster& lhs, const Cluster& rhs) {
			return lhs.numberOfParticles < rhs.numberOfParticles;
		})->numberOfParticles;
	printf("Smallest cluster: %d. Biggest cluster: %d.\n", minCluster, maxCluster);

	// Log.
	this->logFile << this->clusterList.size() << " clusters created (" << duration.count() << " ms) with min/max size " << minCluster << "/" << maxCluster << "; ";
	this->logFile << debugNoNeighbourCounter << " liquid particles w/o neighbour.\n";
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
			particle.clusterPtr = &clusterList.back();
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
				particle.clusterPtr = &cluster;
				break;
			}
		}

		// Create a new cluster since the particle is not in range of others.
		if (!particle.clusterPtr) {
			Cluster cluster;
			//cluster.id = clusterID;
			clusterID++;
			cluster.rootParticleID = particle.id;
			clusterList.push_back(cluster);
			particle.clusterPtr = &clusterList.back();
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

void mmvis_static::StructureEventsClusterVisualization::setClusterColor() {
	auto time_setClusterColor = std::chrono::system_clock::now();

	///
	/// Colourize cluster.
	///
	for (auto & cluster : this->clusterList) {
		// Set random color.
		std::random_device rd;
		std::mt19937_64 mt(rd());
		std::uniform_real_distribution<float> distribution(0, 1);
		cluster.r = distribution(mt);
		cluster.g = distribution(mt);
		cluster.b = distribution(mt);
	}


	///
	/// Colourize particles.
	///
	for (auto & particle : this->particleList) {

		// Gas. Orange.
		if (!particle.clusterPtr) {
			particle.r = .98f;
			particle.g = .78f;
			particle.b = 0.f;
			continue;
		}

		// Cluster.
		particle.r = particle.clusterPtr->r;
		particle.g = particle.clusterPtr->g;
		particle.b = particle.clusterPtr->b;

		if (particle.r < 0) // Debug wrong clusterPtr, points to nothing: particleID = -572662307 for all := points to the same adress.
			printf("%d, ", particle.clusterPtr->rootParticleID);
	}

	// Time measurement.
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_setClusterColor);
	printf("Calculator: Colorized %d clusters in %lld ms\n", this->clusterList.size(), duration.count());
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