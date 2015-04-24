/**
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
#include "StructureEventsDataCall.h"
#include "vislib/graphics/gl/GLSLShader.h"
#include "vislib/graphics/gl/OpenGLTexture2D.h"
#include "glm/glm/glm.hpp"

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
#ifdef _WIN32
#if defined(DEBUG) || defined(_DEBUG)
				HDC dc = ::wglGetCurrentDC();
				HGLRC rc = ::wglGetCurrentContext();
				ASSERT(dc != NULL);
				ASSERT(rc != NULL);
#endif // DEBUG || _DEBUG
#endif // _WIN32
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
			StructureEventsDataCall *getData(unsigned int t, float& outScaling);

			/**
			 * TODO: Document
			 *
			 * @param clipDat points to four floats
			 * @param clipCol points to four floats
			 */
			void getClipData(float *clipDat, float *clipCol);

		private:
			/** The call for data */
			core::CallerSlot getDataSlot;

			/** The call for clipping plane */
			core::CallerSlot getClipPlaneSlot;

			/**
			 * Loads a png texture from file system and creates OGL2 texture in memory.
			 *
			 * Currently unused since we have CreateOGLTexture. However this function
			 * uses a filenameSlot which should be the way to go. Unfortunately the
			 * MM stuff (png loader, slot contents) doesnt work for me.
			 */
			void LoadPngTexture(core::param::ParamSlot *filenameSlot, vislib::graphics::gl::OpenGLTexture2D &ogl2Texture);
			
			/**
			 * Loads a png texture from file system using lodePNG and creates OGL texture in memory.
			 */
			void CreateOGLTextureFromFile(char* filename, GLuint &textureID);

			// The following variables are currently obsolete.
			/** The filepath for the birth texture. */
			core::param::ParamSlot filePathBirthTextureSlot;

			/** The filepath for the death texture. */
			core::param::ParamSlot filePathDeathTextureSlot;

			/** The filepath for the merge texture. */
			core::param::ParamSlot filePathMergeTextureSlot;

			/** The filepath for the merge texture. */
			core::param::ParamSlot filePathSplitTextureSlot;

			/** The eventtype textures. Obsolete, replaced by IDs. */
			vislib::graphics::gl::OpenGLTexture2D birthOGL2Texture;
			/*vislib::graphics::gl::OpenGLTexture2D deathOGL2Texture;
			vislib::graphics::gl::OpenGLTexture2D mergeOGL2Texture;
			vislib::graphics::gl::OpenGLTexture2D splitOGL2Texture;*/

			/** The texture IDs. */
			GLuint textureIDs[4];

			/** The shader for the 3DSprite/Billboard */
			vislib::graphics::gl::GLSLShader billboardShader;

			/** Containing all data for the vertex shader. */
			struct Vertex {
				// The position of the event.
				glm::vec3 position;
				// Generate quads in shader by translating the vertex by this vector.
				glm::vec2 spanQuad;
				// Standard assignment of UV.
				glm::vec2 texUV;
				// Eventtype hardcoded in shader: 0 = birth, 1 = death, 2 = merge, 3 = split
				GLfloat eventType;
				// Color in HSV: Hue, Saturation, Value = [0,1]. Has to be converted in shader.
				glm::vec3 colorHSV;
			};

			GLint shaderAttributeIndex_position,
				shaderAttributeIndex_spanQuad,
				shaderAttributeIndex_texUV,
				shaderAttributeIndex_eventType,
				shaderAttributeIndex_colorHSV;

			/** Vertex Buffer Object for the vertex shader.
			A VBO is a collection of Vectors which in this case resemble the location of each vertex. */
			GLuint vbo;

		};

	} /* namespace mmvis_static */
} /* namespace megamol */

#endif /* MMVISSTATIC_STATICRENDERER_H_INCLUDED */