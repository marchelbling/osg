#include <osg/MatrixTransform>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/ReaderWriter>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgUtil/Optimizer>
#include <stdlib.h>

// makes possibles comparison between statesets
class StateSetComparator : public std::binary_function<osg::StateSet*, osg::StateSet*, bool>
{
    public :
        bool operator()(osg::StateSet* a, osg::StateSet* b) const
        {
            return a->compare(*b) < 0;
        }
};

class MaterialMerger : public osg::NodeVisitor
{
protected:
    // geometries sorted by stateset
	std::map<osg::StateSet*, std::vector<osg::Geometry*>, StateSetComparator> nodes;

public:
    MaterialMerger()
        : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
	{
	}

    void apply(osg::Node& node)
    {
        // presume every data is static for osgUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS
        node.setDataVariance(osg::Object::STATIC);

        traverse(node);
    }

    void apply(osg::Geode& node)
    {
        // for each geometry of each geode
        for (unsigned int i = 0; i < node.getNumDrawables(); ++i)
        {
            osg::Drawable* d = node.getDrawable(i);
            if (d && d->getStateSet())
            {
                // map each geometry, sorted by stateset
                osg::StateSet* ss = d->getStateSet();

                nodes[ss].push_back(d->asGeometry());
            }
        }

        traverse(node);
    }

    osg::ref_ptr<osg::Node> optimize()
    {
        osg::Group* root = new osg::Group();
        osgUtil::Optimizer optimizer;

        for(std::map<osg::StateSet*, std::vector<osg::Geometry*>, StateSetComparator>::iterator it = nodes.begin(); it != nodes.end(); it++)
        {
            // one geode per stateset
            osg::Group* group = new osg::Group();
            group->setStateSet(it->first);
            osg::Geode* geode = new osg::Geode();

            std::vector<osg::Geometry*>& geometries = it->second;
            for(std::vector<osg::Geometry*>::iterator it2 = geometries.begin(); it2 != geometries.end(); it2++)
            {
                osg::Geometry* geometry = *it2;

                osg::MatrixList matrixList = geometry->getWorldMatrices();
                for (unsigned int i = 0; i < matrixList.size(); i++)
                {
                    // get the complete transformation on the geometry and recreate a subgraph of this
                    osg::Geometry* geometry2 = dynamic_cast<osg::Geometry*>(geometry->clone(osg::CopyOp::DEEP_COPY_PRIMITIVES));
                    osg::Geode* geode2 = new osg::Geode();
                    geode2->addDrawable(geometry2);
                    osg::MatrixTransform* transform = new osg::MatrixTransform(matrixList[i]);
                    transform->setDataVariance(osg::Object::STATIC);
                    transform->addChild(geode2);

                    // flatten the geometry using the previous transformation
                    optimizer.optimize(transform, osgUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS);

                    geode->addDrawable(geometry2);
                }
            }
            group->addChild(geode);

            // merge geometries into the one geode
            optimizer.optimize(group, osgUtil::Optimizer::MERGE_GEOMETRY);

            root->addChild(group);
        }

        return root;
    }
};

class ReaderWriterMaterialMerger : public osgDB::ReaderWriter
{
public:
    ReaderWriterMaterialMerger()
    {
        supportsExtension("materialmerger", "Optimizes a scene graph by grouping and merging by materials.");
    }
    
    virtual const char* className() const
	{
		return "ReaderWriterMaterialMerger";
	}

    virtual ReadResult readNode(const std::string& file, const osgDB::ReaderWriter::Options* options) const
    {
        // check if the pseudo loader supports the extension
        std::string ext = osgDB::getLowerCaseFileExtension(file);
        if (!acceptsExtension(ext))
            return ReadResult::FILE_NOT_HANDLED;

        // strip the pseudo extension from the filename
        std::string realName = osgDB::getNameLessExtension(file);

        // check if real file exists
        std::string fileName = osgDB::findDataFile(realName, options);
        if (realName.empty())
            return ReadResult::FILE_NOT_FOUND;

        // read file
        OSG_INFO << "realName = \"" << realName << "\"" << std::endl;
        osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(realName, options);
        if (!node.valid())
        {
            osg::notify(osg::WARN) << "File \"" << fileName << "\" could not be loaded" << std::endl;
            return ReadResult::FILE_NOT_HANDLED;
        }

        MaterialMerger visitor;
        node->accept(visitor);
        osg::ref_ptr<osg::Node> newNode = visitor.optimize();

        return newNode.release();
    }
};

// Add ourself to the Registry to instantiate the reader/writer.
REGISTER_OSGPLUGIN(materialmerger, ReaderWriterMaterialMerger)

/*EOF*/

