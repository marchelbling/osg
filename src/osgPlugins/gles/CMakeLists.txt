SET(TARGET_SRC
    ReaderWriterGLES.cpp
    OpenGLESGeometryOptimizer.cpp
    GeometrySplitterVisitor.cpp
    TriangleStripVisitor.cpp
    IndexMeshVisitor.cpp
    UnIndexMeshVisitor.cpp)

#### end var setup  ###
SET(TARGET_ADDED_LIBRARIES
    osgUtil)
SETUP_PLUGIN(gles)
