// simple pseudo loader to get scene informations

#include <iterator>
#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>

#include "../meta/picojson.h"
#include "StatsVisitor"


class ReaderWriter : public osgDB::ReaderWriter
{
public:

    ReaderWriter()
    {
        supportsExtension("stats","OpenSceneGraph report stats of the scene");
    }

    virtual const char* className() const { return "scene stats pseudo loader"; }

    virtual ReadResult readNode(const std::string& fileName, const osgDB::ReaderWriter::Options* options) const
    {
        std::string ext = osgDB::getLowerCaseFileExtension(fileName);
        if( !acceptsExtension(ext) )
            return ReadResult::FILE_NOT_HANDLED;

        OSG_INFO << "ReaderWriterSceneStats( \"" << fileName << "\" )" << std::endl;

        // strip the pseudo-loader extension
        std::string subName = osgDB::getNameLessExtension( fileName );

        if( subName.empty())
        {
            OSG_WARN << "Missing subfilename for stats pseudo-loader" << std::endl;
            return ReadResult::FILE_NOT_HANDLED;
        }

        std::string jsonFile = parseJsonOption(options);
        OSG_INFO << " json = \"" << jsonFile << "\"" << std::endl;
        OSG_INFO << " subName = \"" << subName << "\"" << std::endl;

        // recursively load the subfile.
        osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(subName, options);
        if( !node )
        {
            // propagate the read failure upwards
            OSG_WARN << "Subfile \"" << subName << "\" could not be loaded" << std::endl;
            return ReadResult::FILE_NOT_HANDLED;
        }

        // make stats
        StatsVisitor sv;
        node->accept(sv);

        // display stats
        sv.print(osg::notify(osg::NOTICE));

        // write to meta.json file
        if (!jsonFile.empty()) {

            // read meta.json
            picojson::value data;
            picojson::object obj;
            {
                std::ifstream fin(jsonFile.c_str());
                if (fin) {
                    std::string err = picojson::parse(data, fin);
                    obj = data.get<picojson::object>();

                    if (!err.empty()) {
                        OSG_WARN << "Error parsing JSON : " << err << std::endl;
                        return ReadResult::FILE_NOT_HANDLED;
                    }
                }
            }

            // add our stats
            obj["stats"] = sv.toJSON();

            // save to meta.json
            std::ofstream fout(jsonFile.c_str());
            if (!fout) {
                OSG_WARN << "Error writing JSON file : " << fileName << std::endl;
                return ReadResult::FILE_NOT_HANDLED;
            }
            fout << picojson::value(obj).serialize();
            fout.close();
        }

        return node.release();
    }

    std::string parseJsonOption(const osgDB::ReaderWriter::Options* options) const
    {
        if (options)
        {
            return options->getOptionString();
        }

        return "";
    }
};

// now register with Registry to instantiate the above
// reader/writer.
REGISTER_OSGPLUGIN(scene_stats, ReaderWriter)
