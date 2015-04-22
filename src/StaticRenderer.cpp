/**
 * StaticRenderer.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Alle Rechte vorbehalten.
 *
 * TODO:
 * - Texture handling.
 * - Embed own StructureEventDataCall.
 * - Make size of billboards dependend on camera.
 */

#include "stdafx.h"
#include "vislib/graphics/gl/IncludeAllGL.h"
#include "StaticRenderer.h"
#include "mmcore/CoreInstance.h"
#include "mmcore/view/CallClipPlane.h"
#include "mmcore/view/CallGetTransferFunction.h"
#include "mmcore/view/CallRender3D.h"
#include "mmcore/param/IntParam.h"
#include "mmcore/param/FilePathParam.h"
#include "mmcore/misc/PngBitmapCodec.h"
#include "vislib/assert.h"

using namespace megamol;
using namespace megamol::core;

/*
 * mmvis_static::StaticRenderer::StaticRenderer
 */
mmvis_static::StaticRenderer::StaticRenderer() : Renderer3DModule(),
	getDataSlot("getdata", "Connects to the data source"),
	getClipPlaneSlot("getclipplane", "Connects to a clipping plane module"),
	filePathBirthTextureSlot("filePathBirthTextureSlot", "The image file for birth events"),
	filePathDeathTextureSlot("filePathDeathTextureSlot", "The image file for death events"),
	filePathMergeTextureSlot("filePathMergeTextureSlot", "The image file for merge events"),
	filePathSplitTextureSlot("filePathSplitTextureSlot", "The image file for split events"),
	birthTextureID(0),
	/*birthOGL2Texture(vislib::graphics::gl::OpenGLTexture2D()),
	deathOGL2Texture(),
	mergeOGL2Texture(),
	splitOGL2Texture(),*/
	billboardShader() {

	this->getDataSlot.SetCompatibleCall<StructureEventsDataCallDescription>();
	this->MakeSlotAvailable(&this->getDataSlot);

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

	// Data is time independent, so it only needs to be loaded once.
	float scaling = 1.0f;
	StructureEventsDataCall *dataCall = getData(1, scaling); // Frame = 1. Wahrscheinlich dataCall komplett überarbeiten und den Frameblödsinn rauswerfen. Die Zeit ist ja im Event gespeichert.


	// Creating test events as long as dataCall doesnt work. 
	vislib::Array<glm::vec3> eventPositions;
	eventPositions.Add({ 0.0f, 0.0f, 0.0f });
	eventPositions.Add({ -1.0f, 0.0f, 0.0f });
	eventPositions.Add({ -1.0f, -1.0f, -1.0f });

	Vertex vertexList[12];

	for (unsigned int eventCounter = 0; eventCounter < eventPositions.Count(); eventCounter++) {
		// Make 4 vertices from one event for quad generation in shader.
		for (unsigned int quadCounter = 0; quadCounter < 4; quadCounter++) {
			glm::vec2 quadSpanModifier;
			unsigned int vertexListCounter = (eventCounter*4 + quadCounter);
			vertexList[vertexListCounter].position = eventPositions[eventCounter];
			switch (quadCounter) {
			case 0:
				quadSpanModifier = { -1.0f, -1.0f };
				break;
			case 1:
				quadSpanModifier = { 1.0f, -1.0f };
				break;
			case 2:
				quadSpanModifier = { 1.0f, 1.0f };
				break;
			case 3:
				quadSpanModifier = { -1.0f, 1.0f };
				break;
			}
			vertexList[vertexListCounter].spanQuad = quadSpanModifier;
			//vertexList[vertexListCounter].position += glm::vec3(quadSpanModifier, 0.0f); // Testdata for usage without spanQuad.
		}
	}
	
	// Create a VBO.
	// Generate 1 (generic) buffer, put the resulting identifier in the vertex buffer object
	glGenBuffers(1, &vbo);
	// Bind the buffer to GL_ARRAY_BUFFER (array of vertices)
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	// Give our vertices to OpenGL (fill the buffer with data). Heed total size (can cost hours! :-( )
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexList), vertexList, GL_STATIC_DRAW);
	
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


	// Get attributes indices.
	// Error occurs, when the attribute is not actively used by the shader since
	// the optimization routines remove unused attributes.
	positionIndex = glGetAttribLocation(this->billboardShader, "eventPosition");
	if (positionIndex == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "eventPosition");
		return 0;
	}
	spanQuadIndex = glGetAttribLocation(this->billboardShader, "spanQuad");
	if (spanQuadIndex == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "spanQuad");
		return 0;
	}

	// Set Texture.
	//LoadPngTexture(&this->filePathBirthTextureSlot, this->birthOGL2Texture);

	return true;
}


/**
 * mmvis_static::StaticRenderer::Render
 */
bool mmvis_static::StaticRenderer::Render(Call& call) {
	view::CallRender3D *callRender = dynamic_cast<view::CallRender3D*>(&call);
	if (callRender == NULL) return false;

	this->billboardShader.Enable();

	// Creating test data as long as dependence on camera doesnt work.
	GLfloat quadSizeModificator = 1.0f;

	// Set sizeModificator of all billboards.
	glUniform1f(this->billboardShader.ParameterLocation("quadSizeModificator"), quadSizeModificator);

	// Bind texture.
	//glEnable(GL_TEXTURE_2D);
	//this->birthOGL2Texture.Bind();
	
	// Set the ID for the shader variable (attribute).
	glEnableVertexAttribArray(positionIndex);
	glEnableVertexAttribArray(spanQuadIndex);

	// Select the VBO again (bind) - required when there are several vbos available.
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	// Describe the date inside the buffer to OpenGL (it can't guess its format automatically)
	glVertexAttribPointer(
		positionIndex,		// attribute index.
		3,					// number of components in the attribute (here: x,y,z)
		GL_FLOAT,			// the type of each element
		GL_FALSE,			// should the values be normalized?
		sizeof(Vertex),		// stride
		0					// offset of first element
		);
	
	glVertexAttribPointer(
		spanQuadIndex,      // attribute index.
		2,                  // number of components in the attribute (here: x,y,z)
		GL_FLOAT,           // the type of each element
		GL_FALSE,           // should the values be normalized?
		sizeof(Vertex),		// stride
		(GLvoid*) (offsetof(Vertex, spanQuad))   // array buffer offset
		);
	
	/* Push each element in buffer_vertices to the vertex shader */
	glDrawArrays(GL_QUADS, 0, 12); // Starting from vertex 0; 12 vertices total. Will depend on size of dataCall.
	
	// Disable each vertex attribute when it is not immediately used!
	glDisableVertexAttribArray(positionIndex);
	glDisableVertexAttribArray(spanQuadIndex);
	// Deselect (bind to 0) the VBO.
	//glBindBuffer(GL_ARRAY_BUFFER, 0);
	//glDisableClientState(GL_VERTEX_ARRAY);
	//glDisable(GL_TEXTURE_2D);

	this->billboardShader.Disable();

	// Deselect (bind to 0) the VBO
	//glBindBuffer(GL_ARRAY_BUFFER, 0);

	return true;
}


/**
 * mmvis_static::StaticRenderer::LoadPngTexture
 */
void mmvis_static::StaticRenderer::LoadPngTexture(param::ParamSlot *filenameSlot, vislib::graphics::gl::OpenGLTexture2D &ogl2Texture) {
	const vislib::TString& filename = filenameSlot->Param<param::FilePathParam>()->Value();
	static vislib::graphics::BitmapImage img;
	static sg::graphics::PngBitmapCodec codec;

	// Has to happen before codec.Load() to be able to load png in img.
	codec.Image() = &img;
	try {
		if (codec.Load(filename)) {
			img.Convert(vislib::graphics::BitmapImage::TemplateByteRGBA);
			ogl2Texture.Create(img.Width(), img.Height(), img.PeekData(), GL_RGBA, GL_UNSIGNED_BYTE);
			//ogl2Texture.Bind(); // in Render

			// Alternative manuelles Laden .
			// GLenum textureTarget = GL_TEXTURE_2D;
			// GLuint textureObj; // ID for the object.

			// glEnable(textureTarget); // OGL 2.
			// glGenTextures(1, &textureObj); // Create 1 texture object, set array pointer.
			// glBindTexture(textureTarget, textureObj); // Operate on this object.
			// glTexImage2D(textureTarget, 0, GL_RGBA, img.Width(), img.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, &img);
			// glTexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			// glTexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			// glBindTexture(textureTarget, 0); // Unbind.
			// glDisable(textureTarget); // OGL 2.
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
	view::CallRender3D *callRender = dynamic_cast<view::CallRender3D*>(&call);
	if (callRender == NULL) return false;

	callRender->SetTimeFramesCount(1);

	mmvis_static::StructureEventsDataCall *dataCall = this->getDataSlot.CallAs<mmvis_static::StructureEventsDataCall>();
	if ((dataCall != NULL) && ((*dataCall)(1))) {
		callRender->AccessBoundingBoxes() = dataCall->AccessBoundingBoxes();

		float scaling = callRender->AccessBoundingBoxes().ObjectSpaceBBox().LongestEdge();
		if (scaling > 0.0000001) {
			scaling = 10.0f / scaling;
		}
		else {
			scaling = 1.0f;
		}
		callRender->AccessBoundingBoxes().MakeScaledWorld(scaling);

	}
	else {
		callRender->AccessBoundingBoxes().Clear();
	}

	return true;
}


/*
 * mmvis_static::StaticRenderer::release
 */
void mmvis_static::StaticRenderer::release(void) {
	this->billboardShader.Release();
	glDeleteTextures(1, &this->birthTextureID);
	/*birthOGL2Texture.Release();
	deathOGL2Texture.Release();
	mergeOGL2Texture.Release();
	splitOGL2Texture.Release();*/
}


/*
 * mmvis_static::StaticRenderer::getData
 */
mmvis_static::StructureEventsDataCall *mmvis_static::StaticRenderer::getData(unsigned int t, float& outScaling) {
	mmvis_static::StructureEventsDataCall *dataCall = this->getDataSlot.CallAs<mmvis_static::StructureEventsDataCall>();
	outScaling = 1.0f;
	if (dataCall != NULL) {
		dataCall->SetFrameID(t);
		if (!(*dataCall)(1)) return NULL;

		// calculate scaling
		outScaling = dataCall->AccessBoundingBoxes().ObjectSpaceBBox().LongestEdge();
		if (outScaling > 0.0000001) {
			outScaling = 10.0f / outScaling;
		}
		else {
			outScaling = 1.0f;
		}

		dataCall->SetFrameID(t);
		if (!(*dataCall)(0)) return NULL;

		return dataCall;
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
