/**
 * StructureEventsDataCall.h
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Copyright (C) 2015 by Richard Hähne, TU Dresden
 * Alle Rechte vorbehalten.
 *
 * Uses pointer for data instead of copying into own
 * structure. Either use:
 * - one pointer for all datatypes and all events
 * - one pointer for all datatypes and each event
 * - one pointer for each datatype and all events
 * - one pointer for each datatype and each event
 * Not yet decided: Depends on Calculation output.
 */

#ifndef MMVISSTATIC_StructureEventsDataCall_H_INCLUDED
#define MMVISSTATIC_StructureEventsDataCall_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "mmcore/AbstractGetData3DCall.h"
#include "mmcore/factories/CallAutoDescription.h"
#include "glm/glm/glm.hpp"
#include <vector>

namespace megamol {
	namespace mmvis_static {

		/**
		 * One event contains a type, a position and a time as well as an agglomeration matrix.
		 */
		class StructureEvent {
		public:
			/** Possible values for the event type */
			enum EventType {
				BIRTH,
				DEATH,
				MERGE,
				SPLIT
			};

			/**
			* Answer the event type
			*
			* @return The event type
			*/
			inline EventType getEventType(void) const {
				return this->type;
			};

			/**
			* Answer the position pointer
			*
			* @return The position pointer
			*/
			inline glm::vec3 getPosition(void) const {
				return this->position;
			};

			/**
			* Sets the event type
			*/
			inline void setEventType(EventType eventType) {
				this->type = eventType;
			};

			inline const void * getLocationData(void) const {
				return this->locationPtr;
			}

			inline unsigned int getLocationStride(void) const {
				return this->locationStride;
			}

			inline unsigned int getTimeStride(void) const {
				return this->timeStride;
			}

			inline unsigned int getTypeStride(void) const {
				return this->typeStride;
			}

		private:

			/** The location pointer */
			const void *locationPtr;

			/** The location stride */
			unsigned int locationStride;

			/** The time pointer */
			const void *timePtr;

			/** The time stride */
			unsigned int timeStride;

			/** The type pointer */
			const void *typePtr;

			/** The type stride */
			unsigned int typeStride;

			/** The agglomeration. */
			glm::mat4 agglomeration;

			/** The event type. */
			EventType type;

			/** The position pointer. */
			glm::vec3 position;

			/** The time step. */
			unsigned int timeStep;
		};

		/**
		 * Container for all structure events. One event contains a type, a position and a time.
		 */
		class StructureEvents {
		public:
			/** Ctor */
			StructureEvents(void);

			/**
			 * Copy ctor
			 * @param src The object to clone from
			 */
			StructureEvents(const StructureEvents& src);

			/** Dtor */
			~StructureEvents(void);
			
		private:
		};

		/**
		 * TODO: This class is a stub!
		 */
		class StructureEventsDataCall : public core::AbstractGetData3DCall {
		public:
			/** Possible values for the event type */
			/*enum EventType {// Likely obsolete.
				BIRTH,
				DEATH,
				MERGE,
				SPLIT
			};*/

			/*struct Event {// Likely obsolete.
				// The agglomeration.
				glm::mat4 agglomeration;
				// The event type.
				EventType type;
				// The position pointer.
				glm::vec3 position;
				// The time step.
				unsigned int timeStep;
			};*/

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

			/** typedef for legacy name */
			//typedef StructureEvent Event;

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

			typedef StructureEvent Event;

			inline UINT32 getEventCount(void) const {
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
			};

		private:
			UINT32 eventCount;
			float maxTime;
			std::vector<Event> eventList;
		};

		/** Description class typedef */
		typedef core::factories::CallAutoDescription<StructureEventsDataCall>
			StructureEventsDataCallDescription;

	} /* namespace mmvis_static */
} /* namespace megamol */

#endif /* MMVISSTATIC_StructureEventsDataCall_H_INCLUDED */