/**
 * StructureEventsCalculator.h
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Copyright (C) 2015 by Richard Hähne, TU Dresden
 * Alle Rechte vorbehalten.
 *
 * Obsolete module, existed for testing purposes. Can be removed safely.
 */

#ifndef MMVISSTATIC_StructureEventsCalculator_H_INCLUDED
#define MMVISSTATIC_StructureEventsCalculator_H_INCLUDED
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


namespace megamol {
	namespace core {
		namespace moldyn {
			class MultiParticleDataCallExtension : public core::moldyn::MultiParticleDataCall {
				// Klasse evtl für MaxStride sowie Koordinatenstrides für Vertex und Colour, evtl hasRadius, hasFloat
			};
		}
	}
}

namespace megamol {
	namespace mmvis_static {
		/**
		 * TODO: This class is a stub!
		 */
		class StructureEventsCalculator : public core::Module {
		public:

			// ID is stored in the map particleList currently.
			struct Particle {
				glm::vec3 position;
				float radius; // Maybe needed for calculation.
				float signedDistance;
				//glm::vec3 signedDistance; // Float or vec3, lets see.
				float opacity; // Not required likely.
			};

			/**
			 * Answer the name of this module.
			 *
			 * @return The name of this module.
			 */
			static const char *ClassName(void) {
				return "StructureEventsCalculator";
			}

			/**
			 * Answer a human readable description of this module.
			 *
			 * @return A human readable description of this module.
			 */
			static const char *Description(void) {
				return "Calculates structure events from particle data";
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
			StructureEventsCalculator(void);

			/// Dtor.
			virtual ~StructureEventsCalculator(void);

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
			 * @param callee The incoming call.
			 *
			 * @return 'true' on success, 'false' on failure.
			 */
			bool getDataCallback(core::Call& callee);

			/**
			 * Called when the extend information is requested by this module.
			 *
			 * @param callee The incoming call.
			 *
			 * @return 'true' on success, 'false' on failure.
			 */
			bool getExtentCallback(core::Call& callee);

			/**
			 * Manipulates the particle data. Currently just one frame!
			 *
			 * @param outData The call receiving the manipulated data
			 * @param inData The call holding the original data
			 *
			 * @return True on success
			 */
			bool manipulateData(
				mmvis_static::StructureEventsDataCall& outData,
				megamol::core::moldyn::MultiParticleDataCall& inData);

			/**
			 * Manipulates the particle data extend information
			 *
			 * @param outData The call receiving the manipulated information
			 * @param inData The call holding the original data
			 *
			 * @return True on success
			 */
			bool manipulateExtent(
				mmvis_static::StructureEventsDataCall& outData,
				megamol::core::moldyn::MultiParticleDataCall& inData);


			/**
			 * Writes the data from a single MultiParticleDataCall frame into particleList.			 
			 */
			void getMPDCFrame(megamol::core::moldyn::MultiParticleDataCall& inData, uint32_t frameID);


			/// The call for incoming data.
			core::CallerSlot inDataSlot;

			/// The call for outgoing data.
			core::CalleeSlot outDataSlot;

			/// The threshold.
			core::param::ParamSlot thresholdSlot;

			/// List with all particles of one frame. Key = position of the particle in the MMPLD particle list.
			std::map<uint64_t, Particle> particleList;
		};

	} /* namespace mmvis_static */
} /* namespace megamol */

#endif /* MMVISSTATIC_StructureEventsCalculator_H_INCLUDED */