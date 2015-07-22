/**
 * StaticRenderer.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Copyright (C) 2015 by Richard Hähne, TU Dresden
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "StaticRenderer.h"

#include "lodepng/lodepng.h"
#include "mmcore/CoreInstance.h"
#include "mmcore/view/CallClipPlane.h"
#include "mmcore/view/CallGetTransferFunction.h"
#include "mmcore/view/CallRender3D.h"
#include "mmcore/param/FilePathParam.h"
#include "mmcore/misc/PngBitmapCodec.h"
#include "mmcore/param/FloatParam.h"
#include "vislib/assert.h"
#include "vislib/graphics/gl/IncludeAllGL.h"

#include <omp.h>
#include <thread>

using namespace megamol;
using namespace megamol::core;


void mmvis_static::VisualAttributes::getValidAttributes(core::param::EnumParam *attributes, mmvis_static::VisualAttributes::ParameterType parameterType) {
	switch (parameterType) {
	//case mmvis_static::VisualAttributes::ParameterType::Agglomeration:
	//	attributes->SetTypePair(0, "Size");
	//	break;
	case mmvis_static::VisualAttributes::ParameterType::Location:
		attributes->SetTypePair(0, "Position");
		break;
	case mmvis_static::VisualAttributes::ParameterType::Time:
		attributes->SetTypePair(0, "Brightness");
		attributes->SetTypePair(1, "Hue");
		//attributes->SetTypePair(2, "Opacity"); // Usage as time would be stupid.
		attributes->SetTypePair(2, "Texture");
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
	//getClipPlaneSlot("getclipplane", "Connects to a clipping plane module"),
	//filePathBirthTextureSlot("filePathBirthTexture", "The image file for birth events"),
	//filePathDeathTextureSlot("filePathDeathTexture", "The image file for death events"),
	//filePathMergeTextureSlot("filePathMergeTexture", "The image file for merge events"),
	//filePathSplitTextureSlot("filePathSplitTexture", "The image file for split events"),
	//eventAgglomerationVisAttrSlot("eventAgglomerationVisAttr", "The visual attribute for the event agglomeration."),
	eventLocationVisAttrSlot("VisAttr::eventLocation", "The visual attribute for the event location."),
	eventTypeVisAttrSlot("VisAttr::eventType", "The visual attribute for the event type."),
	eventTimeVisAttrSlot("VisAttr::eventTime", "The visual attribute for the event time."),
	timeModeSlot("timeMode", "The time of the structure events that are shown. Correspondences to time set by the view."),
	eventTypeModeSlot("eventTypeMode", "The event types to show."),
	glyphSizeSlot("glyphSize", "Size of event glyphs."),
	/* MegaMol configurator doesn't like those (mmvis_static::StaticRenderer is invalid then).
	birthOGL2Texture(vislib::graphics::gl::OpenGLTexture2D()),
	deathOGL2Texture(),
	mergeOGL2Texture(),
	splitOGL2Texture(),
	*/
	billboardShader(), firstPass(true), dataHash(0), numberOfVertices(0) {

	this->getDataSlot.SetCompatibleCall<StructureEventsDataCallDescription>();
	this->MakeSlotAvailable(&this->getDataSlot);

	//this->getClipPlaneSlot.SetCompatibleCall<view::CallClipPlaneDescription>();
	//this->MakeSlotAvailable(&this->getClipPlaneSlot);

	//this->filePathBirthTextureSlot << new param::FilePathParam("");
	//this->MakeSlotAvailable(&this->filePathBirthTextureSlot);
	
	/// No agglomeration supported.
	//core::param::EnumParam *visAttrAgglomeration = new core::param::EnumParam(0);
	//mmvis_static::VisualAttributes::getValidAttributes(visAttrAgglomeration, mmvis_static::VisualAttributes::ParameterType::Agglomeration);
	//this->eventAgglomerationVisAttrSlot << visAttrAgglomeration;
	//this->MakeSlotAvailable(&this->eventAgglomerationVisAttrSlot);

	core::param::EnumParam *visAttrLocation = new core::param::EnumParam(0);
	mmvis_static::VisualAttributes::getValidAttributes(visAttrLocation, mmvis_static::VisualAttributes::ParameterType::Location);
	this->eventLocationVisAttrSlot << visAttrLocation;
	this->MakeSlotAvailable(&this->eventLocationVisAttrSlot);

	core::param::EnumParam *visAttrTime = new core::param::EnumParam(2);
	mmvis_static::VisualAttributes::getValidAttributes(visAttrTime, mmvis_static::VisualAttributes::ParameterType::Time);
	this->eventTimeVisAttrSlot << visAttrTime;
	this->MakeSlotAvailable(&this->eventTimeVisAttrSlot);

	core::param::EnumParam *visAttrType = new core::param::EnumParam(0);
	mmvis_static::VisualAttributes::getValidAttributes(visAttrType, mmvis_static::VisualAttributes::ParameterType::Type);
	this->eventTypeVisAttrSlot << visAttrType;
	this->MakeSlotAvailable(&this->eventTypeVisAttrSlot);

	core::param::EnumParam *timeModeParam = new core::param::EnumParam(0);
	timeModeParam->SetTypePair(0, "All");
	timeModeParam->SetTypePair(1, "Current");
	timeModeParam->SetTypePair(2, "Previous");
	this->timeModeSlot << timeModeParam;
	this->MakeSlotAvailable(&this->timeModeSlot);

	core::param::EnumParam *eventTypeModeParam = new core::param::EnumParam(0);
	eventTypeModeParam->SetTypePair(0, "All");
	eventTypeModeParam->SetTypePair(1, "Birth");
	eventTypeModeParam->SetTypePair(2, "Death");
	eventTypeModeParam->SetTypePair(3, "Merge");
	eventTypeModeParam->SetTypePair(4, "Split");
	this->eventTypeModeSlot << eventTypeModeParam;
	this->MakeSlotAvailable(&this->eventTypeModeSlot);

	this->glyphSizeSlot.SetParameter(new core::param::FloatParam(0.08f, 0.001f, 1.f));
	this->MakeSlotAvailable(&this->glyphSizeSlot);
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
	shaderAttributeIndex_timeTextureType = glGetAttribLocation(this->billboardShader, "timeTextureType");
	if (shaderAttributeIndex_timeTextureType == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "timeTextureType");
		return false;
	}
	shaderAttributeIndex_relativeTime = glGetAttribLocation(this->billboardShader, "relativeTime");
	if (shaderAttributeIndex_relativeTime == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", "relativeTime");
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
	/// The texture loading should be in
	/// ::create(), however no access to data slots
	/// is available there. Therefore firstPass
	/// ensures that the following is only loaded once.
	/////////////////////////////////////////////////

	if (this->firstPass) { // FirstPass is reset below.
		///
		/// Generate textures.
		///
		glGenTextures(4, this->textureIDs); // Creates texture objects, set array pointer.
		char filenameChar[] = "GlyphEventTypeBirth.png"; // Copy file to bin folder.
		CreateOGLTextureFromFile(filenameChar, this->textureIDs[0]);
		char filenameChar2[] = "GlyphEventTypeDeath.png"; // Copy file to bin folder.
		CreateOGLTextureFromFile(filenameChar2, this->textureIDs[1]);
		char filenameChar3[] = "GlyphEventTypeMerge.png"; // Copy file to bin folder.
		CreateOGLTextureFromFile(filenameChar3, this->textureIDs[2]);
		char filenameChar4[] = "GlyphEventTypeSplit.png"; // Copy file to bin folder.
		CreateOGLTextureFromFile(filenameChar4, this->textureIDs[3]);
		//char filenameChar5[] = "GlyphEventTimeBackground.png"; // Copy file to bin folder.
		//CreateOGLTextureFromFile(filenameChar5, this->textureIDs[4]);

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
	//vislib::Array<glm::mat4> eventAgglomeration;
	// Global parameters: MaxTime, number of events.
	GLfloat eventMaxTime = 120;
	*/

	///
	/// Data is time independent(, so it only needs to be loaded once.)
	/// Data can be changed when manipulated by calculation,
	/// so it has to be reloaded and is outside of firstPass.
	///
	float scaling = 1.0f;
	StructureEventsDataCall* dataCall = GetData(scaling);

	if (dataCall == NULL) {
		return false; // Cancel if no data is available. Avoids crash in subsequence code.
	}

	// Debug.
	//printf("Renderer: SE data call, eventtype %d, fkt %d, frames %d, hash %d, unlocker %p\n", dataCall->getEvents().getEventType(1),
	//	dataCall->FunctionCount(), dataCall->FrameCount(), dataCall->DataHash(), dataCall->GetUnlocker()); // Doesnt work!

	/////////////////////////////////////////////////
	/// Set flags for vertex recreation.
	/// Vertex recreation has to be done here since:
	/// - The slots are not available in ::create().
	/// - The UI updates should be heeded.
	/// Downside: Performance impact due to
	/// recreation of VBO (copy to device memory).
	/////////////////////////////////////////////////

	bool recreateVertexBuffer = false;

	if (this->firstPass) {
		recreateVertexBuffer = true;
		this->firstPass = false; // Reset firstPass.
	}

	// Reset dirty flag of slots if dirty.
	//if (this->eventAgglomerationVisAttrSlot.IsDirty()) {
	//	this->eventAgglomerationVisAttrSlot.ResetDirty();
	//	recreateVertexBuffer = true;
	//}
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

	if (this->timeModeSlot.IsDirty()) {
		this->timeModeSlot.ResetDirty();
		recreateVertexBuffer = true;
	}

	if (this->eventTypeModeSlot.IsDirty()) {
		this->eventTypeModeSlot.ResetDirty();
		recreateVertexBuffer = true;
	}

	if (this->dataHash != dataCall->DataHash()) {
		this->dataHash = dataCall->DataHash();
		recreateVertexBuffer = true;
	}

	if (this->frameIdCache != static_cast<unsigned int>(callRender->Time())) {
		//printf("%d != %d\n", this->frameIdCache, static_cast<unsigned int>(callRender->Time())); // Debug.
		this->frameIdCache = static_cast<unsigned int>(callRender->Time());
		recreateVertexBuffer = true;
	}

	///
	/// Read from call and create VBO.
	/// Bad performance with many events since calculations are made.
	///
	if (recreateVertexBuffer) {
		StructureEvents events = dataCall->getEvents();

		// Causes wrong stride calculation: stride*sizeof(ptr). In tests is was not 20 but 80 (20 * 4 byte)!
		//const float* locationPtr = events.getLocation();
		//const float* timePtr = events.getTime();
		//const StructureEvents::EventType* typePtr = events.getType();

		// Cast to uint8 (= 1 byte) for correct stride calculation in for-loop. Mandatory!
		const uint8_t *locationPtr = static_cast<const uint8_t*>(events.getLocation());
		const uint8_t *timePtr = static_cast<const uint8_t*>(events.getTime());
		const uint8_t *typePtr = static_cast<const uint8_t*>(events.getType());

		// Debug.
		//vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO, "SE Renderer: Recreate events: %d, stride: %d, location: %p, time: %p, type: %p",
		//	events.getCount(), events.getStride(), locationPtr, timePtr, typePtr);

		// Container for all vertices.
		std::vector<Vertex> vertexList;

		// Resize vertex to be able to use concurrency for following loop. Currently not implemented, see below.
		vertexList.resize(events.getCount() * 4); // 4 vertices needed for each event to create billboard. Uses constructor of Vertex.

		// Return 0 when detection not possible: http://en.cppreference.com/w/cpp/thread/thread/hardware_concurrency
		int supportedThreads = std::thread::hardware_concurrency();

		if (supportedThreads == 0)
			supportedThreads = 1; // Important for loop and conditional statements.

		size_t concurrentDivideEvents = 0;
		if (supportedThreads > 1)
			concurrentDivideEvents = static_cast<size_t>(static_cast<float>(events.getCount()) / static_cast<float>(supportedThreads)); // Cast likely rounds down.

		#pragma omp parallel for
		for (int thread = 0; thread < supportedThreads; ++thread) {

			/// Set upper bound.
			size_t concurrentMaxEvent;
			if (thread == supportedThreads - 1)	// Avoid missing events due to round down.
				concurrentMaxEvent = events.getCount();
			else
				concurrentMaxEvent = concurrentDivideEvents * (thread + 1);

			/// Set lower bound.
			size_t concurrentStartEvent = 0;
			if (thread > 0)
				concurrentStartEvent = concurrentDivideEvents * thread + 1;

			const uint8_t *concurrentLocationPtr = locationPtr + concurrentStartEvent * events.getStride();
			const uint8_t *concurrentTimePtr = timePtr + concurrentStartEvent * events.getStride();
			const uint8_t *concurrentTypePtr = typePtr + concurrentStartEvent * events.getStride();

			//vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO, // vislib::sys::log causes crash in concurrency.
			//printf("SERenderer Concurrency: Retrieve event thread %d reading events %d - %d.\n", omp_get_thread_num(), concurrentStartEvent, concurrentMaxEvent); // Debug.

			// No concurrency here (incrementation of pointers has to be sync'ed) so outer loop created.
			for (size_t eventCounter = concurrentStartEvent; eventCounter < concurrentMaxEvent; ++eventCounter,
				concurrentLocationPtr += events.getStride(), concurrentTimePtr += events.getStride(), concurrentTypePtr += events.getStride()) {

				// Use correct pointer types.
				const float *locationPtrf = reinterpret_cast<const float*>(concurrentLocationPtr);
				const float *timePtrf = reinterpret_cast<const float*>(concurrentTimePtr);
				const StructureEvents::EventType *timePtrET = reinterpret_cast<const StructureEvents::EventType*>(concurrentTypePtr);

				// Filter time.
				switch (this->timeModeSlot.Param<param::EnumParam>()->Value()) {
				case 0: // All.
					break;
				case 1: // Current.
					if (*timePtrf != floor(callRender->Time()))
						continue;
					break;
				case 2: // Previous.
					if (*timePtrf > callRender->Time())
						continue;
					break;
				}

				// Filter event types.
				switch (this->eventTypeModeSlot.Param<param::EnumParam>()->Value()) {
				case 0: // All.
					break;
				case 1:
					//Debug printf("Birth %d.  ", *timePtrET);
					if (*timePtrET != StructureEvents::EventType::BIRTH)
						continue;
					break;
				case 2:
					//Debug printf("Death %d.  ", *timePtrET);
					if (*timePtrET != StructureEvents::EventType::DEATH)
						continue;
					break;
				case 3:
					//Debug printf("Merge %d.  ", *timePtrET);
					if (*timePtrET != StructureEvents::EventType::MERGE)
						continue;
					break;
				case 4:
					//Debug printf("Split %d.  ", *timePtrET);
					if (*timePtrET != StructureEvents::EventType::SPLIT)
						continue;
					break;
				}

				// Debug.
				//vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO, "Event %d: location (%f, %f, %f), time %f, type %d / %f",
				//	eventCounter, locationPtrf[0], locationPtrf[1], locationPtrf[2], *timePtrf, *timePtrET, static_cast<float>(*timePtrET));

				///
				/// Get Renderer settings and calculate values (expensive!).
				///
				glm::vec3 position; ///< The position of the event.
				GLfloat eventType = -1.f; ///< Eventtype hardcoded in shader: 0 = birth, 1 = death, 2 = merge, 3 = split
				glm::vec3 colorHSV = { 0.0f, 1.0f, 1.0f }; ///< Color in HSV: Hue, Saturation, Value = [0,1]. Converted to rgb in shader.
				GLfloat opacity = 1.0f; ///< Opacity.
				GLfloat timeTextureType = 0.0f; ///< 0 := No time texture. 1 := texture type 1.
				GLfloat relativeTime = 0.0f; ///< Time from 0 to 1, calculated in cpp.

				float minValueColor = .4f; // Minimal value for brightness and opacity.

				// Position.
				if (mmvis_static::VisualAttributes::getAttributeType(&this->eventLocationVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Position) {
					position.x = locationPtrf[0];
					position.y = locationPtrf[1];
					position.z = locationPtrf[2];
				}

				// Time.
				if (mmvis_static::VisualAttributes::getAttributeType(&this->eventTimeVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Hue)
					colorHSV = { *timePtrf / events.getMaxTime(), 1.0f, 1.0f }; // Brightness 100%.

				else if (mmvis_static::VisualAttributes::getAttributeType(&this->eventTimeVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Brightness)
					colorHSV = { 208.f / 360.f, 1.0f, *timePtrf / events.getMaxTime() * (1.f - minValueColor) + minValueColor };  // Defined color, brightness from minValue-100%.

				// Opacity for time is stupid and deactivated in User Interface anyway.
				//else if (mmvis_static::VisualAttributes::getAttributeType(&this->eventTimeVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Opacity)
				//	vertex.opacity = *timePtrf / events.getMaxTime() * (1.f - minValueColor) + minValueColor;  // From minValue-100%.

				else if (mmvis_static::VisualAttributes::getAttributeType(&this->eventTimeVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Texture) {
					colorHSV = { 0.0f, 0.0f, 1.0f };
					timeTextureType = 1.0f;
					float minValueTex = 0.01f;
					relativeTime = *timePtrf / events.getMaxTime() * (1.f - minValueTex) + minValueTex;
				}

				// Type.
				if (mmvis_static::VisualAttributes::getAttributeType(&this->eventTypeVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Texture)
					eventType = static_cast<float>(*timePtrET);

				///
				/// Make 4 vertices from one event for quad generation in shader. Alternatively geometry shader could be used with recent OGL versions.
				///
				for (int quadCounter = 0; quadCounter < 4; ++quadCounter) {
					// Specific properties for the quad corners.
					glm::vec2 quadSpanModifier, texUV;
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

					size_t vertexListCounter = eventCounter * 4 + quadCounter;
					vertexList[vertexListCounter].position = position;
					vertexList[vertexListCounter].spanQuad = quadSpanModifier;
					vertexList[vertexListCounter].texUV = texUV;
					vertexList[vertexListCounter].eventType = eventType;
					vertexList[vertexListCounter].colorHSV = colorHSV;
					vertexList[vertexListCounter].opacity = opacity;
					vertexList[vertexListCounter].timeTextureType = timeTextureType;
					vertexList[vertexListCounter].relativeTime = relativeTime;

					//vertexList[vertexListCounter].position += glm::vec3(quadSpanModifier, 0.0f); // Testdata for usage without spanQuad.
				}
				
				// Create second quad for time with slightly off z-position. Obsolete since done in same texture.
				/*
				if (mmvis_static::VisualAttributes::getAttributeType(&this->eventTimeVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Texture) {
					for (int quadCounter = 0; quadCounter < 4; ++quadCounter) {
						Vertex vertex;
						vertex.opacity = 1.0f;
						vertex.colorHSV = { 0.0f, 0.0f, 1.0f }; // White.
						float minValue = .1f; // Minimal value for quad size.

						// Position (it gets pushed back a little in the shader).
						if (mmvis_static::VisualAttributes::getAttributeType(&this->eventLocationVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Position) {
							vertex.position.x = locationPtrf[0];
							vertex.position.y = locationPtrf[1];
							vertex.position.z = locationPtrf[2];
						}

						// Type misuse.
						if (mmvis_static::VisualAttributes::getAttributeType(&this->eventTypeVisAttrSlot) == mmvis_static::VisualAttributes::AttributeType::Texture)
							vertex.eventType = 4; // It's a misuse of this parameter to tell the shader that this is a time quad.

						// Specific properties for the quad corners. Defines size dependent upon time.
						float quadSpanHeight = *timePtrf / events.getMaxTime() * (1.f - minValue) + minValue; // Range 0 to +1.
						float quadSpanHeightScaled = quadSpanHeight * 2 - 1; // Since quad ranges from -1 to +1 instead of 0 to +1.
						glm::vec2 quadSpanModifier, texUV;
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
							quadSpanModifier = { 1.0f, quadSpanHeightScaled };
							texUV = { 1.0f, quadSpanHeight };
							break;
						case 3:
							quadSpanModifier = { -1.0f, quadSpanHeightScaled };
							texUV = { 0.0f, quadSpanHeight };
							break;
						}
						vertex.spanQuad = quadSpanModifier;
						//vertex.position += glm::vec3(quadSpanModifier, 0.0f); // Testdata for usage without spanQuad.
						vertex.texUV = texUV;

						vertexList.push_back(vertex);
					}
				}
				*/
			}
		}

		// Set dummy data to avoid crash. Not needed with resize.
		/*
		if (vertexList.size() == 0) {
			bool testData = false;
			
			if (!testData) {
				Vertex vertex;
				vertex.position = { 0, 0, 0 };
				vertex.colorHSV = { 0, 0, 0 };
				vertex.opacity = 0;
				vertex.spanQuad = { 0, 0 };
				vertex.texUV = { 0, 0 };
				vertex.eventType = 0;
				vertex.timeTextureType = 0;
				vertex.relativeTime = 0;
				vertexList.push_back(vertex);
			}
			else {
				// Set test data
				int eventMax = 4;
				for (int i = 0; i < eventMax; ++i) {
					for (int quadCounter = 0; quadCounter < 4; ++quadCounter) {
						Vertex vertex;
						vertex.position = { i, i, i };
						vertex.colorHSV = { (float) i / (float) eventMax, (float) i / (float) eventMax, 1 };
						vertex.opacity = 1.0f;
						vertex.eventType = static_cast<float> (i % 4);
						vertex.timeTextureType = 0;
						vertex.relativeTime = 0;

						// Specific properties for the quad corners.
						glm::vec2 quadSpanModifier, texUV;
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
						vertex.texUV = texUV;

						//printf("Dummy event: %d, %f, color %f, type %f\n", i, vertex.position.x, vertex.colorHSV.x, vertex.eventType);

						vertexList.push_back(vertex);
					}
				}
				vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
					"SERenderer: Dummy vertex data set: %d vertices, test (%f, %f).", vertexList.size(), vertexList[10].position.x, vertexList[5].eventType);
			}
			
		}
		*/

		// Debug.
		//vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
		//	"SERenderer: %d vertices. VertexID: (Pos), EventType:\n  %d: (%f, %f, %f), %f\n  %d: (%f, %f, %f), %f",
		//	vertexList.size(), 0, vertexList[0].position.x, vertexList[0].position.y, vertexList[0].position.z, vertexList[0].eventType,
		//	vertexList.size() - 1, vertexList[vertexList.size() - 1].position.x, vertexList[vertexList.size() - 1].position.y,
		//	vertexList[vertexList.size() - 1].position.z, vertexList[vertexList.size() - 1].eventType);

		// Create a VBO.
		// Generate 1 (generic) buffer, put the resulting identifier in the vertex buffer object
		glGenBuffers(1, &vbo);
		// Bind the buffer to GL_ARRAY_BUFFER (array of vertices)
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		// Give our vertices to OpenGL (fill the buffer with data). Heed total size parameter! (I needed hours to track this down)
		glBufferData(GL_ARRAY_BUFFER, vertexList.size() * sizeof(Vertex), &vertexList.front(), GL_STATIC_DRAW);

		// This is needed for glDrawArrays() below.
		this->numberOfVertices = vertexList.size();
	}

	glEnable(GL_DEPTH_TEST);

	// Scale the positions according to the bounding box (see GetData).
	glScalef(scaling, scaling, scaling);

	//glDisable(GL_CULL_FACE);

	this->billboardShader.Enable();

	// Set sizeModificator of all billboards. Scales the billboards in shader. Additional scale to allow very small icons.
	GLfloat quadSizeModificator = this->glyphSizeSlot.Param<param::FloatParam>()->Value() / 10;
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
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, this->textureIDs[3]);
	glUniform1i(this->billboardShader.ParameterLocation("tex2DSplit"), 3);
	//glActiveTexture(GL_TEXTURE4);
	//glBindTexture(GL_TEXTURE_2D, this->textureIDs[4]);
	//glUniform1i(this->billboardShader.ParameterLocation("tex2DTimeBackground"), 4);
	//this->birthOGL2Texture.Bind();
	
	// Set the ID for the shader variable (attribute).
	glEnableVertexAttribArray(shaderAttributeIndex_position);
	glEnableVertexAttribArray(shaderAttributeIndex_spanQuad);
	glEnableVertexAttribArray(shaderAttributeIndex_texUV);
	glEnableVertexAttribArray(shaderAttributeIndex_eventType);
	glEnableVertexAttribArray(shaderAttributeIndex_colorHSV);
	glEnableVertexAttribArray(shaderAttributeIndex_opacity);
	glEnableVertexAttribArray(shaderAttributeIndex_timeTextureType);
	glEnableVertexAttribArray(shaderAttributeIndex_relativeTime);

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
		1,                  // number of components in the attribute
		GL_FLOAT,           // the type of each element
		GL_FALSE,           // should the values be normalized?
		sizeof(Vertex),		// stride
		(GLvoid*)(offsetof(Vertex, opacity))   // array buffer offset
		);

	glVertexAttribPointer(
		shaderAttributeIndex_timeTextureType,      // attribute index.
		1,                  // number of components in the attribute
		GL_FLOAT,           // the type of each element
		GL_FALSE,           // should the values be normalized?
		sizeof(Vertex),		// stride
		(GLvoid*)(offsetof(Vertex, timeTextureType))   // array buffer offset
		);

	glVertexAttribPointer(
		shaderAttributeIndex_relativeTime,      // attribute index.
		1,                  // number of components in the attribute
		GL_FLOAT,           // the type of each element
		GL_FALSE,           // should the values be normalized?
		sizeof(Vertex),		// stride
		(GLvoid*)(offsetof(Vertex, relativeTime))   // array buffer offset
		);

	// Push each element in buffer_vertices to the vertex shader. Thx to OGL 2 we can use quads. :-)
	glDrawArrays(GL_QUADS, 0, static_cast<GLsizei> (this->numberOfVertices)); // Starting from vertex 0; total vertices. Will depend on size of dataCall.
	
	// Disable each vertex attribute when it is not immediately used!
	glDisableVertexAttribArray(shaderAttributeIndex_position);
	glDisableVertexAttribArray(shaderAttributeIndex_spanQuad);
	glDisableVertexAttribArray(shaderAttributeIndex_texUV);
	glDisableVertexAttribArray(shaderAttributeIndex_eventType);
	glDisableVertexAttribArray(shaderAttributeIndex_colorHSV);
	glDisableVertexAttribArray(shaderAttributeIndex_opacity);
	glDisableVertexAttribArray(shaderAttributeIndex_timeTextureType);
	glDisableVertexAttribArray(shaderAttributeIndex_relativeTime);
	// Deselect (bind to 0) the VBO.
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Deselect textures.
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);

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
	glBindTexture(textureTarget, 0); // Unbind.
}


mmvis_static::StructureEventsDataCall* mmvis_static::StaticRenderer::GetData(float& outScaling) {
	mmvis_static::StructureEventsDataCall *dataCall = this->getDataSlot.CallAs<mmvis_static::StructureEventsDataCall>();

	if (dataCall != NULL) {
		//dataCall->SetFrameID(t);
		
		// calculate scaling
		outScaling = dataCall->AccessBoundingBoxes().ObjectSpaceBBox().LongestEdge();
		if (outScaling > 0.0000001) {
			outScaling = 10.0f / outScaling;
		}
		else {
			outScaling = 1.0f;
		}

		if (!(*dataCall)(0)) return NULL;

		return dataCall;
	}
	else {
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_WARN, "Static Renderer: Datacall not available.");
		return NULL;
	}
}


bool mmvis_static::StaticRenderer::GetExtents(Call& call) {
	view::CallRender3D *callRender = dynamic_cast<view::CallRender3D*>(&call);
	if (callRender == NULL)
		return false;

	/// FrameCount and bbox has to be set in SE calculation/reader.

	mmvis_static::StructureEventsDataCall *dataCall = this->getDataSlot.CallAs<mmvis_static::StructureEventsDataCall>();
	if ((dataCall != NULL) && ((*dataCall)(1))) {
		callRender->SetTimeFramesCount(dataCall->FrameCount());
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
		callRender->SetTimeFramesCount(1);
		callRender->AccessBoundingBoxes().Clear();
	}

	return true;
}


void mmvis_static::StaticRenderer::release(void) {
	this->billboardShader.Release();
	glDeleteTextures(4, this->textureIDs);
	/*birthOGL2Texture.Release();
	deathOGL2Texture.Release();
	mergeOGL2Texture.Release();
	splitOGL2Texture.Release();*/
}


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
*/


