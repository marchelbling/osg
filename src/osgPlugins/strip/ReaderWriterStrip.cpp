/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
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

#include <osg/Notify>

#include <osgDB/ReaderWriter>
#include <osgDB/FileNameUtils>
#include <osgDB/Registry>
#include <osgDB/ReadFile>

#include "ColorStripVisitor.h"

#define EXTENSION_NAME "strip"


///////////////////////////////////////////////////////////////////////////

/**
 * An OSG reader plugin for the ".strip" pseudo-loader, which colors each
 * strip with a random color.
 *
 * Usage: <modelfile.ext>.strip
 *
 * example: osgviewer cow.osg.strip
 */

class ReaderWriterStrip : public osgDB::ReaderWriter
{
public:
    ReaderWriterStrip()
    {
        supportsExtension(EXTENSION_NAME,"Strip pseudo loader");
    }

    virtual const char* className() const { return "strip pseudo-loader"; }

    virtual ReadResult readNode(const std::string& fileName, const osgDB::ReaderWriter::Options* options) const
    {
        std::string ext = osgDB::getLowerCaseFileExtension(fileName);
        if( !acceptsExtension(ext) )
            return ReadResult::FILE_NOT_HANDLED;

        OSG_INFO << "ReaderWriterStrip( \"" << fileName << "\" )" << std::endl;

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

        ColorStripVisitor colorStripVisitor;
        node->accept(colorStripVisitor);
        colorStripVisitor.splitStrips();
        return node.release();
    }
};


// Add ourself to the Registry to instantiate the reader/writer.
REGISTER_OSGPLUGIN(strip, ReaderWriterStrip)

/*EOF*/

