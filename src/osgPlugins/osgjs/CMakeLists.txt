SET(TARGET_SRC
  ReaderWriterJSON.cpp
  WriteVisitor.cpp
  JSON_Objects.cpp
  Base64.cpp
  Animation.cpp)

#### end var setup  ###
SET(TARGET_ADDED_LIBRARIES
    osgAnimation
    osgSim
    osgUtil)

SETUP_PLUGIN(osgjs)
