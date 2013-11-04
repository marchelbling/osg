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

#include <osg/NodeVisitor>
#include <osg/Texture2D>
#include <osg/StateSet>
#include <osg/Geode>
#include <string>
#include <osgDB/ReadFile>
#include <osgDB/ReaderWriter>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/Registry>

class ResolveImageFilename : public osg::NodeVisitor
{
public:
    ResolveImageFilename() : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {}

    void applyStateSet(osg::StateSet* ss) 
    {
        if (ss) {
            for (int i = 0; i < 32; ++i) {
                osg::Texture2D* tex = dynamic_cast<osg::Texture2D*>(ss->getTextureAttribute(i, osg::StateAttribute::TEXTURE));
                if (tex && tex->getImage()) {
                    std::string ss = tex->getImage()->getFileName();
                    if (!ss.empty()) {
                        std::string fileName = osgDB::findDataFile( ss );

                        // special case for dds, we dont support it so try to load png / jpg instead
                        std::string extImage = osgDB::getLowerCaseFileExtension(fileName);
                        if (extImage == "dds" || extImage == "tga" || extImage == "tif" || extImage == "tiff" || extImage == "vtf") {
                            bool foundAlternateFile = false;

                            if (!osgDB::findDataFile(fileName+".jpg").empty()) {
                                fileName += ".jpg";
                                foundAlternateFile = true;
                                osg::notify(osg::NOTICE) << "found " << extImage << " texture, try to use " << fileName << ".jpg instead" << std::endl;

                            } else if (!osgDB::findDataFile(fileName+".png").empty()) {
                                fileName += ".png";
                                foundAlternateFile = true;
                                osg::notify(osg::NOTICE) << "found " << extImage << " texture, try to use " << fileName << ".jpg instead" << std::endl;

                            }
                            if (foundAlternateFile) {
                                osg::notify(osg::NOTICE) << "found " << extImage << " texture, will use " << fileName << " instead" << std::endl;
                            } else {
                                osg::notify(osg::WARN) << "found " << extImage << " texture, and no valid alternate image" << std::endl;
                            }
                        }



                        tex->getImage()->setFileName(fileName);
                    }
                }
            }
        }
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
};


class ReaderWriterResolve : public osgDB::ReaderWriter
{
public:
    ReaderWriterResolve()
    {
        supportsExtension("resolve","Resolve image filename Psuedo loader.");
    }
    
    virtual const char* className() const { return "ReaderWriterResolve"; }


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

        ResolveImageFilename visitor;
        node->accept(visitor);
        return node.release();
    }
};


// Add ourself to the Registry to instantiate the reader/writer.
REGISTER_OSGPLUGIN(resolve, ReaderWriterResolve)
