#include "OpenGLESGeometryOptimizer"

#include "AnimationVisitor"
#include "BindPerVertexVisitor"
#include "DetachPrimitiveVisitor"
#include "DrawArrayVisitor"
#include "GeometrySplitterVisitor"
#include "IndexMeshVisitor"
#include "PreTransformVisitor"
#include "TangentSpaceVisitor"
#include "TriangleStripVisitor"
#include "UnIndexMeshVisitor"
#include "WireframeVisitor"


osg::Node* OpenGLESGeometryOptimizer::optimize(osg::Node& node) {
    osg::ref_ptr<osg::Node> model = osg::clone(&node);

    // animation: create regular Geometry if RigGeometry
    AnimationVisitor anim;
    model->accept(anim);

    // wireframe
    if (!_wireframe.empty()) {
        WireframeVisitor wireframe(_wireframe == std::string("inline"));
        model->accept(wireframe);
    }

    // bind per vertex
    BindPerVertexVisitor bindpervertex;
    model->accept(bindpervertex);

    // index (merge exact duplicates + uses simple triangles & lines i.e. no strip/fan/loop)
    IndexMeshVisitor indexer;
    model->accept(indexer);

    // tangent space
    if (_generateTangentSpace) {
        TangentSpaceVisitor tangent(_tangentUnit);
        model->accept(tangent);
    }

    if(!_useDrawArray) {
        // split geometries having some primitive index > _maxIndexValue
        GeometrySplitterVisitor splitter(_maxIndexValue);
        model->accept(splitter);
    }

    // strip
    if(!_disableTriStrip) {
        TriangleStripVisitor strip(_triStripCacheSize, _triStripMinSize, _useDrawArray, !_disableMergeTriStrip);
        model->accept(strip);
    }

    if(_useDrawArray) {
        // drawelements to drawarrays
        DrawArrayVisitor drawarray;
        model->accept(drawarray);
    }
    else if(!_disablePreTransform) {
        // pre-transform
        PreTransformVisitor preTransform;
        model->accept(preTransform);
    }

    // detach wireframe
    DetachPrimitiveVisitor detacher("wireframe", false, _wireframe == std::string("inline"));
    model->accept(detacher);

    return model.release();
}
