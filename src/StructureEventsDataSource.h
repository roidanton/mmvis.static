/**
 * StructureEventsDataSource.h
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Copyright (C) 2015 by Richard Hähne, TU Dresden
 * Alle Rechte vorbehalten.
 */

#ifndef MMVISSTATIC_StructureEventsDataSource_H_INCLUDED
#define MMVISSTATIC_StructureEventsDataSource_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "mmcore/view/AnimDataModule.h"
#include "mmcore/CalleeSlot.h"
#include "mmcore/param/ParamSlot.h"
#include "StructureEventsDataCall.h"
#include "vislib/math/Cuboid.h"
#include "vislib/sys/File.h"
#include "vislib/RawStorage.h"

namespace megamol {
	namespace mmvis_static {
		///
		/// Reads MMSE file.
		///
		/// File format header:
		/// 0..3 char* MagicIdentifier
		/// 4..27 6x float (32 bit) Data set bounding box
		/// 28..51 6x float (32 bit) Data set clipping box
		/// 52..59 uint64_t Number of events
		/// 60..63 float Maximum time of all events
		///
		/// File format body (relative bytes):
		/// 0..11 3x float (32bit) Event position
		/// 12..15 float Event time
		/// 16..19 EventType (int) Event type
		///
		class StructureEventsDataSource : public core::Module {
		//class StructureEventsDataSource : public core::view::AnimDataModule {
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

			/// Ctor.
			StructureEventsDataSource(void);

			/// Dtor.
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

			/// Sets the data.
			bool SetData(StructureEventsDataCall& data);

			/// The file name.
			core::param::ParamSlot filename;

			/// The opened data file.
			vislib::sys::File *file;

			/// The hash id of the data stored
			size_t sedcHash;

			/// The call for data.
			core::CalleeSlot getDataSlot;

			/// The data set bounding box.
			vislib::math::Cuboid<float> bbox;

			/// The data set clipping box.
			vislib::math::Cuboid<float> clipbox;

			/// The size of the header in byte, for file->seek.
			int headerSize;

			/// The data set event count.
			size_t eventCount;

			/// The data set maximum time.
			float maxTime;

			/// The number of time frames (SE doesnt have them), required for the call.
			unsigned int frameCount;

			/// The data.
			vislib::RawStorage eventData;

			/// Flag for data changes.
			bool reReadData;
		};

	} /* namespace mmvis_static */
} /* namespace megamol */

#endif /* MMVISSTATIC_StructureEventsDataSource_H_INCLUDED */