/*
* ContourTree.cpp
*
* Copyright (C) 2009-2015 by MegaMol Team
* Alle Rechte vorbehalten.
*/

#include "stdafx.h"
#include "ContourTree.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include "mmcore/param/IntParam.h"

using namespace megamol;

/*
 * mmvis_static::ContourTree::ContourTree
 */
mmvis_static::ContourTree::ContourTree()
	: stdplugin::datatools::AbstractParticleManipulator("outData", "indata"),
	thresholdSlot("Threshold", "The threshold for the contour tree.") {
	this->thresholdSlot.SetParameter(new core::param::IntParam(10, 1));
	this->MakeSlotAvailable(&this->thresholdSlot);
}


/*
 * mmvis_static::ContourTree::~ContourTree
 */
mmvis_static::ContourTree::~ContourTree(void) {
	this->Release();
}

/*
 * datatools::ContourTree::manipulateData
 */
bool mmvis_static::ContourTree::manipulateData(
	megamol::core::moldyn::MultiParticleDataCall& outData,
	megamol::core::moldyn::MultiParticleDataCall& inData) {
	using megamol::core::moldyn::MultiParticleDataCall;
	int threshold = this->thresholdSlot.Param<core::param::IntParam>()->Value();

	outData = inData; // also transfers the unlocker to 'outData'

	inData.SetUnlocker(nullptr, false); // keep original data locked
										// original data will be unlocked through outData

	unsigned int particleListCount = outData.GetParticleListCount();
	for (unsigned int particleListIndex = 0; particleListIndex < particleListCount; particleListIndex++) {
		MultiParticleDataCall::Particles& particleList = outData.AccessParticles(particleListIndex); // Partikelliste dient zur Gruppieren: Definieren eines gemeinsamen Radius oder/und Color von beliebig vielen Partikeln.

		UINT64 cnt = particleList.GetCount();

		/**
		 * Position eines Partikels
		 * - Bytedatensatz (col_data), Elemente sind eine bestimmte Anzahl von Bytes voneinander entfernt
		 * - stride dient dazu, den Abstand zwischen Elementen "beliebig" zu setzen, um etwa Color oder andere Daten an ein Element anzufügen
		 * 
		 * @see megamol::stdplugin::datatools::ParticleDensityOpacityModule::compute_density_grid
		 */

		// Abstand zwischen den Elementen sicherheitshalber korrigieren: Nur setzen, wenn stride kleiner als die Bytes für XYZ, XYZR(Radius) ist.
		size_t vert_stride = particleList.GetVertexDataStride();
		switch (particleList.GetVertexDataType()) {
		case core::moldyn::SimpleSphericalParticles::VERTDATA_FLOAT_XYZ:
			if (vert_stride < 12) vert_stride = 12;
			break;
		case core::moldyn::SimpleSphericalParticles::VERTDATA_FLOAT_XYZR:
			if (vert_stride < 16) vert_stride = 16;
			break;
		default: throw std::exception();
		}

		// Partikelposition auslesen.
		const uint8_t *vertData = static_cast<const uint8_t*>(particleList.GetVertexData());
		for (uint64_t particleIndex = 0; particleIndex < particleList.GetCount(); particleIndex++, vertData += vert_stride) {
			const float *v = reinterpret_cast<const float*>(vertData); //v[0] = x, v[1] = y, v[2] = z, v[3] = radius
			unsigned int x = static_cast<unsigned int>(::vislib::math::Clamp<float>((v[0] - box.Left()) / rad, 0.0f, static_cast<float>(dim_x - 1)));
			unsigned int y = static_cast<unsigned int>(::vislib::math::Clamp<float>((v[1] - box.Bottom()) / rad, 0.0f, static_cast<float>(dim_y - 1)));
			unsigned int z = static_cast<unsigned int>(::vislib::math::Clamp<float>((v[2] - box.Back()) / rad, 0.0f, static_cast<float>(dim_z - 1)));

			cnt_grid[x + dim_x * (y + dim_y * z)]++;
		}

		/**
		 * Zugriff auf jeden Partikel/Farbe.
		 * Abhängig von Datensatz, hier RGBA, Daten werden mittels Zeiger auf colData gesetzt.
		 * 
		 * @see megamol::stdplugin::datatools::ParticleDensityOpacityModule::makeData
		 */		
		size_t all_cnt = this->count_all_particles(outData);
		int col_step = (use_rgba ? 4 : 1);
		this->colData.EnforceSize(all_cnt * sizeof(float) * col_step);
		size_t ci = 0;
		float *f = this->colData.As<float>();
		for (uint64_t pi = 0; pi < pl.GetCount(); pi++, ci++) {
			f[ci * 4 + 0] = r;
			f[ci * 4 + 1] = g;
			f[ci * 4 + 2] = b;
			f[ci * 4 + 3] = a;
		}

		// Color.
		const void *cd = p.GetColourData();
		unsigned int cds = p.GetColourDataStride();
		MultiParticleDataCall::Particles::ColourDataType cdt = p.GetColourDataType();

		// Vertex.
		const void *vd = p.GetVertexData();
		unsigned int vds = p.GetVertexDataStride();
		MultiParticleDataCall::Particles::VertexDataType vdt = p.GetVertexDataType();

	}
	return true;
}

