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

#include <vector>

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


class Cleaner : public osg::NodeVisitor
{
public:
    Cleaner() : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
    {}

    void apply(osg::Geode& node) {
        for (unsigned int i = 0; i < node.getNumDrawables(); ++i) {
            osg::Drawable* drawable = node.getDrawable(i);

            if(drawable) {
                apply(*drawable);
            }
        }
    }

    // see #75339392
    void apply(osg::Drawable& drawable) {
        osg::Geometry* geometry = drawable.asGeometry();
        if(!geometry) { return; }

        apply(*geometry);
    }

    void apply(osg::Geometry& geometry) {
        cleanUVs(geometry);
    }

protected:
    void cleanUVs(osg::Geometry& geometry) {
        // clean Vec3f UV into Vec2f
        std::vector< osg::ref_ptr<osg::Array> > textures;
        for(unsigned int i = 0 ; i < geometry.getNumTexCoordArrays() ; ++ i) {
            osg::Array* buffer = geometry.getTexCoordArray(i);
            if(buffer) {
                if(dynamic_cast<osg::Vec2Array*>(buffer)) {
                    textures.push_back(buffer);
                }
                else if(dynamic_cast<osg::Vec3Array*>(buffer)) {
                    osg::Vec3Array* coordinates = dynamic_cast<osg::Vec3Array*>(buffer);
                    osg::Vec2Array* cleanCoordinates = new osg::Vec2Array;
                    for(unsigned int j = 0 ; j < coordinates->size() ; ++ j) {
                        osg::Vec3 v = (*coordinates)[j];
                        cleanCoordinates->push_back(osg::Vec2(v.x(), v.y()));
                    }
                    textures.push_back(cleanCoordinates);
                }
            }
            else
                textures.push_back(0);
        }
        geometry.getTexCoordArrayList().swap(textures);
    }
};


class ReaderWriterCleaner : public osgDB::ReaderWriter
{
public:
    struct OptionsStruct {

        OptionsStruct() {
        }
    };

    ReaderWriterCleaner()
    {
        supportsExtension("cleaner","Pseudo-loader to clean geometries.");
    }

    virtual const char* className() const { return "ReaderWriterCleaner"; }


    virtual ReadResult readNode(const std::string& fileName, const osgDB::ReaderWriter::Options* options) const
    {
        std::string ext = osgDB::getLowerCaseFileExtension(fileName);
        if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

        // strip the pseudo-loader extension
        std::string subLocation = osgDB::getNameLessExtension( fileName );
        if ( subLocation.empty() )
            return ReadResult::FILE_NOT_HANDLED;

        OptionsStruct _options;
        _options = parseOptions(options);

        // recursively load the subfile.
        osg::ref_ptr<osg::Node> node = osgDB::readNodeFile( subLocation, options );
        if( !node.valid() )
        {
            // propagate the read failure upwards
            osg::notify(osg::WARN) << "Subfile \"" << subLocation << "\" could not be loaded" << std::endl;
            return ReadResult::FILE_NOT_HANDLED;
        }

        Cleaner visitor;
        node->accept(visitor);

        return node.release();
    }


    ReaderWriterCleaner::OptionsStruct parseOptions(const osgDB::ReaderWriter::Options* options) const
    {
        OptionsStruct localOptions;

        if (options)
        {
            osg::notify(osg::NOTICE) << "options " << options->getOptionString() << std::endl;
            std::istringstream iss(options->getOptionString());
            std::string opt;
            while (iss >> opt)
            {
                // split opt into pre= and post=
                std::string pre_equals;
                std::string post_equals;

                size_t found = opt.find("=");
                if(found != std::string::npos)
                {
                    pre_equals = opt.substr(0,found);
                    post_equals = opt.substr(found+1);
                }
                else
                    pre_equals = opt;
            }
        }
        return localOptions;
    }
};


// Add ourself to the Registry to instantiate the reader/writer.
REGISTER_OSGPLUGIN(cleaner, ReaderWriterCleaner)
