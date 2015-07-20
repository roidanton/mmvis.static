/**
 * StructureEventsClusterVisualization.h
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Copyright (C) 2015 by Richard Hähne, TU Dresden
 * Alle Rechte vorbehalten.
 */

#ifndef MMVISSTATIC_StructureEventsClusterVisualization_H_INCLUDED
#define MMVISSTATIC_StructureEventsClusterVisualization_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "glm/glm/glm.hpp"
#include "mmcore/CalleeSlot.h"
#include "mmcore/CallerSlot.h"
#include "mmcore/Module.h"
#include "mmcore/moldyn/MultiParticleDataCall.h"
#include "mmcore/param/ParamSlot.h"
#include "StructureEventsDataCall.h"

// File operations.
#include <fstream>
#include <iostream>


namespace megamol {
	namespace mmvis_static {
		///
		/// use /EHsc for compiler, see concurrency below
		///
		/// Calculates Structure Events in several steps:
		/// 1) Getting the neighbours of each particle.
		/// 2) Creating clusters by using these neighbours.
		/// 3) Comparing the clusters of two following frames.
		/// 4) Using heuristics to determine structure events from the comparison of (3).
		///
		/// Outputs the events to SEDC.
		/// Outputs the clusters by coloring the particles to MPDC.
		///
		/// Detailed steps:
		/// 1) a) Build particle list from MPDC.
		///    b) Create kD tree for neighbour detection.
		///    c) Use kD tree search algorithm to add neighbours to each particle.
		/// 2) a) Create clusters using the neighbours.
		///    b) Merge clusters of connected components who have less particles
		///       than a user defined cluster size limit.
		/// 3) Cluster comparison by using a common particle matrix and creating
		///    two lists with clusters and their partners (common particles) of
		///    the previous respectively the current frame.
		/// 4) Applying ratio calculations on those lists and using user defined
		///    limits to determine structure events.
		///
		/// Programming comments:
		/// ---------------------
		/// - time measurement with chrono: every step/part step has its distinct time
		///   detection in addition to a measurement of the whole calculation
		///   also see http://stackoverflow.com/a/21995693
		///
		/// - csv, text and console log bloat the source code, so they should get their own class
		///
		/// Parallel/concurrent loops: http://stackoverflow.com/questions/2547531/stl-algorithms-and-concurrent-programming
		/// --------------------------
		/// - ANN not parallelizeable: http://stackoverflow.com/a/2182357
		///
		/// - lack of usage of OpenMP in for loops:
		///   http://stackoverflow.com/questions/17848521/using-openmp-with-c11-range-based-for-loops
		///   - OpenMP doesn't like break and should have predefined size (push_back is not thread safe)
		///		adjusting lists (predefined sizes) and rebuilding for loops often is not valuable (see inline comments and thesis)
		///   - lock variables like push_back: http://stackoverflow.com/questions/2396430/how-to-use-lock-in-openmp
		///
		/// - since the plugin is developed on Windows/VS, the usage of Parallel Patterns Library (PPL) would
		///   work as well, however it has the same limitations like OpenMP regarding efficiency to these algorithms
		///   advantages to OpenMP: http://stackoverflow.com/a/13377387/4566599
		///   speed comparison to OpenMP: http://blogs.msdn.com/b/nativeconcurrency/archive/2009/11/18/concurency-parallel-for-and-concurrency-parallel-for-each.aspx
		///
		/// - parallel mode of libstdc++ is not available to vc++. Maybe could be a minor benifit for some std::find_if, std::max_element.
		///   https://gcc.gnu.org/onlinedocs/libstdc++/manual/parallel_mode.html
		///   PPL parallel_for_each can be used for that: https://msdn.microsoft.com/en-us/library/dd728079.aspx
		///
		/// Logfile storage
		/// ----------------
		/// The files should get an own element <outputFiledir> with attribut path in megamol.cfg, like <shaderdir path="" />.
		///
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

				Particle() : clusterID(-1) {}

				// This constructor wastes memory and takes a lot of time.
				Particle(int radiusModifier) : clusterID(-1) {
					// The compiler should set vector reservation since the vector
					// is stored at a different location in memory than the struct.
					neighbourIDs.reserve(StructureEventsClusterVisualization::getKDTreeMaxNeighbours(radiusModifier));
				}

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
						return pc.cluster.id == clusterID;
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

				PartnerClusters* getPartnerClusters(int clusterId, Direction direction = Direction::forward) {
					std::vector<PartnerClusters>* list;
					if (direction == Direction::backwards)
						list = &this->backwardsList;
					else
						list = &this->forwardList;
					std::vector<PartnerClusters>::iterator it = std::find_if(list->begin(), list->end(), [clusterId](const PartnerClusters& p) -> bool {
						return p.cluster.id == clusterId;
					});
					if (it == list->end())
						return NULL;
					return static_cast<PartnerClusters*>(&(*it));
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
			void determineStructureEvents();

			/// Set colour of particles based on cluster assignment.
			void setClusterColor(bool renewClusterColors);

			/// Set colour of particles based on signed distance.
			void setSignedDistanceColor(float min, float max);

			/// Only sets ids, cluster id and numberOfParticles.
			void setDummyLists(int particleAmount, int clusterAmount, int structureEvents);

			/// Mean value and standard deviation of values: http ://stackoverflow.com/a/7616783/4566599
			MeanStdDev meanStdDeviation(std::vector<double> v);

			/// Converts a position to a normalized vector.
			void normalizeToColorComponent(vislib::math::Vector<float, 3> &output, uint64_t modificator);

			///
			/// Returns minimal maxNeighbours for radius so no particle in radius gets excluded.
			/// Determined by experiments:
			///
			/// radiusMultiplier, maxNeighbours
			/// 4, 35 <- causes zero signed distance one size clusters phenomenon 
			/// 5, 60 <- causes zero signed distance one size clusters phenomenon 
			/// 6, 100
			/// 7, 155
			/// 10, 425
			/// 20, 3270
			///
			static const int getKDTreeMaxNeighbours(const int radiusMultiplier);

			/// Files.
			std::ofstream logFile;
			std::ofstream csvLogFile;
			std::ofstream debugFile;

			/// The call for incoming data.
			core::CallerSlot inDataSlot;

			/// The call for outgoing data.
			core::CalleeSlot outDataSlot;

			/// The call for outgoing StructureEvent data.
			core::CalleeSlot outSEDataSlot;

			/// A label to tag data in output files.
			core::param::ParamSlot outputLabelSlot;

			/// Switch for creating log files and those with quantitative data.
			core::param::ParamSlot quantitativeDataOutputSlot;

			/// Switch the calculation on/off (once started it will last until finished).
			core::param::ParamSlot calculationActiveSlot;

			/// Creates random previous and current data. For I/O tests. Skips steps 1 and 2.
			core::param::ParamSlot createDummyTestDataSlot;

			/// Switch for periodic boundary condition.
			core::param::ParamSlot periodicBoundaryConditionSlot;

			/// Limit of the radius multiplier for the kD-Tree FRsearch.
			core::param::ParamSlot radiusMultiplierSlot;

			/// Limit for cluster merging.
			core::param::ParamSlot minClusterSizeSlot;

			/// Limits for event detection.
			core::param::ParamSlot msMinClusterAmountSlot;
			core::param::ParamSlot msMinCPPercentageSlot;
			core::param::ParamSlot bdMaxCPPercentageSlot;

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

			/// Structure Events. The list should never be cleared/reset so that
			/// it can contain the StructureEvents of all calculated frames.
			std::vector<StructureEvents::StructureEvent> structureEvents;

			/// Cache for maximum time of all structure events.
			float seMaxTimeCache;

			/// The size of the kdTree, for output.
			unsigned int treeSizeOutputCache;

			/// The time of the calculation for output.
			char timeOutputCache[80];

			/// Cache container of a single MMPLD particle list.
			core::moldyn::MultiParticleDataCall::Particles particles;

			/// Color for gas particles.
			std::vector<float> gasColor;

			/// Takes long, OBSOLETE.
			//void sortBySignedDistance();

			/// Iterate through whole list (exhaustive search), only check distance when signed distance is similar.
			/// Must happen after list sorting by signed distance!
			/// Takes forever! OBSOLETE.
			//void findNeighboursBySignedDistance();

			/// First, naiv implementation. OBSOLETE.
			//void createClustersSignedDistanceOnly();
			/// Returns true if the particle is in signed distance range of the reference. OBSOLETE.
			//bool isInSameComponent(const Particle &referenceParticle, const Particle &particle) const;

			/// Get a particle from particleList with particle ID. UNUSED.
			//mmvis_static::StructureEventsClusterVisualization::Particle
			//	mmvis_static::StructureEventsClusterVisualization::_getParticle(const uint64_t particleID) const;

			/// Get a cluster from clusterList with particle ID. UNUSED.
			//mmvis_static::StructureEventsClusterVisualization::Cluster*
			//	mmvis_static::StructureEventsClusterVisualization::_getCluster(const uint64_t rootParticleID) const;
		};

	} /* namespace mmvis_static */
} /* namespace megamol */

#endif /* MMVISSTATIC_StructureEventsClusterVisualization_H_INCLUDED */