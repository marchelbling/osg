#include <osg/NodeVisitor>
#include <osg/Geometry>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/FileNameUtils>
#include <osgDB/Registry>
#include <osgDB/FileUtils>
#include <sstream>
#include <string>

#include "CArray.h"
#include "GeometryCompressionVisitor.h"


using namespace osg;

class ReaderWriterQTZ : public osgDB::ReaderWriter
{
public:
    ReaderWriterQTZ()
    {
        supportsExtension("qtz","Compress Vertices Attributes Pseudo loader.");
    }

    virtual const char* className() const { return "ReaderWriterQTZ"; }

    //TODO: handle ReaderWriterOptions; see ReaderWriterJSON parseOptions
    virtual osgDB::ReaderWriter::ReadResult readNode(const std::string& file,
                                                     const osgDB::ReaderWriter::Options* options) const
    {
        std::string ext = osgDB::getLowerCaseFileExtension(file);
        if(!acceptsExtension(ext))
          return ReadResult::FILE_NOT_HANDLED;

        // check if file exists
        std::string fileName = osgDB::findDataFile(file, options);
        if(fileName.empty())
            return ReadResult::FILE_NOT_FOUND;

        // get the real format used for serialization
        std::string realFileName = osgDB::getNameLessExtension( fileName );
        if(realFileName.empty())
            return ReadResult::FILE_NOT_HANDLED;

        // get the real format unserializer
        ref_ptr<ReaderWriter> rw = getReaderWriter(realFileName);
        if(rw)
        {
          std::ifstream input(fileName.c_str());
          ReadResult result = rw->readNode(input, options);
          input.close();

          // decompress node using user values saved at compression time
          if(result.success())
          {
            ref_ptr<Node> node = dynamic_cast<Node*>(result.getObject());
            GeometryCompressionVisitor compressorVisitor(false);
            setCompressionParameters(compressorVisitor, options);
            node->accept(compressorVisitor);
            return node.release();
          }
          else
            return result.status();
        }
        return ReadResult::ERROR_IN_READING_FILE;
    }

    virtual osgDB::ReaderWriter::WriteResult writeNode(const Node& node,
                                  const std::string& fileName,
                                  const osgDB::ReaderWriter::Options* options) const
    {
      std::string ext = osgDB::getLowerCaseFileExtension(fileName);
      if (!acceptsExtension(ext))
        return WriteResult::FILE_NOT_HANDLED;

      std::string realFileName = osgDB::getNameLessExtension( fileName );
      if(realFileName.empty())
          return WriteResult::FILE_NOT_HANDLED;

      // store compressed arrays in node attributes
      ref_ptr<Node> compressed_node = compress_node(node, options);

      std::string optionsWithWriter = handleFileWriterOption(options, realFileName);

      // dump compressed node using realFileName extension
      ref_ptr<ReaderWriter> rw = getReaderWriter(realFileName);
      if(rw)
      {
        std::ofstream output(fileName.c_str(), std::ios_base::out | std::ios_base::trunc);
        osg::ref_ptr<osgDB::Options> newOptions = new osgDB::Options(optionsWithWriter);

        WriteResult result = rw->writeNode(*compressed_node, output, newOptions);
        output.close();
        return result;
      }
      else
        return WriteResult::ERROR_IN_WRITING_FILE;
    }


  private:
    ReaderWriter* getReaderWriter(std::string const& fileName) const
    {
      ref_ptr<osgDB::Registry> registry = osgDB::Registry::instance();
      std::string ext = osgDB::getLowerCaseFileExtension(fileName);
      return registry->getReaderWriterForExtension(ext);
    }


    void setCompressionParameters(GeometryCompressionVisitor& compressorVisitor,
                                  osgDB::Options const* options) const
    {
      if(options)
      {
        std::istringstream iss(options->getOptionString());
        std::string opt;

        int attributes = 0,
            bytes = 1,
            mode = 0;

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

          if (pre_equals == "vertex")
              attributes |= GeometryCompressionVisitor::vertex;
          if (pre_equals == "normal")
              attributes |= GeometryCompressionVisitor::normal;
          if (pre_equals == "uv")
              attributes |= GeometryCompressionVisitor::uv;
          if (pre_equals == "color")
              attributes |= GeometryCompressionVisitor::color;
          if (pre_equals == "prediction")
            mode |= CArray::prediction;
          if (pre_equals == "quantization")
          {
            bytes = atoi(post_equals.c_str());
            mode |= CArray::quantization;
          }
        }
        compressorVisitor.setAttributes(attributes);
        compressorVisitor.setBytes(bytes);
        compressorVisitor.setMode(mode);
      }

      return;
    }

    Node* compress_node(Node const& node, osgDB::Options const* options) const
    {
      GeometryCompressionVisitor compressorVisitor(true);

      // parse pseudo-loader options
      setCompressionParameters(compressorVisitor, options);

      Node* clone = dynamic_cast<Node *>(node.clone(osg::CopyOp::DEEP_COPY_ALL));
      clone->accept(compressorVisitor);
      return clone;
    }


     std::string handleFileWriterOption(const osgDB::Options* options,
                                            std::string const& fileName) const
     {
       std::string ext = osgDB::getLowerCaseFileExtension(fileName);
       std::string opt;
       if(options)
         opt = options->getOptionString();

       if(ext == std::string("osgt") && opt.find("Ascii") == std::string::npos)
         opt += "Ascii";
       if(ext == std::string("osgx") && opt.find("XML") == std::string::npos)
         opt += "XML";

       return opt;
     }
};

// Add ourself to the Registry to instantiate the reader/writer.
REGISTER_OSGPLUGIN(qtz, ReaderWriterQTZ)
