/*  -*-c++-*- 
 *  Copyright (C) 2010 Cedric Pinson <cedric.pinson@plopbyte.net>
 */

#include "JSON_Objects"
#include <osgDB/WriteFile>
#include <osgDB/FileNameUtils>
#include <osg/Material>
#include <osg/BlendFunc>
#include <osg/BlendColor>
#include <osg/Texture>
#include <osg/CullFace>
#include <osg/Texture2D>
#include <osg/Texture1D>
#include <osg/Image>
#include <sstream>
#include "WriteVisitor"

int JSONObjectBase::level = 0;
unsigned int JSONObject::uniqueID = 0;

std::string JSONObjectBase::indent()
{
    std::string str;
    for (int i = 0; i < JSONObjectBase::level; ++i) {
        str += "  ";
    }
    return str;
}


void JSONMatrix::write(std::ostream& str, WriteVisitor& visitor)
{
    str << "[ ";
    for (unsigned int i = 0; i < _array.size(); i++) {
        _array[i]->write(str, visitor);
        if (i != _array.size() -1)
            str << ", ";
    }
    str <<  " ]";
}


void JSONNode::write(std::ostream& str, WriteVisitor& visitor)
{
    std::vector<std::string> order;
    order.push_back("UniqueID");
    order.push_back("Name");
    order.push_back("TargetName");
    order.push_back("Matrix");
    order.push_back("UpdateCallbacks");
    order.push_back("StateSet");
    writeOrder(str, order, visitor);
}

JSONObject::JSONObject(const unsigned int id)
{
    _uniqueID = id;
    _maps["UniqueID"] = new JSONValue<unsigned int>(id);
}

JSONObject::JSONObject() 
{
    _uniqueID = -1;
}

void JSONObject::addUniqueID() 
{
    _uniqueID = JSONObject::uniqueID++;
    _maps["UniqueID"] = new JSONValue<unsigned int>(_uniqueID);
}

void JSONObject::addChild(const std::string& type, JSONObject* child)
{
    if (!getMaps()["Children"])
        getMaps()["Children"] = new JSONArray;

    JSONObject* jsonObject = new JSONObject();
    jsonObject->getMaps()[type] = child;
    getMaps()["Children"]->asArray()->getArray().push_back(jsonObject);
}

static void writeEntry(std::ostream& str, const std::string& key, JSONObject::JSONMap& map, WriteVisitor& visitor)
{
    if (key.empty())
        return;

    if ( map.find(key) != map.end() &&
         map[ key ].valid() ) {
        
        str << JSONObjectBase::indent() << '"' << key << '"' << ": ";
        map[ key ]->write(str, visitor);
        map.erase(key);

        if (!map.empty()) {
            str << ", ";
            str << "\n";
        }
    }
}
void JSONObject::writeOrder(std::ostream& str, const std::vector<std::string>& order, WriteVisitor& visitor)
{
    str << "{" << std::endl;
    JSONObjectBase::level++;
    for (unsigned int i = 0; i < order.size(); i++) {
        writeEntry(str, order[i], _maps, visitor);
    }

    while(!_maps.empty()) {
        std::string key = _maps.begin()->first;
        writeEntry(str, key, _maps, visitor);
    }

    JSONObjectBase::level--;
    str << std::endl << JSONObjectBase::indent() << "}";
}

void JSONObject::write(std::ostream& str, WriteVisitor& visitor)
{
    OrderList defaultOrder;
    defaultOrder.push_back("UniqueID");
    defaultOrder.push_back("Name");
    defaultOrder.push_back("TargetName");
    writeOrder(str, defaultOrder, visitor);
}


std::pair<unsigned int,unsigned int> JSONVertexArray::writeMergeData(const osg::Array* array, WriteVisitor &visitor)
{
    if (!visitor._mergeBinaryFile.is_open()) {
        std::string filename = visitor.getBinaryFilename();
        visitor._mergeBinaryFile.open(filename.c_str(), std::ios::binary );
    }
    unsigned int offset = visitor._mergeBinaryFile.tellp();
    const char* b = static_cast<const char*>(array->getDataPointer());
    visitor._mergeBinaryFile.write(b, array->getTotalDataSize());
    unsigned int fsize = visitor._mergeBinaryFile.tellp(); 

    // pad to 4 bytes
    unsigned int diff = fsize - (fsize/4) * 4;
    if (diff > 0) {
        char* buffer = "FF00FF00";
        visitor._mergeBinaryFile.write(b, diff);
    }
    return std::pair<unsigned int, unsigned int>(offset, fsize-offset);
}

void JSONVertexArray::write(std::ostream& str, WriteVisitor& visitor)
{
    bool _useExternalBinaryArray = visitor._useExternalBinaryArray;
    bool _mergeAllBinaryFiles = visitor._mergeAllBinaryFiles;
    std::string basename = visitor._baseName;

    addUniqueID();

    std::stringstream url;
    if (visitor._useExternalBinaryArray) {
        if (visitor._mergeAllBinaryFiles)
            url << visitor.getBinaryFilename();
        else
            url << basename << "_" << _uniqueID << ".bin";
    }

    std::string type;
        
    osg::ref_ptr<const osg::Array> array = _arrayData;

    switch (array->getType()) {
    case osg::Array::FloatArrayType:
    case osg::Array::Vec2ArrayType:
    case osg::Array::Vec3ArrayType:
    case osg::Array::Vec4ArrayType:
        type = "Float32Array";
        break;
    case osg::Array::Vec4ubArrayType:
    {
        osg::ref_ptr<osg::Vec4Array> converted = new osg::Vec4Array;
        converted->reserve(array->getNumElements());
            
        const osg::Vec4ubArray* a = dynamic_cast<const osg::Vec4ubArray*>(array.get());
        for (unsigned int i = 0; i < a->getNumElements(); ++i) {
            converted->push_back(osg::Vec4( (*a)[i][0]/255.0,
                                            (*a)[i][1]/255.0,
                                            (*a)[i][2]/255.0,
                                            (*a)[i][3]/255.0));
        }
        array = converted;
        type = "Float32Array";
    }
    break;
    case osg::Array::UShortArrayType:
        type = "Uint16Array";
        break;
    default:
        osg::notify(osg::WARN) << "Array of type " << array->getType() << " not supported" << std::endl;
        break;
    }

    str << "{ " << std::endl;
    JSONObjectBase::level++;
    str << JSONObjectBase::indent() << "\"" << type << "\"" << ": { " << std::endl;
    JSONObjectBase::level++;
    if (_useExternalBinaryArray) {
        str << JSONObjectBase::indent() << "\"File\": \"" << osgDB::getSimpleFileName(url.str()) << "\","<< std::endl;
    } else {
        if (array->getNumElements() == 0) {
            str << JSONObjectBase::indent() << "\"Elements\": [ ],";

        } else {

            switch (array->getType()) {
            case osg::Array::FloatArrayType:
            case osg::Array::Vec2ArrayType:
            case osg::Array::Vec3ArrayType:
            case osg::Array::Vec4ArrayType:
            {
                const float* a = static_cast<const float*>(array->getDataPointer());
                unsigned int size = array->getNumElements() * array->getDataSize();
                writeInlineArrayReal<float>(str, size, a);
            }
            break;
            case osg::Array::DoubleArrayType:
            case osg::Array::Vec2dArrayType:
            case osg::Array::Vec3dArrayType:
            case osg::Array::Vec4dArrayType:
            {
                const double* a = static_cast<const double*>(array->getDataPointer());
                unsigned int size = array->getNumElements() * array->getDataSize();
                writeInlineArrayReal<double>(str, size, a);
            }
            break;
            case osg::Array::ByteArrayType:
            case osg::Array::Vec2bArrayType:
            case osg::Array::Vec3bArrayType:
            case osg::Array::Vec4bArrayType:
            {
                const char* a = static_cast<const char*>(array->getDataPointer());
                unsigned int size = array->getNumElements() * array->getDataSize();
                writeInlineArray<char>(str, size, a);
            }
            break;
            case osg::Array::UByteArrayType:
            case osg::Array::Vec4ubArrayType:
            {
                const unsigned char* a = static_cast<const unsigned char*>(array->getDataPointer());
                unsigned int size = array->getNumElements() * array->getDataSize();
                writeInlineArray<unsigned char>(str, size, a);
            }
            break;
            case osg::Array::UShortArrayType:
            {
                const unsigned short* a = static_cast<const unsigned short*>(array->getDataPointer());
                unsigned int size = array->getNumElements() * array->getDataSize();
                writeInlineArray<unsigned short>(str, size, a);
            }
            break;
            case osg::Array::ShortArrayType:
            case osg::Array::Vec2sArrayType:
            case osg::Array::Vec3sArrayType:
            case osg::Array::Vec4sArrayType:
            {
                const short* a = static_cast<const short*>(array->getDataPointer());
                unsigned int size = array->getNumElements() * array->getDataSize();
                writeInlineArray<short>(str, size, a);
            }
            break;
            case osg::Array::IntArrayType:
            {
                const int* a = static_cast<const int*>(array->getDataPointer());
                unsigned int size = array->getNumElements() * array->getDataSize();
                writeInlineArray<int>(str, size, a);
            }
            break;
            case osg::Array::UIntArrayType:
            {
                const unsigned int* a = static_cast<const unsigned int*>(array->getDataPointer());
                unsigned int size = array->getNumElements() * array->getDataSize();
                writeInlineArray<unsigned int>(str, size, a);
            }
            break;

            }
        }
    }

    str << JSONObjectBase::indent() << "\"Size\": " << array->getNumElements();
    if (_useExternalBinaryArray) {
        str << "," << std::endl;
    } else {
        str << std::endl;
    }

    if (_useExternalBinaryArray) {
        unsigned int size;
        if (_mergeAllBinaryFiles) {
            std::pair<unsigned int, unsigned int> result;
            result = writeMergeData(array.get(), visitor);
            unsigned int offset = result.first;
            size = result.second;
            str << JSONObjectBase::indent() << "\"Offset\": " << offset << std::endl;
        } else {
            size = writeData(array.get(), url.str());
            str << JSONObjectBase::indent() << "\"Offset\": " << 0 << std::endl;
        }
        std::stringstream b;
        osg::notify(osg::NOTICE) << "TypedArray " << type << " " << url.str() << " ";
        if (size/1024.0 < 1.0) {
            osg::notify(osg::NOTICE) << size << " bytes" << std::endl;
        } else if (size/(1024.0*1024.0) < 1.0) { 
            osg::notify(osg::NOTICE) << size/1024.0 << " kb" << std::endl;
        } else {
            osg::notify(osg::NOTICE) << size/(1024.0*1024.0) << " mb" << std::endl;
        }

    }

    JSONObjectBase::level--;
    str << JSONObjectBase::indent() << "}" << std::endl;
    JSONObjectBase::level--;

    str << JSONObjectBase::indent() << "}";
}


JSONVec4Array::JSONVec4Array(const osg::Vec4& v) : JSONVec3Array()
{
    for (int i = 0; i < 4; ++i) {
        _array.push_back(new JSONValue<float>(v[i]));
    }
}

JSONVec5Array::JSONVec5Array(const Vec5& v) : JSONVec3Array()
{
    for (int i = 0; i < 5; ++i) {
        _array.push_back(new JSONValue<float>(v[i]));
    }
}

JSONVec2Array::JSONVec2Array(const osg::Vec2& v) : JSONVec3Array()
{
    for (int i = 0; i < 2; ++i) {
        _array.push_back(new JSONValue<float>(v[i]));
    }
}

JSONVec3Array::JSONVec3Array(const osg::Vec3& v)
{
    for (int i = 0; i < 3; ++i) {
        _array.push_back(new JSONValue<float>(v[i]));
    }
}

void JSONVec3Array::write(std::ostream& str,WriteVisitor& visitor)
{
    str << "[ ";
    for (unsigned int i = 0; i < _array.size(); i++) {
        if (_array[i].valid()) { 
            _array[i]->write(str, visitor);
        } else {
            str << "undefined";
        }
        if (i != _array.size()-1)
            str << ", ";
    }
    str << "]";
}

void JSONKeyframes::write(std::ostream& str,WriteVisitor& visitor)
{
    JSONObjectBase::level++;
    str << "[" << std::endl << JSONObjectBase::indent();
    for (unsigned int i = 0; i < _array.size(); i++) {
        if (_array[i].valid()) {
            _array[i]->write(str, visitor);
        } else {
            str << "undefined";
        }
        if (i != _array.size() -1) {
            str << ",";
            str << "\n" << JSONObjectBase::indent();
        }
    }
    str << " ]";
    JSONObjectBase::level--;
}


void JSONArray::write(std::ostream& str,WriteVisitor& visitor)
{
    str << "[ ";
    for (unsigned int i = 0; i < _array.size(); i++) {
        if (_array[i].valid()) {
            _array[i]->write(str, visitor);
        } else {
            str << "undefined";
        }
        if (i != _array.size() -1) {
            str << ",";
            str << "\n" << JSONObjectBase::indent();
        }
    }
    //str << std::endl << JSONObjectBase::indent() << "]";
    str << " ]";
}






JSONObject* getDrawMode(GLenum mode)
{
    JSONObject* result = 0;
    switch (mode) {
    case GL_POINTS:
        result = new JSONValue<std::string>("POINTS");
        break;
    case GL_LINES:
        result = new JSONValue<std::string>("LINES");
        break;
    case GL_LINE_LOOP:
        result = new JSONValue<std::string>("LINE_LOOP");
        break;
    case GL_LINE_STRIP:
        result = new JSONValue<std::string>("LINE_STRIP");
        break;
    case GL_TRIANGLES:
        result = new JSONValue<std::string>("TRIANGLES");
        break;
    case GL_POLYGON:
        result = new JSONValue<std::string>("TRIANGLE_FAN");
        break;
    case GL_QUAD_STRIP:
    case GL_TRIANGLE_STRIP:
        result = new JSONValue<std::string>("TRIANGLE_STRIP");
        break;
    case GL_TRIANGLE_FAN:
        result = new JSONValue<std::string>("TRIANGLE_FAN");
        break;
    case GL_QUADS:
        osg::notify(osg::WARN) << "exporting quads will not be able to work on opengl es" << std::endl;
        break;
    }
    return result;
}

JSONDrawArray::JSONDrawArray(osg::DrawArrays& array) 
{
    getMaps()["First"] = new JSONValue<int>(array.getFirst());
    getMaps()["Count"] = new JSONValue<int>(array.getCount());
    getMaps()["Mode"] = getDrawMode(array.getMode());
}


JSONDrawArrayLengths::JSONDrawArrayLengths(osg::DrawArrayLengths& array)
{
    getMaps()["First"] = new JSONValue<int>(array.getFirst());
    getMaps()["Mode"] = getDrawMode(array.getMode());

    JSONArray* jsonArray = new JSONArray;
    for (unsigned int i = 0; i < array.size(); i++) {
        jsonArray->getArray().push_back(new JSONValue<int>(array[i]));
    }
    getMaps()["ArrayLengths"] = jsonArray;
}
