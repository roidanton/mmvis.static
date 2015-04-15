/*
* StaticRenderer.h
*
* Copyright (C) 2009-2015 by MegaMol Team
* Alle Rechte vorbehalten.
*/

#ifndef MMVISSTATIC_STATICRENDERER_H_INCLUDED
#define MMVISSTATIC_STATICRENDERER_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "mmcore/view/Renderer3DModule.h"
#include "mmcore/Call.h"
#include "mmcore/CallerSlot.h"
#include "mmcore/param/ParamSlot.h"
#include "mmcore/moldyn/MultiParticleDataCall.h"
#include "vislib/graphics/gl/GLSLShader.h"

namespace megamol {
	namespace mmvis_static {
		
		class StaticRenderer : public core::view::Renderer3DModule {
		public:
			/**
			* Answer the name of this module.
			*
			* @return The name of this module.
			*/
			static const char *ClassName(void) {
				return "StaticRenderer";
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
				return vislib::graphics::gl::GLSLShader::AreExtensionsAvailable();
			}

			/** Ctor. */
			StaticRenderer(void);

			/** Dtor. */
			virtual ~StaticRenderer(void);

		protected:
			/**
			* Implementation of 'Create'.
			*
			* @return 'true' on success, 'false' otherwise.
			*/
			virtual bool create(void);

			/**
			* The get capabilities callback. The module should set the members
			* of 'call' to tell the caller its capabilities.
			*
			* @param call The calling call.
			*
			* @return The return value of the function.
			*/
			virtual bool GetCapabilities(core::Call& call);

			/**
			* The get extents callback. The module should set the members of
			* 'call' to tell the caller the extents of its data (bounding boxes
			* and times).
			*
			* @param call The calling call.
			*
			* @return The return value of the function.
			*/
			virtual bool GetExtents(core::Call& call);

			/**
			* Implementation of 'Release'.
			*/
			virtual void release(void);

			/**
			* The render callback.
			*
			* @param call The calling call.
			*
			* @return The return value of the function.
			*/
			virtual bool Render(core::Call& call);

			/**
			 * Gets the data from a call.
			 * TODO: Write own eventDataCall to submit event data for
			 * custom shapes, colors etc for the element.
			 */
			core::moldyn::MultiParticleDataCall *getData(unsigned int t, float& outScaling);

			/**
			* TODO: Document
			*
			* @param clipDat points to four floats
			* @param clipCol points to four floats
			*/
			void getClipData(float *clipDat, float *clipCol);

			/** The call for Transfer function */
			core::CallerSlot getTFSlot;

			/** A simple black-to-white transfer function texture as fallback */
			unsigned int greyTF;

		private:
			/** The call for data */
			core::CallerSlot getDataSlot;

			/** The call for clipping plane */
			core::CallerSlot getClipPlaneSlot;

			/** The shader */
			vislib::graphics::gl::GLSLShader polygonShader;
		};

	} /* namespace mmvis_static */
} /* namespace megamol */

#endif /* MMVISSTATIC_STATICRENDERER_H_INCLUDED */