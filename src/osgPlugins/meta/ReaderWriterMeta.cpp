/* -*-c++-*- OpenSceneGraph - Copyright (C) Cedric Pinson
 *
 * This application is open source and may be redistributed and/or modified
 * freely and without restriction, both in commercial and non commercial
 * applications, as long as this copyright notice is maintained.
 *
 * This application is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
*/

#include <string>
#include <set>
#include <fstream>
#include <sstream>

#include <osg/NodeVisitor>
#include <osg/Texture2D>
#include <osg/Image>
#include <osg/StateSet>
#include <osg/Geode>
#include <osg/ValueObject>
#include <osgDB/ReadFile>
#include <osgDB/ReaderWriter>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/WriteFile>
#include <osgDB/Registry>


class MetadataExtractor : public osg::NodeVisitor
{
public:
    MetadataExtractor() : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
                          _orphanTextureId(0)
    {}

    void applyStateSet(osg::StateSet* ss)
    {
        if (ss) {
            if(_source.empty()) ss->getUserValue("source_tool", _source);

            int textureListSize = ss->getTextureAttributeList().size();
            for (int i = 0 ; i < textureListSize ; ++i) {
                osg::Texture2D* tex = dynamic_cast<osg::Texture2D*>(
                                                      ss->getTextureAttribute(i, osg::StateAttribute::TEXTURE));
                if (tex && tex->getImage()) {
                    osg::Image* image = tex->getImage();
                    std::string fileName = image->getFileName();

                    if(fileName.empty())
                    {
                      fileName = createTextureFileName();
                      image->setFileName(fileName);
                    }

                    if(!osgDB::fileExists(fileName))
                    {
                      // need to dump image on disk
                      osgDB::writeImageFile(*image, fileName);
                      // alter stateset to reference external file
                      image->setWriteHint(osg::Image::EXTERNAL_FILE);
                    }

                    _textures.insert(osgDB::findDataFile( fileName ));
                }
            }
        }
    }

    std::string createTextureFileName()
    {
        std::ostringstream name;
        name << "skfb_texture_extract_" << _orphanTextureId << ".jpg";
        ++ _orphanTextureId;

        return name.str();
    }

    void apply(osg::Geode& node) {
        osg::StateSet* ss = node.getStateSet();
        if (ss)
            applyStateSet(ss);
        for (unsigned int i = 0; i < node.getNumDrawables(); ++i) {
            osg::Drawable* d = node.getDrawable(i);
            if (d && d->getStateSet()) {
                applyStateSet(d->getStateSet());
            }
        }
        traverse(node);
    }

    void apply(osg::Node& node) {
        osg::StateSet* ss = node.getStateSet();
        if (ss)
            applyStateSet(ss);
        traverse(node);
    }

    void dumpMeta() {
        std::ofstream metaFile("model_metadata");
        metaFile << _source << std::endl; // output blank line if no source was found
        for(std::set<std::string>::const_iterator tex = _textures.begin() ;
            tex != _textures.end() ; ++ tex)
            metaFile << *tex << std::endl;
    }


    protected:
        int _orphanTextureId;
        std::set<std::string> _textures;
        std::string _source;
};


class ReaderWriterMeta : public osgDB::ReaderWriter
{
public:
    ReaderWriterMeta()
    {
        supportsExtension("meta","Pseudo-loader to extract model metadata.");
    }

    virtual const char* className() const { return "ReaderWriterMeta"; }


    virtual ReadResult readNode(const std::string& fileName, const osgDB::ReaderWriter::Options* options) const
    {
        std::string ext = osgDB::getLowerCaseFileExtension(fileName);
        if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

        // strip the pseudo-loader extension
        std::string subLocation = osgDB::getNameLessExtension( fileName );
        if ( subLocation.empty() )
            return ReadResult::FILE_NOT_HANDLED;


        // recursively load the subfile.
        osg::ref_ptr<osg::Node> node = osgDB::readNodeFile( subLocation, options );
        if( !node.valid() )
        {
            // propagate the read failure upwards
            osg::notify(osg::WARN) << "Subfile \"" << subLocation << "\" could not be loaded" << std::endl;
            return ReadResult::FILE_NOT_HANDLED;
        }

        MetadataExtractor visitor;
        node->accept(visitor);
        visitor.dumpMeta();
        return node.release();
    }
};


// Add ourself to the Registry to instantiate the reader/writer.
REGISTER_OSGPLUGIN(meta, ReaderWriterMeta)
