/*
 * TestModule.h
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Alle Rechte vorbehalten.
 */

#ifndef MMVISSTATIC_TESTMODULE_H_INCLUDED
#define MMVISSTATIC_TESTMODULE_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "mmcore/Call.h"
#include "mmcore/CalleeSlot.h"
#include "mmcore/Module.h"
#include "mmcore/param/ParamSlot.h"
#include "mmcore/view/CallGetTransferFunction.h"


namespace megamol {
namespace mmvis_static {

    /**
     * Module to test slot and param functionality.
     */
    class TestModule : public core::Module {
    public:

        /**
         * Answer the name of this module.
         *
         * @return The name of this module.
         */
        static const char *ClassName(void) {
            return "TestModule";
        }

        /**
         * Answer a human readable description of this module.
         *
         * @return A human readable description of this module.
         */
        static const char *Description(void) {
            return "Module to test slot and param functionality.";
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
        TestModule(void);

        /** Dtor. */
        virtual ~TestModule(void);

    private:
		typedef struct {
			double r;       // percent
			double g;       // percent
			double b;       // percent
		} rgb;

		typedef struct {
			double hue; // angle in degrees
			double saturation; // percent
			double value; // percent
		} hsv;

		static rgb hsv2rgb(hsv in);

        /**
         * Implementation of 'Create'.
         *
         * @return 'true' on success, 'false' otherwise.
         */
        virtual bool create(void);

        /**
         * Implementation of 'Release'.
         */
        virtual void release(void);

        /**
         * Callback called when the transfer function is requested.
         *
         * @param call The calling call
         *
         * @return 'true' on success
         */
		bool requestTF(core::Call& call);

        /** The callee slot called on request of a transfer function */
		core::CalleeSlot getTFSlot;

		/** The visual attribute. */
		core::param::ParamSlot visAttrParamSlot;

        /** The OpenGL texture object id */
        unsigned int texID;

        /** The texture size in texel */
        unsigned int texSize;

        /** The texture format */
		core::view::CallGetTransferFunction::TextureFormat texFormat;
    };


} /* end namespace mmvis_static */
} /* end namespace megamol */

#endif /* MMVISSTATIC_TESTMODULE_H_INCLUDED */
