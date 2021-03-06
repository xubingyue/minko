/*
Copyright (c) 2014 Aerys

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "minko/file/AssetLibrary.hpp"
#include "minko/file/SceneParser.hpp"
#include "minko/file/Options.hpp"
#include "minko/file/Dependency.hpp"
#include "minko/file/TextureParser.hpp"
#include "minko/Types.hpp"
#include "minko/component/Transform.hpp"
#include "minko/component/JobManager.hpp"
#include "minko/component/Surface.hpp"
#include "minko/component/BoundingBox.hpp"
#include "minko/component/MasterAnimation.hpp"
#include "minko/scene/Node.hpp"
#include "minko/scene/NodeSet.hpp"

#include "msgpack.hpp"

#include <stack>

using namespace minko;
using namespace minko::file;
using namespace minko::math;

std::unordered_map<int8_t, SceneParser::ComponentReadFunction> SceneParser::_componentIdToReadFunction;


SceneParser::SceneParser()
{
    _geometryParser = file::GeometryParser::create();
    _materialParser = file::MaterialParser::create();
    _textureParser = file::TextureParser::create();

    registerComponent(serialize::PROJECTION_CAMERA,
        std::bind(&deserialize::ComponentDeserializer::deserializeProjectionCamera,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3));

    registerComponent(serialize::TRANSFORM,
        std::bind(&deserialize::ComponentDeserializer::deserializeTransform,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3));

    registerComponent(serialize::AMBIENT_LIGHT,
        std::bind(&deserialize::ComponentDeserializer::deserializeAmbientLight,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3));

    registerComponent(serialize::DIRECTIONAL_LIGHT,
        std::bind(&deserialize::ComponentDeserializer::deserializeDirectionalLight,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3));

    registerComponent(serialize::SPOT_LIGHT,
        std::bind(&deserialize::ComponentDeserializer::deserializeSpotLight,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3));

    registerComponent(serialize::POINT_LIGHT,
        std::bind(&deserialize::ComponentDeserializer::deserializePointLight,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3));

    registerComponent(serialize::SURFACE,
        std::bind(&deserialize::ComponentDeserializer::deserializeSurface,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3));

    registerComponent(serialize::RENDERER,
        std::bind(&deserialize::ComponentDeserializer::deserializeRenderer,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3));

    registerComponent(serialize::ANIMATION,
        std::bind(&deserialize::ComponentDeserializer::deserializeAnimation,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3));

    registerComponent(serialize::SKINNING,
        std::bind(&deserialize::ComponentDeserializer::deserializeSkinning,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3));

    registerComponent(serialize::BOUNDINGBOX,
        std::bind(&deserialize::ComponentDeserializer::deserializeBoundingBox,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3));
}

void
SceneParser::registerComponent(int8_t                    componentId,
                               ComponentReadFunction    readFunction)
{
    _componentIdToReadFunction[componentId] = readFunction;
}

void
SceneParser::parse(const std::string&                    filename,
                   const std::string&                    resolvedFilename,
                   std::shared_ptr<Options>                options,
                   const std::vector<unsigned char>&    data,
                   AssetLibraryPtr                        assetLibrary)
{
    _dependencies->options(options);

    if (!readHeader(filename, data))
        return;

    std::string         folderPath = extractFolderPath(resolvedFilename);

    extractDependencies(assetLibrary, data, _headerSize, _dependenciesSize, options, folderPath);

    msgpack::object        deserialized;
    msgpack::zone        mempool;
    msgpack::unpack((char*)&data[_headerSize + _dependenciesSize], _sceneDataSize, NULL, &mempool, &deserialized);

    msgpack::type::tuple<std::vector<std::string>, std::vector<SerializedNode>> dst;
    deserialized.convert(&dst);

    std::vector<unsigned char>* d = (std::vector<unsigned char>*)&data;
    d->clear();
    d->shrink_to_fit();

    assetLibrary->symbol(filename, parseNode(dst.a1, dst.a0, assetLibrary, options));

    if (_jobList.size() > 0)
    {
        auto jobManager = component::JobManager::create(30);

        for (auto it = _jobList.begin(); it != _jobList.end(); ++it)
            jobManager->pushJob(*it);

        assetLibrary->symbol(filename)->addComponent(jobManager);
    }

    complete()->execute(shared_from_this());
}

scene::Node::Ptr
SceneParser::parseNode(std::vector<SerializedNode>&            nodePack,
                       std::vector<std::string>&            componentPack,
                       AssetLibraryPtr                        assetLibrary,
                       Options::Ptr                            options)
{
    scene::Node::Ptr                                    root;
    std::queue<std::tuple<scene::Node::Ptr, uint>>        nodeStack;
    std::map<int, std::vector<scene::Node::Ptr>>        componentIdToNodes;
    std::map<scene::Node::Ptr, scene::Node::Ptr>        nodeToParentMap;

    for (uint i = 0; i < nodePack.size(); ++i)
    {
        scene::Node::Ptr    newNode            = scene::Node::create();
        uint                layouts            = nodePack[i].a1;
        uint                numChildren        = nodePack[i].a2;
        std::vector<uint>    componentsId    = nodePack[i].a3;
        std::string            uuid            = nodePack[i].a4;

        newNode->layouts(layouts);
        newNode->name(nodePack[i].a0);
        newNode->uuid(uuid);

        for (uint componentId : componentsId)
            componentIdToNodes[componentId].push_back(newNode);

        if (nodeStack.size() == 0)
        {
            root = newNode;

            nodeToParentMap.insert(std::make_pair(root, nullptr));
        }
        else
        {
            scene::Node::Ptr parent = std::get<0>(nodeStack.front());

            std::get<1>(nodeStack.front())--;

            if (std::get<1>(nodeStack.front()) == 0)
                nodeStack.pop();

            nodeToParentMap.insert(std::make_pair(newNode, parent));
        }

        if (numChildren > 0)
            nodeStack.push(std::make_tuple(newNode, numChildren));
    }

    for (auto nodeToParentPair : nodeToParentMap)
    {
        auto node = nodeToParentPair.first;
        auto parent = nodeToParentPair.second;

        if (parent != nullptr)
            parent->addChild(node);
    }

    _dependencies->loadedRoot(root);

    std::set<uint> markedComponent;

    for (uint componentIndex = 0; componentIndex < componentPack.size(); ++componentIndex)
    {
        int8_t            dst = componentPack[componentIndex].at(componentPack[componentIndex].size() - 1);

        if (dst == serialize::SKINNING)
            markedComponent.insert(componentIndex);
        else
        {
            if (_componentIdToReadFunction.find(dst) != _componentIdToReadFunction.end())
            {
                std::shared_ptr<component::AbstractComponent> newComponent = _componentIdToReadFunction[dst](componentPack[componentIndex], assetLibrary, _dependencies);

                for (scene::Node::Ptr node : componentIdToNodes[componentIndex])
                    node->addComponent(newComponent);
            }
        }
    }

    bool isSkinningFree = true; // FIXME

    for (auto componentIndex2 : markedComponent)
    {
        isSkinningFree = false;
        std::shared_ptr<component::AbstractComponent> newComponent = _componentIdToReadFunction[serialize::SKINNING](componentPack[componentIndex2], assetLibrary, _dependencies);

        for (scene::Node::Ptr node : componentIdToNodes[componentIndex2])
		{
            node->addComponent(newComponent);
			node->addComponent(component::MasterAnimation::create());
		}
    }

    if (isSkinningFree)
    {
        auto nodeSet = scene::NodeSet::create(root)->descendants(true)->where([](scene::Node::Ptr n){ return n->components<component::Surface>().size() != 0; });

        for (auto n : nodeSet->nodes())
        {
            n->addComponent(component::BoundingBox::create());
        }
    }

    for (auto nodeToParentPair : nodeToParentMap)
    {
        auto node = nodeToParentPair.first;

        auto newNode = options->nodeFunction()(node);

        if (newNode != node)
        {
            auto parent = node->parent();
            parent->removeChild(node);
            parent->addChild(newNode);
        }
    }

    return root;
}
