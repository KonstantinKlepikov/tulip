INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR} ${TulipCoreBuildInclude} ${TulipCoreInclude})

ADD_LIBRARY(Delaunay-${TulipVersion} SHARED DelaunayTriangulation.cpp)
TARGET_LINK_LIBRARIES(Delaunay-${TulipVersion} ${LibTulipCoreName})

ADD_LIBRARY(Voronoi-${TulipVersion} SHARED VoronoiDiagram.cpp)
TARGET_LINK_LIBRARIES(Voronoi-${TulipVersion} ${LibTulipCoreName})

TULIP_INSTALL_PLUGIN(Delaunay-${TulipVersion} ${TulipPluginsInstallDir})
TULIP_INSTALL_PLUGIN(Voronoi-${TulipVersion} ${TulipPluginsInstallDir})
