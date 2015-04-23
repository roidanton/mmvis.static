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
#include "lodepng/lodepng.h"

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
	deathTextureID(1),
	mergeTextureID(2),
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
	// Eventtype hardcoded in shader: 0 = birth, 1 = death, 2 = merge, (todo: 3 = split)
	vislib::Array<GLfloat> eventType;
	eventType.Add(0.0f);
	eventType.Add(1.0f);
	eventType.Add(2.0f);

	Vertex vertexList[12];

	for (unsigned int eventCounter = 0; eventCounter < eventPositions.Count(); eventCounter++) {
		// Make 4 vertices from one event for quad generation in shader. Alternatively geometry shader could be used too in future.
		for (unsigned int quadCounter = 0; quadCounter < 4; quadCounter++) {
			glm::vec2 quadSpanModifier, texUV;
			unsigned int vertexListCounter = (eventCounter*4 + quadCounter);
			vertexList[vertexListCounter].position = eventPositions[eventCounter];
			vertexList[vertexListCounter].eventType = eventType[eventCounter];
			switch (quadCounter) {
			case 0:
				quadSpanModifier = { -1.0f, -1.0f };
				texUV = { 0.0f, 0.0f };
				break;
			case 1:
				quadSpanModifier = { 1.0f, -1.0f };
				texUV = { 1.0f, 0.0f };
				break;
			case 2:
				quadSpanModifier = { 1.0f, 1.0f };
				texUV = { 1.0f, 1.0f };
				break;
			case 3:
				quadSpanModifier = { -1.0f, 1.0f };
				texUV = { 0.0f, 1.0f };
				break;
			}
			vertexList[vertexListCounter].spanQuad = quadSpanModifier;
			vertexList[vertexListCounter].texUV = texUV;
			//vertexList[vertexListCounter].position += glm::vec3(quadSpanModifier, 0.0f); // Testdata for usage without spanQuad.
		}
	}
	
	printf("Vertex Event Type: %f, %f, %f\n\n", vertexList[0].eventType, vertexList[4].eventType, vertexList[8].eventType);

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
	shaderAttributeIndex_position = glGetAttribLocation(this->billboardShader, "eventPosition");
	if (shaderAttributeIndex_position == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "eventPosition");
		return 0;
	}
	shaderAttributeIndex_spanQuad = glGetAttribLocation(this->billboardShader, "spanQuad");
	if (shaderAttributeIndex_spanQuad == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "spanQuad");
		return 0;
	}
	shaderAttributeIndex_texUV = glGetAttribLocation(this->billboardShader, "texUV");
	if (shaderAttributeIndex_texUV == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "texUV");
		return 0;
	}
	shaderAttributeIndex_eventType = glGetAttribLocation(this->billboardShader, "eventType");
	if (shaderAttributeIndex_eventType == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "eventType");
		return 0;
	}

	glGenTextures(4, this->textureIDs); // Creates 4 texture object, set array pointer.
	char filenameChar[] = "GlyphenEventTypesAsterisk.png"; // Copy file to bin folder.
	CreateOGLTextureFromFile(filenameChar, this->textureIDs[0]);
	char filenameChar2[] = "GlyphenEventTypesCross.png"; // Copy file to bin folder.
	CreateOGLTextureFromFile(filenameChar2, this->textureIDs[1]);
	char filenameChar3[] = "GlyphenEventTypesMerge.png"; // Copy file to bin folder.
	CreateOGLTextureFromFile(filenameChar3, this->textureIDs[2]);
	
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

	glEnable(GL_DEPTH_TEST);

	this->billboardShader.Enable();

	// Creating test data as long as dependence on camera doesnt work.
	GLfloat quadSizeModificator = 1.0f;

	// Set sizeModificator of all billboards.
	glUniform1f(this->billboardShader.ParameterLocation("quadSizeModificator"), quadSizeModificator);

	// Bind texture.
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, this->textureIDs[0]);
	glUniform1i(this->billboardShader.ParameterLocation("tex2DBirth"), 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, this->textureIDs[1]);
	glUniform1i(this->billboardShader.ParameterLocation("tex2DDeath"), 1);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, this->textureIDs[2]);
	glUniform1i(this->billboardShader.ParameterLocation("tex2DMerge"), 2);
	//this->birthOGL2Texture.Bind();
	
	// Set the ID for the shader variable (attribute).
	glEnableVertexAttribArray(shaderAttributeIndex_position);
	glEnableVertexAttribArray(shaderAttributeIndex_spanQuad);
	glEnableVertexAttribArray(shaderAttributeIndex_texUV);
	glEnableVertexAttribArray(shaderAttributeIndex_eventType);

	// Select the VBO again (bind) - required when there are several vbos available.
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	// Describe the date inside the buffer to OpenGL (it can't guess its format automatically)
	glVertexAttribPointer(
		shaderAttributeIndex_position,		// attribute index.
		3,					// number of components in the attribute (here: x,y,z)
		GL_FLOAT,			// the type of each element
		GL_FALSE,			// should the values be normalized?
		sizeof(Vertex),		// stride
		0					// offset of first element
		);
	
	glVertexAttribPointer(
		shaderAttributeIndex_spanQuad,      // attribute index.
		2,                  // number of components in the attribute (here: x,y)
		GL_FLOAT,           // the type of each element
		GL_FALSE,           // should the values be normalized?
		sizeof(Vertex),		// stride
		(GLvoid*) (offsetof(Vertex, spanQuad))   // array buffer offset
		);
	
	glVertexAttribPointer(
		shaderAttributeIndex_texUV,      // attribute index.
		2,                  // number of components in the attribute (here: u,v)
		GL_FLOAT,           // the type of each element
		GL_FALSE,           // should the values be normalized?
		sizeof(Vertex),		// stride
		(GLvoid*)(offsetof(Vertex, texUV))   // array buffer offset
		);

	glVertexAttribPointer(
		shaderAttributeIndex_eventType,      // attribute index.
		1,                  // number of components in the attribute
		GL_FLOAT,			// the type of each element
		GL_FALSE,           // should the values be normalized?
		sizeof(Vertex),		// stride
		(GLvoid*)(offsetof(Vertex, eventType))   // array buffer offset
		);

	/* Push each element in buffer_vertices to the vertex shader, i.e. DRAW :-) */
	glDrawArrays(GL_QUADS, 0, 12); // Starting from vertex 0; 12 vertices total. Will depend on size of dataCall.
	
	// Disable each vertex attribute when it is not immediately used!
	glDisableVertexAttribArray(shaderAttributeIndex_position);
	glDisableVertexAttribArray(shaderAttributeIndex_spanQuad);
	glDisableVertexAttribArray(shaderAttributeIndex_texUV);
	glDisableVertexAttribArray(shaderAttributeIndex_eventType);
	// Deselect (bind to 0) the VBO.
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	this->billboardShader.Disable();

	return true;
}


/**
 * mmvis_static::StaticRenderer::CreateOGLTexture
 */
void mmvis_static::StaticRenderer::CreateOGLTextureFromFile(char* filename, GLuint &textureID) {
	
	// Load image from file using lodepng.
	std::vector<unsigned char> image; //the raw pixels
	unsigned int width, height;
	unsigned int error = lodepng::decode(image, width, height, filename);
	if (error) printf("LodePNG error %d: %s.\n", error, lodepng_error_text(error));

	// Create texture.
	GLenum textureTarget = GL_TEXTURE_2D;
	glBindTexture(textureTarget, textureID); // Operate on this object.
	glTexImage2D(
		textureTarget,		// target
		0,					// level, 0 = base, no minimap,
		GL_RGBA,			// internalformat
		width,				// width
		height,				// height
		0,					// border, always 0 in OpenGL ES
		GL_RGBA,			// format
		GL_UNSIGNED_BYTE,	// type
		&image[0]
		);
	glTexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glBindTexture(textureTarget, 0); // Unbind.
}


/**
* mmvis_static::StaticRenderer::LoadPngTexture
*/
void mmvis_static::StaticRenderer::LoadPngTexture(param::ParamSlot *filenameSlot, vislib::graphics::gl::OpenGLTexture2D &ogl2Texture) {
	const vislib::TString& filename = filenameSlot->Param<param::FilePathParam>()->Value();
	static vislib::graphics::BitmapImage img;
	static sg::graphics::PngBitmapCodec codec;

	printf("Filenameslot: %s\n", filenameSlot->Param<param::FilePathParam>()->Value());

	//char filenameChar[] = "C:/Users/Roi/Bachelor/megamol/plugins/mmvis_static/Assets/GlyphenEventTypesAsterisk8bit.png";
	char filenameChar[] = "GlyphenEventTypesAsterisk.png"; // Copy file to bin folder.

	// Convert TString to vector.
	std::vector<unsigned char> filenameConverted;
	for (unsigned int i = 0; i < filename.Length(); i++) {
		filenameConverted[i] = filename[i];
	}

	printf("Filename: %s\n", *filename);
	printf("Filename Converted: %s\n", filenameConverted);
	printf("Filename Char: %s\n", filenameChar);

	// Has to happen before codec.Load() to be able to load png in img.
	codec.Image() = &img;

	try { // wirft Fehler mit char ("Not a PNG file") bzw. Failed mit TString (da TString leer/fehlerhaft)
		if (codec.Load(filename)) {
			img.Convert(vislib::graphics::BitmapImage::TemplateByteRGBA);
			ogl2Texture.Create(img.Width(), img.Height(), img.PeekData(), GL_RGBA, GL_UNSIGNED_BYTE);
			//ogl2Texture.Bind(); // in Render
		}
		else {
			printf("Failed: Loading texture: %s\n", *filename);
		}
	}
	catch (vislib::Exception ex) {
		printf("Failed texture loading: %s (%s;%d)\n", ex.GetMsgA(), ex.GetFile(), ex.GetLine());
	}
	catch (...) {
		printf("Failed texture loading.\n");
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
	birthOGL2Texture.Release();
	/*deathOGL2Texture.Release();
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
