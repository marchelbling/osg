SET(TARGET_SRC ReaderWriterJSON.cpp WriteVisitor.cpp JSON_Objects.cpp GeometryOperation.cpp Animation.cpp GeometryProcess.cpp UnIndexMeshVisitor.cpp Base64.cpp)

#### end var setup  ###
SET(TARGET_ADDED_LIBRARIES osgAnimation osgUtil osgSim) 
SETUP_PLUGIN(osgjs)
