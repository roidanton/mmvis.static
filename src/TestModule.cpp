/*
 * TestModule.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "vislib/graphics/gl/IncludeAllGL.h"
#include "TestModule.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include "mmcore/param/BoolParam.h"
#include "mmcore/param/FloatParam.h"
#include "mmcore/param/IntParam.h"
#include "mmcore/param/StringParam.h"
#include "mmcore/param/EnumParam.h"
#include "mmcore/utility/ColourParser.h"
#include "vislib/Array.h"
#include "vislib/assert.h"
#include "vislib/math/Vector.h"


namespace megamol {
namespace core {
namespace view {

    static int InterColourComparer(const vislib::math::Vector<float, 5>& lhs, 
            const vislib::math::Vector<float, 5>& rhs) {
        if (lhs[4] >= rhs[4]) {
            if (rhs[4] + vislib::math::FLOAT_EPSILON >= lhs[4]) {
                return 0;
            } else {
                return 1;
            }
        } else {
            if (lhs[4] + vislib::math::FLOAT_EPSILON >= rhs[4]) {
                return 0;
            } else {
                return -1;
            }
        }
    }

} /* end namespace view */
} /* end namespace core */
} /* end namespace megamol */


using namespace megamol;

/*
 * mmvis_static::TestModule::TestModule
 */
mmvis_static::TestModule::TestModule(void) : Module(),
        getTFSlot("gettransferfunction", "Provides the transfer function"),
		visAttrParamSlot("Visual attribute", "The visual attribute."),
        texID(0), texSize(1),
        texFormat(core::view::CallGetTransferFunction::TEXTURE_FORMAT_RGB) {

	core::view::CallGetTransferFunctionDescription cgtfd;
    this->getTFSlot.SetCallback(cgtfd.ClassName(), cgtfd.FunctionName(0),
		&mmvis_static::TestModule::requestTF);
    this->MakeSlotAvailable(&this->getTFSlot);

	core::param::EnumParam *visAttr = new core::param::EnumParam(0);
	visAttr->SetTypePair(0, "Farbwert");
	visAttr->SetTypePair(1, "Helligkeit (HSV)");
	//visAttr->SetTypePair(2, "Transparenz");
	this->visAttrParamSlot << visAttr;
	this->MakeSlotAvailable(&this->visAttrParamSlot);

	// Not possible to assign visAttr a second time for another param. Make new var!
}


/*
 * mmvis_static::TestModule::~TestModule
 */
mmvis_static::TestModule::~TestModule(void) {
    this->Release();
}


/*
 * mmvis_static::TestModule::create
 */
bool mmvis_static::TestModule::create(void) {
    // intentionally empty
    return true;
}


/*
 * mmvis_static::TestModule::release
 */
void mmvis_static::TestModule::release(void) {
    ::glDeleteTextures(1, &this->texID);
    this->texID = 0;
}


/*
 * mmvis_static::TestModule::requestTF
 * Forked from core::view::LinearTransferFunction::requestTF
 */
bool mmvis_static::TestModule::requestTF(core::Call& call) {
	core::view::CallGetTransferFunction *cgtf = dynamic_cast<core::view::CallGetTransferFunction*>(&call);
    if (cgtf == NULL) return false;

    if (this->texID == 0) {
        bool t1de = (glIsEnabled(GL_TEXTURE_1D) == GL_TRUE);
        if (!t1de) glEnable(GL_TEXTURE_1D);
        if (this->texID == 0) glGenTextures(1, &this->texID);

        vislib::Array<vislib::math::Vector<float, 5> > cols;
        vislib::math::Vector<float, 5> cx1, cx2;

		bool validAlpha = false; // For selecting rgb or rgba later on.

		rgb colorRGB;
		hsv colorHSV;
		colorHSV.hue = 1;
		colorHSV.saturation = 1;
		colorHSV.value = 1;
		float opacity = 1;

		cx1[4] = 0.0;
		int steps = 10;
		float stepSize = 1.0f / steps;
		double valueStepSize;

		switch (this->visAttrParamSlot.Param<core::param::EnumParam>()->Value()) {
		case 0: // Color
			valueStepSize = (360 - colorHSV.hue) / 10;
			while (cx1[4] <= 1 + vislib::math::FLOAT_EPSILON) {
				colorRGB = hsv2rgb(colorHSV);
				cx1[0] = colorRGB.r;
				cx1[1] = colorRGB.g;
				cx1[2] = colorRGB.b;
				cx1[3] = opacity;
				cx1[4] = cx1[4];
				if (cx1[3] < 0.99f) validAlpha = true;
				cols.Add(cx1);
				cx1[4] += stepSize;
				colorHSV.hue += valueStepSize;
			}
			break;
		case 1: // Value
			colorHSV.value = .4;
			valueStepSize = (1 - colorHSV.value) / steps;
			while (cx1[4] <= 1 + vislib::math::FLOAT_EPSILON) {
				colorRGB = hsv2rgb(colorHSV);
				cx1[0] = colorRGB.r;
				cx1[1] = colorRGB.g;
				cx1[2] = colorRGB.b;
				cx1[3] = opacity;
				cx1[4] = cx1[4];
				if (cx1[3] < 0.99f) validAlpha = true;
				cols.Add(cx1);
				cx1[4] += stepSize;
				colorHSV.value += valueStepSize;
			}
			break;
		case 2: // Opacity, does not work atm.
			opacity = .4f;
			valueStepSize = (1 - opacity) / steps;
			colorRGB = hsv2rgb(colorHSV);
			while (cx1[4] <= 1 + vislib::math::FLOAT_EPSILON) {
				cx1[0] = colorRGB.r;
				cx1[1] = colorRGB.g;
				cx1[2] = colorRGB.b;
				cx1[3] = opacity;
				cx1[4] = cx1[4];
				if (cx1[3] < 0.99f) validAlpha = true;
				cols.Add(cx1);
				cx1[4] += stepSize;
				opacity += valueStepSize;
			}
			break;
		}

		cols.Sort(&core::view::InterColourComparer);

		this->texSize = 128;
        float *tex = new float[4 * this->texSize];
        int p1, p2;

        cx2 = cols[0];
        p2 = 0;
        for (SIZE_T i = 1; i < cols.Count(); i++) {
            cx1 = cx2;
            p1 = p2;
            cx2 = cols[i];
            ASSERT(cx2[4] <= 1.0f + vislib::math::FLOAT_EPSILON);
            p2 = static_cast<int>(cx2[4] * static_cast<float>(this->texSize - 1));
            ASSERT(p2 < static_cast<int>(this->texSize));
            ASSERT(p2 >= p1);

            for (int p = p1; p <= p2; p++) {
                float al = static_cast<float>(p - p1) / static_cast<float>(p2 - p1);
                float be = 1.0f - al;

                tex[p * 4] = cx1[0] * be + cx2[0] * al;
                tex[p * 4 + 1] = cx1[1] * be + cx2[1] * al;
                tex[p * 4 + 2] = cx1[2] * be + cx2[2] * al;
                tex[p * 4 + 3] = cx1[3] * be + cx2[3] * al;
            }
        }

        GLint otid = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_1D, &otid);
        glBindTexture(GL_TEXTURE_1D, this->texID);

        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, this->texSize, 0, GL_RGBA, GL_FLOAT, tex);

        delete[] tex;

        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);

        glBindTexture(GL_TEXTURE_1D, otid);

        if (!t1de) glDisable(GL_TEXTURE_1D);

        this->texFormat = validAlpha
			? core::view::CallGetTransferFunction::TEXTURE_FORMAT_RGBA
			: core::view::CallGetTransferFunction::TEXTURE_FORMAT_RGB;
    }

    cgtf->SetTexture(this->texID, this->texSize, this->texFormat);

    return true;
}

/*
 * mmvis_static::TestModule::hsv2rgb
 * From http://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
 */
mmvis_static::TestModule::rgb mmvis_static::TestModule::hsv2rgb(hsv in) {
	double      hh, p, q, t, ff;
	long        i;
	rgb         out;

	if (in.saturation <= 0.0) {       // < is bogus, just shuts up warnings
		out.r = in.value;
		out.g = in.value;
		out.b = in.value;
		return out;
	}
	hh = in.hue;
	if (hh >= 360.0) hh = 0.0;
	hh /= 60.0;
	i = (long)hh;
	ff = hh - i;
	p = in.value * (1.0 - in.saturation);
	q = in.value * (1.0 - (in.saturation * ff));
	t = in.value * (1.0 - (in.saturation * (1.0 - ff)));

	switch (i) {
	case 0:
		out.r = in.value;
		out.g = t;
		out.b = p;
		break;
	case 1:
		out.r = q;
		out.g = in.value;
		out.b = p;
		break;
	case 2:
		out.r = p;
		out.g = in.value;
		out.b = t;
		break;

	case 3:
		out.r = p;
		out.g = q;
		out.b = in.value;
		break;
	case 4:
		out.r = t;
		out.g = p;
		out.b = in.value;
		break;
	case 5:
	default:
		out.r = in.value;
		out.g = p;
		out.b = q;
		break;
	}
	return out;
}
