/**
 * StructureEventsDataSource.h
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Alle Rechte vorbehalten.
 */

#ifndef MMVISSTATIC_StructureEventsDataSource_H_INCLUDED
#define MMVISSTATIC_StructureEventsDataSource_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "mmcore/CalleeSlot.h"
#include "mmcore/param/ParamSlot.h"
#include "mmcore/Module.h"

namespace megamol {
	namespace mmvis_static {
		/**
		 * TODO: This class is a stub!
		 */
		class StructureEventsDataSource : public core::Module {
		public:
			/**
			 * Answer the name of this module.
			 *
			 * @return The name of this module.
			 */
			static const char *ClassName(void) {
				return "StructureEventsDataSource";
			}

			/**
			 * Answer a human readable description of this module.
			 *
			 * @return A human readable description of this module.
			 */
			static const char *Description(void) {
				return "A custom renderer.";
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
			StructureEventsDataSource(void);

			/** Dtor. */
			virtual ~StructureEventsDataSource(void);

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
			* Callback receiving the update of the file name parameter.
			*
			* @param slot The updated ParamSlot.
			*
			* @return Always 'true' to reset the dirty flag.
			*/
			bool filenameChanged(core::param::ParamSlot& slot);

			/**
			* Gets the data from the source.
			*
			* @param caller The calling call.
			*
			* @return 'true' on success, 'false' on failure.
			*/
			bool getDataCallback(core::Call& caller);

			/**
			* Gets the data from the source.
			*
			* @param caller The calling call.
			*
			* @return 'true' on success, 'false' on failure.
			*/
			bool getExtentCallback(core::Call& caller);

			/** The file name */
			core::param::ParamSlot filename;

			/** The call for data */
			core::CalleeSlot getDataSlot;
		};

	} /* namespace mmvis_static */
} /* namespace megamol */

#endif /* MMVISSTATIC_StructureEventsDataSource_H_INCLUDED */