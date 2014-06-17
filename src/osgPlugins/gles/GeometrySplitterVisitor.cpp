#include "GeometrySplitterVisitor"


void GeometrySplitterVisitor::apply(osg::Geometry& geometry) {
    GeometryIndexSplitter splitter(_maxIndexValue);

    if (splitter.split(geometry)) {
        unsigned int nbParents = geometry.getNumParents();
        for(unsigned int i = 0 ; i < nbParents ; ++ i) {
            osg::Node* parent = geometry.getParent(i);

            // TODO: Geode will soon be deprecated
            if(parent && parent->asGeode()) {
                osg::Geode* geode = parent->asGeode();
                for (unsigned int j = 0; j < splitter._geometryList.size(); j++) {
                    osg::Geometry* splittedGeometry = splitter._geometryList[j];
                    geode->addDrawable(splittedGeometry);
                }
                geode->removeDrawable(&geometry);
            }
        }

        for (unsigned int j = 0; j < splitter._geometryList.size(); j++) {
            setProcessed(splitter._geometryList[j]);
        }
    }
    setProcessed(geometry);
}
