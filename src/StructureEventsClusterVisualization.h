/**
 * StructureEventsClusterVisualization.h
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Copyright (C) 2015 by Richard Hähne, TU Dresden
 * Alle Rechte vorbehalten.
 *
 * Adds color to each cluster detected by the contour
 * tree algorithm.
 *
 * ToDo:
 *  - Contour tree algorithm
 */

#ifndef MMVISSTATIC_StructureEventsClusterVisualization_H_INCLUDED
#define MMVISSTATIC_StructureEventsClusterVisualization_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "mmcore/CallerSlot.h"
#include "mmcore/CalleeSlot.h"
#include "mmcore/param/ParamSlot.h"
#include "mmcore/Module.h"
#include "mmcore/moldyn/MultiParticleDataCall.h"
#include "StructureEventsDataCall.h"
#include "glm/glm/glm.hpp"

#include <map>
#include <functional>

// File operations.
#include <iostream>
#include <fstream>


namespace megamol {
	namespace mmvis_static {
		/**
		 * TODO: This class is a stub!
		 */
		class StructureEventsClusterVisualization : public core::Module {
		public:

			struct MeanStdDev {
				double mean = 0;
				double deviation = 0;
			};

			struct Cluster;

			///
			/// Dichtgepacktes struct.
			/// It is important that no automatic padding is inserted by compiler
			/// therefore all datatypes have sizes of 4 or 8.
			///
			/// MPDC data types: FLOAT_XYZR, FLOAT_RGB.
			/// Stride = (4 byte * (4 + 3 + 1)) + 8 byte + Cluster + Neighbours.
			///
			struct Particle {
				float x, y, z, radius;
				float r, g, b;
				float signedDistance;
				uint64_t id;

				// Store a pointer to the cluster. Bad when cluster is moved in memory.
				//Cluster* clusterPtr = NULL;
				// Store the cluster id. Save when cluster list is moved in memory.
				int clusterID = -1;

				// Store pointer to the neighbours. Bad when they are moved in memory.
				//std::vector<Particle*> neighbourPtrs;
				// Store ids of the neighbours. Save when particle list is moved in memory.
				std::vector<uint64_t> neighbourIDs;

				// Copy & store neighbour directly to avoid costly
				// list search everytime we need a neighbour.
				// Doesn't work, takes way too much memory!
				// std::vector<Particle> neighbours;

				bool operator==(const Particle& rhs) const {
					return this->id == rhs.id;
				}
			};

			struct Cluster {
				uint64_t rootParticleID;
				uint64_t numberOfParticles = 0;
				int id;
				float r, g, b = 0; // For visualization.

				bool operator==(const Cluster& rhs) const {
					return this->id == rhs.id;
				}
			};

			class PartnerClusters {
			public:
				struct PartnerCluster {
					Cluster cluster;
					int commonParticles = 0;
					bool isGasCluster = false;

					//PartnerClusters &parent;  // Reference to parent
					//PartnerCluster(PartnerClusters &ccs) : parent(ccs) {}  // Initialise reference in constructor

					/// Common percentage with this (partner) cluster.
					double getCommonPercentage() {
						return (static_cast<float> (this->commonParticles) / static_cast<float> (this->cluster.numberOfParticles)) * 100;
					}

					/// Common percentage with parent cluster.
					double getClusterCommonPercentage(const Cluster& c) {
						return (static_cast<float> (this->commonParticles) / static_cast<float> (c.numberOfParticles)) * 100;
					}
					/*
					PartnerCluster operator=(const PartnerCluster& rhs) {
						//PartnerCluster cc(rhs.parent);
						cc.cluster = rhs.cluster;
						cc.commonParticles = rhs.commonParticles;
						return cc;
					}
					*/
					bool operator==(const PartnerCluster& rhs) const {
						return this->cluster == rhs.cluster;
					}

					//bool operator<(const PartnerCluster& rhs) const {
					//	return this->commonParticles > rhs.commonParticles;
					//}
				};
			private:
				std::vector<PartnerCluster> partners;
				//std::multimap<int, PartnerCluster, std::greater<int>> partners; // Highest int first.
				int minCommonParticles = -1;
				int maxCommonParticles = -1;
				int totalCommonParticles = 0;
				double minCommonPercentage = -1.f;
				double maxCommonPercentage = -1.f;
				double totalCommonPercentage = -1.f;

			public:
				Cluster cluster;
				void addPartner(Cluster newCluster, int commonParticles) {
					PartnerCluster PartnerCluster;
					PartnerCluster.cluster = newCluster;
					PartnerCluster.commonParticles = commonParticles;
					this->partners.push_back(PartnerCluster);
					//this->partners.insert(std::pair<int, PartnerCluster> (commonParticles, PartnerCluster));

					
					totalCommonParticles += commonParticles;

					// Max and min.
					if (minCommonParticles < 0 || minCommonParticles > commonParticles)
						minCommonParticles = commonParticles;
					if (maxCommonParticles < 0 || maxCommonParticles < commonParticles)
						maxCommonParticles = commonParticles;
					if (minCommonPercentage < 0 || minCommonPercentage > PartnerCluster.getClusterCommonPercentage(this->cluster))
						minCommonPercentage = PartnerCluster.getClusterCommonPercentage(this->cluster);
					if (maxCommonPercentage < 0 || maxCommonPercentage < PartnerCluster.getClusterCommonPercentage(this->cluster))
						maxCommonPercentage = PartnerCluster.getClusterCommonPercentage(this->cluster);
				}

				void addPartner(Cluster newCluster, int commonParticles, bool gasCluster) {
					if (gasCluster) {
						PartnerCluster PartnerCluster;
						PartnerCluster.cluster = newCluster;
						PartnerCluster.commonParticles = commonParticles;
						PartnerCluster.isGasCluster = true;
					}
					else
						this->addPartner(newCluster, commonParticles);
				}

				void sortPartners() {
					std::sort(this->partners.begin(), this->partners.end(), [](const PartnerCluster& lhs, const PartnerCluster& rhs) {
						return (lhs.commonParticles > rhs.commonParticles);
					});
				}

				PartnerCluster getPartner(const int partnerPosition) const {
					return this->partners[partnerPosition];
				}

				/// Amount of partner clusters with ClusterCommonPercentage / TotalCommonPercentage >= percentage %.
				/// Previous->current: For possible split detection, not optimal for big clusters.
				/// Current->previous: For possible merge detection, not optimal for big clusters.
				int getBigPartnerAmount(const double percentage) const {
					if (this->getTotalCommonPercentage() == 0)
						return -1; // It's so 90s.

					int count = 0;
					double ratio = percentage / 100;
					for (auto partner : this->partners) {
						if (partner.getClusterCommonPercentage(this->cluster) / this->getTotalCommonPercentage() >= ratio)
							count++;
					}
					return count;
				}

				/// Amount of partner clusters with ClusterCommonPercentage / TotalCommonPercentage <= percentage %.
				/// For noise detection.
				int getSmallPartnerAmount(const double percentage) const {
					if (this->getTotalCommonPercentage() == 0)
						return -1; // It's so 90s.

					int count = 0;
					double ratio = percentage / 100;
					for (auto partner : this->partners) {
						if (partner.getClusterCommonPercentage(this->cluster) / this->getTotalCommonPercentage() <= ratio)
							count++;
					}
					return count;
				}

				/// Average number of common particles. 
				/// For similar cluster detection.
				/// For noise detection.
				double getAveragePartnerCommonPercentage() const {
					return this->getTotalCommonPercentage() / static_cast<double>(this->partners.size());
				}

				/// Common particle to cluster size ratio.
				/// Previous->current: For shrink detection. For death detection.
				/// Current->previous: For growth detection. For birth detection.
				double getTotalCommonPercentage() const {
					if (this->totalCommonPercentage < 0)
						return (static_cast<float> (this->totalCommonParticles) / static_cast<float> (this->cluster.numberOfParticles)) * 100;
					return this->totalCommonPercentage;
				}

				/// Ratio of common particles of the biggest partner to common particle ratio of this cluster.
				/// For similar cluster detection.
				double getLocalMaxTotalPercentage() const { // Name props to Andreas.
					if (this->getTotalCommonPercentage() == 0)
						return -1; // It's so 90s.

					return (this->maxCommonPercentage / this->getTotalCommonPercentage()) * 100;
				}

				/// For similar cluster detection.
				/// For noise detection.
				/// For birth detection (backwards).
				/// For death detection (forward).
				int getNumberOfPartners() const {
					return static_cast<int> (partners.size());
				}

				int getMinCommonParticles() const {
					return this->minCommonParticles;
				}

				int getMaxCommonParticles() const {
					return this->maxCommonParticles;
				}

				int getTotalCommonParticles() const {
					return this->totalCommonParticles;
				}

				double getMinCommonPercentage() const {
					return this->minCommonPercentage;
				}

				double getMaxCommonPercentage() const {
					return this->maxCommonPercentage;
				}

				bool hasPartnerCluster(int clusterID) const {
					auto it = std::find_if(this->partners.begin(), this->partners.end(), [clusterID](const PartnerCluster& pc) -> bool {
						if (pc.cluster.id == clusterID)
							return true;
					});
					if (it != partners.end())
						return true;
					else
						return false;
				}
			};

			class PartnerClustersList {
			private:
			public:
				std::vector<PartnerClusters> forwardList;
				std::vector<PartnerClusters> backwardsList;

				enum class Direction : int {
					forward,
					backwards
				};

				PartnerClusters getMaxPercentage(Direction direction = Direction::forward) {
					std::vector<PartnerClusters>* list;
					if (direction == Direction::backwards)
						list = &this->backwardsList;
					else
						list = &this->forwardList;
					auto it = std::max_element(list->begin(), list->end(), [](const PartnerClusters& lhs, const PartnerClusters& rhs) {
						return lhs.getTotalCommonPercentage() < rhs.getTotalCommonPercentage();
					});
					return *it;
				}

				PartnerClusters getMinPercentage(Direction direction = Direction::forward) {
					std::vector<PartnerClusters>* list;
					if (direction == Direction::backwards)
						list = &this->backwardsList;
					else
						list = &this->forwardList;
					auto it = std::min_element(list->begin(), list->end(), [](const PartnerClusters& lhs, const PartnerClusters& rhs) {
						return lhs.getTotalCommonPercentage() < rhs.getTotalCommonPercentage();
					});
					return *it;
				}

				PartnerClusters getPartnerClusters(int clusterId, Direction direction = Direction::forward) {
					std::vector<PartnerClusters>* list;
					if (direction == Direction::backwards)
						list = &this->backwardsList;
					else
						list = &this->forwardList;
					std::vector<PartnerClusters>::iterator it = std::find_if(list->begin(), list->end(), [clusterId](const PartnerClusters& p) -> bool {
						return p.cluster.id == clusterId;
					});
					return *it;
				}

				/// List of PartnerClusters who contains the clusterid as parent.
				/// @param direction Direction of the cluster whos clusterId is given.
				std::vector<PartnerClusters*> getParentPartnerClusters(int clusterId, Direction directionOfGivenCluster = Direction::forward) {
					std::vector<PartnerClusters>* parentList;
					std::vector<PartnerClusters*> returnList;
					if (directionOfGivenCluster == Direction::backwards)
						parentList = &this->forwardList;
					else
						parentList = &this->backwardsList;
					for (auto partnerClusters : *parentList) {
						if (partnerClusters.hasPartnerCluster(clusterId))
							returnList.push_back(&partnerClusters);
					}
					return returnList;
				}
			};

			/**
			 * Answer the name of this module.
			 *
			 * @return The name of this module.
			 */
			static const char *ClassName(void) {
				return "StructureEventsClusterVisualization";
			}

			/**
			 * Answer a human readable description of this module.
			 *
			 * @return A human readable description of this module.
			 */
			static const char *Description(void) {
				return "Calculates clusters from particle data";
			}

			/**
			 * Answers whether this module is available on the current system.
			 *
			 * @return 'true' if the module is available, 'false' otherwise.
			 */
			static bool IsAvailable(void) {
				return true;
			}

			/// Ctor.
			StructureEventsClusterVisualization(void);

			/// Dtor.
			virtual ~StructureEventsClusterVisualization(void);

		private:

			/**
			 * Implementation of 'Create'.
			 *
			 * @return 'true' on success, 'false' otherwise.
			 */
			virtual bool create(void);

			/**
			 * Implementation of 'Release'.
			 */
			virtual void release(void);

			/**
			 * Called when the data is requested by this module.
			 *
			 * @param caller The incoming call.
			 *
			 * @return 'true' on success, 'false' on failure.
			 */
			bool getDataCallback(core::Call& caller);
			bool getSEDataCallback(core::Call& caller);

			/**
			 * Called when the extend information is requested by this module.
			 *
			 * @param caller The incoming call.
			 *
			 * @return 'true' on success, 'false' on failure.
			 */
			bool getExtentCallback(core::Call& caller);
			bool getSEExtentCallback(core::Call& caller);

			/**
			 * Manipulates the particle data.
			 *
			 * @param outData The call receiving the manipulated data
			 * @param inData The call holding the original data
			 *
			 * @return True on success
			 */
			bool manipulateData(
				core::moldyn::MultiParticleDataCall& outData,
				//mmvis_static::StructureEventsDataCall& outSEData,
				core::moldyn::MultiParticleDataCall& inData);

			/**
			 * Manipulates the particle data extend information
			 *
			 * @param outData The call receiving the manipulated information
			 * @param inData The call holding the original data
			 *
			 * @return True on success
			 */
			bool manipulateExtent(
				core::moldyn::MultiParticleDataCall& outData,
				core::moldyn::MultiParticleDataCall& inData);

			/// Writes the data from a single MultiParticleDataCall frame into particleList.			 
			void setData(core::moldyn::MultiParticleDataCall& data);

			void buildParticleList(core::moldyn::MultiParticleDataCall& data,
				uint64_t& globalParticleIndex, float& globalRadius, uint8_t (&globalColor)[4], float& globalColorIndexMin, float& globalColorIndexMax);

			/// Set neighbours in particleList.
			void findNeighboursWithKDTree(megamol::core::moldyn::MultiParticleDataCall& data);

			/// Set neighbour with highest depth as next path object.
			void createClustersFastDepth();

			/// Merge small clusters into bigger ones.
			void mergeSmallClusters();

			/// Compare clusters of two frames.
			void compareClusters();

			/// Sets the StructureEvents.
			void setStructureEvents();

			/// Set colour of particles based on cluster assignment.
			void setClusterColor(bool renewClusterColors);

			/// Set colour of particles based on signed distance.
			void setSignedDistanceColor(float min, float max);

			void sortBySignedDistance();

			/// Only sets ids, cluster id and numberOfParticles.
			void setDummyLists(int particleAmount, int clusterAmount, int structureEvents);

			/// Mean value and standard deviation of values: http ://stackoverflow.com/a/7616783/4566599
			MeanStdDev meanStdDeviation(std::vector<double> v);

			/// Files.
			std::ofstream logFile;
			std::ofstream debugFile;

			/// The call for incoming data.
			core::CallerSlot inDataSlot;

			/// The call for outgoing data.
			core::CalleeSlot outDataSlot;

			/// The call for outgoing StructureEvent data.
			core::CalleeSlot outSEDataSlot;

			/// The knob to manually start the calculation.
			core::param::ParamSlot activateCalculationSlot;

			/// Switch for periodic boundary condition.
			core::param::ParamSlot periodicBoundaryConditionSlot;

			/// Limit for cluster merging.
			core::param::ParamSlot minClusterSizeSlot;

			/// Limits for event detection.
			core::param::ParamSlot minMergeSplitPercentageSlot;
			core::param::ParamSlot minMergeSplitAmountSlot;
			core::param::ParamSlot maxBirthDeathPercentageSlot;

			/// The hash id of the data stored
			size_t dataHash;

			/// The hash id of the outgoing SEDC.
			size_t sedcHash;

			/// The frame id of the data stored
			unsigned int frameId;

			/// List with all particles of one frame. Key = position of the particle in the MMPLD particle list.
			std::vector<Particle> particleList;
			std::vector<Particle> previousParticleList;

			/// List with all clusters.
			std::vector<Cluster> clusterList;
			std::vector<Cluster> previousClusterList;

			/// Cluster comparison.
			PartnerClustersList partnerClustersList;

			/// Structure Events.
			std::vector<StructureEvents::StructureEvent> structureEvents;

			/// For merge and compare functions.
			int minClusterSize;

			/// The size of the kdTree, global for output purpose.
			unsigned int treeSize;

			/// Cache container of a single MMPLD particle list.
			core::moldyn::MultiParticleDataCall::Particles particles;

			/// Iterate through whole list (exhaustive search), only check distance when signed distance is similar.
			/// Must happen after list sorting by signed distance!
			/// Takes forever! OBSOLETE.
			void findNeighboursBySignedDistance();

			/// First, naiv implementation. OBSOLETE.
			void createClustersSignedDistanceOnly();
			/// Returns true if the particle is in signed distance range of the reference. OBSOLETE.
			bool isInSameComponent(const Particle &referenceParticle, const Particle &particle) const;

			/// Get a particle from particleList with particle ID. UNUSED.
			mmvis_static::StructureEventsClusterVisualization::Particle
				mmvis_static::StructureEventsClusterVisualization::_getParticle(const uint64_t particleID) const;

			/// Get a cluster from clusterList with particle ID. UNUSED.
			mmvis_static::StructureEventsClusterVisualization::Cluster*
				mmvis_static::StructureEventsClusterVisualization::_getCluster(const uint64_t rootParticleID) const;
		};

	} /* namespace mmvis_static */
} /* namespace megamol */

#endif /* MMVISSTATIC_StructureEventsClusterVisualization_H_INCLUDED */