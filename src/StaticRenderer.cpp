/*
* StaticRenderer.cpp
*
* Copyright (C) 2009-2015 by MegaMol Team
* Alle Rechte vorbehalten.
*
* TODO:
* - Custom renderer.
* - Create own eventDataCall.
*/

#include "stdafx.h"
#include "vislib/graphics/gl/IncludeAllGL.h"
#include "StaticRenderer.h"
#include "mmcore/CoreInstance.h"
#include "mmcore/view/CallClipPlane.h"
#include "mmcore/view/CallGetTransferFunction.h"
#include "mmcore/view/CallRender3D.h"
#include "mmcore/param/IntParam.h"
#include "vislib/assert.h"

using namespace megamol;
using namespace megamol::core;

/*
 * mmvis_static::StaticRenderer::StaticRenderer
 */
mmvis_static::StaticRenderer::StaticRenderer() : Renderer3DModule(),
	getDataSlot("getdata", "Connects to the data source"),
	getTFSlot("gettransferfunction", "Connects to the transfer function module"),
	getClipPlaneSlot("getclipplane", "Connects to a clipping plane module"),
	greyTF(0),
	polygonShader() {

	this->getDataSlot.SetCompatibleCall<moldyn::MultiParticleDataCallDescription>();
	this->MakeSlotAvailable(&this->getDataSlot);

	this->getTFSlot.SetCompatibleCall<view::CallGetTransferFunctionDescription>();
	this->MakeSlotAvailable(&this->getTFSlot);

	this->getClipPlaneSlot.SetCompatibleCall<view::CallClipPlaneDescription>();
	this->MakeSlotAvailable(&this->getClipPlaneSlot);
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

	vislib::graphics::gl::ShaderSource vert, frag;

	if (!instance()->ShaderSourceFactory().MakeShaderSource("polygon::vertex", vert)) {
		return false;
	}
	if (!instance()->ShaderSourceFactory().MakeShaderSource("polygon::fragment", frag)) {
		return false;
	}

	//printf("\nVertex Shader:\n%s\n\nFragment Shader:\n%s\n",
	//    vert.WholeCode().PeekBuffer(),
	//    frag.WholeCode().PeekBuffer());

	try {
		if (!this->polygonShader.Create(vert.Code(), vert.Count(), frag.Code(), frag.Count())) {
			vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
				"Unable to compile sphere shader: Unknown error\n");
			return false;
		}

	}
	catch (vislib::graphics::gl::AbstractOpenGLShader::CompileException ce) {
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
			"Unable to compile sphere shader (@%s): %s\n",
			vislib::graphics::gl::AbstractOpenGLShader::CompileException::CompileActionName(
			ce.FailedAction()), ce.GetMsgA());
		return false;
	}
	catch (vislib::Exception e) {
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
			"Unable to compile sphere shader: %s\n", e.GetMsgA());
		return false;
	}
	catch (...) {
		vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
			"Unable to compile sphere shader: Unknown exception\n");
		return false;
	}

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
	this->polygonShader.Release();
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

/*
 * mmvis_static::StaticRenderer::Render
 */
bool mmvis_static::StaticRenderer::Render(Call& call) {
	view::CallRender3D *cr = dynamic_cast<view::CallRender3D*>(&call);
	if (cr == NULL) return false;

	float scaling = 1.0f;
	moldyn::MultiParticleDataCall *c2 = this->getData(static_cast<unsigned int>(cr->Time()), scaling);
	if (c2 == NULL) return false;

	float clipDat[4];
	float clipCol[4];
	this->getClipData(clipDat, clipCol);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

	float viewportStuff[4];
	::glGetFloatv(GL_VIEWPORT, viewportStuff);
	glPointSize(vislib::math::Max(viewportStuff[2], viewportStuff[3]));
	if (viewportStuff[2] < 1.0f) viewportStuff[2] = 1.0f;
	if (viewportStuff[3] < 1.0f) viewportStuff[3] = 1.0f;
	viewportStuff[2] = 2.0f / viewportStuff[2];
	viewportStuff[3] = 2.0f / viewportStuff[3];

	this->polygonShader.Enable();
	glUniform4fv(this->polygonShader.ParameterLocation("viewAttr"), 1, viewportStuff);
	glUniform3fv(this->polygonShader.ParameterLocation("camIn"), 1, cr->GetCameraParameters()->Front().PeekComponents());
	glUniform3fv(this->polygonShader.ParameterLocation("camRight"), 1, cr->GetCameraParameters()->Right().PeekComponents());
	glUniform3fv(this->polygonShader.ParameterLocation("camUp"), 1, cr->GetCameraParameters()->Up().PeekComponents());

	glUniform4fv(this->polygonShader.ParameterLocation("clipDat"), 1, clipDat);
	glUniform4fv(this->polygonShader.ParameterLocation("clipCol"), 1, clipCol);

	glScalef(scaling, scaling, scaling);

	unsigned int cial = glGetAttribLocationARB(this->polygonShader, "colIdx");

	for (unsigned int i = 0; i < c2->GetParticleListCount(); i++) {
		moldyn::MultiParticleDataCall::Particles &parts = c2->AccessParticles(i);
		float minC = 0.0f, maxC = 0.0f;
		unsigned int colTabSize = 0;

		// colour
		switch (parts.GetColourDataType()) {
		case moldyn::MultiParticleDataCall::Particles::COLDATA_NONE:
			glColor3ubv(parts.GetGlobalColour());
			break;
		case moldyn::MultiParticleDataCall::Particles::COLDATA_UINT8_RGB:
			glEnableClientState(GL_COLOR_ARRAY);
			glColorPointer(3, GL_UNSIGNED_BYTE, parts.GetColourDataStride(), parts.GetColourData());
			break;
		case moldyn::MultiParticleDataCall::Particles::COLDATA_UINT8_RGBA:
			glEnableClientState(GL_COLOR_ARRAY);
			glColorPointer(4, GL_UNSIGNED_BYTE, parts.GetColourDataStride(), parts.GetColourData());
			break;
		case moldyn::MultiParticleDataCall::Particles::COLDATA_FLOAT_RGB:
			glEnableClientState(GL_COLOR_ARRAY);
			glColorPointer(3, GL_FLOAT, parts.GetColourDataStride(), parts.GetColourData());
			break;
		case moldyn::MultiParticleDataCall::Particles::COLDATA_FLOAT_RGBA:
			glEnableClientState(GL_COLOR_ARRAY);
			glColorPointer(4, GL_FLOAT, parts.GetColourDataStride(), parts.GetColourData());
			break;
		case moldyn::MultiParticleDataCall::Particles::COLDATA_FLOAT_I: {
			glEnableVertexAttribArrayARB(cial);
			glVertexAttribPointerARB(cial, 1, GL_FLOAT, GL_FALSE, parts.GetColourDataStride(), parts.GetColourData());

			glEnable(GL_TEXTURE_1D);

			view::CallGetTransferFunction *cgtf = this->getTFSlot.CallAs<view::CallGetTransferFunction>();
			if ((cgtf != NULL) && ((*cgtf)())) {
				glBindTexture(GL_TEXTURE_1D, cgtf->OpenGLTexture());
				colTabSize = cgtf->TextureSize();
			}
			else {
				glBindTexture(GL_TEXTURE_1D, this->greyTF);
				colTabSize = 2;
			}

			glUniform1i(this->polygonShader.ParameterLocation("colTab"), 0);
			minC = parts.GetMinColourIndexValue();
			maxC = parts.GetMaxColourIndexValue();
			glColor3ub(127, 127, 127);
		} break;
		default:
			glColor3ub(127, 127, 127);
			break;
		}

		// radius and position
		switch (parts.GetVertexDataType()) {
		case moldyn::MultiParticleDataCall::Particles::VERTDATA_NONE:
			continue;
		case moldyn::MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZ:
			glEnableClientState(GL_VERTEX_ARRAY);
			glUniform4f(this->polygonShader.ParameterLocation("inConsts1"), parts.GetGlobalRadius(), minC, maxC, float(colTabSize));
			glVertexPointer(3, GL_FLOAT, parts.GetVertexDataStride(), parts.GetVertexData());
			break;
		case moldyn::MultiParticleDataCall::Particles::VERTDATA_FLOAT_XYZR:
			glEnableClientState(GL_VERTEX_ARRAY);
			glUniform4f(this->polygonShader.ParameterLocation("inConsts1"), -1.0f, minC, maxC, float(colTabSize));
			glVertexPointer(4, GL_FLOAT, parts.GetVertexDataStride(), parts.GetVertexData());
			break;
		default:
			continue;
		}

		glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(parts.GetCount()));

		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableVertexAttribArrayARB(cial);
		glDisable(GL_TEXTURE_1D);
	}

	c2->Unlock();

	this->polygonShader.Disable();

	glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);

	return true;
}
