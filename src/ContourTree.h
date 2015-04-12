/*
* ContourTree.h
*
* Copyright (C) 2009-2015 by MegaMol Team
* Alle Rechte vorbehalten.
*/

#ifndef MMVISSTATIC_CONTOURTREE_H_INCLUDED
#define MMVISSTATIC_CONTOURTREE_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "AbstractParticleManipulator.h"
#include "mmcore/param/ParamSlot.h"

namespace megamol {
	namespace mmvis_static {
		
		class ContourTree : public stdplugin::datatools::AbstractParticleManipulator	{
		public:
			/**
			* Answer the name of this module.
			*
			* @return The name of this module.
			*/
			static const char *ClassName(void) {
				return "ContourTree";
			}

			/**
			* Answer a human readable description of this module.
			*
			* @return A human readable description of this module.
			*/
			static const char *Description(void) {
				return "Creates a contour tree based upon a signed distance function in the MMPLD.";
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
			ContourTree(void);

			/** Dtor. */
			virtual ~ContourTree(void);
		protected:

			/**
			* Manipulates the particle data
			*
			* @remarks the default implementation does not changed the data
			*
			* @param outData The call receiving the manipulated data
			* @param inData The call holding the original data
			*
			* @return True on success
			*/
			virtual bool manipulateData(
				megamol::core::moldyn::MultiParticleDataCall& outData,
				megamol::core::moldyn::MultiParticleDataCall& inData);

		private:
			core::param::ParamSlot thresholdSlot;
		};

	} /* namespace mmvis_static */
} /* namespace megamol */

#endif /* MMVISSTATIC_CONTOURTREE_H_INCLUDED */