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
#include "TangentSpace"
#include "JSON_Objects"

#include "GeometryOperation"
#include "Animation"
#include "WriteVisitor"
#include "UnIndexMeshVisitor"



using namespace osg;



// the idea is to create true Geometry if skeleton with RigGeometry
struct FakeUpdateVisitor : public osgUtil::UpdateVisitor
{
    FakeUpdateVisitor() {
        setFrameStamp(new osg::FrameStamp());
    }
};

class ReaderWriterJSON : public osgDB::ReaderWriter
{
public:

     struct OptionsStruct {
         bool generateTangentSpace;
         int tangentSpaceTextureUnit;
         bool disableTriStrip;
         bool disableMergeTriStrip;
         int resizeTextureUpToPowerOf2;
         int triStripCacheSize;
         bool useDrawArray;
         bool enableWireframe;
         bool useExternalBinaryArray;
         bool mergeAllBinaryFiles;
         bool inlineImages;

         OptionsStruct() {
             generateTangentSpace = false;
             tangentSpaceTextureUnit = 0;
             disableTriStrip = false;
             disableMergeTriStrip = false;
             resizeTextureUpToPowerOf2 = 0;
             triStripCacheSize = 16;
             useDrawArray = false;
             enableWireframe = false;
             useExternalBinaryArray = false;
             mergeAllBinaryFiles = false;
             inlineImages = false;
         }
    };


    ReaderWriterJSON()
    {
        supportsExtension("osgjs","OpenSceneGraph Javascript implementation format");
        supportsExtension("unindex","Unindex the geometry");
        supportsOption("generateTangentSpace","Build tangent space to each geometry");
        supportsOption("tangentSpaceTextureUnit=<unit>","Specify on wich texture unit normal map is");
        supportsOption("triStripCacheSize=<int>","set the cache size when doing tristrip");
        supportsOption("disableMergeTriStrip","disable the merge of all tristrip into one");
        supportsOption("resizeTextureUpToPowerOf2=<int>","Specify the maximum power of 2 allowed dimension for texture. Using 0 will disable the functionality and no image resizing will occur.");
        supportsOption("disableTriStrip","disable generation of tristrip");
        supportsOption("useDrawArray","prefer drawArray instead of drawelement with split of geometry");
        supportsOption("enableWireframe","create a wireframe geometry for each triangles geometry");
        supportsOption("useExternalBinaryArray","create binary files for vertex arrays");
        supportsOption("mergeAllBinaryFiles","merge all binary files into one to avoid multi request on a server");
        supportsOption("inlineImages","insert base64 encoded images instead of referring to them");
    }

    virtual const char* className() const { return "OSGJS json Writer"; }

    virtual ReadResult readNode(const std::string& fileName, const Options* options) const;

    virtual WriteResult writeNode(const Node& node, const std::string& fileName, const osgDB::ReaderWriter::Options* options) const
    {
        std::string ext = osgDB::getFileExtension(fileName);
        if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

        {
            std::ofstream fout(fileName.c_str());
            if (fout)
            {
                OptionsStruct _options;
                _options = parseOptions(options);
                WriteResult res = writeNodeModel(node, fout, osgDB::getNameLessExtension(fileName), _options);
                fout.close();
                return res;
            }
        }
        return WriteResult("Unable to open file for output");
    }

    virtual WriteResult writeNode(const Node& node, std::ostream& fout, const osgDB::ReaderWriter::Options* options) const
    {
        if (!fout) {
            return WriteResult("Unable to write to output stream");
        }

        OptionsStruct _options;
        _options = parseOptions(options);
        return writeNodeModel(node, fout, "stream", _options);
    }

    virtual WriteResult writeNodeModel(const Node& node, std::ostream& fout, const std::string& basename, const OptionsStruct& options) const
    {
        // process regular model
        osg::ref_ptr<osg::Node> model = osg::clone(&node);
        //osgDB::writeNodeFile(*model, "cloned.osg");
        FakeUpdateVisitor fakeUpdateVisitor;
        model->accept(fakeUpdateVisitor);

        GeometryWireframeVisitor wireframer;
        if (options.enableWireframe) {
            model->accept(wireframer);
        }


//        StatsVisitor sceneStats;
//        model->accept(sceneStats);
//        sceneStats.dump();

        OpenGLESGeometryOptimizerVisitor visitor;

        // generated in model when indexed
        if (options.generateTangentSpace) {
            visitor.setTexCoordChannelForTangentSpace(options.tangentSpaceTextureUnit);
        }

        visitor.setUseDrawArray(options.useDrawArray);
        visitor.setTripStripCacheSize(options.triStripCacheSize);
        visitor.setDisableTriStrip(options.disableTriStrip);
        visitor.setDisableMergeTriStrip(options.disableMergeTriStrip);
        model->accept(visitor);

        if (!options.enableWireframe) {
            osg::notify(osg::NOTICE) << "SceneNbTriangles:" << visitor._sceneNbTriangles << std::endl;
            osg::notify(osg::NOTICE) << "SceneNbVertexes:" << visitor._sceneNbVertexes << std::endl;
        }

        WriteVisitor writer;
        try {
            //osgDB::writeNodeFile(*model, "/tmp/debug_osgjs.osg");
            writer.setBaseName(basename);
            writer.useExternalBinaryArray(options.useExternalBinaryArray);
            writer.mergeAllBinaryFiles(options.mergeAllBinaryFiles);
            writer.inlineImages(options.inlineImages);
            writer.setMaxTextureDimension(options.resizeTextureUpToPowerOf2);
            model->accept(writer);
            if (writer._root.valid()) {
                writer.write(fout);
                return WriteResult::FILE_SAVED;
            }
        } catch (...) {
            osg::notify(osg::FATAL) << "can't save osgjs file" << std::endl;
            return WriteResult("Unable to write to output stream");
        }
        return WriteResult("Unable to write to output stream");
    }

    ReaderWriterJSON::OptionsStruct parseOptions(const osgDB::ReaderWriter::Options* options) const
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

                if (pre_equals == "useDrawArray")
                {
                    localOptions.useDrawArray = true;
                }
                if (pre_equals == "enableWireframe")
                {
                    localOptions.enableWireframe = true;
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
                if (pre_equals == "useExternalBinaryArray")
                {
                    localOptions.useExternalBinaryArray = true;
                }
                if (pre_equals == "mergeAllBinaryFiles")
                {
                    localOptions.mergeAllBinaryFiles = true;
                }

                if (pre_equals == "inlineImages")
                {
                    localOptions.inlineImages = true;
                }

                if (post_equals.length() > 0)
                {
                    int value = atoi(post_equals.c_str());
                    if (pre_equals == "tangentSpaceTextureUnit") {
                        localOptions.tangentSpaceTextureUnit = value;
                    }
                    if (pre_equals == "triStripCacheSize") {
                        localOptions.triStripCacheSize = value;
                    }
                    if (pre_equals == "resizeTextureUpToPowerOf2") {
                        localOptions.resizeTextureUpToPowerOf2 = osg::Image::computeNearestPowerOfTwo(value);
                    }
                }
            }
        }
        return localOptions;
    }
protected:

};

osgDB::ReaderWriter::ReadResult ReaderWriterJSON::readNode(const std::string& file, const Options* options) const
{
    std::string ext = osgDB::getLowerCaseFileExtension(file);
    if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

    // strip the pseudo-loader extension
    std::string fileName = osgDB::getNameLessExtension( file );

    fileName = osgDB::findDataFile( fileName, options );
    if (fileName.empty()) return ReadResult::FILE_NOT_FOUND;

    osg::Node *node = osgDB::readNodeFile( fileName, options );
    if (!node)
        return ReadResult::FILE_NOT_HANDLED;


    if (ext == "unindex") {
        UnIndexMeshVisitor unindex;
        node->accept(unindex);
        return node;
    }

    return ReadResult::FILE_NOT_HANDLED;
}

// now register with Registry to instantiate the above
// reader/writer.
REGISTER_OSGPLUGIN(osgjs, ReaderWriterJSON)
