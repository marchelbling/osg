//    copyright: 'Cedric Pinson cedric@plopbyte.com'
#include <osg/Image>
#include <osg/Notify>
#include <osg/Geode>
#include <osg/GL>
#include <osg/Version>
#include <osg/Endian>
#include <osg/Projection>
#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>

#include <osgUtil/UpdateVisitor>
#include <osgDB/ReaderWriter>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>

#include <osgDB/Registry>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>

#include <osgAnimation/UpdateMatrixTransform>
#include <osgAnimation/AnimationManagerBase>
#include <osgAnimation/BasicAnimationManager>

#include <sstream>
#include <cassert>
#include <map>

#include "UnIndexMeshVisitor"
#include "TangentSpace"
#include "GeometryOperation"

using namespace osg;


// the idea is to create true Geometry if skeleton with RigGeometry
struct FakeUpdateVisitor : public osgUtil::UpdateVisitor
{
    FakeUpdateVisitor() {
        setFrameStamp(new osg::FrameStamp());
    }
};

class ReaderWriterGLES : public osgDB::ReaderWriter
{
public:

     struct OptionsStruct {
         bool enableWireframe;
         bool generateTangentSpace;
         int tangentSpaceTextureUnit;
         bool disableTriStrip;
         bool disableMergeTriStrip;
         unsigned int triStripCacheSize;
         unsigned int triStripMinSize;
         bool useDrawArray;
         bool disableIndex;

         OptionsStruct() {
             enableWireframe = false;
             generateTangentSpace = false;
             tangentSpaceTextureUnit = 0;
             disableTriStrip = false;
             disableMergeTriStrip = false;
             triStripCacheSize = 16;
             triStripMinSize = 2;
             useDrawArray = false;
             disableIndex = false;
         }
    };


    ReaderWriterGLES()
    {
        supportsExtension("gles","OpenGL ES optimized format");

        supportsOption("enableWireframe","create a wireframe geometry for each triangles geometry");
        supportsOption("generateTangentSpace","Build tangent space to each geometry");
        supportsOption("tangentSpaceTextureUnit=<unit>","Specify on wich texture unit normal map is");
        supportsOption("triStripCacheSize=<int>","set the cache size when doing tristrip");
        supportsOption("triStripMinSize=<int>","set the minimum accepted length for a strip");
        supportsOption("disableMergeTriStrip","disable the merge of all tristrip into one");
        supportsOption("disableTriStrip","disable generation of tristrip");
        supportsOption("useDrawArray","prefer drawArray instead of drawelement with split of geometry");
        supportsOption("disableIndex","Do not index the geometry");
    }

    virtual const char* className() const { return "GLES Optimizer"; }

    virtual osg::Node* optimizeModel(const Node& node, const OptionsStruct& options) const
    {
        // process regular model
        osg::ref_ptr<osg::Node> model = osg::clone(&node);
        FakeUpdateVisitor fakeUpdateVisitor;
        model->accept(fakeUpdateVisitor);

        GeometryWireframeVisitor wireframer;
        if (options.enableWireframe)
            model->accept(wireframer);

        OpenGLESGeometryOptimizerVisitor visitor;

        // generated in model when indexed
        if (options.generateTangentSpace) {
            visitor.setTexCoordChannelForTangentSpace(options.tangentSpaceTextureUnit);
        }

        visitor.setUseDrawArray(options.useDrawArray);
        visitor.setTripStripCacheSize(options.triStripCacheSize);
        visitor.setTripStripMinSize(options.triStripMinSize);
        visitor.setDisableTriStrip(options.disableTriStrip);
        visitor.setDisableMergeTriStrip(options.disableMergeTriStrip);
        model->accept(visitor);

        osg::notify(osg::NOTICE) << "SceneNbIndexedTriangles:" << visitor._sceneNbTriangles << std::endl;
        osg::notify(osg::NOTICE) << "SceneNbIndexedVertexes:" << visitor._sceneNbVertexes << std::endl;

        if (options.disableIndex)
        {
            UnIndexMeshVisitor unindex;
            model->accept(unindex);
        }

        return model.release();
    }


    virtual ReadResult readNode(const std::string& fileName, const osgDB::ReaderWriter::Options* options) const
    {
        std::string ext = osgDB::getLowerCaseFileExtension(fileName);
        if( !acceptsExtension(ext) )
            return ReadResult::FILE_NOT_HANDLED;

        OSG_INFO << "ReaderWriterGLES( \"" << fileName << "\" )" << std::endl;

        // strip the pseudo-loader extension
        std::string realName = osgDB::getNameLessExtension( fileName );

        if (realName.empty())
            return ReadResult::FILE_NOT_HANDLED;

        // recursively load the subfile.
        osg::ref_ptr<osg::Node> node = osgDB::readNodeFile( realName, options );
        if( !node )
        {
            // propagate the read failure upwards
            OSG_WARN << "Subfile \"" << realName << "\" could not be loaded" << std::endl;
            return ReadResult::FILE_NOT_HANDLED;
        }

        OptionsStruct _options;
        _options = parseOptions(options);
        node = optimizeModel(*node, _options);

        return node.release();
    }


    ReaderWriterGLES::OptionsStruct parseOptions(const osgDB::ReaderWriter::Options* options) const
    {
        OptionsStruct localOptions;

        if (options)
        {
            osg::notify(NOTICE) << "options " << options->getOptionString() << std::endl;
            std::istringstream iss(options->getOptionString());
            std::string opt;
            while (iss >> opt)
            {
                // split opt into pre= and post=
                std::string pre_equals;
                std::string post_equals;

                size_t found = opt.find("=");
                if(found!=std::string::npos)
                {
                    pre_equals = opt.substr(0,found);
                    post_equals = opt.substr(found+1);
                }
                else
                {
                    pre_equals = opt;
                }

                if (pre_equals == "enableWireframe")
                {
                    localOptions.enableWireframe = true;
                }
                if (pre_equals == "useDrawArray")
                {
                    localOptions.useDrawArray = true;
                }
                if (pre_equals == "disableMergeTriStrip")
                {
                    localOptions.disableMergeTriStrip = true;
                }
                if (pre_equals == "disableTriStrip")
                {
                    localOptions.disableTriStrip = true;
                }
                if (pre_equals == "generateTangentSpace")
                {
                    localOptions.generateTangentSpace = true;
                }
                if (pre_equals == "disableIndex")
                {
                    localOptions.disableIndex = true;
                }

                if (post_equals.length()>0)
                {
                    if (pre_equals == "tangentSpaceTextureUnit") {
                        localOptions.tangentSpaceTextureUnit = atoi(post_equals.c_str());
                    }
                    if (pre_equals == "triStripCacheSize") {
                        localOptions.triStripCacheSize = atoi(post_equals.c_str());
                    }
                    if (pre_equals == "triStripMinSize") {
                        localOptions.triStripMinSize = atoi(post_equals.c_str());
                    }
                }
            }
        }
        return localOptions;
    }
protected:

};

// now register with Registry to instantiate the above
// reader/writer.
REGISTER_OSGPLUGIN(gles, ReaderWriterGLES)
