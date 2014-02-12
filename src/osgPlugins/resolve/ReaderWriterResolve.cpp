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

#include <fstream>
#include <map>
#include <string>
#include <iterator>

#include <osg/NodeVisitor>
#include <osg/Texture2D>
#include <osg/StateSet>
#include <osg/Geode>
#include <osgDB/ReadFile>
#include <osgDB/ReaderWriter>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/Registry>

#include "picojson.h"

class ResolveImageFilename : public osg::NodeVisitor
{
public:
    ResolveImageFilename() : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
    {
        _textureMapper = extractTextureMapFromJsonMetadata();
    }

    void applyStateSet(osg::StateSet* ss)
    {
        if (ss) {
            int textureListSize = ss->getTextureAttributeList().size();
            for (int i = 0 ; i < textureListSize ; ++i) {
                osg::Texture2D* tex = dynamic_cast<osg::Texture2D*>(
                                                      ss->getTextureAttribute(i, osg::StateAttribute::TEXTURE));
                if (tex && tex->getImage()) {
                    osg::Image* image = tex->getImage();
                    std::string fileName = image->getFileName();
                    std::map<std::string, std::string>::const_iterator mappedNameIt = _textureMapper.find(fileName);
                    if(mappedNameIt != _textureMapper.end())
                    {
                        image->setFileName(mappedNameIt->second);
                    }
                    else
                    {
                        exit(1); // this should never happen as all
                                 // textures should have been treated
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

    std::map<std::string, std::string> extractTextureMapFromJsonMetadata()
    {
        std::ifstream json("meta.json");
        std::istream_iterator<char> input(json);

        picojson::value v;
        std::string err;
        picojson::parse(v, input, std::istream_iterator<char>(), &err);

        if (! err.empty()) std::cerr << err << std::endl;

        std::map<std::string, std::string> mapping;
        if(v.is<picojson::object>())
        {
            const picojson::object& hash = v.get<picojson::object>();
            picojson::object::const_iterator texture_values = hash.find("textures");
            if(texture_values != hash.end())
            {
                if(texture_values->second.is<picojson::array>())
                {
                    picojson::array textures = texture_values->second.get<picojson::array>();
                    for(picojson::array::const_iterator texture = textures.begin() ;
                        texture != textures.end() ; ++ texture)
                    {
                        std::string texture_name = texture->to_str();
                        picojson::object::const_iterator tex = hash.find(texture_name);
                        if(tex != hash.end())
                        {
                            std::string key   = tex->first;
                            std::string value = tex->second.to_str();
                            mapping.insert(std::pair<std::string, std::string>(key, value));
                        }
                    }
                }
            }

        }
        return mapping;
    }

    protected:
        std::map<std::string, std::string> _textureMapper;
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
