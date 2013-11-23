/*
* Copyright 2006 Sony Computer Entertainment Inc.
*
* Licensed under the SCEA Shared Source License, Version 1.0 (the "License"); you may not use this
* file except in compliance with the License. You may obtain a copy of the License at:
* http://research.scea.com/scea_shared_source_license.html
*
* Unless required by applicable law or agreed to in writing, software distributed under the License
* is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
* implied. See the License for the specific language governing permissions and limitations under the
* License.
*/

#include "daeReader.h"

#include <dae.h>
#include <dae/domAny.h>
#include <dom/domCOLLADA.h>
#include <dom/domInstanceWithExtra.h>
#include <dom/domProfile_COMMON.h>
#include <dom/domConstants.h>

#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>
#include <osg/UserDataContainer>
#include <osg/ValueObject>

#include <osg/BlendColor>
#include <osg/BlendFunc>
#include <osg/Texture2D>
#include <osg/TexEnv>
#include <osg/LightModel>

#include <osgDB/ReadFile>

using namespace osgDAE;

daeReader::Options::Options() :
	strictTransparency(false),
	precisionHint(0),
	usePredefinedTextureUnits(true),
	tessellateMode(TESSELLATE_POLYGONS_AS_TRIFAN)   // Use old tessellation behaviour as default
{
}

daeReader::daeReader(DAE *dae_, const Options * pluginOptions) :
	_dae(dae_),
	_rootNode(NULL),
	_document(NULL),
	_visualScene(NULL),
	_numlights(0),
	_currentInstance_effect(NULL),
	_currentEffect(NULL),
	_authoringTool(UNKNOWN),
	_invertTransparency(false),
	_pluginOptions(pluginOptions ? *pluginOptions : Options()),
	_assetUnitName("meter"),
	_assetUnitMeter(1.0),
	_assetUp_axis(UPAXISTYPE_Y_UP)
{
}

daeReader::~daeReader()
{
}

bool daeReader::processDocument( const std::string& fileURI)
{

	daeElement *colladaElement;


	daeInt count, result;

	if (!_document)
	{
		OSG_WARN << "Load failed in COLLADA DOM" << std::endl;
		return false;
	}
	OSG_INFO << "URI loaded: " << fileURI << std::endl;

	if ( !_document->getScene() || !_document->getScene()->getInstance_visual_scene() )
	{
		OSG_WARN << "No scene found!" << std::endl;
		return false;
	}

	if (_document->getAsset())
	{
		const domAsset::domContributor_Array& ContributorArray = _document->getAsset()->getContributor_array();
		size_t NumberOfContributors = ContributorArray.getCount();
		size_t CurrentContributor;
		for (CurrentContributor = 0; CurrentContributor < NumberOfContributors; CurrentContributor++)
		{
			if (ContributorArray[CurrentContributor]->getAuthoring_tool())
			{
				const char szBlender[] = "Blender";
				const char szDazStudio[] = "DAZ|Studio";
				const char szSketchup[] = "Google SketchUp";
				const char szFbx[] = "FBX";
				const char szMaya[] = "Maya";
				const char szPhotoshop[] = "Adobe Photoshop";

				xsString Tool = ContributorArray[CurrentContributor]->getAuthoring_tool()->getValue();
				_authoringToolName = Tool;

				if (strncmp(Tool, szBlender, strlen(szBlender)) == 0)
					_authoringTool = BLENDER;
				else if (strncmp(Tool, szDazStudio, strlen(szDazStudio)) == 0)
					_authoringTool = DAZ_STUDIO;
				else if (strncmp(Tool, szFbx, strlen(szFbx)) == 0)
					_authoringTool = FBX_CONVERTER;
				else if (strncmp(Tool, szSketchup, strlen(szSketchup)) == 0)
					_authoringTool = GOOGLE_SKETCHUP;
				else if (strncmp(Tool, szMaya, strlen(szMaya)) == 0)
					_authoringTool = MAYA;
				else if (strncmp(Tool, szPhotoshop, strlen(szPhotoshop)) == 0)
					_authoringTool = ADOBE_PHOTOSHOP;
			}
		}
		if (_document->getAsset()->getUnit())
		{
			if (NULL != _document->getAsset()->getUnit()->getName())
				_assetUnitName = std::string(_document->getAsset()->getUnit()->getName());
			if (0 != _document->getAsset()->getUnit()->getMeter())
				_assetUnitMeter = _document->getAsset()->getUnit()->getMeter();
		}
		if (_document->getAsset()->getUp_axis())
			_assetUp_axis = _document->getAsset()->getUp_axis()->getValue();
	}

	domInstanceWithExtra *ivs = _document->getScene()->getInstance_visual_scene();
	_visualScene = daeSafeCast< domVisual_scene >( getElementFromURI( ivs->getUrl() ) );
	if ( _visualScene == NULL )
	{
		OSG_WARN << "Unable to locate visual scene!" << std::endl;
		return false;
	}

	if (daeDatabase* database = _dae->getDatabase())
	{
		_invertTransparency = findInvertTransparency(database);

		// build a std::map for lookup if Group or PositionAttitudeTransform should be created,
		// i.e, make it easy to check if a instance_rigid_body targets a visual node
		domInstance_rigid_body *pDomInstanceRigidBody;
		count = database->getElementCount(NULL, COLLADA_TYPE_INSTANCE_RIGID_BODY, NULL);
		for (int i=0; i<count; i++)
		{
			result = database->getElement(&colladaElement, i, NULL, COLLADA_TYPE_INSTANCE_RIGID_BODY);

			if (result == DAE_OK)
			{
				pDomInstanceRigidBody = daeSafeCast<domInstance_rigid_body>(colladaElement);
				if (pDomInstanceRigidBody)
				{
					domNode *node = daeSafeCast<domNode>(pDomInstanceRigidBody->getTarget().getElement());
					if (node && node->getId())
					{
						_targetMap[ std::string(node->getId()) ] = true;
					}
				}
			}
		}

		// Build a map of elements that are targetted by animations
		count = database->getElementCount(NULL, COLLADA_TYPE_CHANNEL, NULL);
		for (int i=0; i<count; i++)
		{
			result = database->getElement(&colladaElement, i, NULL, COLLADA_TYPE_CHANNEL);

			if (result == DAE_OK)
			{
				domChannel* pDomChannel = daeSafeCast<domChannel>(colladaElement);
				if (pDomChannel)
				{
					std::string target = pDomChannel->getTarget();
					size_t openparenthesis = target.find_first_of('(');
					if (openparenthesis != std::string::npos) target.erase(openparenthesis);
					daeSIDResolver resolver(pDomChannel, target.c_str());
					daeElement *pDaeElement = resolver.getElement();
					if (pDaeElement)
					{
						_daeElementDomChannelMap.insert(daeElementDomChannelMap::value_type(pDaeElement, pDomChannel));
					}
					else
					{
						OSG_WARN << "Could not locate <channel> target "  << pDomChannel->getTarget()<< std::endl;
					}
				}
			}
		}

		// Find all nodes that are used as bones. Note that while many files
		// identify nodes with type="JOINT", some don't do this, while others
		// identify every node as a joint, making it meaningless.
		std::vector<domInstance_controller*> instanceControllers;
		database->typeLookup(instanceControllers);
		for (size_t i = 0; i < instanceControllers.size(); ++i)
		{
			domInstance_controller* pInstanceController = instanceControllers[i];

			domController *pDomController = daeSafeCast<domController>(getElementFromURI(pInstanceController->getUrl()));
			if (!pDomController)
			{
				OSG_WARN << "Failed to locate controller " << pInstanceController->getUrl().getURI() << std::endl;
				continue;
			}

			const domInstance_controller::domSkeleton_Array& domSkeletonURIs = pInstanceController->getSkeleton_array();
			std::vector<daeElement*> searchIn;

			for (size_t i = 0; i < domSkeletonURIs.getCount(); ++i)
			{
				if (daeElement* el = getElementFromURI(domSkeletonURIs[i]->getValue()))
				{
					searchIn.push_back(el);
					if (domNode* pJoint = daeSafeCast<domNode>(el))
					{
						_jointSet.insert(pJoint);
					}
				}
			}

			if (searchIn.empty())
			{
				searchIn.push_back(_visualScene);
			}

			const domSkin* pSkin = pDomController->getSkin();
			if (!pSkin) continue;
			const domSkin::domJoints* pJoints = pSkin->getJoints();
			if (!pJoints) continue;
			const domInputLocal_Array& inputURIs = pJoints->getInput_array();

			domSource* pDomJointsSource = NULL;
			for (size_t i=0; i < inputURIs.getCount(); i++)
			{
				if (!strcmp(inputURIs[i]->getSemantic(), COMMON_PROFILE_INPUT_JOINT))
				{
					pDomJointsSource = daeSafeCast<domSource>(getElementFromURI(inputURIs[i]->getSource()));
					if (!pDomJointsSource)
					{
						OSG_WARN << "Could not find skin joints source '" << inputURIs[i]->getSource().getURI() << "'" <<std::endl;
					}
				}
			}

			if (!pDomJointsSource)
			{
			}
			else if (domIDREF_array* pDomIDREFs = pDomJointsSource->getIDREF_array())
			{
				for (size_t i = 0; i < pDomIDREFs->getCount(); ++i)
				{
					if (domNode* pJoint = daeSafeCast<domNode>(getElementFromIDRef(pDomIDREFs->getValue().get(i))))
					{
						_jointSet.insert(pJoint);
					}
				}
			}
			else if (domName_array* pDomNames = pDomJointsSource->getName_array())
			{
				for (size_t i = 0; i < pDomNames->getCount(); ++i)
				{
					daeString target = pDomNames->getValue().get(i);
					for (size_t j = 0; j < searchIn.size(); ++j)
					{
						daeSIDResolver resolver(searchIn[j], target);
						if (domNode* pJoint = daeSafeCast<domNode>(resolver.getElement()))
						{
							_jointSet.insert(pJoint);
						}
					}
				}
			}
		}
	}

	// Build the actual scene graph based on the visual scene
	_rootNode = processVisualScene( _visualScene );

	osgAnimation::BasicAnimationManager* pOsgAnimationManager = processAnimationLibraries(_document);
	if (pOsgAnimationManager)
	{
		_rootNode->addUpdateCallback(pOsgAnimationManager);
	}

	return true;
}

void daeReader::clearCaches ()
{
	_geometryMap.clear();
	_materialMap.clear();
	_materialMap2.clear();
}

bool daeReader::convert( std::istream& fin )
{
	clearCaches();

	// set fileURI to null device
	const std::string fileURI("from std::istream");

	// get the size of the file and rewind
	fin.seekg(0, std::ios::end);
	std::streampos length = fin.tellg();
	fin.seekg(0, std::ios::beg);

	// use a vector as buffer and read from stream
	std::vector<char> buffer(length);
	fin.read(&buffer[0], length);

	_document = _dae->openFromMemory(fileURI, &buffer[0]);

	return processDocument (fileURI);
}

bool daeReader::convert( const std::string &fileURI )
{
	clearCaches();

	_document = _dae->open(fileURI);

	return processDocument (fileURI);
}

void daeReader::addChild(osg::Group* group, osg::Node* node)
{
	if (dynamic_cast<osgAnimation::Bone*>(node))
	{
		unsigned index = 0;
		while (index < group->getNumChildren() &&
			dynamic_cast<osgAnimation::Bone*>(group->getChild(index)))
		{
			++index;
		}
		group->insertChild(index, node);
	}
	else
	{
		group->addChild(node);
	}
}

osg::Group* daeReader::turnZUp()
{
	osg::PositionAttitudeTransform* pat = NULL;

	// If not Z axis up we need to rotate scene to bring the Z axis up
	if (_assetUp_axis != UPAXISTYPE_Z_UP)
	{
		pat = new osg::PositionAttitudeTransform();
		if (_assetUp_axis == UPAXISTYPE_Y_UP)
		{
			pat->setAttitude(osg::Quat(osg::inDegrees(90.0f), osg::Vec3(1.0f,0.0f,0.0f)));
		}
		else //(m_AssetUp_axis == UPAXISTYPE_X_UP)
		{
			pat->setAttitude(osg::Quat(osg::inDegrees(90.0f), osg::Vec3(0.0f,1.0f,0.0f)));
		}
	}

	_assetUp_axis = UPAXISTYPE_Z_UP;
	return pat;
}

osg::Group* daeReader::processVisualScene( domVisual_scene *scene )
{
	osg::Group *retVal;
	_rootStateSet = new osg::StateSet();

	unsigned int nbVisualSceneGroup=scene->getNode_array().getCount();
	if (nbVisualSceneGroup==0)
	{
		OSG_WARN << "No visual scene group found !" << std::endl;
		retVal = new osg::Group();
		retVal->setName("Empty Collada scene");
	}
	else
	{
		retVal = turnZUp();

		if (!retVal)
		{
			retVal = new osg::Group;
		}

		_skinInstanceControllers.clear();

		const domNode_Array& node_array = scene->getNode_array();
		for (size_t i = 0; i < node_array.getCount(); i++)
		{
			if (osg::Node* node = processNode(node_array[i], false))
			{
				addChild(retVal, node);
			}
		}
		// once all material and stateset collected.
		{
			domMaterialStateSetMap::iterator iter = _materialMap.begin();
			while (iter != _materialMap.end() )
			{
				// Reuse material
				osg::StateSet* ss = iter->second.get();
				domMaterial*const dm = iter->first;
				saveMaterialToStateSetMetaData(dm, ss);

				{
					const domExtra_Array& ExtraArray = dm->getExtra_array();
					size_t NumberOfExtras = ExtraArray.getCount();
					size_t CurrentExtra;
					for (CurrentExtra = 0; CurrentExtra < NumberOfExtras; CurrentExtra++)
					{
						const domTechnique_Array& TechniqueArray = ExtraArray[CurrentExtra]->getTechnique_array();
						size_t NumberOfTechniques = TechniqueArray.getCount();
						size_t CurrentTechnique;
						for (CurrentTechnique = 0; CurrentTechnique < NumberOfTechniques; CurrentTechnique++)
						{
							OSG_WARN << "Unsupported Profile: " << TechniqueArray[CurrentTechnique]->getProfile() << std::endl;
						}
					}
				}

				++iter;
			}
		}
		processSkins();

		if (retVal->getName().empty())
		{
			if (retVal->getNumChildren())
			{
				retVal->setName("Collada visual scene group");
			}
			else
			{
				retVal->setName("Empty Collada scene (import failure)");
			}
		}
	}
	retVal->setStateSet(_rootStateSet.get());

	return retVal;
}

//TODO: remove useful debug code
void printDomAny( domAny *anyElement, unsigned int indent = 0 )
{
	char *indentStr = new char[ indent + 1 ];
	indentStr[ 0 ] = '\0';
	for( unsigned int i = 0; i < indent; ++i )
	{
		strcat( indentStr, "\t" );
	}
	osg::notify(osg::NOTICE) << indentStr << "Element: " << anyElement->getElementName() <<  std::endl;
	unsigned int numAttrs = anyElement->getAttributeCount();
	for ( unsigned int currAttr = 0; currAttr < numAttrs; ++currAttr )
	{
		osg::notify(osg::NOTICE) << indentStr << "\tAttribute: "<< anyElement->getAttributeName( currAttr ) << " has value: " << anyElement->getAttributeValue( currAttr )  << std::endl;
	}
	unsigned int numChildren = anyElement->getContents().getCount();
	for ( unsigned int currChild = 0; currChild < numChildren; ++currChild )
	{
		if ( anyElement->getContents()[currChild]->getElementType() == COLLADA_TYPE::ANY )
		{
			domAny *child = (domAny*)(daeElement*)anyElement->getContents()[currChild];
			printDomAny( child );
		}
	}
	if ( anyElement->getValue() != NULL )
	{
		osg::notify(osg::NOTICE) <<  indentStr << "Value: " <<  anyElement->getValue()   << std::endl;
	}
	delete []indentStr;
}

//TODO: find a way to list texture not used but listed in collada.
// technique texture gives a "sampler"
// "sampler" links to  a "surface" 
// "surface" links to  a "init_from" image
// "image" links to  a "init_from"  texture file path
void getSamplerSource(daeDatabase* database, std::string &samplerId, std::string &imageId) 
{
	domElement* param;
	// sampler to surface
	std::string surfaceId;
	unsigned int count = database->getElementCount(NULL, "source", NULL);
	for ( unsigned int imageIdx = 0; imageIdx < count; ++imageIdx )
	{
		daeInt error = database->getElement((daeElement**)&param, imageIdx,   NULL, "source", NULL);
		domElement* surfaceEl = param->getParentElement();
		std::string sid = surfaceEl->getParentElement()->getAttribute("sid");
		if ( strcmp( sid.c_str(), samplerId.c_str() ) == 0 )
		{
			//for ( unsigned int imageChildIdx = 0; imageChildIdx < param->getChildren().getCount(); ++imageChildIdx )
			{

				// daeElement * el = param->getChildren()[imageChildIdx];
				// if ( strcmp( el->getTypeName(), "init_from" ) == 0 )
				{
					//el->getTypeName();
					std::string path = param->getCharData();
					osg::notify(osg::NOTICE) <<   "Sampler Surface: " <<  path   << std::endl;
					surfaceId = path;
					break;
				}
			}
		}
	}
	if (surfaceId.empty())
		return;

	// surface to init_from
	std::string init_from;
	unsigned int count2 = database->getElementCount(NULL, "profile_COMMON", NULL);
	for ( unsigned int imageIdx = 0; imageIdx < count2; ++imageIdx )
	{
		daeInt error = database->getElement((daeElement**)&param, imageIdx, NULL, "profile_COMMON", NULL);
		for ( unsigned int imageChildIdx = 0; imageChildIdx < param->getChildren().getCount(); ++imageChildIdx )
		{
			domElement* surfaceEl = param->getChildren()[imageChildIdx];
			std::string sid = surfaceEl->getAttribute("sid");
			if (strcmp( sid.c_str(), surfaceId.c_str() ) == 0)
			{
				osg::notify(osg::NOTICE) <<    surfaceEl->getTypeName()   << std::endl;
				osg::notify(osg::NOTICE) <<    surfaceEl->getElementName()   << std::endl;
				for ( unsigned int imageChildIdx2 = 0; imageChildIdx2 < surfaceEl->getChildren().getCount(); ++imageChildIdx2 )
				{
					daeElement * el = surfaceEl->getChildren()[imageChildIdx2];
					if ( strcmp( el->getElementName(), "surface" ) == 0 )
					{						
						for ( unsigned int imageChildIdx3 = 0; imageChildIdx3 < el->getChildren().getCount(); ++imageChildIdx3 )
						{
							daeElement * elChild = el->getChildren()[imageChildIdx3];
							if ( strcmp( elChild->getElementName(), "init_from" ) == 0 )
							{
								std::string path = elChild->getCharData();
								osg::notify(osg::NOTICE) <<   "Init From: " <<  path   << std::endl;
								init_from = path;
								imageId = init_from;
								return;
							}
						}
					}
				}
			}
		}
	}
}
osg::Texture2D *daeReader::getTextureNotColladed(daeDatabase* database, std::string &samplerId) 
{
	std::string imageId;
	getSamplerSource(database, samplerId, imageId);

	std::string texturePath;
	domLibrary_images* images;
	daeInt error = database->getElement((daeElement**)&images, 0, NULL, COLLADA_TYPE_LIBRARY_IMAGES, NULL);
	for ( unsigned int imageIdx = 0; imageIdx < images->getImage_array().getCount(); ++imageIdx )
	{
		domImage *image = images->getImage_array()[imageIdx];
		std::string pathComplete = this->processImagePath(image);
		if ( strcmp( image->getName(), imageId.c_str() ) == 0 )
		{
			osg::notify(osg::NOTICE) <<   "Image Path: " <<  pathComplete   << std::endl;
			osg::Image* ibl_mage = osgDB::readImageFile ( pathComplete ); 
			if (ibl_mage)
			{
				osg::Texture2D *tex = new osg::Texture2D();
				tex->setImage(ibl_mage);
				return tex;
			}
		}
	}
	return 0;
}
/* when collada dom doesn't recognize an image/sampler/surface as binded... have to search it by hand*/
int getTexUnit(osg::StateSet* stateset, const std::string &fileName)
{
	const unsigned int tuListSize = stateset->getTextureAttributeList().size();
	for(unsigned int i=0;i<tuListSize;++i)
	{
		osg::Texture* texture = dynamic_cast<osg::Texture*>(stateset->getTextureAttribute(i,osg::StateAttribute::TEXTURE));
		if (texture)
		{
			osg::Image *img = texture->getImage(0);
			if ( img && strcmp( img->getFileName().c_str(), fileName.c_str() ) == 0 )
			{
				osg::notify(osg::NOTICE) <<   "Texture: " <<  img->getFileName()   <<  " => Texunit: " << i << std::endl;
				return i;			
			}
		}
	}    
	return -1;
}
void daeReader::saveMetadataMap(osg::StateSet* stateset, const xsNCName &texPath, unsigned int unit_reserved, unsigned int unit, const std::string &prefix, const std::string &midfix)
{    
	daeString texName = daeString(texPath);
	osg::Texture2D* tex =  getTexture(texName, (TextureUnitUsage) unit_reserved);

	if (tex)
	{
		std::stringstream ss;
		std::stringstream ssMeta;

		int tunit = getTexUnit(stateset, tex->getImage()->getFileName());
		ss << (tunit != -1 ? tunit : unit);
		ssMeta << prefix << "_" << midfix << "_" << "unit";
		stateset->setUserValue(ssMeta.str(), ss.str());
	}
}
//TODO: fukk everything should og into daeReader::processProfileCOMMON
// but that means death by merging&patching update with master/main osg...
void daeReader::saveMaterialToStateSetMetaData(domMaterial*const material, osg::StateSet* stateset) 
{
	std::string s = "collada";
	stateset->setUserValue("source", s);    
	stateset->setUserValue("source_tool", _authoringToolName);

	unsigned int unit = 0;

	const bool usePredefTexUnit = _pluginOptions.usePredefinedTextureUnits;

	domInstance_effect * _currentInstance_effect = material->getInstance_effect();
	domEffect *effect = daeSafeCast< domEffect >( getElementFromURI( _currentInstance_effect->getUrl() ) );
	if (effect)
	{
		domFx_profile_abstract_Array &arr = effect->getFx_profile_abstract_array();
		for ( size_t i = 0; i < arr.getCount(); i++ )
		{
			domFx_profile_abstract *pa = daeSafeCast< domFx_profile_abstract > ( arr[i] );
			domProfile_COMMON *pc = daeSafeCast< domProfile_COMMON >( arr[i] );
			if (pc != NULL )
			{
				domProfile_COMMON::domTechnique *teq = pc->getTechnique();

				/*
				domProfile_COMMON::domTechnique::domConstant *c = teq->getConstant();
				*/
				domProfile_COMMON::domTechnique::domBlinn *b = teq->getBlinn();				
				if (b)
				{

					// ambient
					if(b->getAmbient())
					{
						if(b->getAmbient()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = b->getAmbient()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("blinn_ambient_color", ss.str());
						}
						if (b->getAmbient()->getTexture())
						{
							saveMetadataMap(stateset, b->getAmbient()->getTexture()->getTexture(), 
								AMBIENT_OCCLUSION_UNIT,  (usePredefTexUnit ? AMBIENT_OCCLUSION_UNIT : unit++), "blinn", "ambient");
						}
					}

					// diffuse
					if(b->getDiffuse())
					{
						if(b->getDiffuse()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = b->getDiffuse()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("blinn_diffuse_color", ss.str());
						}
						if (b->getDiffuse()->getTexture())
						{
							saveMetadataMap(stateset, b->getDiffuse()->getTexture()->getTexture(), 
								MAIN_TEXTURE_UNIT, (usePredefTexUnit ? MAIN_TEXTURE_UNIT : unit++), "blinn", "diffuse");

						}
					}

					// emissive
					if(b->getEmission())
					{
						if(b->getEmission()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = b->getEmission()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("blinn_emission_color", ss.str());
						}
						if (b->getEmission()->getTexture())
						{
							saveMetadataMap(stateset, b->getEmission()->getTexture()->getTexture(), 
								ILLUMINATION_MAP_UNIT,  (usePredefTexUnit ? ILLUMINATION_MAP_UNIT : unit++), "blinn", "emission");                          
						}
					}

					// specular
					if(b->getSpecular())
					{
						if(b->getSpecular()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = b->getSpecular()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("blinn_specular_color", ss.str());
						}
						if (b->getSpecular()->getTexture())
						{
							saveMetadataMap(stateset, b->getSpecular()->getTexture()->getTexture(), 
								SPECULAR_MAP_UNIT,  (usePredefTexUnit ? SPECULAR_MAP_UNIT : unit++), "blinn", "specular");                           

						}
					}
					// specular exponent
					if(b->getShininess())
					{
						if(b->getShininess() != NULL && b->getShininess()->getFloat() != NULL )
						{
							std::stringstream ss;
							domFloat f = b->getShininess()->getFloat()->getValue();
							ss << f ;
							stateset->setUserValue("blinn_shininess_amount", ss.str());
						}
					}

					// Reflectivity
					if(b->getReflective())
					{
						if(b->getReflective()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = b->getReflective()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("blinn_reflective_color", ss.str());
						}
						if (b->getReflective()->getTexture())
						{
							saveMetadataMap(stateset, b->getReflective()->getTexture()->getTexture(), 
								REFLECTIVE_MAP_UNIT,  (usePredefTexUnit ? REFLECTIVE_MAP_UNIT : unit++), "blinn", "reflective"); 
						}
					}
					if(b->getReflectivity())
					{
						if(b->getReflectivity() != NULL && b->getReflectivity()->getFloat() != NULL )
						{
							std::stringstream ss;
							domFloat f = b->getReflectivity()->getFloat()->getValue();
							ss << f ;
							stateset->setUserValue("blinn_reflective_amount", ss.str());
						}
					}

					// translucency
					// TODO: read furthermore  "Determining Transparency" in 1.4.1 spec				
					if(b->getTransparent())
					{
						if(b->getTransparent() != NULL && b->getTransparency() != NULL && b->getTransparency()->getFloat() != NULL )
						{
							std::stringstream ss;
							domFloat f = b->getTransparency()->getFloat()->getValue();
							ss << f;
							stateset->setUserValue("blinn_transparency_amount", ss.str());
						}
						if (b->getTransparent()->getTexture()) 
						{
							saveMetadataMap(stateset, b->getTransparent()->getTexture()->getTexture(), 
								TRANSPARENCY_MAP_UNIT,  (usePredefTexUnit ? TRANSPARENCY_MAP_UNIT : unit++), "blinn", "transparency");
						}
					}
				}
				domProfile_COMMON::domTechnique::domPhong *p = teq->getPhong();			
				if (p)
				{
					unit = 0;
					// ambient
					if(p->getAmbient())
					{
						if(p->getAmbient()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = p->getAmbient()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("phong_ambient_color", ss.str());
						}
						if (p->getAmbient()->getTexture())
						{
							saveMetadataMap(stateset, p->getAmbient()->getTexture()->getTexture(), 
								AMBIENT_OCCLUSION_UNIT,  (usePredefTexUnit ? AMBIENT_OCCLUSION_UNIT : unit++), "phong", "ambient");
						}
					}

					// diffuse
					if(p->getDiffuse())
					{
						if(p->getDiffuse()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = p->getDiffuse()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("phong_diffuse_color", ss.str());
						}
						if (p->getDiffuse()->getTexture())
						{
							saveMetadataMap(stateset, p->getDiffuse()->getTexture()->getTexture(), 
								MAIN_TEXTURE_UNIT,  (usePredefTexUnit ? MAIN_TEXTURE_UNIT : unit++), "phong", "diffuse");
						}
					}

					// emissive
					if(p->getEmission())
					{
						if(p->getEmission()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = p->getEmission()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("phong_emission_color", ss.str());
						}
						if (p->getEmission()->getTexture())
						{
							saveMetadataMap(stateset, p->getEmission()->getTexture()->getTexture(), 
								ILLUMINATION_MAP_UNIT,  (usePredefTexUnit ? ILLUMINATION_MAP_UNIT : unit++), "phong", "emission");
						}
					}

					// specular
					if(p->getSpecular())
					{
						if(p->getSpecular()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = p->getSpecular()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("phong_specular_color", ss.str());
						}
						if (p->getSpecular()->getTexture())
						{
							saveMetadataMap(stateset, p->getSpecular()->getTexture()->getTexture(), 
								SPECULAR_MAP_UNIT,  (usePredefTexUnit ? SPECULAR_MAP_UNIT : unit++), "phong", "specular");
						}
					}
					// specular exponent
					if(p->getShininess())
					{
						if(p->getShininess() != NULL && p->getShininess()->getFloat() != NULL )
						{
							std::stringstream ss;
							domFloat f = p->getShininess()->getFloat()->getValue();
							ss << f ;
							stateset->setUserValue("phong_shininess_amount", ss.str());
						}
					}

					// Reflectivity
					if(p->getReflective())
					{
						if(p->getReflective()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = p->getReflective()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("phong_reflective_color", ss.str());
						}
						if (p->getReflective()->getTexture())
						{
							saveMetadataMap(stateset, p->getReflective()->getTexture()->getTexture(), 
								REFLECTIVE_MAP_UNIT,  (usePredefTexUnit ? REFLECTIVE_MAP_UNIT : unit++), "phong", "reflective");
						}
					}
					if(p->getReflectivity())
					{
						if(p->getReflectivity() != NULL && p->getReflectivity()->getFloat() != NULL )
						{
							std::stringstream ss;
							domFloat f = p->getReflectivity()->getFloat()->getValue();
							ss << f ;
							stateset->setUserValue("phong_reflective_amount", ss.str());
						}
					}

					// translucency
					// TODO: read furthermore  "Determining Transparency" in 1.4.1 spec				
					if(p->getTransparent())
					{
						if(p->getTransparent() != NULL && p->getTransparency() != NULL && p->getTransparency()->getFloat() != NULL )
						{
							std::stringstream ss;
							domFloat f = p->getTransparency()->getFloat()->getValue();
							ss << f;
							stateset->setUserValue("phong_transparency_amount", ss.str());
						}
						if (p->getTransparent()->getTexture()) 
						{
							saveMetadataMap(stateset, p->getTransparent()->getTexture()->getTexture(), 
								TRANSPARENCY_MAP_UNIT,  (usePredefTexUnit ? TRANSPARENCY_MAP_UNIT : unit++), "phong", "transparency");
						}
					}
				}
				domProfile_COMMON::domTechnique::domLambert *l = teq->getLambert();			
				if (l)
				{
					unit = 0;
					// ambient
					if(l->getAmbient())
					{
						if(l->getAmbient()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = l->getAmbient()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("lambert_ambient_color", ss.str());
						}
						if (l->getAmbient()->getTexture())
						{
							saveMetadataMap(stateset, l->getAmbient()->getTexture()->getTexture(), 
								AMBIENT_OCCLUSION_UNIT,  (usePredefTexUnit ? AMBIENT_OCCLUSION_UNIT : unit++), "lambert", "ambient");
						}
					}

					// diffuse
					if(l->getDiffuse())
					{
						if(l->getDiffuse()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = l->getDiffuse()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("lambert_diffuse_color", ss.str());
						}
						if (l->getDiffuse()->getTexture())
						{
							saveMetadataMap(stateset, l->getDiffuse()->getTexture()->getTexture(), 
								MAIN_TEXTURE_UNIT,  (usePredefTexUnit ? MAIN_TEXTURE_UNIT : unit++), "lambert", "diffuse");
						}
					}

					// emissive
					if(l->getEmission())
					{
						if(l->getEmission()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = l->getEmission()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("lambert_emission_color", ss.str());
						}
						if (l->getEmission()->getTexture())
						{
							saveMetadataMap(stateset, l->getEmission()->getTexture()->getTexture(), 
								ILLUMINATION_MAP_UNIT,  (usePredefTexUnit ? ILLUMINATION_MAP_UNIT : unit++), "lambert", "emission");
						}
					}


					// Reflectivity
					if(l->getReflective())
					{
						if(l->getReflective()->getColor() != NULL )
						{
							std::stringstream ss;
							domFloat4 &f4 = l->getReflective()->getColor()->getValue();
							ss << "[ " << f4[0] << ", " << f4[1] << ", " << f4[2] << "]";
							stateset->setUserValue("lambert_reflective_color", ss.str());
						}
						if (l->getReflective()->getTexture())
						{
							std::stringstream ss;
							saveMetadataMap(stateset, l->getReflective()->getTexture()->getTexture(), 
								REFLECTIVE_MAP_UNIT,  (usePredefTexUnit ? REFLECTIVE_MAP_UNIT : unit++), "lambert", "reflective");
						}
					}
					if(l->getReflectivity())
					{
						if(l->getReflectivity() != NULL && l->getReflectivity()->getFloat() != NULL )
						{
							std::stringstream ss;
							domFloat f = l->getReflectivity()->getFloat()->getValue();
							ss << f ;
							stateset->setUserValue("lambert_reflective_amount", ss.str());
						}
					}

					// translucency
					// TODO: read furthermore  "Determining Transparency" in 1.4.1 spec				
					if(l->getTransparent())
					{
						if(l->getTransparent() != NULL && l->getTransparency() != NULL && l->getTransparency()->getFloat() != NULL )
						{
							std::stringstream ss;
							domFloat f = l->getTransparency()->getFloat()->getValue();
							ss << f;
							stateset->setUserValue("lambert_transparency_amount", ss.str());
						}
						if (l->getTransparent()->getTexture()) 
						{
							saveMetadataMap(stateset, l->getTransparent()->getTexture()->getTexture(), 
								TRANSPARENCY_MAP_UNIT,  (usePredefTexUnit ? TRANSPARENCY_MAP_UNIT : unit++), "lambert", "transparency");
						}
					}
				}
			}

		}
	}

	if (daeDatabase* database = _dae->getDatabase())
	{
		domExtra *extra;	
		unsigned int numExtra = database->getElementCount(NULL, COLLADA_TYPE_EXTRA, NULL);
		for ( unsigned int el_number = 0; el_number < numExtra; ++el_number )
		{
			daeInt error = database->getElement((daeElement**)&extra, el_number, NULL, COLLADA_TYPE_EXTRA, NULL);

			daeElement *parentEl = extra->getParentElement();

			//osg::notify(osg::NOTICE) << "=> Parent Name: " << parentEl->getTypeName() << std::endl;
			//if ( strcmp( parentEl->getTypeName(), "technique" ) == 0 )
			//    osg::notify(osg::NOTICE) << "=> Parent sid: " << parentEl->getAttribute("sid") << std::endl;

			//
			unsigned int numChildrenTeq = extra->getTechnique_array().getCount();
			for( unsigned int currChildTeq = 0; currChildTeq < numChildrenTeq; ++currChildTeq )
			{
				domTechnique *teq = extra->getTechnique_array()[currChildTeq];

				//osg::notify(osg::NOTICE) << "=> Profile: " << teq->getProfile() << std::endl;

				//if ( strcmp( teq->getProfile(), "custom" ) == 0 )
				{
					unsigned int numChildren = teq->getContents().getCount();
					for( unsigned int currChild = 0; currChild < numChildren; ++currChild )
					{
						if ( teq->getContents()[currChild]->getElementType() == COLLADA_TYPE::ANY )
						{
							domAny *child = (domAny*)(daeElement*)teq->getContents()[currChild];
							if ( strcmp( child->getElementName(), "bump" ) == 0 )
							{
								//osg::notify(osg::NOTICE) <<  "BUMPMAP: " << child->getElementName() <<  std::endl;
								const daeElementRefArray& arrRef = child->getContents();
								unsigned int numChildren = arrRef.getCount();
								for ( unsigned int currChildBump = 0; currChildBump < numChildren; ++currChildBump )
								{
									if ( strcmp( child->getContents()[currChildBump]->getElementName(), "texture" ) == 0 )
									{
										daeElementRef elRef = arrRef[currChildBump];
										domAny *childTexture = (domAny*)(daeElement*)elRef;
										domAny* pAny = (domAny*)elRef.cast();
										domCommon_color_or_texture_type_complexType::domTexture *childDomTex = daeSafeCast< domCommon_color_or_texture_type_complexType::domTexture > (elRef.cast());

										/*
										unsigned int numAttrs = childTexture->getAttributeCount();for ( unsigned int currAttr = 0; currAttr < numAttrs; ++currAttr )
										{
										osg::notify(osg::NOTICE) << "\tAttribute: "<< childTexture->getAttributeName( currAttr ) << " has value: " << childTexture->getAttributeValue( currAttr )  << std::endl;
										}*/


										std::stringstream ss;
										std::string texPath = childTexture->getAttribute( "texture" );
										daeString texName = daeString(texPath.c_str());
										osg::Texture2D* tex =  getTexture(texName, BUMP_MAP_UNIT);
										if (tex)
										{                                   
											unsigned int texCoordUnit = usePredefTexUnit ? BUMP_MAP_UNIT : unit++;
											stateset->setTextureAttributeAndModes(texCoordUnit, tex);											
											std::string texCoord = childTexture->getAttribute( "texcoord" );
											_texCoordSetMap[TextureToCoordSetMap::key_type(stateset, BUMP_MAP_UNIT)] = texCoordUnit;     

											int texUnit = getTexUnit(stateset, tex->getImage()->getFileName());
											ss << (texUnit != -1 ? texUnit : texCoordUnit);
											stateset->setUserValue("extra_bump_unit", ss.str());
											ss.str("");
											ss <<  texCoord;
											stateset->setUserValue("extra_bump_texcoord", ss.str());

											if (childTexture->getChild("extra") 
												&& childTexture->getChild("extra")->getChild("technique")
												&& childTexture->getChild("extra")->getChild("technique")->getChild("amount"))
											{
												std::string texValue = childTexture->getChild("extra")->getChild("technique")->getChild("amount")->getCharData();
												stateset->setUserValue("extra_bump_amount", texValue);
											}
										}
									}
								}
							}
							else if ( strcmp( child->getElementName(), "displacement" ) == 0 )
							{
								//osg::notify(osg::NOTICE) <<  "BUMPMAP: " << child->getElementName() <<  std::endl;
								const daeElementRefArray& arrRef = child->getContents();
								unsigned int numChildren = arrRef.getCount();
								for ( unsigned int currChildBump = 0; currChildBump < numChildren; ++currChildBump )
								{
									if ( strcmp( child->getContents()[currChildBump]->getElementName(), "texture" ) == 0 )
									{
										daeElementRef elRef = arrRef[currChildBump];
										domAny *childTexture = (domAny*)(daeElement*)elRef;
										domAny* pAny = (domAny*)elRef.cast();
										domCommon_color_or_texture_type_complexType::domTexture *childDomTex = daeSafeCast< domCommon_color_or_texture_type_complexType::domTexture > (elRef.cast());

										/*
										unsigned int numAttrs = childTexture->getAttributeCount();for ( unsigned int currAttr = 0; currAttr < numAttrs; ++currAttr )
										{
										osg::notify(osg::NOTICE) << "\tAttribute: "<< childTexture->getAttributeName( currAttr ) << " has value: " << childTexture->getAttributeValue( currAttr )  << std::endl;
										}*/


										std::stringstream ss;
										std::string texPath = childTexture->getAttribute( "texture" );
										daeString texName = daeString(texPath.c_str());
										osg::Texture2D* tex =  getTexture(texName, BUMP_MAP_UNIT);
										if (tex)
										{                                   
											unsigned int texCoordUnit = usePredefTexUnit ? BUMP_MAP_UNIT : unit++;
											stateset->setTextureAttributeAndModes(texCoordUnit, tex);											
											std::string texCoord = childTexture->getAttribute( "texcoord" );
											_texCoordSetMap[TextureToCoordSetMap::key_type(stateset, BUMP_MAP_UNIT)] = texCoordUnit;     

											int texUnit = getTexUnit(stateset, tex->getImage()->getFileName());
											ss << (texUnit != -1 ? texUnit : texCoordUnit);
											stateset->setUserValue("extra_normal_unit", ss.str());
											ss.str("");
											ss <<  texCoord;
											stateset->setUserValue("extra_normal_texcoord", ss.str());

											if (childTexture->getChild("extra") 
												&& childTexture->getChild("extra")->getChild("technique")
												&& childTexture->getChild("extra")->getChild("technique")->getChild("amount"))
											{
												std::string texValue = childTexture->getChild("extra")->getChild("technique")->getChild("amount")->getCharData();
												stateset->setUserValue("extra_normal_amount", texValue);
											}
										}
									}
								}
							}
							else if ( strcmp( child->getElementName(), "imagebased" ) == 0 )
							{
								//osg::notify(osg::NOTICE) <<  "IBL: " << child->getElementName() <<  std::endl;
								unsigned int numChildren = child->getContents().getCount();
								for ( unsigned int currChildBump = 0; currChildBump < numChildren; ++currChildBump )
								{
									if ( strcmp( child->getContents()[currChildBump]->getElementName(), "texture" ) == 0 )
									{
										domAny *childTexture = (domAny*)(daeElement*)child->getContents()[currChildBump];

										/*
										unsigned int numAttrs = childTexture->getAttributeCount();
										for ( unsigned int currAttr = 0; currAttr < numAttrs; ++currAttr )
										{
										osg::notify(osg::NOTICE) << "\tAttribute: "<< childTexture->getAttributeName( currAttr ) << " has value: " << childTexture->getAttributeValue( currAttr )  << std::endl;
										}
										*/
										std::stringstream ss;
										std::string texPath = childTexture->getAttribute( "texture" );
										daeString texName = daeString(texPath.c_str());
										osg::Texture2D* tex =  getTexture(texName, IMAGE_BASE_LIGHT_MAP_UNIT);

										// TODO when whe know what to do with IBL texture on render side
										// (this allow adding it to the stateset by finding/creating missing texture)
										if (!tex)
											tex = this->getTextureNotColladed(database, texPath);		

										if (tex && tex->getImage())
										{
											unsigned int texCoordUnit = usePredefTexUnit ? IMAGE_BASE_LIGHT_MAP_UNIT : unit++;
											stateset->setTextureAttributeAndModes(texCoordUnit, tex);											
											std::string texCoord = childTexture->getAttribute( "texcoord" );
											_texCoordSetMap[TextureToCoordSetMap::key_type(stateset, IMAGE_BASE_LIGHT_MAP_UNIT)] = texCoordUnit;     

											int texUnit = getTexUnit(stateset, tex->getImage()->getFileName());
											ss << (texUnit != -1 ? texUnit : texCoordUnit);
											stateset->setUserValue("extra_imagebased_unit", ss.str());
											ss.str("");
											ss <<  texCoord;
											stateset->setUserValue("extra_imagebased_texcoord", ss.str());
											ss.str("");

											if (childTexture->getChild("extra") 
												&& childTexture->getChild("extra")->getChild("technique")
												&& childTexture->getChild("extra")->getChild("technique")->getChild("amount"))
											{
												std::string texValue = childTexture->getChild("extra")->getChild("technique")->getChild("amount")->getCharData();
												stateset->setUserValue("extra_imagebased_amount", texValue);
											}
										}
									}
								}
							}
							else
							{
								// helpful for debugging new Profiles
								//printDomAny( child );
							}
						}
						else if ( teq->getContents()[currChild]->getElementType() == COLLADA_TYPE::PARAM )
						{
							domParam *param = daeSafeCast<domParam>( teq->getContents()[currChild] );
							//use param element
						}
						else
						{
							osg::notify(osg::NOTICE) << "Error: Invalid element for profile custom" << std::endl;
						}
					}
				}
			}
		}
	}
}


osg::Group* daeReader::processExtras(domNode *node)
{
	// See if one of the extras contains OpenSceneGraph specific information
	unsigned int numExtras = node->getExtra_array().getCount();
	for (unsigned int currExtra=0; currExtra < numExtras; currExtra++)
	{
		domExtra* extra = node->getExtra_array()[currExtra];
		domTechnique* teq = NULL;

		daeString extraType = extra->getType();
		if (extraType)
		{
			if (strcmp(extraType, "Switch") == 0)
			{
				teq = getOpenSceneGraphProfile(extra);
				if (teq)
				{
					return processOsgSwitch(teq);
				}
			}
			else if (strcmp(extraType, "MultiSwitch") == 0)
			{
				teq = getOpenSceneGraphProfile(extra);
				if (teq)
				{
					return processOsgMultiSwitch(teq);
				}
			}
			else if (strcmp(extraType, "LOD") == 0)
			{
				teq = getOpenSceneGraphProfile(extra);
				if (teq)
				{
					return processOsgLOD(teq);
				}
			}
			else if (strcmp(extraType, "DOFTransform") == 0)
			{
				teq = getOpenSceneGraphProfile(extra);
				if (teq)
				{
					return processOsgDOFTransform(teq);
				}
			}
			else if (strcmp(extraType, "Sequence") == 0)
			{
				teq = getOpenSceneGraphProfile(extra);
				if (teq)
				{
					return processOsgSequence(teq);
				}
			}
		}
	}
	return new osg::Group;
}

void daeReader::processNodeExtra(osg::Node* osgNode, domNode *node)
{
	// See if one of the extras contains OpenSceneGraph specific information
	unsigned int numExtras = node->getExtra_array().getCount();

	for (unsigned int currExtra=0; currExtra < numExtras; currExtra++)
	{
		domExtra* extra = node->getExtra_array()[currExtra];

		daeString extraType = extra->getType();
		if (extraType && (strcmp(extraType, "Node") == 0))
		{
			domTechnique* teq = getOpenSceneGraphProfile(extra);
			if (teq)
			{
				domAny* any = daeSafeCast< domAny >(teq->getChild("Descriptions"));
				if (any)
				{
					osg::Node::DescriptionList descriptions;
					unsigned int numChildren = any->getChildren().getCount();
					for (unsigned int currChild = 0; currChild < numChildren; currChild++)
					{
						domAny* child = daeSafeCast<domAny>(any->getChildren()[currChild]);
						if (child)
						{
							if (strcmp(child->getElementName(), "Description" ) == 0 )
							{
								std::string value = child->getValue();
								descriptions.push_back(value);
							}
							else
							{
								OSG_WARN << "Child of element 'Descriptions' is not of type 'Description'" << std::endl;
							}
						}
						else
						{
							OSG_WARN << "Element 'Descriptions' does not contain expected elements." << std::endl;
						}
					}
					osgNode->setDescriptions(descriptions);
				}
				else
				{
					OSG_WARN << "Expected element 'Descriptions' not found" << std::endl;
				}
			}
		}
		else
		{
			OSG_WARN << "Unsupported Extra Type: " << extra->getType() << std::endl;
		}
	}
}

domTechnique* daeReader::getOpenSceneGraphProfile(domExtra* extra)
{
	unsigned int numTeqs = extra->getTechnique_array().getCount();

	for ( unsigned int currTeq = 0; currTeq < numTeqs; ++currTeq )
	{
		// Only interested in OpenSceneGraph technique
		if (strcmp( extra->getTechnique_array()[currTeq]->getProfile(), "OpenSceneGraph" ) == 0 )
		{
			return extra->getTechnique_array()[currTeq];
		}
	}
	return NULL;
}


// <node>
// attributes:
// id, name, sid, type, layer
// child elements:
// 0..1 <asset>
// 0..* <lookat>, <matrix>, <rotate>, <scale>, <skew>, <translate>
// 0..* <instance_camera>
// 0..* <instance_controller>
// 0..* <instance_geometry>
// 0..* <instance_light>
// 0..* <instance_node>
// 0..* <node>
// 0..* <extra>
osg::Node* daeReader::processNode( domNode *node, bool skeleton)
{
	// First we need to determine what kind of OSG node we need
	// If there exist any of the <lookat>, <matrix>, <rotate>, <scale>, <skew>, <translate> elements
	// or if a COLLADA_TYPE_INSTANCE_RIGID_BODY targets this node we need a MatrixTransform
	int coordcount =    node->getRotate_array().getCount() +
		node->getScale_array().getCount() +
		node->getTranslate_array().getCount() +
		node->getLookat_array().getCount() +
		node->getMatrix_array().getCount() +
		node->getSkew_array().getCount();

	// See if it is targeted by an animation
	bool targeted = false;
	if (node->getId())
	{
		targeted = _targetMap[std::string(node->getId())];
	}


	osg::Group *resultNode = NULL;

	bool isBone = skeleton || isJoint(node);

	if (coordcount > 0 || targeted || isBone)
	{
		// TODO
		// single matrix -> MatrixTransform
		// scale, euler, translate -> PositionAttitudeTransform
		// if targeted -> StackedTransform
		// otherwise a flattened -> MatrixTransform
		resultNode = processOsgMatrixTransform(node, isBone);
	}
	else
	{
		// No transform data, determine node type based on it's available extra data
		resultNode = processExtras(node);
	}

	// See if there is generic node info attached as extra
	processNodeExtra(resultNode, node);

	if (resultNode->getName().empty())
	{
		std::string name = "";
		if (node->getId())
			name = node->getId();
		if (node->getName())
			name = node->getName();
		resultNode->setName( name );
	}

	osg::Group* attachTo = resultNode;

	if (!skeleton && isJoint(node))
	{
		skeleton = true;
		osgAnimation::Skeleton* pOsgSkeleton = getOrCreateSkeleton(node);
		pOsgSkeleton->addChild(resultNode);
		attachTo = resultNode;
		resultNode = pOsgSkeleton;
	}

	// 0..* <instance_camera>
	const domInstance_camera_Array& cameraInstanceArray = node->getInstance_camera_array();
	for ( size_t i = 0; i < cameraInstanceArray.getCount(); i++ )
	{
		daeElement *el = getElementFromURI( cameraInstanceArray[i]->getUrl());
		domCamera *c = daeSafeCast< domCamera >( el );

		if (c)
			addChild(attachTo, processCamera( c ));
		else
			OSG_WARN << "Failed to locate camera " << cameraInstanceArray[i]->getUrl().getURI() << std::endl;
	}

	// 0..* <instance_controller>
	const domInstance_controller_Array& controllerInstanceArray = node->getInstance_controller_array();
	for ( size_t i = 0; i < controllerInstanceArray.getCount(); i++ )
	{
		osg::Node* pOsgNode = processInstanceController( controllerInstanceArray[i]);

		// A skin controller may return NULL,  since the RigGeometry is added as
		// child of the skeleton and the skeleton already is added to the scenegraph
		if (pOsgNode)
		{
			addChild(attachTo, pOsgNode);
		}
	}

	// 0..* <instance_geometry>
	const domInstance_geometry_Array& geometryInstanceArray = node->getInstance_geometry_array();
	for ( size_t i = 0; i < geometryInstanceArray.getCount(); i++ )
	{
		addChild(attachTo, processInstanceGeometry( geometryInstanceArray[i] ));
	}

	// 0..* <instance_light>
	const domInstance_light_Array& lightInstanceArray = node->getInstance_light_array();
	for ( size_t i = 0; i < lightInstanceArray.getCount(); i++ )
	{
		daeElement *el = getElementFromURI( lightInstanceArray[i]->getUrl());
		domLight *pDomLight = daeSafeCast< domLight >( el );

		if (pDomLight)
			addChild(attachTo, processLight(pDomLight));
		else
			OSG_WARN << "Failed to locate light " << lightInstanceArray[i]->getUrl().getURI() << std::endl;
	}

	// 0..* <instance_node>
	const domInstance_node_Array& nodeInstanceArray = node->getInstance_node_array();
	for ( size_t i = 0; i < nodeInstanceArray.getCount(); i++ )
	{
		daeElement *el = getElementFromURI( nodeInstanceArray[i]->getUrl());
		domNode *n = daeSafeCast< domNode >( el );

		if (n)
			// Recursive call
				addChild(attachTo, processNode( n, skeleton ));
		else
			OSG_WARN << "Failed to locate node " << nodeInstanceArray[i]->getUrl().getURI() << std::endl;
	}

	// 0..* <node>
	const domNode_Array& nodeArray = node->getNode_array();
	for ( size_t i = 0; i < nodeArray.getCount(); i++ )
	{
		// Recursive call
		addChild(attachTo, processNode( nodeArray[i], skeleton ));
	}

	return resultNode;
}
