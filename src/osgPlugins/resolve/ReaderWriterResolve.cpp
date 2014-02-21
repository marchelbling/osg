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
    typedef std::map<std::string, std::string> resolve_map;
    typedef std::pair<std::string, std::string> resolve_pair;

    ResolveImageFilename(std::string const& path) : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
    {
        _textureMapper = extractTextureMapFromJsonMetaData(path);
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
                    if(!fileName.empty())
                    {
                        resolve_map::const_iterator mappedNameIt = _textureMapper.find(fileName);
                        if(mappedNameIt != _textureMapper.end())
                        {
                            image->setFileName(mappedNameIt->second);
                        }
                        else
                        {
                            std::cout << "Resolve pseudo-loader error while remapping '" << mappedNameIt->first
                                      << "' to '" << mappedNameIt->second <<"'" << std::endl;
                            exit(1); // this should never happen as all
                                     // textures should have been treated previously
                        }
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

    resolve_map extractTextureMapFromJsonMetaData(std::string const& path)
    {
        std::ifstream json(path.c_str());
        std::string content((std::istreambuf_iterator<char>(json)),
                             std::istreambuf_iterator<char>());

        picojson::value v;
        std::string err;
        picojson::parse(v, content.c_str(), content.c_str() + content.size(), &err);


        if (! err.empty()) std::cerr << err << std::endl;

        resolve_map mapping;
        if(v.is<picojson::object>())
        {
            const picojson::object& hash = v.get<picojson::object>();
            picojson::object::const_iterator resolve = hash.find("resolve");
            if(resolve != hash.end())
            {
                if(resolve->second.is<picojson::object>())
                {
                    picojson::object remapping = resolve->second.get<picojson::object>();
                    for(picojson::object::const_iterator remap = remapping.begin() ;
                        remap != remapping.end() ; ++ remap)
                    {
                        std::string key   = remap->first;
                        std::string value = remap->second.to_str();
                        mapping.insert(resolve_pair(key,   value));
                        // To avoid issues if we traverse a same node multiple
                        // times, we map the uuided image to itself. The other
                        // solution would be to tag visited nodes but it's would
                        // be more code for almost the same performance
                        mapping.insert(resolve_pair(value, value));
                    }
                }
            }

        }
        return mapping;
    }

protected:
    resolve_map _textureMapper;
};


class ReaderWriterResolve : public osgDB::ReaderWriter
{
public:
    struct OptionsStruct {
        std::string input;

        OptionsStruct() {
            input = "meta.json";
        }
    };

    ReaderWriterResolve()
    {
        supportsExtension("resolve","Resolve image filename Pseudo loader.");
        supportsOption("input","Path from where metadata json file should be read");
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

        ResolveImageFilename visitor(_options.input);
        node->accept(visitor);
        return node.release();
    }

    ReaderWriterResolve::OptionsStruct parseOptions(const osgDB::ReaderWriter::Options* options) const
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

                if (pre_equals == "input")
                    localOptions.input = post_equals;
            }
        }
        return localOptions;
    }
};


// Add ourself to the Registry to instantiate the reader/writer.
REGISTER_OSGPLUGIN(resolve, ReaderWriterResolve)
