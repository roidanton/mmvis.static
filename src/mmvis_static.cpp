/**
 * mmvis_static.cpp
 *
 * Copyright (C) 2009-2015 by MegaMol Team
 * Copyright (C) 2015 by Richard Hähne, TU Dresden
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "mmvis_static/mmvis_static.h"

#include "mmcore/api/MegaMolCore.std.h"
#include "mmcore/utility/plugins/Plugin200Instance.h"
#include "mmcore/versioninfo.h"
#include "vislib/vislibversion.h"

// Modules.
#include "StaticRenderer.h"
#include "StructureEventsCalculation.h"
#include "StructureEventsDataSource.h"
#include "StructureEventsWriter.h"

// Calls.
#include "StructureEventsDataCall.h"

/* anonymous namespace hides this type from any other object files */
namespace {
    /** Implementing the instance class of this plugin */
    class plugin_instance : public ::megamol::core::utility::plugins::Plugin200Instance {
    public:
        /** ctor */
        plugin_instance(void)
            : ::megamol::core::utility::plugins::Plugin200Instance(

                /* machine-readable plugin assembly name */
                "mmvis_static",

                /* human-readable plugin description */
                "MegaMol Plugin for detection and static visualization of structural processes.") {

            // here we could perform addition initialization
        };
        /** Dtor */
        virtual ~plugin_instance(void) {
            // here we could perform addition de-initialization
        }
        /** Registers modules and calls */
        virtual void registerClasses(void) {

            // register modules here:
			this->module_descriptions.RegisterAutoDescription<megamol::mmvis_static::StaticRenderer>();
			this->module_descriptions.RegisterAutoDescription<megamol::mmvis_static::StructureEventsCalculation>();
			this->module_descriptions.RegisterAutoDescription<megamol::mmvis_static::StructureEventsDataSource>();
			this->module_descriptions.RegisterAutoDescription<megamol::mmvis_static::StructureEventsWriter>();

            // register calls here:
			this->call_descriptions.RegisterAutoDescription<megamol::mmvis_static::StructureEventsDataCall>();
        }
        MEGAMOLCORE_PLUGIN200UTIL_IMPLEMENT_plugininstance_connectStatics
    };
}


/*
 * mmplgPluginAPIVersion
 */
MMVIS_STATIC_API int mmplgPluginAPIVersion(void) {
    MEGAMOLCORE_PLUGIN200UTIL_IMPLEMENT_mmplgPluginAPIVersion
}


/*
 * mmplgGetPluginCompatibilityInfo
 */
MMVIS_STATIC_API
::megamol::core::utility::plugins::PluginCompatibilityInfo *
mmplgGetPluginCompatibilityInfo(
megamol::core::utility::plugins::ErrorCallback onError) {
	// compatibility information with core and vislib
	using megamol::core::utility::plugins::PluginCompatibilityInfo;
	using megamol::core::utility::plugins::LibraryVersionInfo;
	using megamol::core::utility::plugins::SetLibraryVersionInfo;

	PluginCompatibilityInfo *ci = new PluginCompatibilityInfo;
	ci->libs_cnt = 2;
	ci->libs = new LibraryVersionInfo[2];

	SetLibraryVersionInfo(ci->libs[0], "MegaMolCore",
		MEGAMOL_CORE_MAJOR_VER, MEGAMOL_CORE_MINOR_VER, MEGAMOL_CORE_MAJOR_REV, MEGAMOL_CORE_MINOR_REV, 0
#if defined(DEBUG) || defined(_DEBUG)
		| MEGAMOLCORE_PLUGIN200UTIL_FLAGS_DEBUG_BUILD
#endif
#if defined(MEGAMOL_CORE_DIRTY) && (MEGAMOL_CORE_DIRTY != 0)
		| MEGAMOLCORE_PLUGIN200UTIL_FLAGS_DIRTY_BUILD
#endif
		);

	SetLibraryVersionInfo(ci->libs[1], "vislib",
		VISLIB_VERSION_MAJOR, VISLIB_VERSION_MINOR, VISLIB_VERSION_REVISION, VISLIB_VERSION_BUILD, 0
#if defined(DEBUG) || defined(_DEBUG)
		| MEGAMOLCORE_PLUGIN200UTIL_FLAGS_DEBUG_BUILD
#endif
#if defined(VISLIB_DIRTY_BUILD) && (VISLIB_DIRTY_BUILD != 0)
		| MEGAMOLCORE_PLUGIN200UTIL_FLAGS_DIRTY_BUILD
#endif
		);

	return ci;
}


/*
 * mmplgReleasePluginCompatibilityInfo
 */
MMVIS_STATIC_API
void mmplgReleasePluginCompatibilityInfo(
        ::megamol::core::utility::plugins::PluginCompatibilityInfo* ci) {
    // release compatiblity data on the correct heap
    MEGAMOLCORE_PLUGIN200UTIL_IMPLEMENT_mmplgReleasePluginCompatibilityInfo(ci)
}


/*
 * mmplgGetPluginInstance
 */
MMVIS_STATIC_API
::megamol::core::utility::plugins::AbstractPluginInstance*
mmplgGetPluginInstance(
        ::megamol::core::utility::plugins::ErrorCallback onError) {
    MEGAMOLCORE_PLUGIN200UTIL_IMPLEMENT_mmplgGetPluginInstance(plugin_instance, onError)
}


/*
 * mmplgReleasePluginInstance
 */
MMVIS_STATIC_API
void mmplgReleasePluginInstance(
        ::megamol::core::utility::plugins::AbstractPluginInstance* pi) {
    MEGAMOLCORE_PLUGIN200UTIL_IMPLEMENT_mmplgReleasePluginInstance(pi)
}
