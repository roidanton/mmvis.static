/**
 * StructureEventsDataCall.h
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Copyright (C) 2015 by Richard Hähne, TU Dresden
 * Alle Rechte vorbehalten.
 */

#ifndef MMVISSTATIC_StructureEventsDataCall_H_INCLUDED
#define MMVISSTATIC_StructureEventsDataCall_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "mmcore/AbstractGetData3DCall.h"
#include "mmcore/factories/CallAutoDescription.h"
#include "vislib/sys/Log.h"
#include <vector>

namespace megamol {
	namespace mmvis_static {

		/**
		 * Container for all events.
		 * The event attributes are the position, the time, the type
		 * as well as an agglomeration matrix.
		 *
		 * @remark Future optimizations: Type doesn't need to be stored
		 * per event but instead four lists of events could be used,
		 * one for each event type.
		 */

		class StructureEvents {
		public:

			/// Possible values for the event type.
			enum EventType : int {
				BIRTH,
				DEATH,
				MERGE,
				SPLIT
			};

			///
			/// Dichtgepacktes struct.
			/// It is important that no automatic padding is inserted by compiler
			/// therefore all datatypes have sizes of 4 or 8.
			///
			/// Stride = (4 byte * (3 + 1)) + 4 byte = 20.
			///
			struct StructureEvent {
				float x, y, z;
				float time;
				StructureEvents::EventType type; // 4 byte.
			};

			/// Ctor.
			StructureEvents(void);

			/// Dtor.
			virtual ~StructureEvents(void);

			/// Ctor.
			StructureEvents(const StructureEvents& src);

			inline const void* getLocation(void) const {
				return this->locationPtr;
			}

			inline const void* getTime(void) const {
				return this->timePtr;
			}

			inline const void* getType(void) const {
				return this->typePtr;
			}

			inline unsigned int getStride(void) const {
				if (this->stride == 0)
					return getCalculatedStride();
				return this->stride;
			}

			inline void setStride(unsigned int stride) {
				printf("Data call set stride %d.\n\n", stride); // Debug.
				this->stride = stride;
			}

			/// Only use an array of struct StructureEvent.
			/// Stride is added automatically.
			inline void setEvents(
				const float *location,
				const float *time,
				//const uint8_t *type,
				const EventType *type,
				const float maxTime,
				const size_t count) {
				this->locationPtr = location;
				this->timePtr = time;
				this->typePtr = type;
				this->stride = getCalculatedStride();
				this->maxTime = maxTime;
				this->count = count;
			}

			/**
			 * Answer the event type.
			 *
			 * @return The event type as EventType.
			 */
			inline static EventType getEventType(int typeCode) {
				try {
					switch (typeCode){
					case 0:
						return EventType::BIRTH;
					case 1:
						return EventType::DEATH;
					case 2:
						return EventType::MERGE;
					case 3:
						return EventType::SPLIT;
					}
					throw "Invalid EventType code";
				}
				catch (char* error){
					vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
						"mmvis_static::StructureEventsDataCall %s: %d", error, typeCode);
				}
			};

			inline const size_t getCount(void) const {
				return this->count;
			};

			inline const float getMaxTime() const {
				return maxTime;
			};

			/**
			 * Assignment operator.
			 * Makes a deep copy of all members. While for data these are only
			 * pointers, the pointer to the unlocker object is also copied.
			 *
			 * @param rhs The right hand side operand
			 *
			 * @return A reference to this
			 */
			StructureEvents& operator=(const StructureEvents& rhs);

		private:

			inline unsigned int getCalculatedStride(void) const {
				return sizeof(StructureEvent);
			}

			// The location pointer, 4 byte
			const float *locationPtr;

			// The time pointer, 4 byte
			const float *timePtr;

			// The type pointer, 1 byte. 0 := Birth, 1 := Death, 2 := Merge, 3 := Split as in shader.
			const EventType *typePtr;

			// The stride.
			unsigned int stride = 0; // Bad style, too lazy for constructor.

			// The agglomeration.
			//glm::mat4 agglomeration;

			// The number of objects stored.
			size_t count;

			// Maximum time of events.
			float maxTime = 0; // Bad style, too lazy for constructor.

		};


		/**
		 * The call containing structure events data.
		 * Header contains stride, count and maxTime in addition to dataExtend.
		 */
		class StructureEventsDataCall : public core::AbstractGetData3DCall {
		public:

			/**
			 * Answer the name of the objects of this description.
			 *
			 * @return The name of the objects of this description.
			 */
			static const char *ClassName(void) {
				return "StructureEventsDataCall";
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
			 * Answer the number of functions used for this call.
			 *
			 * @return The number of functions used for this call.
			 */
			static unsigned int FunctionCount(void) {
				return AbstractGetData3DCall::FunctionCount();
			}

			/**
			 * Answer the name of the function used for this call.
			 *
			 * @param idx The index of the function to return it's name.
			 *
			 * @return The name of the requested function.
			 */
			static const char * FunctionName(unsigned int idx) {
				return AbstractGetData3DCall::FunctionName(idx);
			}

			/** Ctor. */
			StructureEventsDataCall(void);

			/** Dtor. */
			virtual ~StructureEventsDataCall(void);

			/**
			 * Assignment operator.
			 * Makes a deep copy of all members. While for data these are only
			 * pointers, the pointer to the unlocker object is also copied.
			 *
			 * @param rhs The right hand side operand
			 *
			 * @return A reference to this
			 */
			StructureEventsDataCall& operator=(const StructureEventsDataCall& rhs);

			StructureEvents& getEvents() {
				return this->events;
			};

			/*inline UINT32 getEventCount(void) const {
				return this->eventCount;
			};

			inline Event getEvent(UINT32 eventIndex) const {
				return eventList[eventIndex];
			};

			inline float getMaxTime() const {
				return maxTime;
			};

			// Likely obsolete.
			inline unsigned int getEventStride() const {
				return sizeof(Event);
			};*/

		private:
			StructureEvents events;

			/*
			UINT32 eventCount;
			float maxTime;
			std::vector<Event> eventList;
			*/
		};

		/** Description class typedef */
		typedef core::factories::CallAutoDescription<StructureEventsDataCall>
			StructureEventsDataCallDescription;

	} /* namespace mmvis_static */
} /* namespace megamol */

#endif /* MMVISSTATIC_StructureEventsDataCall_H_INCLUDED */