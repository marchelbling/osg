SET(TARGET_SRC
    ReaderWriterGLES.cpp
    GeometryOperation.cpp
    GeometryProcess.cpp
    UnIndexMeshVisitor.cpp)

#### end var setup  ###
SET(TARGET_ADDED_LIBRARIES
    osgUtil)
SETUP_PLUGIN(gles)
