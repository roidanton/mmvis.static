/**
 * StructureEventsClusterVisualization.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Copyright (C) 2015 by Richard Hähne, TU Dresden
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "StructureEventsClusterVisualization.h"

#include "ANN/ANN.h"
#include "mmcore/param/BoolParam.h"
#include "mmcore/param/FloatParam.h"
#include "mmcore/param/IntParam.h"
#include "mmcore/param/StringParam.h"
#include "vislib/math/Vector.h"
#include "vislib/sys/Log.h"

#include <chrono>
#include <functional>
#include <numeric>
#include <random>
#include <string>
#include <time.h>

// Non OpenMP concurrency
//#include <ppl.h>
//#include <thread>
//#include <mutex>

using namespace megamol;
using namespace megamol::core;

/**
 * mmvis_static::StructureEventsClusterVisualization::StructureEventsClusterVisualization
 */
mmvis_static::StructureEventsClusterVisualization::StructureEventsClusterVisualization() : Module(),
	inDataSlot("in data", "Connects to the data source. Expects signed distance particles"),
	outDataSlot("out data", "Slot to request data from this calculation."),
	outSEDataSlot("out SE data", "Slot to request StructureEvents data from this calculation."),
	outputLabelSlot("outputLabel", "A label to tag data in output files."),
	quantitativeDataOutputSlot("quantitativeDataOutput", "Create log files with quantitative data."),
	calculationActiveSlot("active", "Switch the calculation on/off (once started it will last until finished)."),
	createDummyTestDataSlot("createDummyTestData", "Creates random previous and current data. For I/O tests. Skips steps 1 and 2."),
	periodicBoundaryConditionSlot("NeighbourSearch::periodicBoundary", "Periodic boundary condition for dataset."),
	radiusMultiplierSlot("NeighbourSearch::radiusMultiplier", "The multiplicator for the particle radius definining the area for the neighbours search."),
	msMinClusterAmountSlot("StructureEvents::msMinClusterAmount", "Minimal number of clusters for merge/split event detection."),
	msMinCPPercentageSlot("StructureEvents::msMinCPPercentage", "Minimal ratio of common particles of each cluster for merge/split event detection."),
	bdMaxCPPercentageSlot("StructureEvents::bdMaxCPPercentage", "Maximal ratio of common particles for birth/death event detection."),
	minClusterSizeSlot("ClusterCreation::minClusterSize", "Minimal allowed cluster size in connected components, smaller clusters will be merged with bigger clusters if possible."),
	dataHash(0), sedcHash(0), seMaxTimeCache(0), frameId(0), treeSizeOutputCache(0), gasColor({ .98f, .78f, 0.f }) {

	this->inDataSlot.SetCompatibleCall<core::moldyn::MultiParticleDataCallDescription>();
	this->MakeSlotAvailable(&this->inDataSlot);

	this->outDataSlot.SetCallback("MultiParticleDataCall", "GetData", &StructureEventsClusterVisualization::getDataCallback);
	this->outDataSlot.SetCallback("MultiParticleDataCall", "GetExtent", &StructureEventsClusterVisualization::getExtentCallback);
	this->MakeSlotAvailable(&this->outDataSlot);

	this->outSEDataSlot.SetCallback("StructureEventsDataCall", "GetData", &StructureEventsClusterVisualization::getSEDataCallback);
	this->outSEDataSlot.SetCallback("StructureEventsDataCall", "GetExtent", &StructureEventsClusterVisualization::getSEExtentCallback);
	this->MakeSlotAvailable(&this->outSEDataSlot);

	this->outputLabelSlot.SetParameter(new param::StringParam(""));
	this->MakeSlotAvailable(&this->outputLabelSlot);

	this->quantitativeDataOutputSlot.SetParameter(new param::BoolParam(true));
	this->MakeSlotAvailable(&this->quantitativeDataOutputSlot);

	this->calculationActiveSlot.SetParameter(new param::BoolParam(false));
	this->MakeSlotAvailable(&this->calculationActiveSlot);

	this->createDummyTestDataSlot.SetParameter(new param::BoolParam(false));
	this->MakeSlotAvailable(&this->createDummyTestDataSlot);

	this->periodicBoundaryConditionSlot.SetParameter(new core::param::BoolParam(true));
	this->MakeSlotAvailable(&this->periodicBoundaryConditionSlot);

	this->radiusMultiplierSlot.SetParameter(new core::param::IntParam(5, 2, 10));
	this->MakeSlotAvailable(&this->radiusMultiplierSlot);

	this->msMinCPPercentageSlot.SetParameter(new core::param::FloatParam(40, 20, 45));
	this->MakeSlotAvailable(&this->msMinCPPercentageSlot);

	this->msMinClusterAmountSlot.SetParameter(new core::param::IntParam(2, 2));
	this->MakeSlotAvailable(&this->msMinClusterAmountSlot);

	this->bdMaxCPPercentageSlot.SetParameter(new core::param::FloatParam(2, 2, 10));
	this->MakeSlotAvailable(&this->bdMaxCPPercentageSlot);

	this->minClusterSizeSlot.SetParameter(new core::param::IntParam(10, 8));
	this->MakeSlotAvailable(&this->minClusterSizeSlot);
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

	/// Doesnt work like that, no CallAs.
	/// Using another Callback for SEDC data.
	//StructureEventsDataCall *outSedc = this->outSEDataSlot.CallAs<StructureEventsDataCall>();
	//if (outSedc == NULL) {
	//	this->debugFile << "No StructureEventsDataCall connected.\n";
	//}

	*inMpdc = *outMpdc; // Get the correct request time.
	if (!(*inMpdc)(0)) return false;

	//if (!this->manipulateData(*outMpdc, *inMpdc, *outSedc)) {
	if (!this->manipulateData(*outMpdc, *inMpdc)) {
		inMpdc->Unlock();
		return false;
	}

	inMpdc->Unlock();

	return true;
}


bool mmvis_static::StructureEventsClusterVisualization::getSEDataCallback(Call& caller) {
	using megamol::core::moldyn::MultiParticleDataCall;

	StructureEventsDataCall* outSedc = dynamic_cast<StructureEventsDataCall*>(&caller);
	if (outSedc == NULL) return false;

	//printf("Calc: Structure Events: %d, location: %p, time: %p, type: %p\n",
	//	this->structureEvents.size(), &this->structureEvents.front().x, &this->structureEvents.front().time, &this->structureEvents.front().type);

	if (this->structureEvents.size() > 0) {
		// Debug.
		//vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO, "Calculator: Sent %d events.", this->structureEvents.size());

		// Send data to the call.
		StructureEvents* events = &outSedc->getEvents();
		events->setEvents(&this->structureEvents.front().x,
			&this->structureEvents.front().time,
			&this->structureEvents.front().type,
			this->seMaxTimeCache,
			this->structureEvents.size());
	}

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


bool mmvis_static::StructureEventsClusterVisualization::getSEExtentCallback(Call& caller) {
	using megamol::core::moldyn::MultiParticleDataCall;

	//Works. printf("Calc: SE Extend Callback\n");

	StructureEventsDataCall *outSedc = dynamic_cast<StructureEventsDataCall*>(&caller);
	if (outSedc == NULL) return false;

	MultiParticleDataCall *inMpdc = this->inDataSlot.CallAs<MultiParticleDataCall>();
	if (inMpdc == NULL) return false;

	/// Frame has to be set by MPDC data call, so using MPDC outData is mandatory!

	outSedc->SetExtent(inMpdc->FrameCount(), inMpdc->AccessBoundingBoxes());
	outSedc->SetDataHash(this->sedcHash); // To track changes in Renderer.

	//printf("%d\n", inMpdc->FrameCount());
	
	if (inMpdc->FrameCount() == 0) // No mpdc out is set.
		outSedc->SetFrameCount(1);
	else
		outSedc->SetFrameCount(inMpdc->FrameCount());
	inMpdc->Unlock();

	return true;
}


/**
 * mmvis_static::StructureEventsClusterVisualization::manipulateData
 */
bool mmvis_static::StructureEventsClusterVisualization::manipulateData (
	megamol::core::moldyn::MultiParticleDataCall& outData,
	//mmvis_static::StructureEventsDataCall& outSEData,
	megamol::core::moldyn::MultiParticleDataCall& inData) {

	//printf("Calculator: FrameIDs in: %d, out: %d, stored: %d.\n", inData.FrameID(), outData.FrameID(), this->frameId); // Debug.

	if (this->calculationActiveSlot.Param<param::BoolParam>()->Value()) {

		// Recalculate everything if dirty slots.
		bool reCalculate = false;
		if (this->minClusterSizeSlot.IsDirty()) {
			this->minClusterSizeSlot.ResetDirty();
			reCalculate = true;
		}
		if (this->radiusMultiplierSlot.IsDirty()) {
			this->radiusMultiplierSlot.ResetDirty();
			reCalculate = true;
		}

		// Only calculate when inData has changed frame or hash (data has been manipulated).
		if ((this->frameId != inData.FrameID()) || (this->dataHash != inData.DataHash()) || (inData.DataHash() == 0) || reCalculate) {
			this->frameId = inData.FrameID();
			this->dataHash = inData.DataHash();
			this->setData(inData);
		}

		// Recalculate StructureEvents if dirty slots.
		bool reCalculateSE = false;
		if (this->msMinCPPercentageSlot.IsDirty()) {
			this->msMinCPPercentageSlot.ResetDirty();
			reCalculateSE = true;
		}
		if (this->msMinClusterAmountSlot.IsDirty()) {
			this->msMinClusterAmountSlot.ResetDirty();
			reCalculateSE = true;
		}
		if (this->bdMaxCPPercentageSlot.IsDirty()) {
			this->bdMaxCPPercentageSlot.ResetDirty();
			reCalculateSE = true;
		}
		if (reCalculateSE) {
			determineStructureEvents();
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

	///
	/// Log output.
	///
	if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
		this->logFile.open("SECalc.log", std::ios_base::app | std::ios_base::out);
		this->csvLogFile.open("SECalc.csv", std::ios_base::app | std::ios_base::out);
		std::ifstream csvLogFilePeekTest;
		csvLogFilePeekTest.open("SECalc.csv");
		this->debugFile.open("SECalcDebug.log");

		// Set header to csv if not set.
		if (csvLogFilePeekTest.peek() == std::ifstream::traits_type::eof()) {
			this->csvLogFile
				<< "Label; "
				<< "Time [y-m-d h:m:s]; "
				<< "Frame ID [#frame]; "
				<< "Particles [#particles]; "
				<< "ParticleList [ms]; "
				<< "kdTree [ms]; "
				<< "kDSearch radius multiplier [#radiusMult]; "
				<< "kDSearch max neighbours [#neighbours]; "
				<< "Neighbours [ms]; "
				<< "Clusters [#clusters]; "
				<< "MinCluster [#particles]; "
				<< "MaxCluster [#particles]; "
				<< "SizeOneClusters for Fast Depth debug [#clusters]; "
				<< "MinSizeClusters for Fast Depth debug [#clusters]; "
				<< "Particles in clusters [#particles]; "
				<< "Particles in gas [#particles]; "
				<< "Fast Depth [ms]; "
				<< "Minimum cluster limit [#particles]; "
				<< "Particles merged [#particles]; "
				<< "Clusters removed [#clusters]; "
				<< "SizeOneClusters for Merge Clusters debug [#clusters]; "
				<< "MinSizeClusters for Merge Clusters debug [#clusters]; "
				<< "Merge Clusters [ms]; "
				<< "Forward min common particle ratio [%minCommonParts]; "
				<< "Forward mean common particle ratio [%meanCommonParts]; "
				<< "Forward common particle ratio std deviation [%devCommonParts]; "
				<< "Forward max common particle ratio [%maxCommonParts]; "
				<< "Backwards min common particle ratio [%minCommonParts]; "
				<< "Backwards mean common particle ratio [%meanCommonParts]; "
				<< "Backwards common particle ratio std deviation [%devCommonParts]; "
				<< "Backwards max common particle ratio [%maxCommonParts]; "
				<< "Compare Clusters [ms]; "
				<< "Minimal number of big partner clusters for merge / split [#cluster]; "
				<< "Minimal common particles ratio limit of big partners for merge / split [%minCommonParts]; "
				<< "Maximum total common particles ratio limit for birth / death [%maxCommonParts]; "
				<< "Births [#events]; "
				<< "Deaths [#events]; "
				<< "Merges [#events]; "
				<< "Splits [#events]; "
				<< "Total events [#events]; "
				<< "Determine structure events [ms]; "
				<< "Complete calculation [ms]; "
				<< "ParticleList [kiB]; "
				<< "Previous particleList [kiB]; "
				<< "kdTree [kiB]; "
				<< "ClusterList [kiB]; "
				<< "Previous clusterList [kiB]; "
				<< "Comparison partnersList [kiB]; "
				<< "StructureEventsList [kiB]; "
				<< "Total list memory [kiB]"
				<< "\n";
			this->csvLogFile
				<< "; " // Label.
				<< "y-m-d h:m:s; "
				<< "#frame; "
				<< "#particles; "
				<< "ms; "
				<< "ms; "
				<< "#radius; "
				<< "#neighbours; "
				<< "ms; "
				<< "#clusters; "
				<< "#particles; "
				<< "#particles; "
				<< "#clusters; "
				<< "#clusters; "
				<< "#particles; "
				<< "#particles; "
				<< "ms; "
				<< "#particles; "
				<< "#particles; "
				<< "#clusters; "
				<< "#clusters; "
				<< "#clusters; "
				<< "ms; "
				<< "%minCommonParts; "
				<< "%meanCommonParts; "
				<< "%devCommonParts; "
				<< "%maxCommonParts; "
				<< "%minCommonParts; "
				<< "%meanCommonParts; "
				<< "%devCommonParts; "
				<< "%maxCommonParts; "
				<< "ms; "
				<< "#cluster; "
				<< "%minCommonParts; "
				<< "%maxCommonParts; "
				<< "#events; "
				<< "#events; "
				<< "#events; "
				<< "#events; "
				<< "#events; "
				<< "ms; "
				<< "ms; "
				<< "kiB; "
				<< "kiB; "
				<< "kiB; "
				<< "kiB; "
				<< "kiB; "
				<< "kiB; "
				<< "kiB; "
				<< "kiB"
				<< "\n";
		}
		csvLogFilePeekTest.close();

		// Get current time in readable format. http://stackoverflow.com/a/10467633/4566599, http://stackoverflow.com/a/14387042/4566599
		time_t     now = time(0);
		struct tm  timezone;
		localtime_s(&timezone, &now);
		strftime(this->timeOutputCache, sizeof(this->timeOutputCache), "%Y-%m-%d %X", &timezone); // http://en.cppreference.com/w/cpp/chrono/c/strftime

		vislib::StringA label(this->outputLabelSlot.Param<param::StringParam>()->Value());

		// Create horizontal line of correct length.
		const int sizeOfHead = 6 + (this->frameId < 10 ? 1 : (this->frameId < 100 ? 2 : (this->frameId < 1000 ? 3 : 10))) // Frame.
			+ 7 + sizeof(this->timeOutputCache) // Time.
			+ label.Length(); // Label.
		std::string splitLine;
		for (int i = 0; i < sizeOfHead; ++i) {
			splitLine.append("-");
		}
		this->logFile
			<< "StructureEvents Calculation (SE Calc) " << this->timeOutputCache
			<< ", frame " << this->frameId  // For MMPLDs with single frame it is 0 of course. Alternatively data.FrameID() (returns same).
			<< ", Kennzeichen " << label.PeekBuffer()
			<< "\n"
			<< splitLine << "\n";

		this->csvLogFile
			<< label.PeekBuffer() << "; " // Label
			<< this->timeOutputCache << "; " // Time
			<< this->frameId << "; "; // Frame ID
	}

	///
	/// Calculation steps.
	///

	uint64_t globalParticleIndex = 0;
	float globalRadius = 0;
	uint8_t globalColor[4] = { 0, 0, 0, 0 };
	float globalColorIndexMin = 0, globalColorIndexMax = 0;
	//float signedDistanceMin = 0, signedDistanceMax = 0;

	auto time_completeCalculation = std::chrono::system_clock::now();

	if (this->createDummyTestDataSlot.Param<param::BoolParam>()->Value())
		this->setDummyLists(10000, 100, 50);
	else {
		///
		/// 1st step.
		///
		this->buildParticleList(data, globalParticleIndex, globalRadius, globalColor, globalColorIndexMin, globalColorIndexMax);
		this->findNeighboursWithKDTree(data);

		///
		/// 2nd step.
		///
		this->createClustersFastDepth();
		this->mergeSmallClusters();
	}

	
	///
	/// 3rd and 4th step and output to SEDC.
	/// testEventsCSVFile
	if (this->previousClusterList.size() > 0 && this->previousParticleList.size() > 0) {
		this->compareClusters();
		this->determineStructureEvents();
		this->setClusterColor(false);
	}
	else {
		///
		/// Log output.
		///
		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			this->logFile << "Skipped step 3 and step 4 since no previous clusters available.\n";
			for (int numberOfSkippedFields = 0; numberOfSkippedFields < 18; ++numberOfSkippedFields)
				this->csvLogFile << "; ";
		}
		this->setClusterColor(true);
	}

	///
	/// Output to MPDC.
	/// Fill MultiParticleDataCall::Particles with data of the local container.
	///
	unsigned int particleStride = sizeof(Particle);

	this->particles.SetCount(globalParticleIndex);
	this->particles.SetGlobalRadius(globalRadius);
	this->particles.SetGlobalColour(globalColor[0], globalColor[1], globalColor[2]);
	this->particles.SetColourMapIndexValues(globalColorIndexMin, globalColorIndexMax);

	this->particles.SetVertexData(MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR, &this->particleList[0], particleStride);
	this->particles.SetColourData(MultiParticleDataCall::Particles::COLDATA_FLOAT_RGB, &this->particleList[0].r, particleStride);

	///
	/// Log output.
	///
	{// Time measurement.
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_completeCalculation);
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
			"SECalc: Calculation finished in %lld ms.", duration.count());

		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			this->logFile << "Calculation finished in " << duration.count() << " ms ";
			this->csvLogFile << duration.count() << "; "; // Complete calculation (ms)
		}
	}

	if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {

		// Old stuff.
		//printf("Calculator: ParticleStride: %d, ParticleCount: %d, RandomParticleColor: (%f, %f, %f)\n",
		//	particleStride, globalParticleIndex, particleList[0].r, particleList[0].g, particleList[0].b);
		// For testing if sorting works.
		//printf("Calculator: ParticleList SignedDistance max: %f, min: %f\n",
		//	this->particleList.front().signedDistance, this->particleList.back().signedDistance);
		//printf("Calculator: Particle SignedDistance max: %f, min: %f\n",
		//	signedDistanceMax, signedDistanceMin);

		// Data structure sizes.
		int unitConversion = 1024;

		// Determine representative size of all particles including their neighbours.
		size_t innerVectorSize = 0;
		for (auto & particle : this->particleList)
			innerVectorSize += particle.neighbourIDs.size() * sizeof(uint64_t);

		size_t particleBytes = this->particleList.size() * sizeof(Particle) + innerVectorSize;
		particleBytes /= unitConversion;

		innerVectorSize = 0;
		for (auto & particle : this->previousParticleList)
			innerVectorSize += particle.neighbourIDs.size() * sizeof(uint64_t);

		size_t previousParticleBytes = this->previousParticleList.size() * sizeof(Particle) + innerVectorSize;
		previousParticleBytes /= unitConversion;

		// Tree and clusters.
		size_t kdtreeBytes = this->treeSizeOutputCache / unitConversion;
		size_t clusterBytes = this->clusterList.size() * sizeof(Cluster) / unitConversion;
		size_t previousClusterBytes = this->clusterList.size() * sizeof(Cluster) / unitConversion;

		// Comparison: Partner clusters and their partners.
		innerVectorSize = 0;
		for (auto & partnerClusters : this->partnerClustersList.forwardList)
			innerVectorSize += partnerClusters.getNumberOfPartners() * sizeof(PartnerClusters::PartnerCluster);
		size_t forwardListBytes = this->partnerClustersList.forwardList.size() * sizeof(PartnerClusters) + innerVectorSize;

		innerVectorSize = 0;
		for (auto & partnerClusters : this->partnerClustersList.backwardsList)
			innerVectorSize += partnerClusters.getNumberOfPartners() * sizeof(PartnerClusters::PartnerCluster);
		size_t backwardsListBytes = this->partnerClustersList.backwardsList.size() * sizeof(PartnerClusters) + innerVectorSize;

		size_t partnerClustersBytes = (forwardListBytes + backwardsListBytes) / unitConversion;

		// Structure events.
		size_t seBytes = this->structureEvents.size() * sizeof(StructureEvents) / unitConversion;

		size_t totalSize = particleBytes + previousParticleBytes + kdtreeBytes + clusterBytes + previousClusterBytes + partnerClustersBytes + seBytes;

		this->logFile
			<< "using "
			<< particleBytes << " kiB particles, "
			//<< previousParticleBytes << " previousParticleList, "
			<< kdtreeBytes << " kiB kdTree, "
			<< clusterBytes << " kiB clusters, "
			<< partnerClustersBytes << " kiB comparison partners, "
			<< seBytes << " kiB StructureEvents and "
			<< totalSize << " kiB for lists in total.";
		this->csvLogFile
			<< particleBytes << "; " //  // ParticleList(kiB)
			<< previousParticleBytes << "; " // Previous particleList (kiB)
			<< kdtreeBytes << "; " // kdTree (kiB)
			<< clusterBytes << "; " // ClusterList (kiB)
			<< previousClusterBytes << "; " // Previous clusterList (kiB)
			<< partnerClustersBytes << "; " // Comparison partnersList (kiB)
			<< seBytes << "; " // StructureEventsList (kiB)
			<< totalSize << "; "; // Total list memory (kiB)

		this->logFile << "\n\n";
		this->logFile.close();

		this->csvLogFile << "\n";
		this->csvLogFile.close();

		this->debugFile.close();
	}
}


void mmvis_static::StructureEventsClusterVisualization::buildParticleList(megamol::core::moldyn::MultiParticleDataCall& data,
	uint64_t& globalParticleIndex, float& globalRadius, uint8_t(&globalColor)[4], float& globalColorIndexMin, float& globalColorIndexMax) {
	using megamol::core::moldyn::MultiParticleDataCall;

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

	/// Heed size of neighbour IDs when reserving memory. A lot more than needed gets reserved,
	/// since maxNeighbours is never reached with radius kD search and gas particles have no neighbours at all.
	/// This is wrong (neighbour vector is stored elsewhere) and bad (way too much memory gets reserved).
	//float perParticleNeighbourBytes = static_cast<float>(this->getKDTreeMaxNeighbours(this->radiusMultiplierSlot.Param<param::IntParam>()->Value()) * sizeof(uint64_t));
	//size_t allParticleNeighbourIDSize = static_cast<size_t>(perParticleNeighbourBytes / (float) sizeof(Particle) * globalParticleCnt);
	//particleList.reserve(globalParticleCnt + allParticleNeighbourIDSize);

	particleList.reserve(globalParticleCnt);

	///
	/// Build list.
	///
	auto time_buildList = std::chrono::system_clock::now();

	for (unsigned int particleListIndex = 0; particleListIndex < data.GetParticleListCount(); ++particleListIndex) {

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
			vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_WARN, "Particlelist %d skipped, no vertex data.", particleListIndex);
			continue; // Skip this particle list.
		}

		// Check for correct vertex type.
		if (particles.GetVertexDataType() != MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZ
			&& particles.GetVertexDataType() != MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR) {
			vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_WARN, "Particlelist %d skipped, vertex are not floats.", particleListIndex);
			continue; // Skip this particle list.
		}

		// Check for correct color type.
		if (particles.GetColourDataType() != MultiParticleDataCall::Particles::COLDATA_FLOAT_I) {
			vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_WARN, "Particlelist %d skipped, COLDATA_FLOAT_I expected.", particleListIndex);
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

		for (uint64_t particleIndex = 0; particleIndex < particles.GetCount(); ++particleIndex, vertexPtr += vertexStride, colourPtr += colourStride) {

			Particle particle;
			//Particle particle(this->radiusMultiplierSlot.Param<param::IntParam>()->Value()); // This doubles creation time! Don't do this, it is horrible (see Constructor description).

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
			this->particleList.push_back(particle);
		}
		globalParticleIndex += static_cast<size_t>(particles.GetCount());
	}
	
	///
	/// Log output.
	///
	{// Time measurement. 4s
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_buildList);
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
			"SECalc step 1: Created particle list with %d elements in %lld ms.", particleList.size(), duration.count());
		
		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			this->logFile
				<< "Step 1 (build particleList, create kdTree and find neighbours):\n"
				<< "  a) ParticleList with " << particleList.size() << " particles (" << duration.count() << " ms)\n";
			this->csvLogFile
				<< particleList.size() << "; " // Particles (#)
				<< duration.count() << "; "; // ParticleList (ms)
		}
	}
}


void mmvis_static::StructureEventsClusterVisualization::findNeighboursWithKDTree(megamol::core::moldyn::MultiParticleDataCall& data) {

	auto time_buildTree = std::chrono::system_clock::now();

	///
	/// Create k-d-Tree.
	///
	/// nn_idx return value of search functions matches position in ANNpointArray,
	/// which matches the particle ID in particleList as long as particleList
	/// is not resorted!
	///

	ANNpoint annPtsData = new ANNcoord[3 * this->particleList.size()]; // Container for pointdata. Can be deleted at the end of the function.
	ANNpointArray annPts = new ANNpoint[this->particleList.size()];
	//uint64_t annPtsCounter = 0;
	for (auto & particle : this->particleList) {
		// Creating new points causes missing memory deallocation since they can't be deleted at the end of the function.
		//ANNpoint qTree = new ANNcoord[3]; // Mustn't be deleted before the end of the function.
		//qTree[0] = static_cast<ANNcoord>(particle.x);
		//qTree[1] = static_cast<ANNcoord>(particle.y);
		//qTree[2] = static_cast<ANNcoord>(particle.z);
		//annPts[annPtsCounter] = qTree;
		//annPtsCounter++;
		// //delete[] qTree; // Wrong here, mustn't be called before end of the function!
		
		annPtsData[(particle.id * 3) + 0] = static_cast<ANNcoord>(particle.x);
		annPtsData[(particle.id * 3) + 1] = static_cast<ANNcoord>(particle.y);
		annPtsData[(particle.id * 3) + 2] = static_cast<ANNcoord>(particle.z);
	}
	for (size_t i = 0; i < this->particleList.size(); ++i) {
		annPts[i] = annPtsData + (i * 3);
	}

	ANNkd_tree* tree = new ANNkd_tree(annPts, static_cast<int>(this->particleList.size()), 3);

	this->treeSizeOutputCache = tree->nPoints() * sizeof(ANNcoord) * 3; // One ANNPoint consists of 3 ANNcoords here.

	///
	/// Log output.
	///
	{ // Time measurement. 7s
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_buildTree);
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
			"SECalc step 1: Created kD-tree in %lld ms.", duration.count());

		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			this->logFile << "  b) kD-tree (" << duration.count() << " ms)\n";
			this->csvLogFile << duration.count() << "; "; // kdTree (ms)
		}
	}

	///
	/// Get bounding box for periodic boundary condition.
	///
	auto bbox = data.AccessBoundingBoxes().ObjectSpaceBBox();
	bbox.EnforcePositiveSize(); // paranoia says Sebastian. Well, nothing compared to list pointer consistency checks.
	auto bbox_cntr = bbox.CalcCenter();
	
	const bool periodicBoundary = this->periodicBoundaryConditionSlot.Param<megamol::core::param::BoolParam>()->Value();

	///
	/// Find and store neighbours.
	///
	const auto time_findNeighbours = std::chrono::system_clock::now();

	const bool debugSkipParticles = false; // Skip particles for faster tests.
	const bool useFRSearch = true; // Use kD tree search with radius.
	const int radiusMultiplier = this->radiusMultiplierSlot.Param<param::IntParam>()->Value();

	const int maxNeighbours = this->getKDTreeMaxNeighbours(radiusMultiplier);

	ANNdist sqrRadius = powf(radiusMultiplier * particleList[0].radius, 2);

	///
	/// Log output.
	///
	if (useFRSearch)
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
		"SECalc step 1: annkFRSearch with radius %d (%.2f) and %d max neighbours.", radiusMultiplier, sqrRadius, maxNeighbours);
	else
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
			"SECalc step 1: annkSearch %d max neighbours.", maxNeighbours);
	
	if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
		this->logFile << "  c) Neighbours";

		if (useFRSearch) {
			this->logFile
				<< " annkFRSearch with " << radiusMultiplier << "*radius and "
				<< maxNeighbours << " max neighbours";
			this->csvLogFile
				<< radiusMultiplier << "; " // Neighbours radius multiplier
				<< maxNeighbours << "; "; // Neighbours max neighbours
		}
		else {
			this->logFile << " annkSearch with " << maxNeighbours << " max neighbours";
			this->csvLogFile
				<< "; " // Neighbours radius multiplier, empty.
				<< maxNeighbours << "; "; // Neighbours max neighbours
		}
	}

	uint64_t debugSkippedNeighbours = 0; // Not usable with concurrency.
	uint64_t debugAddedNeighbours = 0; // Not usable with concurrency.

	/// OpenMP. Doesnt work b/c ANN64d.dll throws exception (Zugriffsverletzung) for annkFRSearch, annkSearch:
	/// Since parallel access by several threads to the same tree!
	/// Furthermore ANN doesnt seem to work with parallelisation at all since it uses the same shared memory for all search structures! http://stackoverflow.com/a/2182357
	//#pragma omp parallel for
	//for (int i = 0; i < this->particleList.size(); ++i) {
	//	auto particle = this->particleList[i];
	
	/// PPL. Doesnt work b/c ANN64d.dll throws exception (Zugriffsverletzung) for annkFRSearch, annkSearch:
	/// Since parallel access by several threads to the same tree!
	/// Furthermore ANN doesnt seem to work with parallelisation at all since it uses the same shared memory for all search structures! http://stackoverflow.com/a/2182357
	//concurrency::parallel_for_each(this->particleList.begin(), this->particleList.end(),
	//	[maxNeighbours, periodicBoundary, bbox_cntr, bbox, useFRSearch, tree, sqrRadius](Particle particle) {

	for (auto & particle : this->particleList) {

		// Skip particles for faster testing. Not usable with concurrency.
		if (debugSkipParticles && particle.id > 10000)
			break;

		ANNidxArray   nn_idx = 0;
		ANNdistArray  dd = 0;
		nn_idx = new ANNidx[maxNeighbours];
		dd = new ANNdist[maxNeighbours];

		ANNpoint q = new ANNcoord[3];

		// The three loops are for periodic boundary condition.
		for (int x_s = 0; x_s < (periodicBoundary ? 2 : 1); ++x_s) {
			for (int y_s = 0; y_s < (periodicBoundary ? 2 : 1); ++y_s) {
				for (int z_s = 0; z_s < (periodicBoundary ? 2 : 1); ++z_s) {

					q[0] = static_cast<ANNcoord>(particle.x);
					q[1] = static_cast<ANNcoord>(particle.y);
					q[2] = static_cast<ANNcoord>(particle.z);

					if (x_s > 0) q[0] = static_cast<ANNcoord>(particle.x + ((particle.x > bbox_cntr.X()) ? -bbox.Width() : bbox.Width()));
					if (y_s > 0) q[1] = static_cast<ANNcoord>(particle.y + ((particle.y > bbox_cntr.Y()) ? -bbox.Height() : bbox.Height()));
					if (z_s > 0) q[2] = static_cast<ANNcoord>(particle.z + ((particle.z > bbox_cntr.Z()) ? -bbox.Depth() : bbox.Depth()));

					if (useFRSearch) {
						tree->annkFRSearch(
							q,				// the query point
							sqrRadius,		// squared radius of query ball
							maxNeighbours,	// number of neighbors to return
							nn_idx,			// nearest neighbor array (modified)
							dd				// dist to near neighbors as squared distance (modified)
							);				// error bound (optional).
					}
					else {
						tree->annkSearch(
							q,				// the query point
							maxNeighbours,	// number of neighbors to return
							nn_idx,			// nearest neighbor array (modified)
							dd				// dist to near neighbors as squared distance (modified)
							);				// error bound (optional).
					}

					for (size_t i = 0; i < maxNeighbours; ++i) {
						if (nn_idx[i] == ANN_NULL_IDX) {
							debugSkippedNeighbours++; // Deactivate if concurrent loop.
							continue;
						}
						if (dd[i] < 0.001f) // Exclude self to catch ANN_ALLOW_SELF_MATCH = true.
							continue;

						debugAddedNeighbours++; // Deactivate if concurrent loop.

						// Store whole particle in neighbour list: Takes a lot of memory and time! At maximum maxNeighbours = 3 and 2*r works on test machine.
						//particle.neighbours.push_back(_getParticle(nn_idx[i])); // Search eates up to 500ms/Particle + high memory consumption!
						//particle.neighbours.push_back(this->particleList[nn_idx[i]]);

						// Requires untouched particleList but needs a lot less memory!
						//particle.neighbourPtrs.push_back(&this->particleList[nn_idx[i]]);
						
						// Safer method than above and still low memory consumption.
						particle.neighbourIDs.push_back(nn_idx[i]);
					}

					// Progress. Deactivate if concurrent loop.
					if (debugAddedNeighbours % 100000 == 0)
						vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
							"SECalc step 1 progress: Neighbours: %d added, %d out of FRSearch radius.", debugAddedNeighbours, debugSkippedNeighbours);
				}
			}
		}

		delete[] nn_idx;
		delete[] dd;
		delete[] q;
	}
	//});

	///
	/// Log output.
	///
	{ // Time measurement depends heavily on maxNeighbours and sqrRadius (when using annkFRSearch) as well as on save method in particleList.
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_findNeighbours);
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
			"SECalc step 1: Neighbours set in %lld ms with %d added and %d out of FRSearch radius.\n", duration.count(), debugAddedNeighbours, debugSkippedNeighbours);

		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			this->logFile << ", added " << debugAddedNeighbours << " neighbours with " << debugSkippedNeighbours << " particles out of FRSearch radius (" << duration.count() << " ms)\n";
			this->csvLogFile << duration.count() << "; "; // Neighbours (ms)
		}
	}

	///
	/// Test.
	///
	/*
	uint64_t id = 1000;
	if (this->particleList[id].neighbourIDs.size() > 0) {
		//Particle nearestNeighbour = *this->particleList[id].neighbourPtrs[0];
		Particle nearestNeighbour = this->particleList[this->particleList[id].neighbourIDs[0]];

		printf("Calculator: Particle %d at (%2f, %2f, %2f) nearest neighbour %d at (%2f, %2f, %2f).\n",
			particleList[id].id, particleList[id].x, particleList[id].y, particleList[id].z,
			nearestNeighbour.id, nearestNeighbour.x, nearestNeighbour.y, nearestNeighbour.z);
	}
	*/

	delete tree;
	delete[] annPts;
	delete[] annPtsData;

	/// From ANN manual v1.1, page 8: http://www.cs.umd.edu/~mount/ANN/Files/1.1/ANNmanual_1.1.pdf
	/// The library allocates a small amount of storage, which is shared by all search structures
	///	built during the program’s lifetime.Because the data is shared, it is not deallocated,
	///	even when the all the individual structures are deleted.To avoid the resulting(minor) memory
	///	leak, the following function can be called after all search structures have been destroyed.
	annClose();
}


void mmvis_static::StructureEventsClusterVisualization::createClustersFastDepth() {
	auto time_createCluster = std::chrono::system_clock::now();

	size_t debugNoNeighbourCounter = 0;
	size_t debugUsedExistingClusterCounter = 0;
	size_t debugNumberOfGasParticles = 0;

	int clusterID = 0;

	// For testing Zero signed distance one size clusters phenomenon. Only seen at 5*radius, not at 4 yet.
	std::ofstream testCFDCSVFile;

	if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
		// CFD == Cluster Fast Depth
		std::string filename = "SECalc CFD Test.csv";
		testCFDCSVFile.open(filename.c_str(), std::ios_base::app | std::ios_base::out);
		std::ifstream peekTest;
		peekTest.open(filename.c_str());

		if (peekTest.peek() == std::ifstream::traits_type::eof()) {
			testCFDCSVFile
				<< "Time; "
				<< "Frame ID; "
				<< "Cluster ID; "
				<< "Amount of particles; "
				<< "Particle ID; "
				<< "SignedDistance; "
				<< "Amount of neighbours; "
				<< "PosX; "
				<< "PosY; "
				<< "PosZ; "
				<< "\n";
		}
		peekTest.close();
	}

	///
	/// Reallocation of clusterList (due to growth) makes pointer invalid. Either fix by:
	/// 1) give clusterList big size, so list doesnt resize.
	/// 2) change references to ids, clusterList mustn't be resorted but may be reallocated in memory.
	/// 3) use id's in struct Cluster (+ vector may be resorted, - access very slow)
	///
	/// It has been fixed with method (1) (for speed) and method (2).
	///

	// Avoid reallocation of clusterList, method (1) - though it's a waste of memory it saves time (no reallocation)!
	if (this->radiusMultiplierSlot.Param<param::IntParam>()->Value() < 3)
		this->clusterList.reserve(this->particleList.size() / 5); // Low radius multiplier will create a lot of clusters. Only important for test runs.
	else
		this->clusterList.reserve(this->particleList.size() / 20);

	/// #pragma omp parallel for
	/// Particle handling could be parallelized, if the traversed particles are not added to the cluster
	/// and cluster creation/selection is thread safe. However the speedup might be negative!
	// for (auto partIT = this->particleList.begin(); partIT < this->particleList.end(); ++partIT) {
	//	auto particle = *partIT;
	for (auto & particle : this->particleList) {
		if (particle.signedDistance < 0) {
			debugNumberOfGasParticles++; // Not usable with concurrency.
			continue; // Skip gas.
		}

		//if (particle.neighbourPtrs.size() == 0) {
		if (particle.neighbourIDs.size() == 0) {
			debugNoNeighbourCounter++; // Not usable with concurrency.
			continue; // Skip particles without neighbours.
		}

		//if (particle.clusterPtr)
		if (particle.clusterID != -1)
			continue; // Skip particles that already belong to a cluster.

		///
		/// Traverse the neighbours to find the deepest neighbour.
		///
		auto time_addParticlePath = std::chrono::system_clock::now();

		// Container for deepest neighbours.
		std::vector<uint64_t> parsedParticleIDs;

		Particle deepestNeighbour = particle; // Initial condition.

		for (;;) {
			float signedDistance = 0; // For comparison of neighbours.
			Particle currentParticle = deepestNeighbour; // Set last deepest particle as new current.

			//for (int i = 0; i < currentParticle.neighbourPtrs.size(); ++i) {
			//	if (currentParticle.neighbourPtrs[i]->signedDistance > signedDistance) {
			//		signedDistance = currentParticle.neighbourPtrs[i]->signedDistance;
			//		deepestNeighbour = *currentParticle.neighbourPtrs[i];
			//	}
			//}

			// Get deepest neighbour.
			for (int i = 0; i < currentParticle.neighbourIDs.size(); ++i) {
				if (this->particleList[currentParticle.neighbourIDs[i]].signedDistance > signedDistance) {
					signedDistance = this->particleList[currentParticle.neighbourIDs[i]].signedDistance;
					deepestNeighbour = this->particleList[currentParticle.neighbourIDs[i]];
				}
			}

			// Add current deepest neighbour to the list containing all the parsed particles.
			parsedParticleIDs.push_back(deepestNeighbour.id);
			
			if (currentParticle.signedDistance >= deepestNeighbour.signedDistance) { // Current particle is local maximum.
				//Debug printf("%f >= %f\n", currentParticle.signedDistance, deepestNeighbour.signedDistance); // Works.

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
					//cluster.id = static_cast<int> (this->clusterList.size());
					cluster.id = clusterID; clusterID++; // Clearer than statement above.
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
				
				///
				/// Add particle to cluster.
				///
				//particle.clusterPtr = clusterPtr;
				particle.clusterID = clusterPtr->id;
				(*clusterPtr).numberOfParticles++;

				// Check for list ids consistency. Paranoia!
				if (particle.clusterID != this->clusterList[particle.clusterID].id) {
					vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
						"SECalc step 2 (build): Cluster ID and position in cluster don't match: %d != %d!", particle.clusterID, this->clusterList[particle.clusterID].id);
					this->debugFile << "SECalc step 2 (build) error: Cluster ID and position in cluster don't match: "
						<< particle.clusterID << " != " << this->clusterList[particle.clusterID].id << "!\n";
				}

				///
				/// Add the deepest neighbours found within the traversion to the cluster.
				/// Exceptions:
				/// - those that are already in a cluster
				/// - the last neighbour since it was added just to check
				///   if the current particle is the deepest particle.
				///
				parsedParticleIDs.pop_back(); // Remove last deepest neighbour since it is not deeper than current particle.
				for (auto & particleID : parsedParticleIDs) {
					
					// Check for list ids consistency. Paranoia!
					if (this->particleList[particleID].id != particleID) {
						vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
							"SECalc step 2 (build): Particle ID and position in particle list don't match: %d != %d!", particleID, this->particleList[particleID].id);
						this->debugFile << "SECalc step 2 (build) error: Particle ID and position in particle list don't match: "
							<< particleID << " != " << this->particleList[particleID].id << "!\n";
					}

					if (this->particleList[particleID].clusterID >= 0)
						continue; // Skip particles already in a cluster.

					//this->particleList[particleID].clusterPtr = clusterPtr;
					this->particleList[particleID].clusterID = clusterPtr->id; // Requires untouched (i.e. sorting forbidden) particleList!
					(*clusterPtr).numberOfParticles++; // Caused a bug since particles that are already part of the cluster are added again. Checks above avoid this now.
				}
				
				//this->debugFile << clusterPtr << " with root " << (*clusterPtr).rootParticleID << ", parsed particles " << parsedParticleIDs.size() << " , number of particles " << (*clusterPtr).numberOfParticles << ".\n"; // Debugging black particles (clusterList got reallocated during creation).

				if (this->clusterList.size() % 1000 == 0) {
					vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
						"SECalc step 2 progress: Amount of clusters %d. Particle %d. Liquid particles w/o neighbours %d.", this->clusterList.size(), particle.id, debugNoNeighbourCounter);
				}

				// Progress, to not loose patience when waiting for results.
				if (particle.id % 100000 == 0) {
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_addParticlePath);
					auto durTotal = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_createCluster);
					vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
						"SECalc step 2 progress: Cluster for p %d (%lld ms, total %lld ms), pathlength %d.\nCluster elements: %d. Total number of clusters: %d.",
							particle.id, duration.count(), durTotal.count(), parsedParticleIDs.size(), (*clusterPtr).numberOfParticles, this->clusterList.size());
				}
				break;
			}
			//printf("Particle %d with deepest neighbour %d.\n", currentParticle.id, deepestNeighbour.id); // Debug.
		}
	}
	
	///
	/// Log output.
	///
	{ // Time measurement and Debug.
		
		// Min/Max Clusters.
		const uint64_t minCluster = std::min_element(
			this->clusterList.begin(), this->clusterList.end(), [](const Cluster& lhs, const Cluster& rhs) {
			return lhs.numberOfParticles < rhs.numberOfParticles;
		})->numberOfParticles;
		const uint64_t maxCluster = std::max_element(
			this->clusterList.begin(), this->clusterList.end(), [](const Cluster& lhs, const Cluster& rhs) {
			return lhs.numberOfParticles < rhs.numberOfParticles;
		})->numberOfParticles;

		// Time measurement.
		const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_createCluster);

		///
		/// Debug values.
		///

		int debugParticleInClustersNumber = 0;
		int debugSizeOneClusters = 0; // For testing MergeClusters produces adjacent gas particle clusters theory.
		int debugMinSizeClusters = 0; // For testing MergeClusters produces adjacent gas particle clusters theory.
		const bool skipCFDOutput = this->clusterList.size() > 50000 ? true : false; // Output takes forever with thousands of clusters and particles.

		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			for (auto & cluster : this->clusterList) {
				// Particles in clusters.
				debugParticleInClustersNumber += static_cast<int>(cluster.numberOfParticles);

				// Clusters smaller clusterMinSize. E.g. for testing Zero signed distance one size clusters phenomenon.
				if (cluster.numberOfParticles < this->minClusterSizeSlot.Param<param::IntParam>()->Value()) {
					
					// For log.
					debugMinSizeClusters++;
					if (cluster.numberOfParticles == 1)
						debugSizeOneClusters++;
					
					if (!skipCFDOutput) {
						// For testing Single Zero Signed Distance Clusters theory.
						for (auto & particle : this->particleList) {
							if (particle.clusterID == cluster.id) {
								testCFDCSVFile
									<< this->timeOutputCache << "; "
									<< this->frameId << "; "
									<< cluster.id << "; "
									<< cluster.numberOfParticles << "; "
									<< particle.id << "; "
									<< particle.signedDistance << "; "
									<< particle.neighbourIDs.size() << "; "
									<< particle.x << "; "
									<< particle.y << "; "
									<< particle.z << "\n";
							}
						}
					}
				}
			}
		}
		
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
			"SECalc Step 2: %d (%d/%d) clusters created in %lld ms. %d particles used existing clusters.\nDebug: %d liquid particles w/o neighbour, %d size one clusters.",
			this->clusterList.size(), minCluster, maxCluster, duration.count(), debugUsedExistingClusterCounter, debugNoNeighbourCounter, debugSizeOneClusters);

		// Check cluster creation.
		assert(debugParticleInClustersNumber + debugNumberOfGasParticles + debugNoNeighbourCounter == particleList.size());

		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			this->logFile
				<< "Step 2 (create and merge clusters):\n"
				<< "  a) " << this->clusterList.size() << " clusters created with min/max sizes " << minCluster << "/" << maxCluster
				<< " and particles in gas/cluster " << debugNumberOfGasParticles << "/" << debugParticleInClustersNumber << " (" << duration.count() << " ms)\n"
				<< "     while "
				<< debugUsedExistingClusterCounter << " particles used existing clusters"
				<< " (debug: " << debugNoNeighbourCounter << " liquid particles w/o neighbours"
				<< ", " << debugSizeOneClusters << " size one clusters"
				<< ", " << debugMinSizeClusters << " min size clusters)"
				<< "\n";
			this->csvLogFile
				<< this->clusterList.size() << "; " // Clusters (#)
				<< minCluster << "; " // MinCluster (#particles)
				<< maxCluster << "; " // MaxCluster (#particles)
				<< debugSizeOneClusters << "; " // SizeOneClusters for Fast Depth debug (#clusters)
				<< debugMinSizeClusters << "; " // MinSizeClusters for Fast Depth debug (#clusters)
				<< debugParticleInClustersNumber << "; " // Particles in clusters (#)
				<< debugNumberOfGasParticles << "; " // Particles in gas (#)
				<< duration.count() << "; "; // Fast Depth (ms)

			testCFDCSVFile.close();
		}
	}
}


void mmvis_static::StructureEventsClusterVisualization::mergeSmallClusters() {
	auto time_mergeClusters = std::chrono::system_clock::now();

	int mergedParticles = 0;

	#pragma omp parallel for
	for (int i = 0; i < this->particleList.size(); ++i) {
		Particle particle = this->particleList[i];
	//for (auto particle : this->particleList) {
		if (particle.signedDistance < 0)
			continue; // Skip gas.

		//if (particle.neighbourPtrs.size() == 0)
		if (particle.neighbourIDs.size() == 0)
			continue; // Skip particles without neighbours.

		//if (!particle.clusterPtr)
		if (particle.clusterID == -1)
			continue; // Skip particles w/o pointers, mandatory for test runs.

		// Check for list ids consistency. Paranoia!
		if (particle.clusterID != this->clusterList[particle.clusterID].id) {
			vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
				"SECalc step 2 (merge): Cluster ID and position in cluster don't match: %d != %d!", particle.clusterID, this->clusterList[particle.clusterID].id);
			this->debugFile << "SECalc step 2 (merge) Error: Cluster ID and position in cluster don't match: "
				<< particle.clusterID << " != " << this->clusterList[particle.clusterID].id << "!\n";
		}
		if (this->clusterList[particle.clusterID].rootParticleID != this->particleList[this->clusterList[particle.clusterID].rootParticleID].id) {
			vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
				"SECalc step 2 (merge): Root particle ID and position in particle list don't match: %d != %d!",
				this->clusterList[particle.clusterID].rootParticleID, this->particleList[this->clusterList[particle.clusterID].rootParticleID].id);
			this->debugFile << "SECalc step 2 (merge) error: Root particle ID and position in particleList don't match: "
				<< this->clusterList[particle.clusterID].rootParticleID << " != " << this->particleList[this->clusterList[particle.clusterID].rootParticleID].id << "!\n";
		}

		//if (particle.clusterPtr->numberOfParticles >= this->minClusterSize)
		if (this->clusterList[particle.clusterID].numberOfParticles >= this->minClusterSizeSlot.Param<param::IntParam>()->Value()) // Requires untouched (i.e. sorting forbidden) clusterList!
			continue; // Skip particles of bigger clusters.

		///
		/// Add clusters in neighbourhood which are bigger than this->minClusterSize.
		///

		//std::mutex neighbourClusterIDs_mutex; // Mutex for thread safety w/o OpenMP.

		std::vector<int> neighbourClusterIDs;

		//#pragma omp parallel for
		//for (int i = 0; i < particle.neighbourIDs.size(); ++i) {
		//	Particle* neighbour = &this->particleList[particle.neighbourIDs[i]];
		//for (auto neighbour : particle.neighbourPtrs) {
		for (auto neighbourID : particle.neighbourIDs) {
			Particle* neighbour = &this->particleList[neighbourID];

			//if (!neighbour->clusterPtr)
			if (neighbour->clusterID == -1)
				continue; // Skip particles w/o pointers, mandatory for test runs.

			//if (neighbour->clusterPtr->rootParticleID != particle.clusterPtr->rootParticleID
			//	&& neighbour->clusterPtr->numberOfParticles >= this->minClusterSize)
			//	neighbourClusters.push_back(neighbour->clusterPtr);
			if (this->clusterList[neighbour->clusterID].rootParticleID != this->clusterList[particle.clusterID].rootParticleID
				&& this->clusterList[neighbour->clusterID].numberOfParticles >= this->minClusterSizeSlot.Param<param::IntParam>()->Value()) {
				//neighbourClusters.push_back(&this->clusterList[neighbour->clusterID]);
				//std::lock_guard<std::mutex> lk(neighbourClusterIDs_mutex); // Mutex for thread safety of push_back w/o OpenMP.
				//#pragma omp critical // Mutex for thread safety of push_back. Inefficient.
				neighbourClusterIDs.push_back(this->clusterList[neighbour->clusterID].id); // Not thread safe!
			}
		}

		///
		/// 2nd level.
		/// If no clusters in range, check neighbours of neighbours.
		///
		if (neighbourClusterIDs.size() == 0) {

			//#pragma omp parallel for
			for (int i = 0; i < particle.neighbourIDs.size(); ++i) {
				Particle* neighbour = &this->particleList[particle.neighbourIDs[i]];
			//for (auto neighbourIT = particle.neighbourIDs.begin(); neighbourIT < particle.neighbourIDs.end(); ++neighbourIT) {
			//	Particle* neighbour = &this->particleList[*neighbourIT];
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
					//	&& secondaryNeighbour->clusterPtr->numberOfParticles >= this->minClusterSize)
					//	neighbourClusters.push_back(secondaryNeighbour->clusterPtr);
					if (this->clusterList[secondaryNeighbour->clusterID].rootParticleID != this->clusterList[particle.clusterID].rootParticleID
						&& this->clusterList[secondaryNeighbour->clusterID].numberOfParticles >= this->minClusterSizeSlot.Param<param::IntParam>()->Value()) {
						//#pragma omp critical // Mutex for thread safety of push_back. Inefficient.
						neighbourClusterIDs.push_back(this->clusterList[secondaryNeighbour->clusterID].id); // Not thread safe!
					}
				}
			}
		}

		///
		/// 3rd level.
		/// If no clusters in range, check neighbours of neighbour neighbours.
		///
		if (neighbourClusterIDs.size() == 0) {
			//#pragma omp parallel for
			for (int i = 0; i < particle.neighbourIDs.size(); ++i) {
				Particle* neighbour = &this->particleList[particle.neighbourIDs[i]];
			//for (auto neighbourIT = particle.neighbourIDs.begin(); neighbourIT < particle.neighbourIDs.end(); ++neighbourIT) {
			//	Particle* neighbour = &this->particleList[*neighbourIT];

				for (auto secondaryNeighbourID : neighbour->neighbourIDs) {
					Particle* secondaryNeighbour = &this->particleList[secondaryNeighbourID];

					// Check neighbours of neighbours neighbour.
					for (auto tertiaryNeighbourID : secondaryNeighbour->neighbourIDs) {
						Particle* tertiaryNeighbour = &this->particleList[tertiaryNeighbourID];

						if (tertiaryNeighbour->clusterID == -1)
							continue; // Skip particles w/o pointers, mandatory for test runs.

						if (this->clusterList[tertiaryNeighbour->clusterID].rootParticleID != this->clusterList[particle.clusterID].rootParticleID
							&& this->clusterList[tertiaryNeighbour->clusterID].numberOfParticles >= this->minClusterSizeSlot.Param<param::IntParam>()->Value()) {
							//#pragma omp critical // Mutex for thread safety of push_back. Inefficient.
							neighbourClusterIDs.push_back(this->clusterList[tertiaryNeighbour->clusterID].id); // Not thread safe!
						}
					}
				}
			}
		}

		// If still no other clusters are in range it is assumed there are no connected other clusters, so this particle stays in the small cluster.
		if (neighbourClusterIDs.size() == 0)
			continue;

		///
		/// Determine new cluster.
		///
		
		//this->debugFile << "Angle: ";

		int newClusterID;
		double M_PI = 3.14159265358979323846;
		double smallestAngle = 2*M_PI;

		// Direction of particle to its root.
		Particle particleClusterRoot = this->particleList[this->clusterList[particle.clusterID].rootParticleID];  // Requires untouched (i.e. sorting forbidden) clusterList and particleList!
		vislib::math::Vector<float, 3> dirParticle;
		dirParticle.SetX(particle.x - particleClusterRoot.x);
		dirParticle.SetY(particle.y - particleClusterRoot.y);
		dirParticle.SetZ(particle.z - particleClusterRoot.z);

		//std::vector<double> neighbourClustersAngle; // See below why deactivated.
		//neighbourClustersAngle.resize(neighbourClusterIDs.size());

		// Direction of particle to its neighbour clusters roots.
		#pragma omp parallel for
		for (int ncid = 0; ncid < neighbourClusterIDs.size(); ++ncid) {
			int clusterID = neighbourClusterIDs[ncid];
		//for (auto clusterID : neighbourClusterIDs) {
			Particle clusterRoot = this->particleList[this->clusterList[clusterID].rootParticleID]; // Requires untouched (i.e. sorting forbidden) clusterList and particleList!
			vislib::math::Vector<float, 3> dirNeighbourCluster;
			dirNeighbourCluster.SetX(particle.x - clusterRoot.x);
			dirNeighbourCluster.SetY(particle.y - clusterRoot.y);
			dirNeighbourCluster.SetZ(particle.z - clusterRoot.z);

			// Get angle.
			double angle = dirParticle.Angle(dirNeighbourCluster);

			if (angle > M_PI)
				angle -= 2 * M_PI;

			//neighbourClustersAngle[ncid] = angle; // See below why deactivated.

			// Smallest angle. Doing this comparison outside of this for loop
			// may accelerate concurrency by avoiding mutex? Nope it does not,
			// so it stays here.
			#pragma omp critical // Mutex for thread safety. 
			{
				if (angle < smallestAngle) {
					newClusterID = clusterID;
					smallestAngle = angle;
				}
			}
			//this->debugFile << angle << ", ";
		}

		//this->debugFile << "\nSmallest Angle: " << smallestAngle << ".\n";

		// Find smallest angle. Not faster than inside the angle for loop!
		//for (int ncid = 0; ncid < neighbourClusterIDs.size(); ++ncid) {
		//	int clusterID = neighbourClusterIDs[ncid];
		//	double angle = neighbourClustersAngle[ncid];
		//	if (angle < smallestAngle) {
		//		newClusterID = clusterID;
		//		smallestAngle = angle;
		//	}
		//}

		// Eventually set new cluster.
		this->clusterList[particle.clusterID].numberOfParticles--; // Remove particle from old cluster.
		particle.clusterID = newClusterID;
		this->clusterList[particle.clusterID].numberOfParticles++; // Add particle to new cluster.

		#pragma omp critical // Mutex for thread safety. 
		mergedParticles++;
	}


	///
	/// No deletion of clusters here to not destroy
	/// referencing by vector indices. Instead zero size
	/// particle clusters should be ignored by the
	/// compareCluster function.
	///

	///
	/// Log output.
	///

	int removedClusters = 0; // Count removed clusters.
	int debugSizeOneClusters = 0; // For testing MergeClusters produces adjacent gas particle clusters theory.
	int debugMinSizeClusters = 0; // For testing MergeClusters produces adjacent gas particle clusters theory.
	for (auto & cluster : this->clusterList) {
		if (cluster.numberOfParticles < this->minClusterSizeSlot.Param<param::IntParam>()->Value()) {
			debugMinSizeClusters++;
			if (cluster.numberOfParticles == 0)
				removedClusters++;
			if (cluster.numberOfParticles == 1)
				debugSizeOneClusters++;
		}
	}


	{ // Time measurement.
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_mergeClusters);
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
			"SECalc Step 2: %d particles merged and %d clusters removed with min cluster size of %d particles (%lld ms).",
			mergedParticles, removedClusters, this->minClusterSizeSlot.Param<param::IntParam>()->Value(), duration.count());

		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			this->logFile
				<< "  b) " << mergedParticles << " particles merged and "
				<< removedClusters << " clusters removed with "
				<< "min cluster size of " << this->minClusterSizeSlot.Param<param::IntParam>()->Value() << " particles (" << duration.count() << " ms)"
				<< "\n"
				<< "     (debug: "
				<< debugSizeOneClusters << " size one clusters, "
				<< debugMinSizeClusters << " min size clusters)"
				<< "\n";
			this->csvLogFile
				<< this->minClusterSizeSlot.Param<param::IntParam>()->Value() << "; " // Minimum cluster limit (particles)
				<< mergedParticles << "; " // Particles merged (#)
				<< removedClusters << "; " // Clusters removed (#)
				<< debugSizeOneClusters << "; " // SizeOneClusters for Merge Clusters debug [#clusters]
				<< debugMinSizeClusters << "; " // MinSizeClusters for Merge Clusters debug [#clusters]
				<< duration.count() << "; "; // Merge Clusters (ms)
		}
	}

	///
	/// Method 1: Search for nearest cluster (= root particle)
	/// Bad since connected components would be needed to see if cluster is adjacent.
	/// 
	/*
	int rootParticleIndex = 0;
	ANNpoint rootPts = new ANNcoord[3 * this->clusterList.size()]; // Bigger than needed.
	
	std::vector<Cluster> minClusters;
	minClusters.reserve(this->clusterList.size()); // Bigger than needed to avoid reallocation.

	for (auto cluster : this->clusterList) {
		if (cluster.numberOfParticles < this->minClusterSize) {
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

	if (this->previousClusterList.size() == 0 || this->previousParticleList.size() == 0) {
		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			this->debugFile << "SECCalc step 3: No previous data, quit cluster comparison.\n";
		}
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_WARN,
			"SECCalc step 3: No previous data, quit cluster comparison.");
		return;
	}

	std::ofstream compareAllFile;
	//std::ofstream compareSummaryFile;
	std::ofstream forwardListFile;
	std::ofstream backwardsListFile;

	// SECC == Structure Events Cluster Compare.
	std::string filenameEnd = " f" + std::to_string(this->frameId) + " p" + std::to_string(this->particleList.size());
	std::string filename = "SECC All" + filenameEnd + ".log";
	compareAllFile.open(filename.c_str());
	//filename = "SECC Summary" + filenameEnd + ".log";
	//compareSummaryFile.open(filename.c_str());
	filename = "SECC forwardList" + filenameEnd + ".csv";
	forwardListFile.open(filename.c_str());
	filename = "SECC backwardsList" + filenameEnd + ".csv";
	backwardsListFile.open(filename.c_str());

	auto time_compareClusters = std::chrono::system_clock::now();

	///
	/// Create a compareMatrix to see how many particles the clusters have in common.
	/// The inner vector contains the columns, the outer vector contains the rows.
	/// The columns represents the previous clusters. Column id == cluster id of previous clusterList.
	/// The rows represents the current clusters. Row id == cluster id of current clusterList.
	/// Access by clusterComparisonMatrix[row][column].
	///

	std::vector<std::vector<int>> clusterComparisonMatrix;
	clusterComparisonMatrix.resize(this->clusterList.size(), std::vector<int>(this->previousClusterList.size(), 0));
	//clusterComparisonMatrix.resize(this->clusterList.size() + 1, std::vector<int>(this->previousClusterList.size() + 1, 0));	// Adding a "gas cluster" at the end of each list.

	// Set gas cluster ids (for analyzing the algorithm). For debugging.
	//int currentGasClusterID = static_cast<int>(this->clusterList.size());
	//int previousGasClusterID = static_cast<int>(this->previousClusterList.size());

	// Previous to current particle comparison.
	for (int pid = 0; pid < this->particleList.size(); ++pid) { // Since particleList size stays the same for each frame, one loop is just fine.
		if (this->particleList[pid].clusterID != -1 && this->previousParticleList[pid].clusterID != -1) // Skip gas.
			clusterComparisonMatrix[this->particleList[pid].clusterID][this->previousParticleList[pid].clusterID]++; // Race condition, so no parallel processing.

		// Uses a "gas cluster" at the end of each clusterList to catch particles who were partly in gas or stay in gas. For debugging.
		//int clusterID = -2;
		//int previousClusterID = -2;
		//if (this->previousParticleList[pid].clusterID == -1)
		//	previousClusterID = previousGasClusterID;
		//else
		//	previousClusterID = this->previousParticleList[pid].clusterID;
		//if (this->particleList[pid].clusterID == -1)
		//	clusterID = currentGasClusterID;
		//else
		//	clusterID = this->particleList[pid].clusterID;
		//clusterComparisonMatrix[clusterID][previousClusterID]++; // Race condition.
	}
	
	// Count gas and percentage.
	int gasCountPrevious, gasCountCurrent;
	gasCountPrevious = gasCountCurrent = 0;
	for (auto & particle : this->previousParticleList) {
		if (particle.clusterID < 0)
			gasCountPrevious++;
	}
	for (auto & particle : this->particleList) {
		if (particle.clusterID < 0)
			gasCountCurrent++;
	}
	double gasPercentagePrevious = gasCountPrevious / static_cast<double> (this->previousParticleList.size()) * 100;
	double gasPercentageCurrent = gasCountCurrent / static_cast<double> (this->particleList.size()) * 100;

	///
	/// Debug output.
	/// Check size of comparison matrix.
	///
	/*
	if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
		this->debugFile << "Compare Matrix size: " << clusterComparisonMatrix.size() << " x " << clusterComparisonMatrix[0].size() << ".\n";

		int sum = 0;
		std::vector<int> rowSums;
		rowSums.resize(clusterComparisonMatrix.size(), 0);
		for (int row = 0; row < clusterComparisonMatrix.size(); ++row) {
			for (int column = 0; column < clusterComparisonMatrix[row].size(); ++column) {
				sum += clusterComparisonMatrix[row][column];
				rowSums[row] += clusterComparisonMatrix[row][column];
			}
		}
		this->debugFile << "Comparison Matrix sum + gasCount current/previous: " << sum << " + " << gasCountCurrent << "/" << gasCountPrevious;
		//this->debugFile << " and the sum of each row:\n";
		//for (auto & value : rowSums)
		//	this->debugFile << value << "\n";
	}
	*/

	///
	/// Detecting the type of event.
	/// First checking all new clusters for each previous cluster (forward),
	/// then vice versa (backwards).
	///

	///
	/// Forward check.
	///
	for (int pcid = 0; pcid < this->previousClusterList.size(); ++pcid) {

		PartnerClusters partnerClusters;

		// Add gas cluster. For debugging.
		//if (pcid == previousGasClusterID) {
		//	Cluster cluster;
		//	cluster.id = previousGasClusterID;
		//	cluster.numberOfParticles = gasCountPrevious;
		//	partnerClusters.cluster = cluster;
		//}
		//else {

		if (this->previousClusterList[pcid].numberOfParticles == 0)
			continue; // Skip merged clusters.
		
		/// Zero signed distance one size clusters phenomenon do _not_ produce false event, so this is not needed for that problem.
		/// MergeClusters produces adjacent gas particle clusters not occured yet, so not filtered here.
		//if (this->previousClusterList[pcid].numberOfParticles == 1)
			// Some 1 particles clusters are left
			// Zero signed distance one size clusters phenomenon:
			// - several particles with signedDistance = 0 build single particle clusters. Causes _no_ false event detection.
			// MergeClusters produces adjacent gas particle clusters theory (not yet occured); would produce bad results:
			// - kD search radius could be too high
			// - so fast-depth adds gas particles to clusters which are next to those clusters (no problem so far)
			// - during merging clusters they could be left behind when they are choosen first for the merge process.
			//   Since mergeClusters has a limited search range currently they might not find bigger clusters and remain
			//   in the same cluster. Instead their neighbours get merged because they have a lower distance to close
			//   bigger clusters. This results in small clusters which produces wrong births/deaths (especially gas particles).
			// - liquid particles could also be left in 1 particle clusters after merge b/c of the limited
			//   search range, especially when minClusterSize is big
		//	continue;

		partnerClusters.cluster = this->previousClusterList[pcid];

		//}

		//for (int cid = 0; cid < this->clusterList.size() + 1; ++cid) { // +1 for gas cluster. For debugging.
		for (int cid = 0; cid < this->clusterList.size(); ++cid) {

			// Add gas cluster.
			//if (cid == currentGasClusterID) {
			//	Cluster cluster;
			//	cluster.id = currentGasClusterID;
			//	cluster.numberOfParticles = gasCountCurrent;
			//	int numberOfCommonParticles = clusterComparisonMatrix[cid][pcid];
			//	if (numberOfCommonParticles > 0) {
			//		partnerClusters.addPartner(cluster, numberOfCommonParticles, true);
			//	}
			//	continue;
			//}

			int numberOfCommonParticles = clusterComparisonMatrix[cid][pcid];

			if (numberOfCommonParticles > 0) {
				partnerClusters.addPartner(this->clusterList[cid], numberOfCommonParticles);
			}
		}

		///
		/// Log output.
		///
		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			compareAllFile << "Previous cluster " << partnerClusters.cluster.id
				<< ", " << partnerClusters.cluster.numberOfParticles << " particles"
				<< ", " << partnerClusters.getTotalCommonParticles() << " common particles (" << partnerClusters.getTotalCommonPercentage() << "%)"
				<< ", " << partnerClusters.getLocalMaxTotalPercentage() << " % max to total ratio"
				<< ", common particles min/max (" << partnerClusters.getMinCommonParticles() << ", " << partnerClusters.getMaxCommonParticles() << ")"
				<< ", percentage min/max (" << partnerClusters.getMinCommonPercentage() << "%, " << partnerClusters.getMaxCommonPercentage() << "%)"
				<< ", " << partnerClusters.getNumberOfPartners() << " partner clusters"
				<< "\n";

			partnerClusters.sortPartners();

			for (int i = 0; i < partnerClusters.getNumberOfPartners(); ++i) {
				PartnerClusters::PartnerCluster cc = partnerClusters.getPartner(i);
				compareAllFile << "Cluster " << cc.cluster.id << " (size " << cc.cluster.numberOfParticles << "): " << cc.commonParticles << " common"
					<< ", ratio this/global (" << cc.getCommonPercentage() << "%, " << cc.getClusterCommonPercentage(partnerClusters.cluster) << "%)"
					<< "\n";
			}
			compareAllFile << "\n";
		}

		this->partnerClustersList.forwardList.push_back(partnerClusters);
	}


	///
	/// Backwards check.
	///
	for (int cid = 0; cid < this->clusterList.size(); ++cid) {

		PartnerClusters partnerClusters;

		// Add gas cluster. For debugging.
		//if (cid == currentGasClusterID) {
		//	Cluster cluster;
		//	cluster.id = currentGasClusterID;
		//	cluster.numberOfParticles = gasCountCurrent;
		//	partnerClusters.cluster = cluster;
		//}
		//else {

		if (this->clusterList[cid].numberOfParticles == 0)
			continue; // Skip merged clusters.

		/// See explanations in forward.
		//if (this->clusterList[cid].numberOfParticles == 1)
			// Some 1 particles clusters are left: see explanations in forward
		//	continue;

		partnerClusters.cluster = this->clusterList[cid];

		//}
		

		//for (int pcid = 0; pcid < this->previousClusterList.size() + 1; ++pcid) { // +1 for gas cluster. For debugging.
		for (int pcid = 0; pcid < this->previousClusterList.size(); ++pcid) {

			// Add gas cluster. For debugging.
			//if (pcid == previousGasClusterID) {
			//	Cluster cluster;
			//	cluster.id = previousGasClusterID;
			//	cluster.numberOfParticles = gasCountPrevious;
			//	int numberOfCommonParticles = clusterComparisonMatrix[cid][pcid];
			//	if (numberOfCommonParticles > 0) {
			//		partnerClusters.addPartner(cluster, numberOfCommonParticles, true);
			//	}
			//	continue;
			//}

			int numberOfCommonParticles = clusterComparisonMatrix[cid][pcid];

			if (numberOfCommonParticles > 0) {
				partnerClusters.addPartner(this->previousClusterList[pcid], numberOfCommonParticles);
			}
		}

		///
		/// Log output.
		///
		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			compareAllFile << "Cluster " << partnerClusters.cluster.id
				<< ", " << partnerClusters.cluster.numberOfParticles << " particles"
				<< ", " << partnerClusters.getTotalCommonParticles() << " common particles (" << partnerClusters.getTotalCommonPercentage() << "%)"
				<< ", " << partnerClusters.getLocalMaxTotalPercentage() << " % max to total ratio"
				<< ", common particles min/max (" << partnerClusters.getMinCommonParticles() << ", " << partnerClusters.getMaxCommonParticles() << ")"
				<< ", percentage min/max (" << partnerClusters.getMinCommonPercentage() << "%, " << partnerClusters.getMaxCommonPercentage() << "%)"
				<< ", " << partnerClusters.getNumberOfPartners() << " partner clusters"
				<< "\n";

			partnerClusters.sortPartners();

			for (int i = 0; i < partnerClusters.getNumberOfPartners(); ++i) {
				PartnerClusters::PartnerCluster pc = partnerClusters.getPartner(i);
				compareAllFile << "Previous cluster " << pc.cluster.id << " (size " << pc.cluster.numberOfParticles << "): " << pc.commonParticles << " common"
					<< ", ratio this/global (" << pc.getCommonPercentage() << "%, " << pc.getClusterCommonPercentage(partnerClusters.cluster) << "%)"
					<< "\n";
			}
			compareAllFile << "\n";
		}
		
		this->partnerClustersList.backwardsList.push_back(partnerClusters);
	}

	///
	/// Naive coloring by comparing clusterIDs.
	///
	/*
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
	*/

	///
	/// Coloring of descendant clusters by using their biggest ancestor.
	///
	#pragma omp parallel for
	for (int cli = 0; cli < this->clusterList.size(); ++cli) {
		bool colored = false;
		if (this->clusterList[cli].numberOfParticles > 0) {
			PartnerClusters* pcs = this->partnerClustersList.getPartnerClusters(this->clusterList[cli].id, PartnerClustersList::Direction::backwards);
			if (pcs != NULL) {
				for (int pci = 0; pci < pcs->getNumberOfPartners(); ++pci) {
					PartnerClusters::PartnerCluster pc = pcs->getPartner(pci);
					// Get partner with most common particles.
					if (pcs->getMaxCommonParticles() == pc.commonParticles) {
						clusterList[cli].r = pc.cluster.r;
						clusterList[cli].g = pc.cluster.g;
						clusterList[cli].b = pc.cluster.b;
						colored = true;
						break;
					}
				}
			}
		}
		if (colored == false) {

			// Use root particle position for coloring.
			const Particle* p = &this->particleList[this->clusterList[cli].rootParticleID];
			vislib::math::Vector<float, 3> color(p->x, p->y, p->z);
			this->normalizeToColorComponent(color, p->id);
			clusterList[cli].r = color.GetX();
			clusterList[cli].g = color.GetY();
			clusterList[cli].b = color.GetZ();

			// Set random color. Alternative.
			//std::random_device rd;
			//std::mt19937_64 mt(rd());
			//std::uniform_real_distribution<float> distribution(0, 1);
			//clusterList[cli].r = distribution(mt);
			//clusterList[cli].g = distribution(mt);
			//clusterList[cli].b = distribution(mt);
		}
	}

	///
	/// Log output.
	///
	if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {

		///
		/// Evaluation:
		/// - csv to create diagrams
		/// - for frame to frame comparison: Mean value and standard deviation of values.
		///
		std::vector<double> vTotalCommonPercentage;
		forwardListFile << "Cluster id; Cluster size [#]; Common particles [#]; Common particles [%]; Partners [#]; Average partner common particles [%]; "
			<< "LocalMaxTotal [%]; "
			<< "75 % bp; 50 % bp; 45 % bp; 40 % bp; 35 % bp; 30 % bp; 25 % bp; 20 % bp; 10 % sp; 1 % sp; "
			<< "bp = big partners, sp = small partners"
			<< "\n";
		for (auto partnerClusters : this->partnerClustersList.forwardList) {
			forwardListFile << partnerClusters.cluster.id << ";"
				<< partnerClusters.cluster.numberOfParticles << ";"
				<< partnerClusters.getTotalCommonParticles() << ";"
				<< partnerClusters.getTotalCommonPercentage() << ";"
				<< partnerClusters.getNumberOfPartners() << ";"
				<< partnerClusters.getAveragePartnerCommonPercentage() << ";"
				<< partnerClusters.getLocalMaxTotalPercentage() << ";"
				<< partnerClusters.getBigPartnerAmount(75) << ";"
				<< partnerClusters.getBigPartnerAmount(50) << ";"
				<< partnerClusters.getBigPartnerAmount(45) << ";"
				<< partnerClusters.getBigPartnerAmount(40) << ";"
				<< partnerClusters.getBigPartnerAmount(35) << ";"
				<< partnerClusters.getBigPartnerAmount(30) << ";"
				<< partnerClusters.getBigPartnerAmount(25) << ";"
				<< partnerClusters.getBigPartnerAmount(20) << ";"
				<< partnerClusters.getSmallPartnerAmount(10) << ";"
				<< partnerClusters.getSmallPartnerAmount(1)
				<< "\n";
			vTotalCommonPercentage.push_back(partnerClusters.getTotalCommonPercentage());
		}

		MeanStdDev mdTotalCommonPercentageFwd = meanStdDeviation(vTotalCommonPercentage);
		vTotalCommonPercentage.clear();

		backwardsListFile << "Cluster id; Cluster size [#]; Common particles [#]; Common particles [%]; Partners [#]; Average partner common particles [%]; "
			<< "LocalMaxTotal [%]; "
			<< "75 % bp; 50 % bp; 45 % bp; 40 % bp; 35 % bp; 30 % bp; 25 % bp; 20 % bp; 10 % sp; 1 % sp; "
			<< "bp = big partners, sp = small partners"
			<< "\n";
		for (auto partnerClusters : this->partnerClustersList.backwardsList) {
			backwardsListFile << partnerClusters.cluster.id << ";"
				<< partnerClusters.cluster.numberOfParticles << ";"
				<< partnerClusters.getTotalCommonParticles() << ";"
				<< partnerClusters.getTotalCommonPercentage() << ";"
				<< partnerClusters.getNumberOfPartners() << ";"
				<< partnerClusters.getAveragePartnerCommonPercentage() << ";"
				<< partnerClusters.getLocalMaxTotalPercentage() << ";"
				<< partnerClusters.getBigPartnerAmount(75) << ";"
				<< partnerClusters.getBigPartnerAmount(50) << ";"
				<< partnerClusters.getBigPartnerAmount(45) << ";"
				<< partnerClusters.getBigPartnerAmount(40) << ";"
				<< partnerClusters.getBigPartnerAmount(35) << ";"
				<< partnerClusters.getBigPartnerAmount(30) << ";"
				<< partnerClusters.getBigPartnerAmount(25) << ";"
				<< partnerClusters.getBigPartnerAmount(20) << ";"
				<< partnerClusters.getSmallPartnerAmount(10) << ";"
				<< partnerClusters.getSmallPartnerAmount(1)
				<< "\n";
			vTotalCommonPercentage.push_back(partnerClusters.getTotalCommonPercentage());
		}

		MeanStdDev mdTotalCommonPercentageBw = meanStdDeviation(vTotalCommonPercentage);

		///
		/// Frame to frame evaluation: Min/Max/Mean/StdDev.
		///
		PartnerClusters maxPercentageFwd = partnerClustersList.getMaxPercentage();
		PartnerClusters minPercentageFwd = partnerClustersList.getMinPercentage();
		PartnerClusters maxPercentageBw = partnerClustersList.getMaxPercentage(PartnerClustersList::Direction::backwards);
		PartnerClusters minPercentageBw = partnerClustersList.getMinPercentage(PartnerClustersList::Direction::backwards);

		this->logFile << "Step 3 (compare clusters), forward/fw (previous -> current), backwards/bw (current -> previous):"
			<< "\n"
			<< "  - Gas ratio frames: previous " << gasPercentagePrevious << "% (" << gasCountPrevious << "), "
			<< "current " << gasPercentageCurrent << "% (" << gasCountCurrent << ")."
			<< "\n"
			<< "  - Fw ratio: "
			<< "Min " << minPercentageFwd.getTotalCommonPercentage() << "% (cl " << minPercentageFwd.cluster.id << ", localMax to total ratio " << minPercentageFwd.getLocalMaxTotalPercentage() << "%), "
			<< "Mean " << mdTotalCommonPercentageFwd.mean << "% (std deviation " << mdTotalCommonPercentageFwd.deviation << "%), "
			<< "Max " << maxPercentageFwd.getTotalCommonPercentage() << "% (cl " << maxPercentageFwd.cluster.id << ", localMax to total ratio " << maxPercentageFwd.getLocalMaxTotalPercentage() << "%)"
			<< "\n"
			<< "  - Bw ratio: "
			<< "Min " << minPercentageBw.getTotalCommonPercentage() << "% (cl " << minPercentageBw.cluster.id << ", localMax to total ratio " << minPercentageBw.getLocalMaxTotalPercentage() << "%), "
			<< "Mean " << mdTotalCommonPercentageBw.mean << "% (std deviation " << mdTotalCommonPercentageBw.deviation << "%), "
			<< "Max " << maxPercentageBw.getTotalCommonPercentage() << "% (cl " << maxPercentageBw.cluster.id << ", localMax to total ratio " << maxPercentageBw.getLocalMaxTotalPercentage() << "%)"
			<< "\n";
		
		this->csvLogFile << minPercentageFwd.getTotalCommonPercentage() << "; " // Forward min common particle ratio (%)
			<< mdTotalCommonPercentageFwd.mean << "; " // Forward mean common particle ratio (%)
			<< mdTotalCommonPercentageFwd.deviation << "; " // Forward common particle ratio std deviation (%)
			<< maxPercentageFwd.getTotalCommonPercentage() << "; " // Forward max common particle ratio (%)
			<< minPercentageBw.getTotalCommonPercentage() << "; " // Backwards min common particle ratio (%)
			<< mdTotalCommonPercentageBw.mean << "; " // Backwards mean common particle ratio (%)
			<< mdTotalCommonPercentageBw.deviation << "; " // Backwards common particle ratio std deviation (%)
			<< maxPercentageBw.getTotalCommonPercentage() << "; "; // Backwards max common particle ratio (%)

		///
		/// Summary evaluation: Most common/uncommon clusters, critical values have to be evaluated depending on:
		/// - how many clusters are close to MinPercentage
		/// - how many clusters are close to MaxPercentage
		/// - how many clusters are close to median.
		///
		/// Number of clusters with TotalCommonPercentage > 70%, 50%, 30%, ... . Fwd: Detect split, bw: Detect merge.
		/// Number of clusters with TotalCommonPercentage < 5%, 15%, 25%. Fwd: Detect birth, bw: Detect death.? Don't.
		/// Specific numbers have to be tested!
		///
	}

	{ // Time measurement.
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_compareClusters);
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
			"SECalc step 3: Compared clusters (%lld ms).\n", duration.count());

		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			this->logFile << "  - step 3 required " << duration.count() << " ms\n";
			this->csvLogFile << duration.count() << "; "; // Compare Clusters (ms)
		}
	}

	compareAllFile.close();
}


void mmvis_static::StructureEventsClusterVisualization::determineStructureEvents() {

	if (this->partnerClustersList.forwardList.size() == 0 || this->partnerClustersList.backwardsList.size() == 0) {
		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			this->debugFile << "SECalc step 4: No comparison data, quit determination of StructureEvents.\n";
		}
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_WARN,
			"SECalc step 4: No comparison data, quit determination of StructureEvents.");
		return;
	}

	///
	/// Log output.
	///

	std::ofstream testEventsCSVFile;

	// 0 := Birth, 1 := Death, 2 := Merge, 3 := Split.
	int eventAmount[4] = { 0 };
	
	// For test output.
	int partnerAmount25p3, partnerAmount30p2, partnerAmount30p3, partnerAmount35p2, partnerAmount40p2, partnerAmount45p2;
	partnerAmount25p3 = partnerAmount30p2 = partnerAmount30p3 = partnerAmount35p2 = partnerAmount40p2 = partnerAmount45p2 = 0;
	const int birthDeathTestAmount = 5;
	int deathAmount[birthDeathTestAmount] = { 0 };

	if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
		// DSE == Determine Structure Events
		//std::string filenameEnd = " f" + std::to_string(this->frameId) + " p" + std::to_string(this->particleList.size());
		std::string filename = "SECalc DSE Test.csv"; // +filenameEnd + ".log";
		testEventsCSVFile.open(filename.c_str(), std::ios_base::app | std::ios_base::out);
		std::ifstream testEventsCSVFilePeekTest;
		testEventsCSVFilePeekTest.open(filename.c_str());

		if (testEventsCSVFilePeekTest.peek() == std::ifstream::traits_type::eof()) {
			testEventsCSVFile
				<< "Time; "
				<< "Frame ID; "
				<< "25%, 3+ Split (#prevClusters); "
				<< "30%, 2+ Split (#prevClusters); "
				<< "30%, 3+ Split (#prevClusters); "
				<< "35%, 2+ Split (#prevClusters); "
				<< "40%, 2+ Split (#prevClusters); "
				<< "45%, 2+ Split (#prevClusters); ";

			for (int limit = 0; limit < birthDeathTestAmount; ++limit)
				testEventsCSVFile << limit + 1 << "% Death (#prevClusters); ";

			testEventsCSVFile
				<< "25%, 3+ Merge (#clusters); "
				<< "30%, 2+ Merge (#clusters); "
				<< "30%, 3+ Merge (#clusters); "
				<< "35%, 2+ Merge (#clusters); "
				<< "40%, 2+ Merge (#clusters); "
				<< "45%, 2+ Merge (#clusters); ";

			for (int limit = 0; limit < birthDeathTestAmount; ++limit)
				testEventsCSVFile << limit + 1 << "% Birth (#clusters); ";

			testEventsCSVFile
				<< "Split and merge are using limits for big partners"
				<< "\n";
		}
		testEventsCSVFilePeekTest.close();

		testEventsCSVFile
			<< this->timeOutputCache << "; "
			<< this->frameId << "; ";
	}
	
	auto time_setStructureEvents = std::chrono::system_clock::now();

	///
	/// Forward direction.
	///
	for (auto partnerClusters : this->partnerClustersList.forwardList) {
		
		if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
			// For test output.
			if (partnerClusters.getBigPartnerAmount(25) > 2)
				partnerAmount25p3++;
			if (partnerClusters.getBigPartnerAmount(30) > 1)
				partnerAmount30p2++;
			if (partnerClusters.getBigPartnerAmount(30) > 2)
				partnerAmount30p3++;
			if (partnerClusters.getBigPartnerAmount(35) > 1)
				partnerAmount35p2++;
			if (partnerClusters.getBigPartnerAmount(40) > 1)
				partnerAmount40p2++;
			if (partnerClusters.getBigPartnerAmount(45) > 1)
				partnerAmount45p2++;
			for (int limit = 0; limit < birthDeathTestAmount; ++limit) {
				if (partnerClusters.getNumberOfPartners() == 0 || partnerClusters.getTotalCommonPercentage() <= limit + 1) {
					deathAmount[limit]++;
				}
			}
		}

		// Detect split.
		if (partnerClusters.getBigPartnerAmount(this->msMinCPPercentageSlot.Param<param::FloatParam>()->Value()) >= this->msMinClusterAmountSlot.Param<param::IntParam>()->Value()) {
			StructureEvents::StructureEvent se;
			se.x = this->previousParticleList[partnerClusters.cluster.rootParticleID].x;
			se.y = this->previousParticleList[partnerClusters.cluster.rootParticleID].y;
			se.z = this->previousParticleList[partnerClusters.cluster.rootParticleID].z;
			se.time = static_cast<float>(this->frameId);
			se.type = StructureEvents::SPLIT;
			this->structureEvents.push_back(se);
			eventAmount[3]++;
		}

		// Detect death.
		if (partnerClusters.getNumberOfPartners() == 0 || partnerClusters.getTotalCommonPercentage() <= this->bdMaxCPPercentageSlot.Param<param::FloatParam>()->Value()) {
			StructureEvents::StructureEvent se;
			se.x = this->previousParticleList[partnerClusters.cluster.rootParticleID].x;
			se.y = this->previousParticleList[partnerClusters.cluster.rootParticleID].y;
			se.z = this->previousParticleList[partnerClusters.cluster.rootParticleID].z;
			se.time = static_cast<float>(this->frameId);
			se.type = StructureEvents::DEATH;
			this->structureEvents.push_back(se);
			eventAmount[1]++;
		}
	}

	// Test output.
	if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
		testEventsCSVFile
			<< partnerAmount25p3 << "; " // Split 25%, 3+ (#prevClusters)
			<< partnerAmount30p2 << "; " // Split 30%, 2+ (#prevClusters)
			<< partnerAmount30p3 << "; " // Split 30%, 3+ (#prevClusters)
			<< partnerAmount35p2 << "; " // Split 35%, 2+ (#prevClusters)
			<< partnerAmount40p2 << "; " // Split 40%, 2+ (#prevClusters)
			<< partnerAmount45p2 << "; " // Split 45%, 2+ (#prevClusters)
			<< deathAmount[0] << "; " // Death 1%
			<< deathAmount[1] << "; " // Death 2%
			<< deathAmount[2] << "; " // Death 3%
			<< deathAmount[3] << "; " // Death 4%
			<< deathAmount[4] << "; "; // Death 5%
	}
	
	///
	/// Backward direction.
	///

	// For test output.
	partnerAmount25p3 = partnerAmount30p2 = partnerAmount30p3 = partnerAmount35p2 = partnerAmount40p2 = partnerAmount45p2 = 0;
	int birthAmount[5] = { 0 };

	for (auto partnerClusters : this->partnerClustersList.backwardsList) {
		// For test output.
		if (partnerClusters.getBigPartnerAmount(25) > 2)
			partnerAmount25p3++;
		if (partnerClusters.getBigPartnerAmount(30) > 1)
			partnerAmount30p2++;
		if (partnerClusters.getBigPartnerAmount(30) > 2)
			partnerAmount30p3++;
		if (partnerClusters.getBigPartnerAmount(35) > 1)
			partnerAmount35p2++;
		if (partnerClusters.getBigPartnerAmount(40) > 1)
			partnerAmount40p2++;
		if (partnerClusters.getBigPartnerAmount(45) > 1)
			partnerAmount45p2++;
		for (int limit = 0; limit < birthDeathTestAmount; ++limit) {
			if (partnerClusters.getNumberOfPartners() == 0 || partnerClusters.getTotalCommonPercentage() <= limit + 1) {
				birthAmount[limit]++;
			}
		}

		// Detect merge.
		if (partnerClusters.getBigPartnerAmount(this->msMinCPPercentageSlot.Param<param::FloatParam>()->Value()) >= this->msMinClusterAmountSlot.Param<param::IntParam>()->Value()) {
			StructureEvents::StructureEvent se;
			se.x = this->particleList[partnerClusters.cluster.rootParticleID].x;
			se.y = this->particleList[partnerClusters.cluster.rootParticleID].y;
			se.z = this->particleList[partnerClusters.cluster.rootParticleID].z;
			se.time = static_cast<float>(this->frameId);
			se.type = StructureEvents::MERGE;
			this->structureEvents.push_back(se);
			eventAmount[2]++;
		}

		// Detect birth.
		if (partnerClusters.getNumberOfPartners() == 0 || partnerClusters.getTotalCommonPercentage() <= this->bdMaxCPPercentageSlot.Param<param::FloatParam>()->Value()) {
			StructureEvents::StructureEvent se;
			se.x = this->particleList[partnerClusters.cluster.rootParticleID].x;
			se.y = this->particleList[partnerClusters.cluster.rootParticleID].y;
			se.z = this->particleList[partnerClusters.cluster.rootParticleID].z;
			se.time = static_cast<float>(this->frameId);
			se.type = StructureEvents::DEATH;
			this->structureEvents.push_back(se);
			eventAmount[0]++;
		}
	}
	
	// Test output.
	if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
		testEventsCSVFile
			<< partnerAmount25p3 << "; " // Merge 25%, 3+ (#prevClusters)
			<< partnerAmount30p2 << "; " // Merge 30%, 2+ (#clusters)
			<< partnerAmount30p3 << "; " // Merge 30%, 3+ (#clusters)
			<< partnerAmount35p2 << "; " // Merge 35%, 2+ (#clusters)
			<< partnerAmount40p2 << "; " // Merge 40%, 2+ (#clusters)
			<< partnerAmount45p2 << "; " // Merge 45%, 2+ (#clusters)
			<< birthAmount[0] << "; " // Birth 1%
			<< birthAmount[1] << "; " // Birth 2%
			<< birthAmount[2] << "; " // Birth 3%
			<< birthAmount[3] << "; " // Birth 4%
			<< birthAmount[4] << "; "; // Birth 5%
	}

	///
	/// Set maximum time.
	///
	this->seMaxTimeCache = std::max_element(
		this->structureEvents.begin(), this->structureEvents.end(), [](const StructureEvents::StructureEvent& lhs, const StructureEvents::StructureEvent& rhs) {
		return lhs.time < rhs.time;
	})->time;

	///
	/// Change hash to flag that sedc data has changed.
	///
	this->sedcHash = this->sedcHash != 1 ? 1 : 2;

	///
	/// Log output.
	///
	// Time measurement.
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_setStructureEvents);
	vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
		"SECalc step 4: Determined structure events (%lld ms).", duration.count());

	if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
		this->logFile
			<< "Step 4 (structure events) detected " << std::accumulate(eventAmount, eventAmount + 4, 0) << " events"
			<< " (" << duration.count() << " ms):\n"
			<< "  - "
			<< eventAmount[0] << " births, "
			<< eventAmount[1] << " deaths, "
			<< eventAmount[2] << " merges, "
			<< eventAmount[3] << " splits"
			<< "\n"
			<< "  - limits for merge/split detection:"
			<< " at least " << this->msMinClusterAmountSlot.Param<param::IntParam>()->Value() << " partner clusters"
			<< " with " << this->msMinCPPercentageSlot.Param<param::FloatParam>()->Value() << "% common particles ratio"
			<< "\n"
			<< "  - maximum limit of total common particles ratio for birth/death: "
			<< this->bdMaxCPPercentageSlot.Param<param::FloatParam>()->Value() << "%"
			<< "\n";
		this->csvLogFile
			<< this->msMinClusterAmountSlot.Param<param::IntParam>()->Value() << "; " // Minimal number of big partner clusters for merge / split (#cluster)
			<< this->msMinCPPercentageSlot.Param<param::FloatParam>()->Value() << "; " // Minimal common particles ratio limit of big partners for merge / split (%)
			<< this->bdMaxCPPercentageSlot.Param<param::FloatParam>()->Value() << "; " // Maximum total common particles ratio limit for birth / death (%)
			<< eventAmount[0] << "; " // Births (#events)
			<< eventAmount[1] << "; " // Deaths (#events)
			<< eventAmount[2] << "; " // Merges (#events)
			<< eventAmount[3] << "; " // Splits (#events)
			<< std::accumulate(eventAmount, eventAmount + 4, 0) << "; " // Total events (#events)
			<< duration.count() << "; "; // Determine structure events (ms)

		testEventsCSVFile << "\n";
		testEventsCSVFile.close();
	}
}


void mmvis_static::StructureEventsClusterVisualization::setClusterColor(const bool renewClusterColors) {
	auto time_setClusterColor = std::chrono::system_clock::now();

	size_t debugBlackParticles = 0;

	///
	/// Colourize cluster.
	///
	if (renewClusterColors) {
		#pragma omp parallel for
		for (int cli = 0; cli < this->clusterList.size(); ++cli) {
			
			// Set color by particle position.
			const Particle* p = &this->particleList[this->clusterList[cli].rootParticleID];
			vislib::math::Vector<float, 3> color(p->x, p->y, p->z);
			this->normalizeToColorComponent(color, p->id);
			clusterList[cli].r = color.GetX();
			clusterList[cli].g = color.GetY();
			clusterList[cli].b = color.GetZ();

			// Set random color.
			//std::random_device rd;
			//std::mt19937_64 mt(rd());
			//std::uniform_real_distribution<float> distribution(0, 1);
			//clusterList[cli].r = distribution(mt);
			//clusterList[cli].g = distribution(mt);
			//clusterList[cli].b = distribution(mt);
			if (this->clusterList[cli].rootParticleID < 0) { // Debug.
				printf("Cl: %d.\n", this->clusterList[cli].rootParticleID);
			}
		}
	}

	///
	/// Colourize particles.
	///
	#pragma omp parallel for
	for (int pli = 0; pli < this->particleList.size(); ++pli) {
		Particle* particle = &this->particleList[pli];

		// Gas. Orange.
		if (particle->clusterID == -1) {
			particle->r = this->gasColor[0];
			particle->g = this->gasColor[1];
			particle->b = this->gasColor[2];
			continue;
		}

		// Cluster.
		particle->r = this->clusterList[particle->clusterID].r;
		particle->g = this->clusterList[particle->clusterID].g;
		particle->b = this->clusterList[particle->clusterID].b;

		/*
		if (particle.r < 0) { // Debug wrong clusterPtr, points to nothing: ints = -572662307; floats = -1998397155538108400.000000 for all -> points to reallocated address!
			debugBlackParticles++;
			if (particle.id % 100 == 0)
				printf("Part: %d, %d (%f, %f, %f).\n", this->clusterList[particle.clusterID].rootParticleID, this->clusterList[particle.clusterID].numberOfParticles, particle.r, particle.g, particle.b);
		}
		*/
	}

	///
	/// Log output.
	///

	// Time measurement.
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_setClusterColor);
	vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
		"SECalc output: Colorized %d clusters with (debug) %d black particles (%lld ms).",
		this->clusterList.size(), debugBlackParticles, duration.count());
	//this->logFile << debugBlackParticles << " black p.";
}


void mmvis_static::StructureEventsClusterVisualization::setSignedDistanceColor(const float min, const float max) {
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
			element.r = this->gasColor[0];
			element.g = this->gasColor[1];
			element.b = this->gasColor[2];
			//element.g = 1 - element.signedDistance / min; element.r = element.b = 0.f;
		}
	}
}


void mmvis_static::StructureEventsClusterVisualization::setDummyLists(int particleAmount, int clusterAmount, int eventAmount) {
	this->particleList.resize(particleAmount);
	this->previousParticleList.resize(particleAmount);
	this->clusterList.resize(clusterAmount);
	this->previousClusterList.resize(clusterAmount);
	this->structureEvents.resize(eventAmount);

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

		// Set root particle.
		std::uniform_int_distribution<uint64_t> distRoot(0, particleAmount - 1);
		this->previousClusterList[i].rootParticleID = distRoot(mt);
		this->clusterList[i].rootParticleID = distRoot(mt);
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
		std::uniform_int_distribution<int> distribution2(std::max(0, this->particleList[i].clusterID - streuung), std::min(clusterAmount - 1, this->particleList[i].clusterID + streuung));
		this->previousParticleList[i].clusterID = distribution2(mt);

		// Set position.
		std::uniform_real_distribution<float> disPos(0, 500);
		this->particleList[i].x = disPos(mt);
		this->particleList[i].y = disPos(mt);
		this->particleList[i].z = disPos(mt);
	}
	
	// Cluster event.
	#pragma omp parallel for
	for (int i = 0; i < eventAmount; ++i) {
		std::random_device rd;
		std::mt19937_64 mt(rd());
		std::uniform_real_distribution<float> disPos(0, 100);
		this->structureEvents[i].x = disPos(mt);
		this->structureEvents[i].y = disPos(mt);
		this->structureEvents[i].z = disPos(mt);

		std::uniform_int_distribution<int> disTime(0, 140);
		this->structureEvents[i].time = static_cast<float> (disTime(mt));

		std::uniform_int_distribution<int> disType(0, 3);
		this->structureEvents[i].type = StructureEvents::getEventType(disType(mt));
	}


	///
	/// Log output.
	///
	if (this->quantitativeDataOutputSlot.Param<param::BoolParam>()->Value()) {
		this->logFile << "Skipped step 1 and step 2 since dummy list is set.\n";
		this->csvLogFile << particleAmount << "; ";
		for (int numberOfSkippedFields = 0; numberOfSkippedFields < 5; ++numberOfSkippedFields)
			this->csvLogFile << "; ";
		this->csvLogFile << clusterAmount << "; ";
		for (int numberOfSkippedFields = 0; numberOfSkippedFields < 13; ++numberOfSkippedFields)
			this->csvLogFile << "; ";
	}

	vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
		"SECalc: Skipped step 1 and step 2 since dummy list is set.\nDummy lists set: %d, %d, %d.", particleAmount, clusterAmount, eventAmount);
}


mmvis_static::StructureEventsClusterVisualization::MeanStdDev
mmvis_static::StructureEventsClusterVisualization::meanStdDeviation(std::vector<double> v) {
	double sum = std::accumulate(v.begin(), v.end(), 0.0);
	double mean = sum / v.size();
	
	std::vector<double> diff(v.size());
	std::transform(v.begin(), v.end(), diff.begin(),
		std::bind2nd(std::minus<double>(), mean));
	double sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
	double stdev = std::sqrt(sq_sum / v.size());
	
	MeanStdDev md;
	md.mean = mean;
	md.deviation = stdev;
	return md;
}


const int mmvis_static::StructureEventsClusterVisualization::getKDTreeMaxNeighbours(const int radiusMultiplier) {
	int maxNeighbours = 0;
	switch (radiusMultiplier){
	case 2: // Not tested, so using safe maxNeighbours amount.
	case 3: // Not tested, so using safe maxNeighbours amount.
	case 4:
		maxNeighbours = 35;
		break;
	case 5:
		maxNeighbours = 60;
		break;
	case 6:
		maxNeighbours = 100;
		break;
	case 7:
		maxNeighbours = 155;
		break;
	case 8: // Not tested, so using safe maxNeighbours amount.
	case 9: // Not tested, so using safe maxNeighbours amount.
	case 10:
		maxNeighbours = 425;
		break;
	}
	return maxNeighbours;
}


void mmvis_static::StructureEventsClusterVisualization::normalizeToColorComponent(vislib::math::Vector<float, 3> &output, const uint64_t modificator) {

	int gasColorSimilarity = 0;
	float epsilon = 0.3f; // Needs to be big to catch different brightness but similar hue.

	for (int i = 0; i < 3; ++i) {
		output[i] *= modificator % 1000;
		//output[i] += modificator; // If modificator big, impact is too big: everything is gray.
		for (int division = 10; division <= 100000000; division *= 10) {
			if (output[i] <= division) {
				output[i] /= division;
				break;
			}
		}
		if (output[i] > 1)
			output[i] = .5f; // Fallback for values that are too big.

		if (output[i] >= this->gasColor[i] - epsilon && output[i] <= this->gasColor[i] + epsilon)
			gasColorSimilarity++;
	}

	// Check similarities to gas
	if (gasColorSimilarity == 3) {
		/*
		for (int i = 0; i < 3; ++i) {
			output[i] += 1.f / (float)(modificator % 8 + 1);
			if (output[i] > 1.f) {
				if (output[i] <= 1.5f)
					output[i] -= .5f;
				else
					output[i] -= 1.f;
			}
		}
		*/

		int item = modificator % 3;
		output[item] += .3f;
		if (output[item] > 1.f)
			output[item] -= 1.f;
	}

	if (output[0] + output[1] + output[2] < .2f) // I don't like dark colors.
		output[modificator % 3] += .5f;
	//color.Normalise(); // Big position component gets favored (everything mainly green).
}


/*
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
