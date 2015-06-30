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
				// Stores the cluster ID. Should replace the ptr in future so list can be moved in memory!
				int clusterID = -1;

				// Store pointer to the neighbours. Bad when they are moved in memory.
				//std::vector<Particle*> neighbourPtrs;
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

			class ConnectedClusters {
			public:
				struct ConnectedCluster {
					Cluster cluster;
					int commonParticles = 0;

					//ConnectedClusters &parent;  // Reference to parent
					//ConnectedCluster(ConnectedClusters &ccs) : parent(ccs) {}  // Initialise reference in constructor

					float getCommonPercentage() {
						return (static_cast<float> (this->commonParticles) / static_cast<float> (this->cluster.numberOfParticles)) * 100.f;
					}

					float getClusterCommonPercentage(const Cluster& c) {
						return (static_cast<float> (this->commonParticles) / static_cast<float> (c.numberOfParticles)) * 100.f;
					}
					/*
					ConnectedCluster operator=(const ConnectedCluster& rhs) {
						//ConnectedCluster cc(rhs.parent);
						cc.cluster = rhs.cluster;
						cc.commonParticles = rhs.commonParticles;
						return cc;
					}
					*/
					bool operator==(const ConnectedCluster& rhs) const {
						return this->cluster == rhs.cluster;
					}

					//bool operator<(const ConnectedCluster& rhs) const {
					//	return this->commonParticles > rhs.commonParticles;
					//}
				};
			private:
				std::vector<ConnectedCluster> connected;
				//std::multimap<int, ConnectedCluster, std::greater<int>> connected; // Highest int first.
				int minCommonParticles = -1;
				int maxCommonParticles = -1;
				int totalCommonParticles = 0;
				float minCommonPercentage = -1.f;
				float maxCommonPercentage = -1.f;

				// Amount of partner clusters with ClusterCommonPercentage / TotalCommonPercentage > 25%.
				// Previous->current: Can be a split.
				// Current->previous: Can be a merge.
				int biggestPartnerAmount = 0;

			public:
				Cluster cluster;
				void addConnected(Cluster newCluster, int commonParticles) {
					ConnectedCluster connectedCluster;
					connectedCluster.cluster = newCluster;
					connectedCluster.commonParticles = commonParticles;
					this->connected.push_back(connectedCluster);
					//this->connected.insert(std::pair<int, ConnectedCluster> (commonParticles, connectedCluster));

					totalCommonParticles += commonParticles;

					if (minCommonParticles < 0 || minCommonParticles > commonParticles)
						minCommonParticles = commonParticles;
					if (maxCommonParticles < 0 || maxCommonParticles < commonParticles)
						maxCommonParticles = commonParticles;
					if (minCommonPercentage < 0 || minCommonPercentage > connectedCluster.getClusterCommonPercentage(this->cluster))
						minCommonPercentage = connectedCluster.getClusterCommonPercentage(this->cluster);
					if (maxCommonPercentage < 0 || maxCommonPercentage < connectedCluster.getClusterCommonPercentage(this->cluster))
						maxCommonPercentage = connectedCluster.getClusterCommonPercentage(this->cluster);
				}

				void sortConnected() {
					//std::sort(this->connected.begin(), this->connected.end());
					std::sort(this->connected.begin(), this->connected.end(), [](const ConnectedCluster& lhs, const ConnectedCluster& rhs) {
						return (lhs.commonParticles > rhs.commonParticles);
					});
				}

				ConnectedCluster getConnected(int connectedPosition) {
					return this->connected[connectedPosition];
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

				float getMinCommonPercentage() const {
					return this->minCommonPercentage;
				}

				float getMaxCommonPercentage() const {
					return this->maxCommonPercentage;
				}

				float getTotalCommonPercentage() const {
					return (static_cast<float> (this->totalCommonParticles) / static_cast<float> (this->cluster.numberOfParticles)) * 100;
				}

				float getLocalTotalRatio() const { // Name props to Andreas.
					return (this->maxCommonPercentage / this->getTotalCommonPercentage()) * 100;
				}

				int getNumberOfConnected() const {
					return static_cast<int> (connected.size());
				}
			};

			class ConnectedClustersList {
			private:
			public:
				std::vector<ConnectedClusters> connectedClusterListForward;
				std::vector<ConnectedClusters> connectedClusterListBackwards;

				/*auto getMaxPercentage() {
					return std::max_element(connectedClusterListForward.begin(), connectedClusterListForward.end(), [](const ConnectedClusters& lhs, const ConnectedClusters& rhs) {
						return lhs.getTotalCommonPercentage() < rhs.getTotalCommonPercentage();
					});
				}*/
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

			/**
			 * Called when the extend information is requested by this module.
			 *
			 * @param caller The incoming call.
			 *
			 * @return 'true' on success, 'false' on failure.
			 */
			bool getExtentCallback(core::Call& caller);

			/**
			 * Manipulates the particle data. Currently just one frame!
			 *
			 * @param outData The call receiving the manipulated data
			 * @param inData The call holding the original data
			 *
			 * @return True on success
			 */
			bool manipulateData(
				core::moldyn::MultiParticleDataCall& outData,
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

			/**
			 * Writes the data from a single MultiParticleDataCall frame into particleList.			 
			 */
			void setData(core::moldyn::MultiParticleDataCall& data);

			/// Set neighbours in particleList.
			void findNeighboursWithKDTree();

			/// Set neighbour with highest depth as next path object.
			void createClustersFastDepth();

			/// Merge small clusters into bigger ones.
			void mergeSmallClusters(int minClusterSize);

			/// Compare clusters of two frames.
			void compareClusters();

			/// Set colour of particles based on cluster assignment.
			void setClusterColor(bool renewClusterColors);

			/// Set colour of particles based on signed distance.
			void setSignedDistanceColor(float min, float max);

			void sortBySignedDistance();

			/// Only sets ids, cluster id and numberOfParticles.
			void setDummyLists();

			/// Answer if the calculation should be done.
			bool activateCalculationTrigger();

			/// Iterate through whole list (exhaustive search), only check distance when signed distance is similar.
			/// Must happen after list sorting by signed distance!
			/// Takes forever! OBSOLETE.
			void findNeighboursBySignedDistance();

			/// First, naiv implementation. OBSOLETE.
			void createClustersSignedDistanceOnly();
			/// Returns true if the particle is in signed distance range of the reference. OBSOLETE.
			bool isInSameComponent(const Particle &referenceParticle, const Particle &particle) const;

			/// Get a particle from particleList with particle ID.
			mmvis_static::StructureEventsClusterVisualization::Particle
				mmvis_static::StructureEventsClusterVisualization::_getParticle(const uint64_t particleID) const;

			/// Get a cluster from clusterList with particle ID.
			mmvis_static::StructureEventsClusterVisualization::Cluster*
				mmvis_static::StructureEventsClusterVisualization::_getCluster(const uint64_t rootParticleID) const;

			/// Files.
			std::ofstream logFile;
			std::ofstream debugFile;

			/// The call for incoming data.
			core::CallerSlot inDataSlot;

			/// The call for outgoing data.
			core::CalleeSlot outDataSlot;

			/// Bool flag to activate software cursor rendering.
			bool activateCalculation;

			/// The knob to manually start the calculation.
			core::param::ParamSlot activateCalculationSlot;

			/// The hash id of the data stored
			size_t dataHash;

			/// The frame id of the data stored
			unsigned int frameId;

			/// List with all particles of one frame. Key = position of the particle in the MMPLD particle list.
			std::vector<Particle> particleList;
			std::vector<Particle> previousParticleList;

			/// List with all clusters.
			std::vector<Cluster> clusterList;
			std::vector<Cluster> previousClusterList;

			/// The size of the kdTree, global for output purpose.
			unsigned int treeSize;

			/// Cache container of a single MMPLD particle list.
			core::moldyn::MultiParticleDataCall::Particles particles;
		};

	} /* namespace mmvis_static */
} /* namespace megamol */

#endif /* MMVISSTATIC_StructureEventsClusterVisualization_H_INCLUDED */