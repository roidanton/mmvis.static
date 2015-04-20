/**
 * StructureEventsCalculator.h
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Alle Rechte vorbehalten.
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

namespace megamol {
	namespace mmvis_static {
		/**
		 * TODO: This class is a stub!
		 */
		class StructureEventsCalculator : public core::Module {
		public:
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

			/** Ctor. */
			StructureEventsCalculator(void);

			/** Dtor. */
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
			* Gets the data from the source.
			*
			* @param caller The calling call.
			*
			* @return 'true' on success, 'false' on failure.
			*/
			bool getDataCallback(core::Call& caller);

			/** The call for data */
			core::CallerSlot getDataSlot;

			/** The call for data */
			core::CalleeSlot sendDataSlot;
		};

	} /* namespace mmvis_static */
} /* namespace megamol */

#endif /* MMVISSTATIC_StructureEventsCalculator_H_INCLUDED */