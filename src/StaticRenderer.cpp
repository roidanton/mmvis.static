/**
 * StaticRenderer.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Alle Rechte vorbehalten.
 *
 * TODO:
 * - Fix billboard ratio (always quadratic and not dependent on window ratio as it is currently)
 * - Add parameter agglomeration.
 * - Embed own StructureEventDataCall.
 * - Enable orthographic mode?
 * - Make size of billboards also dependent on camera
 */

#include "stdafx.h"
#include "vislib/graphics/gl/IncludeAllGL.h"
#include "StaticRenderer.h"
#include "mmcore/CoreInstance.h"
#include "mmcore/view/CallClipPlane.h"
#include "mmcore/view/CallGetTransferFunction.h"
#include "mmcore/view/CallRender3D.h"
#include "mmcore/param/FilePathParam.h"
#include "mmcore/misc/PngBitmapCodec.h"
#include "vislib/assert.h"
#include "lodepng/lodepng.h"

using namespace megamol;
using namespace megamol::core;

void mmvis_static::VisualAttributes::getValidAttributes(core::param::EnumParam *attributes, mmvis_static::VisualAttributes::ParameterType parameterType) {
	switch (parameterType) {
	case mmvis_static::VisualAttributes::ParameterType::Agglomeration:
		attributes->SetTypePair(0, "Size");
		break;
	case mmvis_static::VisualAttributes::ParameterType::Location:
		attributes->SetTypePair(0, "Position");
		break;
	case mmvis_static::VisualAttributes::ParameterType::Time:
		attributes->SetTypePair(0, "Brightness");
		attributes->SetTypePair(1, "Hue");
		attributes->SetTypePair(2, "Opacity");
		break;
	case mmvis_static::VisualAttributes::ParameterType::Type:
		attributes->SetTypePair(0, "Texture");
		break;
	}
}

mmvis_static::VisualAttributes::AttributeType mmvis_static::VisualAttributes::getAttributeType(wchar_t* attribute) {
	try {
		if (attribute == _T("Brightness") || attribute == _T("brightness")) {
			return mmvis_static::VisualAttributes::AttributeType::Brightness;
		}
		if (attribute == _T("Hue") || attribute == _T("hue")) {
			return mmvis_static::VisualAttributes::AttributeType::Hue;
		}
		if (attribute == _T("Opacity") || attribute == _T("opacity")) {
			return mmvis_static::VisualAttributes::AttributeType::Opacity;
		}
		if (attribute == _T("Position") || attribute == _T("Position")) {
			return mmvis_static::VisualAttributes::AttributeType::Position;
		}
		if (attribute == _T("Size") || attribute == _T("size")) {
			return mmvis_static::VisualAttributes::AttributeType::Size;
		}
		if (attribute == _T("Texture") || attribute == _T("texture")) {
			return mmvis_static::VisualAttributes::AttributeType::Texture;
		}
		throw "Invalid attribute";
	}
	catch (char* error){
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
			"mmvis_static::VisualAttributes %s: %ls\n", error, attribute);
	}
	//attributeType = mmvis_static::VisualAttributes::AttributeType::Brightness;
	// The missing return is intentionally as the cpp compiler doesn't check.
}

mmvis_static::VisualAttributes::AttributeType mmvis_static::VisualAttributes::getAttributeType(core::param::ParamSlot *attributeSlot) {
	vislib::TString activeValue;
	try {
		activeValue = attributeSlot->Param<core::param::EnumParam>()->ValueString();
	}
	catch (...) {
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
			"Invalid EnumParam.\n");
	}
	try {
		if (activeValue.Equals(_T("Brightness"), false)) {
			return mmvis_static::VisualAttributes::AttributeType::Brightness;
		}
		if (activeValue.Equals(_T("Hue"), false)) {
			return mmvis_static::VisualAttributes::AttributeType::Hue;
		}
		if (activeValue.Equals(_T("Opacity"), false)) {
			return mmvis_static::VisualAttributes::AttributeType::Opacity;
		}
		if (activeValue.Equals(_T("Position"), false)) {
			return mmvis_static::VisualAttributes::AttributeType::Position;
		}
		if (activeValue.Equals(_T("Size"), false)) {
			return mmvis_static::VisualAttributes::AttributeType::Size;
		}
		if (activeValue.Equals(_T("Texture"), false)) {
			return mmvis_static::VisualAttributes::AttributeType::Texture;
		}
		throw "Invalid attribute";
	}
	catch (char* error){
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
			"mmvis_static::VisualAttributes %s: %ls\n", error, activeValue);
	}
}


/**
 * mmvis_static::StaticRenderer::StaticRenderer
 */
mmvis_static::StaticRenderer::StaticRenderer() : Renderer3DModule(),
	getDataSlot("getdata", "Connects to the data source"),
	getClipPlaneSlot("getclipplane", "Connects to a clipping plane module"),
	filePathBirthTextureSlot("filePathBirthTextureSlot", "The image file for birth events"),
	filePathDeathTextureSlot("filePathDeathTextureSlot", "The image file for death events"),
	filePathMergeTextureSlot("filePathMergeTextureSlot", "The image file for merge events"),
	filePathSplitTextureSlot("filePathSplitTextureSlot", "The image file for split events"),
	eventAgglomerationVisAttrSlot("eventAgglomerationVisAttrSlot", "The visual attribute for the event agglomeration."),
	eventLocationVisAttrSlot("eventLocationVisAttrSlot", "The visual attribute for the event location."),
	eventTypeVisAttrSlot("eventTypeVisAttrSlot", "The visual attribute for the event type."),
	eventTimeVisAttrSlot("eventTimeVisAttrSlot", "The visual attribute for the event time."),
	/* MegaMol configurator doesn't like those (mmvis_static::StaticRenderer is invalid then).
	birthOGL2Texture(vislib::graphics::gl::OpenGLTexture2D()),
	deathOGL2Texture(),
	mergeOGL2Texture(),
	splitOGL2Texture(),
	*/
	billboardShader(),
	firstPass(true)
	{

	this->getDataSlot.SetCompatibleCall<StructureEventsDataCallDescription>();
	this->MakeSlotAvailable(&this->getDataSlot);

	this->getClipPlaneSlot.SetCompatibleCall<view::CallClipPlaneDescription>();
	this->MakeSlotAvailable(&this->getClipPlaneSlot);

	this->filePathBirthTextureSlot << new param::FilePathParam("");
	this->MakeSlotAvailable(&this->filePathBirthTextureSlot);
	
	core::param::EnumParam *visAttrAgglomeration = new core::param::EnumParam(0);
	mmvis_static::VisualAttributes::getValidAttributes(visAttrAgglomeration, mmvis_static::VisualAttributes::ParameterType::Agglomeration);
	this->eventAgglomerationVisAttrSlot << visAttrAgglomeration;
	this->MakeSlotAvailable(&this->eventAgglomerationVisAttrSlot);

	core::param::EnumParam *visAttrLocation = new core::param::EnumParam(0);
	mmvis_static::VisualAttributes::getValidAttributes(visAttrLocation, mmvis_static::VisualAttributes::ParameterType::Location);
	this->eventLocationVisAttrSlot << visAttrLocation;
	this->MakeSlotAvailable(&this->eventLocationVisAttrSlot);

	core::param::EnumParam *visAttrTime = new core::param::EnumParam(1);
	mmvis_static::VisualAttributes::getValidAttributes(visAttrTime, mmvis_static::VisualAttributes::ParameterType::Time);
	this->eventTimeVisAttrSlot << visAttrTime;
	this->MakeSlotAvailable(&this->eventTimeVisAttrSlot);

	core::param::EnumParam *visAttrType = new core::param::EnumParam(0);
	mmvis_static::VisualAttributes::getValidAttributes(visAttrType, mmvis_static::VisualAttributes::ParameterType::Type);
	this->eventTypeVisAttrSlot << visAttrType;
	this->MakeSlotAvailable(&this->eventTypeVisAttrSlot);
}

/**
 * mmvis_static::StaticRenderer::~StaticRenderer
 */
mmvis_static::StaticRenderer::~StaticRenderer(void) {
	this->Release();
}

/**
 * mmvis_static::StaticRenderer::create
 */
bool mmvis_static::StaticRenderer::create(void) {
	ASSERT(IsAvailable());
	
	///
	/// Load, build, link shader.
	///
	vislib::graphics::gl::ShaderSource vert, frag;

	if (!instance()->ShaderSourceFactory().MakeShaderSource("billboard::vertex", vert)) {
		return false;
	}
	if (!instance()->ShaderSourceFactory().MakeShaderSource("billboard::fragment", frag)) {
		return false;
	}

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


	///
	/// Get attributes indices.
	/// Error occurs, when the attribute is not actively used by the shader since
	/// the optimization routines remove unused attributes. 
	///
	shaderAttributeIndex_position = glGetAttribLocation(this->billboardShader, "eventPosition");
	if (shaderAttributeIndex_position == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "eventPosition");
		return false;
	}
	shaderAttributeIndex_spanQuad = glGetAttribLocation(this->billboardShader, "spanQuad");
	if (shaderAttributeIndex_spanQuad == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "spanQuad");
		return false;
	}
	shaderAttributeIndex_texUV = glGetAttribLocation(this->billboardShader, "texUV");
	if (shaderAttributeIndex_texUV == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "texUV");
		return false;
	}
	shaderAttributeIndex_eventType = glGetAttribLocation(this->billboardShader, "eventType");
	if (shaderAttributeIndex_eventType == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "eventType");
		return false;
	}
	shaderAttributeIndex_colorHSV = glGetAttribLocation(this->billboardShader, "colorHSV");
	if (shaderAttributeIndex_colorHSV == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "colorHSV");
		return false;
	}
	shaderAttributeIndex_opacity = glGetAttribLocation(this->billboardShader, "opacity");
	if (shaderAttributeIndex_opacity == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "opacity");
		return false;
	}

	return true;
}


/**
 * mmvis_static::StaticRenderer::Render
 */
bool mmvis_static::StaticRenderer::Render(Call& call) {
	view::CallRender3D *callRender = dynamic_cast<view::CallRender3D*>(&call);
	if (callRender == NULL) return false;

	/////////////////////////////////////////////////
	/// The data and texture loading should be in
	/// ::create(), however no access to data slots
	/// is available there. Therefore firstPass
	/// ensures that the following is only loaded once.
	/////////////////////////////////////////////////

	if (firstPass) { // FirstPass is reset below.

		///
		/// Data is time independent, so it only needs to be loaded once.
		///
		//float scaling = 1.0f;
		//dataCall = getData(1, scaling); // Frame = 1. Wahrscheinlich dataCall komplett überarbeiten und Frames rauswerfen. Die Zeit ist ja im Event gespeichert.
		dataCall = this->getDataSlot.CallAs<mmvis_static::StructureEventsDataCall>();

		if (dataCall == NULL) {
			return false; // Cancel if no data is available. Avoids crash in subsequence code.
		}

		///
		/// Generate textures.
		///
		glGenTextures(4, this->textureIDs); // Creates 4 texture object, set array pointer.
		char filenameChar[] = "GlyphenEventTypesAsterisk.png"; // Copy file to bin folder.
		CreateOGLTextureFromFile(filenameChar, this->textureIDs[0]);
		char filenameChar2[] = "GlyphenEventTypesCross.png"; // Copy file to bin folder.
		CreateOGLTextureFromFile(filenameChar2, this->textureIDs[1]);
		char filenameChar3[] = "GlyphenEventTypesMerge.png"; // Copy file to bin folder.
		CreateOGLTextureFromFile(filenameChar3, this->textureIDs[2]);
		//char filenameChar4[] = "GlyphenEventTypesSplit.png"; // Copy file to bin folder.
		//CreateOGLTextureFromFile(filenameChar4, this->textureIDs[3]);

		// Set Texture. This method is likely obsolete!
		//LoadPngTexture(&this->filePathBirthTextureSlot, this->birthOGL2Texture);
	}

	// Creating three test events as long as dataCall doesnt work. 
	// Per Event parameters: Position, Time, Type, Agglomeration.
	/*
	vislib::Array<glm::vec3> eventPositions;
	eventPositions.Add({ 0.0f, 0.0f, 0.0f });
	eventPositions.Add({ -1.0f, 0.0f, 0.0f });
	eventPositions.Add({ -1.0f, -1.0f, -1.0f });
	vislib::Array<GLfloat> eventType; // ToDo: Make enum.
	eventType.Add(0.0f);
	eventType.Add(1.0f);
	eventType.Add(2.0f);
	vislib::Array<GLfloat> eventTime;
	eventTime.Add(1);
	eventTime.Add(40);
	eventTime.Add(100);
	// Number of additional events in certain defined ranges (good for zoom levels) or better dynamically loaded? Needs testing!
	vislib::Array<glm::mat4> eventAgglomeration;
	// Global parameters: MaxTime, number of events.
	GLfloat eventMaxTime = 120;
	*/

	/////////////////////////////////////////////////
	/// Set flags for vertex recreation.
	/// Vertex recreation has to be done here since:
	/// - The slots are not available in ::create().
	/// - The UI updates should be heeded.
	/// Downside: Performance impact due to
	/// recreation of VBO (copy to device memory).
	/////////////////////////////////////////////////

	bool recreateVertexBuffer = false;

	if (firstPass) {
		recreateVertexBuffer = true;
		firstPass = false; // Reset firstPass.
	}

	// Reset dirty flag of slots if dirty.
	if (this->eventAgglomerationVisAttrSlot.IsDirty()) {
		this->eventAgglomerationVisAttrSlot.ResetDirty();
		recreateVertexBuffer = true;
	}
	if (this->eventLocationVisAttrSlot.IsDirty()) {
		this->eventLocationVisAttrSlot.ResetDirty();
		recreateVertexBuffer = true;
	}
	if (this->eventTimeVisAttrSlot.IsDirty()) {
		this->eventTimeVisAttrSlot.ResetDirty();
		recreateVertexBuffer = true;
	}
	if (this->eventTypeVisAttrSlot.IsDirty()) {
		this->eventTypeVisAttrSlot.ResetDirty();
		recreateVertexBuffer = true;
	}

	///
	/// Read from call and create VBO.
	///
	if (recreateVertexBuffer) {
		// Container for all vertices. ToDo: Make own function for improved readability. Own class isn't meaningful (too many parameters)!
		std::vector<Vertex> vertexList;

		// Get data from call.
		StructureEvents events = dataCall->getEvents();
		const float* locationPtr = events.getLocation();
		const float* timePtr = events.getTime();
		const StructureEvents::EventType* typePtr = events.getType();

		for (int eventCounter = 0; eventCounter < events.getCount(); ++eventCounter, locationPtr += events.getStride(), timePtr += events.getStride(), typePtr += events.getStride()) {
			// Make 4 vertices from one event for quad generation in shader. Alternatively geometry shader could be used too in future.
			for (int quadCounter = 0; quadCounter < 4; ++quadCounter) {
				Vertex vertex;
				glm::vec2 quadSpanModifier, texUV;
				vertex.opacity = 1.0f;
				vertex.colorHSV = { 0.0f, 1.0f, 1.0f };
				float minValue = .4f; // Minimal value for brightness and opacity.
				//unsigned int vertexListCounter = (eventCounter*4 + quadCounter);
				
				// Position.
				if (mmvis_static::VisualAttributes::getAttributeType(&this->eventLocationVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Position) {
					vertex.position.x = locationPtr[0];
					vertex.position.y = locationPtr[1];
					vertex.position.z = locationPtr[2];
				}
				
				// Time.
				if (mmvis_static::VisualAttributes::getAttributeType(&this->eventTimeVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Hue)
					vertex.colorHSV = { *timePtr / events.getMaxTime(), 1.0f, 1.0f }; // Brightness 100%.
				else if (mmvis_static::VisualAttributes::getAttributeType(&this->eventTimeVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Brightness)
					vertex.colorHSV = { 0.0f, 1.0f, *timePtr / events.getMaxTime() * (1.f - minValue) + minValue };  // Color red, brightness from minValue-100%.
				else if (mmvis_static::VisualAttributes::getAttributeType(&this->eventTimeVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Opacity) {
					vertex.opacity = *timePtr / events.getMaxTime() * (1.f - minValue) + minValue;  // From minValue-100%.
				}

				// Type.
				if (mmvis_static::VisualAttributes::getAttributeType(&this->eventTypeVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Texture)
					vertex.eventType = static_cast<float>(*typePtr); // ToDo: Convert enum -> float.
				
				// Specific properties for the quad corners.
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
				vertex.spanQuad = quadSpanModifier;
				//vertex.position += glm::vec3(quadSpanModifier, 0.0f); // Testdata for usage without spanQuad.
				vertex.texUV = texUV;

				vertexList.push_back(vertex);
			}
		}

		// Create a VBO.
		// Generate 1 (generic) buffer, put the resulting identifier in the vertex buffer object
		glGenBuffers(1, &vbo);
		// Bind the buffer to GL_ARRAY_BUFFER (array of vertices)
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		// Give our vertices to OpenGL (fill the buffer with data). Heed total size parameter! (I needed hours to track this down)
		glBufferData(GL_ARRAY_BUFFER, vertexList.size() * sizeof(Vertex), &vertexList.front(), GL_STATIC_DRAW);
	}

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
	glEnableVertexAttribArray(shaderAttributeIndex_colorHSV);
	glEnableVertexAttribArray(shaderAttributeIndex_opacity);

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

	glVertexAttribPointer(
		shaderAttributeIndex_colorHSV,      // attribute index.
		3,                  // number of components in the attribute (here: h,s,v)
		GL_FLOAT,           // the type of each element
		GL_FALSE,           // should the values be normalized?
		sizeof(Vertex),		// stride
		(GLvoid*)(offsetof(Vertex, colorHSV))   // array buffer offset
		);

	glVertexAttribPointer(
		shaderAttributeIndex_opacity,      // attribute index.
		1,                  // number of components in the attribute (here: h,s,v)
		GL_FLOAT,           // the type of each element
		GL_FALSE,           // should the values be normalized?
		sizeof(Vertex),		// stride
		(GLvoid*)(offsetof(Vertex, opacity))   // array buffer offset
		);

	// Push each element in buffer_vertices to the vertex shader. Thx to OGL 2 we can use quads. :-)
	glDrawArrays(GL_QUADS, 0, 12); // Starting from vertex 0; 12 vertices total. Will depend on size of dataCall.
	
	// Disable each vertex attribute when it is not immediately used!
	glDisableVertexAttribArray(shaderAttributeIndex_position);
	glDisableVertexAttribArray(shaderAttributeIndex_spanQuad);
	glDisableVertexAttribArray(shaderAttributeIndex_texUV);
	glDisableVertexAttribArray(shaderAttributeIndex_eventType);
	glDisableVertexAttribArray(shaderAttributeIndex_colorHSV);
	glDisableVertexAttribArray(shaderAttributeIndex_opacity);
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
/*
void mmvis_static::StaticRenderer::LoadPngTexture(param::ParamSlot *filenameSlot, vislib::graphics::gl::OpenGLTexture2D &ogl2Texture) {
	// Slot hat Wert: C:\Users\Roi\Bachelor\megamol\plugins\mmvis_static\Assets\GlyphenEventTypesAsterisk.png
	const vislib::TString filename = filenameSlot->Param<param::FilePathParam>()->Value();
	static vislib::graphics::BitmapImage img;
	static sg::graphics::PngBitmapCodec codec;

	// Convert TString to vector for usage with lodePNG (not in this function anymore).
	std::vector<unsigned char> filenameConverted;
	for (int i = 0; i < filename.Length(); ++i) {
		filenameConverted[i] = filename[i]; // Does this assignment even work? There shouldn't be an element i for the vector available?
	}

	// Debug.
	printf("Filenameslot value: %s\n", filenameSlot->Param<param::FilePathParam>()->Value()); // Leer, vermutlich müsste TString -> string.
	printf("Filename Value: %s\n", *filename); // (null)
	printf("Filename stored adress: %p\n", filename); // leer
	printf("Filename adress: %p\n", &filename); // Three arbitrary, unidentifiable characters.
	printf("Filename Vector pointer: %p\n", filenameConverted); // Three arbitrary, unidentifiable characters, different from before.
	printf("Filename Vector adress: %p\n", &filenameConverted); // Three arbitrary, unidentifiable characters, same as before (points to same adress).

	// Has to happen before codec.Load() to be able to load png in img.
	codec.Image() = &img;

	char filenameChar[] = "GlyphenEventTypesAsterisk.png"; // Copy file to bin folder.

	try { // wirft Fehler mit char - "Not a PNG file"
		if (codec.Load(filenameChar)) { // Mit TString Fehler ("Failed texture loading"), deswegen hardcoded char verwendet.
			img.Convert(vislib::graphics::BitmapImage::TemplateByteRGBA);
			ogl2Texture.Create(img.Width(), img.Height(), img.PeekData(), GL_RGBA, GL_UNSIGNED_BYTE);
			//ogl2Texture.Bind(); // in Render
		}
		else {
			printf("Failed texture loading.\n");
		}
	}
	catch (vislib::Exception ex) {
		printf("Failed texture loading: %s (%s;%d)\n", ex.GetMsgA(), ex.GetFile(), ex.GetLine());
	}
	catch (...) {
		printf("Failed texture loading.\n");
	}
}
*/

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
	glDeleteTextures(4, this->textureIDs);
	/*birthOGL2Texture.Release();
	deathOGL2Texture.Release();
	mergeOGL2Texture.Release();
	splitOGL2Texture.Release();*/
}


/*
 * mmvis_static::StaticRenderer::getData
 */
/*
mmvis_static::StructureEventsDataCall *mmvis_static::StaticRenderer::getData(unsigned int t, float& outScaling) {
	mmvis_static::StructureEventsDataCall *dataCall = this->getDataSlot.CallAs<mmvis_static::StructureEventsDataCall>();
	outScaling = 1.0f;

	// TODO: Remove frames.
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
		printf("Datacall not available!\n");
		return NULL;
	}
}
*/

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
