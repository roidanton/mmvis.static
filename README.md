# mmvis_static
A MegaMol Plugin for detection and visualization of structural processes.

The data is made up of two million particles of a liquid that rapidly expands within 150 time steps since it is surrounded by nothing (vacuum). A scalar field reflects the deepness of the particles inside agglomerations.

Skeletal extractions and contour trees are evaluated for usage and are discarded since the particle data is neither a continuous surface nor a grid nor a mesh. However the work makes usage of the idea of these methods by using extrema, specifically local maxima, to classify the spatial data structure. A so called Fast-Depth algorithm is created.

The steps incorporate

1. analyzing the data structure by creating neighbourhood relations,
2. using them to create clusters,
3. comparing those clusters across two frames and
4. using heuristics to create events out of that comparison.

Finally those events are visualized in the same space as the particles.

The plugin is part of a thesis written at the Faculty of Computer Science, Institute of Software- and Multimediatechnology, Chair of computer graphics and visualization at the TU Dresden, Germany.

The thesis is available in the thesis branch of this repository (WIP until August 10th 2015).

# MegaMol
From the [products website](http://go.visus.uni-stuttgart.de/megamol/): MegaMol™ is a visualization middleware used to visualize point-based molecular datasets.
