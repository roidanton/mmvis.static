/**
 * StructureEventsClusterKDTree.h
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

#ifndef MMVISSTATIC_StructureEventsClusterKDTree_H_INCLUDED
#define MMVISSTATIC_StructureEventsClusterKDTree_H_INCLUDED
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


namespace megamol {
	namespace mmvis_static {
		/**
		 * TODO: This class is a stub!
		 */
		class StructureEventsClusterKDTree : public core::Module {
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
				Cluster* clusterPtr = NULL; // old algorithm
				std::vector<uint64_t> neighbourIDs;
				std::vector<Particle*> neighbourPtrs;

				bool operator==(const Particle& rhs) const {
					return this->id == rhs.id;
				}
			};

			struct Cluster {
				Particle* rootPtr;
				uint64_t id;

				bool operator==(const Cluster& rhs) const {
					return this->id == rhs.id;
				}
			};

			/**
			 * Answer the name of this module.
			 *
			 * @return The name of this module.
			 */
			static const char *ClassName(void) {
				return "StructureEventsClusterKDTree";
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
			StructureEventsClusterKDTree(void);

			/// Dtor.
			virtual ~StructureEventsClusterKDTree(void);

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

			void setSignedDistanceColor(float min, float max);

			void setClusterColor();

			/// Iterate through whole list (exhaustive search), only check distance when signed distance is similar
			void findNeighboursBySignedDistance();

			/// Set neightbours with k-d-Tree.
			void findNeighboursByKDTree();

			void createClustersFastDepth();

			/// First, naiv implementation.
			void createClustersSignedDistanceOnly();
			/// Returns true if the particle is in signed distance range of the reference.
			bool isInSameComponent(const Particle &referenceParticle, const Particle &particle) const;

			/// The call for incoming data.
			core::CallerSlot inDataSlot;

			/// The call for outgoing data.
			core::CalleeSlot outDataSlot;

			/** The hash id of the data stored */
			size_t dataHash;

			/** The frame id of the data stored */
			unsigned int frameId;

			/// List with new Colors.
			std::vector<float> newColors;

			/// List with all particles of one frame. Key = position of the particle in the MMPLD particle list.
			std::vector<Particle> particleList;

			/// List with all clusters.
			std::vector<Cluster> clusterList;

			/// Cache container of a single list of particles.
			core::moldyn::MultiParticleDataCall::Particles particles;
		};

	} /* namespace mmvis_static */
} /* namespace megamol */

#endif /* MMVISSTATIC_StructureEventsClusterKDTree_H_INCLUDED */