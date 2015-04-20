/**
 * StaticRenderer.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Alle Rechte vorbehalten.
 *
 * TODO:
 * - Texture handling.
 * - Embed own StructureEventDataCall.
 */

#include "stdafx.h"
#include "vislib/graphics/gl/IncludeAllGL.h"
#include "vislib/graphics/gl/OpenGLTexture2D.h"
#include "StaticRenderer.h"
#include "mmcore/CoreInstance.h"
#include "mmcore/view/CallClipPlane.h"
#include "mmcore/view/CallGetTransferFunction.h"
#include "mmcore/view/CallRender3D.h"
#include "mmcore/param/IntParam.h"
#include "mmcore/param/FilePathParam.h"
#include "mmcore/moldyn/MultiParticleDataCall.h" // Should be replaced by #include "StructureEventsDataCall.h"
#include "StructureEventsDataCall.h"
#include "mmcore/misc/PngBitmapCodec.h"
#include "vislib/assert.h"

using namespace megamol;
using namespace megamol::core;

/*
 * mmvis_static::StaticRenderer::StaticRenderer
 */
mmvis_static::StaticRenderer::StaticRenderer() : Renderer3DModule(),
	getDataSlot("getdata", "Connects to the data source"),
	getDataSlot2("getdataSED", "Connects to the data source"),
	getTFSlot("gettransferfunction", "Connects to the transfer function module"),
	getClipPlaneSlot("getclipplane", "Connects to a clipping plane module"),
	filePathBirthTextureSlot("filePathBirthTextureSlot", "The image file for birth events"),
	filePathDeathTextureSlot("filePathDeathTextureSlot", "The image file for death events"),
	filePathMergeTextureSlot("filePathMergeTextureSlot", "The image file for merge events"),
	filePathSplitTextureSlot("filePathSplitTextureSlot", "The image file for split events"),
	greyTF(0),
	billboardShader() {

	this->getDataSlot.SetCompatibleCall<moldyn::MultiParticleDataCallDescription>();
	this->MakeSlotAvailable(&this->getDataSlot);
	
	this->getDataSlot2.SetCompatibleCall<StructureEventsDataCallDescription>();
	this->MakeSlotAvailable(&this->getDataSlot2);

	this->getTFSlot.SetCompatibleCall<view::CallGetTransferFunctionDescription>();
	this->MakeSlotAvailable(&this->getTFSlot);

	this->getClipPlaneSlot.SetCompatibleCall<view::CallClipPlaneDescription>();
	this->MakeSlotAvailable(&this->getClipPlaneSlot);

	this->filePathBirthTextureSlot << new param::FilePathParam("");
	this->MakeSlotAvailable(&this->filePathBirthTextureSlot);

}

/*
 * mmvis_static::StaticRenderer::~StaticRenderer
 */
mmvis_static::StaticRenderer::~StaticRenderer(void) {
	this->Release();
}

/*
 * mmvis_static::StaticRenderer::create
 */
bool mmvis_static::StaticRenderer::create(void) {
	ASSERT(IsAvailable());

	// An array of 4 vectors which represents 4 vertices for the quad.
	static const GLfloat fQuad[12] = {
		-.2f, -.1f, 0.0f,
		-.2f, -.6f, 0.0f,
		 .2f, -.1f, 0.0f,
		 .2f, -.6f, 0.0f
	};

	/**
	 * Create a VBO.
	 * A VBO is a collection of Vectors which in this case resemble the location of each vertex.
	 */
	// Generate 1 (generic) buffer, put the resulting identifier in the vertex buffer object
	glGenBuffers(1, &vbo);
	// Bind the buffer to GL_ARRAY_BUFFER (array of vertices)
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	// Give our vertices to OpenGL (fill the buffer with data).
	glBufferData(GL_ARRAY_BUFFER, sizeof(fQuad), fQuad, GL_STATIC_DRAW);

	/**
	 * Load shader
	 */
	vislib::graphics::gl::ShaderSource vert, frag;

	if (!instance()->ShaderSourceFactory().MakeShaderSource("billboard::vertex", vert)) {
		return false;
	}
	if (!instance()->ShaderSourceFactory().MakeShaderSource("billboard::fragment", frag)) {
		return false;
	}

	//printf("\nVertex Shader:\n%s\n\nFragment Shader:\n%s\n",
	//    vert.WholeCode().PeekBuffer(),
	//    frag.WholeCode().PeekBuffer());

	try {
		if (!this->billboardShader.Create(vert.Code(), vert.Count(), frag.Code(), frag.Count())) {
			vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
				"Unable to compile sprite shader: Unknown error\n");
			return false;
		}

	}
	catch (vislib::graphics::gl::AbstractOpenGLShader::CompileException ce) {
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
			"Unable to compile sprite shader (@%s): %s\n",
			vislib::graphics::gl::AbstractOpenGLShader::CompileException::CompileActionName(
			ce.FailedAction()), ce.GetMsgA());
		return false;
	}
	catch (vislib::Exception e) {
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
			"Unable to compile sprite shader: %s\n", e.GetMsgA());
		return false;
	}
	catch (...) {
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
			"Unable to compile sprite shader: Unknown exception\n");
		return false;
	}

	// Set Texture.
	LoadPngTexture(&this->filePathBirthTextureSlot);

	/*
	// Set Texture.
	glEnable(GL_TEXTURE_1D);
	glGenTextures(1, &this->greyTF);
	unsigned char tex[6] = {
		0, 0, 0, 255, 255, 255
	};
	glBindTexture(GL_TEXTURE_1D, this->greyTF);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, tex);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glBindTexture(GL_TEXTURE_1D, 0);

	glDisable(GL_TEXTURE_1D);
	*/
	return true;
}

/*
 * mmvis_static::StaticRenderer::GetCapabilities
 */
bool mmvis_static::StaticRenderer::GetCapabilities(Call& call) {
	view::CallRender3D *cr = dynamic_cast<view::CallRender3D*>(&call);
	if (cr == NULL) return false;

	cr->SetCapabilities(
		view::CallRender3D::CAP_RENDER
		| view::CallRender3D::CAP_LIGHTING
		| view::CallRender3D::CAP_ANIMATION
		);

	return true;
}

/*
 * mmvis_static::StaticRenderer::GetExtents
 */
bool mmvis_static::StaticRenderer::GetExtents(Call& call) {
	view::CallRender3D *cr = dynamic_cast<view::CallRender3D*>(&call);
	if (cr == NULL) return false;

	moldyn::MultiParticleDataCall *c2 = this->getDataSlot.CallAs<moldyn::MultiParticleDataCall>();
	if ((c2 != NULL) && ((*c2)(1))) {
		cr->SetTimeFramesCount(c2->FrameCount());
		cr->AccessBoundingBoxes() = c2->AccessBoundingBoxes();

		float scaling = cr->AccessBoundingBoxes().ObjectSpaceBBox().LongestEdge();
		if (scaling > 0.0000001) {
			scaling = 10.0f / scaling;
		}
		else {
			scaling = 1.0f;
		}
		cr->AccessBoundingBoxes().MakeScaledWorld(scaling);

	}
	else {
		cr->SetTimeFramesCount(1);
		cr->AccessBoundingBoxes().Clear();
	}

	return true;
}


/*
 * mmvis_static::StaticRenderer::release
 */
void mmvis_static::StaticRenderer::release(void) {
	this->billboardShader.Release();
	::glDeleteTextures(1, &this->greyTF);
}


/*
 * mmvis_static::StaticRenderer::getData
 */
moldyn::MultiParticleDataCall *mmvis_static::StaticRenderer::getData(unsigned int t, float& outScaling) {
	moldyn::MultiParticleDataCall *c2 = this->getDataSlot.CallAs<moldyn::MultiParticleDataCall>();
	outScaling = 1.0f;
	if (c2 != NULL) {
		c2->SetFrameID(t);
		if (!(*c2)(1)) return NULL;

		// calculate scaling
		outScaling = c2->AccessBoundingBoxes().ObjectSpaceBBox().LongestEdge();
		if (outScaling > 0.0000001) {
			outScaling = 10.0f / outScaling;
		}
		else {
			outScaling = 1.0f;
		}

		c2->SetFrameID(t);
		if (!(*c2)(0)) return NULL;

		return c2;
	}
	else {
		return NULL;
	}
}


/*
 * mmvis_static::StaticRenderer::getClipData
 */
void mmvis_static::StaticRenderer::getClipData(float *clipDat, float *clipCol) {
	view::CallClipPlane *ccp = this->getClipPlaneSlot.CallAs<view::CallClipPlane>();
	if ((ccp != NULL) && (*ccp)()) {
		clipDat[0] = ccp->GetPlane().Normal().X();
		clipDat[1] = ccp->GetPlane().Normal().Y();
		clipDat[2] = ccp->GetPlane().Normal().Z();
		vislib::math::Vector<float, 3> grr(ccp->GetPlane().Point().PeekCoordinates());
		clipDat[3] = grr.Dot(ccp->GetPlane().Normal());
		clipCol[0] = static_cast<float>(ccp->GetColour()[0]) / 255.0f;
		clipCol[1] = static_cast<float>(ccp->GetColour()[1]) / 255.0f;
		clipCol[2] = static_cast<float>(ccp->GetColour()[2]) / 255.0f;
		clipCol[3] = static_cast<float>(ccp->GetColour()[3]) / 255.0f;

	}
	else {
		clipDat[0] = clipDat[1] = clipDat[2] = clipDat[3] = 0.0f;
		clipCol[0] = clipCol[1] = clipCol[2] = 0.75f;
		clipCol[3] = 1.0f;
	}
}


/**
 * mmvis_static::StaticRenderer::Render
 */
bool mmvis_static::StaticRenderer::Render(Call& call) {
	view::CallRender3D *cr = dynamic_cast<view::CallRender3D*>(&call);
	if (cr == NULL) return false;

	this->billboardShader.Enable();

	/**
	 * Bind position.
	 */
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// Set the ID for the shader variable (attribute).
	glEnableVertexAttribArray(0);
	// Select the VBO again (bind) - required when there are several vbos available.
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	// Telling the pipeline how to interpret the data inside the buffer. Put the VBO in the attributes list at index 0.
	glVertexAttribPointer(  // Position.
		0,                  // attribute 0. Position in shader layout.
		3,                  // number of components in the attribute (here: x,y,z)
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		sizeof(float) * 5,	// stride, 3*position + 2*texture
		(void*)0            // array buffer offset (e.g. in a VBO with position+normal+color the offset for normal and color could be set)
	);

	/**
	 * Bind texture.
	 TODO: Make texture class attribute.
	 */
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(  // Texturecoords.
		1,                  // attribute 1. Texture in shader layout.
		2,                  // number of components in the attribute (here: u,v)
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		sizeof(float)*5,	// stride, 3*position + 2*texture
		(const GLvoid*)12   // array buffer offset: texture after x,y,z
		);
	//texture->Bind(); // predefined function in Texture object.

	/**
	 * MVP
	 */
	// Rotate to always look into the camera.
	// 1. Richtungsvektor aus Eventposition-Camera position bestimmen.
	// 2. Normale soll diesen Richtungsvektor bekommen: Selbe Rotation auf Vertices übertragen? Klar!
	// vislib::math::Point<vislib::graphics::SceneSpaceType, 3U> camPosition = cr->GetCameraParameters()->Position();

	// Skalierung abhängig von Kameraabstand machen, so dass:
	// - aus großer Entfernung zu sehen
	// - bei minimaler Entfernung nicht bildschirmfüllend

	// Translate using data from StructureEventCall.

	// Resultierende Transformationsmatrix (aktuell nur temporäre Skalierung).
	Matrix4f worldMatrix;

	// Testing trans.
	static float scale = 0.0f;
	scale += 0.001f;

	worldMatrix.m[0][0] = 1.0f; worldMatrix.m[0][1] = 0.0f; worldMatrix.m[0][2] = 0.0f; worldMatrix.m[0][3] = sinf(scale);
	worldMatrix.m[1][0] = 0.0f; worldMatrix.m[1][1] = 1.0f; worldMatrix.m[1][2] = 0.0f; worldMatrix.m[1][3] = 0.0f;
	worldMatrix.m[2][0] = 0.0f; worldMatrix.m[2][1] = 0.0f; worldMatrix.m[2][2] = 1.0f; worldMatrix.m[2][3] = 0.0f;
	worldMatrix.m[3][0] = 0.0f; worldMatrix.m[3][1] = 0.0f; worldMatrix.m[3][2] = 0.0f; worldMatrix.m[3][3] = 1.0f;

	glUniformMatrix4fv(this->billboardShader.ParameterLocation("worldMatrix"), 1, GL_TRUE, &worldMatrix.m[0][0]);
	
	// Camera Matrices.
	// Viewmatrix: camPosition, lookAt, upside.
	/*
	vislib::math::Point<vislib::graphics::SceneSpaceType, 3U> camPosition = cr->GetCameraParameters()->Position();
	vislib::math::Point<vislib::graphics::SceneSpaceType, 3U> camLook = cr->GetCameraParameters()->LookAt();
	vislib::math::Vector<vislib::graphics::SceneSpaceType, 3U> camUp = cr->GetCameraParameters()->Up();

	Matrix4f viewMatrix;

	GL_MODELVIEW_MATRIX

	glUniformMatrix4fv(this->billboardShader.ParameterLocation("viewMatrix"), 1, GL_TRUE, &viewMatrix.m[0][0]);

	// Projection matrix: fov, aspect, nearDist, farDist.
	//cr->GetCameraParameters()->ApertureAngle();
	*/


	// Finally draw the vertices.
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); // Starting from vertex 0; 4 vertices total -> 1 quad
	
	// Disable each vertex attribute when it is not immediately used!
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	this->billboardShader.Disable();

	// Deselect (bind to 0) the VBO
	//glBindBuffer(GL_ARRAY_BUFFER, 0);

	return true;
}


/**
 * mmvis_static::StaticRenderer::LoadPngTexture
 */
void mmvis_static::StaticRenderer::LoadPngTexture(param::ParamSlot *filenameSlot) {
	const vislib::TString& filename = filenameSlot->Param<param::FilePathParam>()->Value();
	static vislib::graphics::BitmapImage img;
	static sg::graphics::PngBitmapCodec codec;

	// Has to happen before codec.Load()!
	codec.Image() = &img;
	try {
		if (codec.Load(filename)) {
			img.Convert(vislib::graphics::BitmapImage::TemplateByteRGBA);
			vislib::graphics::gl::OpenGLTexture2D tex;
			tex.Create(img.Width(), img.Height(), img.PeekData(), GL_RGBA, GL_UNSIGNED_BYTE); //?
			//tex.Bind(); // in Render

			// Alternative manuelles Laden .
			GLenum textureTarget = GL_TEXTURE_2D;
			GLuint textureObj; // ID for the object.

			glGenTextures(1, &textureObj); // Create 1 texture object, set array pointer.
			glBindTexture(textureTarget, textureObj); // Operate on this object.
			glTexImage2D(textureTarget, 0, GL_RGBA, img.Width(), img.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, &img);
			glTexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glBindTexture(textureTarget, 0); // Unbind.
		}
		else {
			printf("Failed: Load\n");
		}
	}
	catch (vislib::Exception ex) {
		printf("Failed: %s (%s;%d)\n", ex.GetMsgA(), ex.GetFile(), ex.GetLine());
	}
	catch (...) {
		printf("Failed\n");
	}
}


/**
 * mmvis_static::StaticRenderer::ComputeFOVProjection
 */
void mmvis_static::StaticRenderer::ComputeFOVProjection(Matrix4f &matrix, float fov, float aspect, float nearDist, float farDist, bool leftHanded /* = true */) {
	//
	// General form of the Projection Matrix
	//
	// uh = Cot( fov/2 ) == 1/Tan(fov/2)
	// uw / uh = 1/aspect
	// 
	//   uw         0       0       0
	//    0        uh       0       0
	//    0         0      f/(f-n)  1
	//    0         0    -fn/(f-n)  0
	//
	// Make result to be identity first

	// check for bad parameters to avoid divide by zero:
	// if found, assert and return an identity matrix.
	if (fov <= 0 || aspect == 0)
	{
		ASSERT(fov > 0 && aspect != 0);
		return;
	}

	float frustumDepth = farDist - nearDist;
	float oneOverDepth = 1 / frustumDepth;

	matrix.m[1][1] = 1 / tan(0.5f * fov);
	matrix.m[0][0] = (leftHanded ? 1 : -1) * matrix.m[1][1] / aspect;
	matrix.m[2][2] = farDist * oneOverDepth;
	matrix.m[3][2] = (-farDist * nearDist) * oneOverDepth;
	matrix.m[2][3] = 1;
	matrix.m[3][3] = 0;
}
