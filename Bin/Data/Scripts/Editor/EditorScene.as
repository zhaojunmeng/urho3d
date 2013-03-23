// Urho3D editor scene handling

#include "Scripts/Editor/EditorSceneWindow.as"
#include "Scripts/Editor/EditorNodeWindow.as"

const int PICK_GEOMETRIES = 0;
const int PICK_LIGHTS = 1;
const int PICK_ZONES = 2;
const int PICK_RIGIDBODIES = 3;
const int MAX_PICK_MODES = 4;

Scene@ editorScene;

String sceneFileName;
String instantiateFileName;
CreateMode instantiateMode = REPLICATED;
bool sceneModified = false;
bool runUpdate = false;

Array<Node@> selectedNodes;
Array<Component@> selectedComponents;
Node@ editNode;
Array<Node@> editNodes;
Array<Component@> editComponents;
uint numEditableComponentsPerNode = 1;

Array<XMLFile@> copyBuffer;
bool copyBufferLocal = false;

bool inSelectionModify = false;

void ClearSelection()
{
    selectedNodes.Clear();
    selectedComponents.Clear();
    editNode = null;
    editNodes.Clear();
    editComponents.Clear();
    numEditableComponentsPerNode = 1;

    HideGizmo();
}

void CreateScene()
{
    // Create a scene only once here
    editorScene = Scene("");
    
    // Allow access to the scene from the console
    script.defaultScene = editorScene;
    
    // Always pause the scene, and do updates manually
    editorScene.active = false;

    // Camera is not bounded to a scene but still need to be created once here
    CreateCamera();
}

void ResetScene()
{
    ClearSelection();

    suppressSceneChanges = true;

    // Create a scene with default values, these will be overridden when loading scenes
    editorScene.Clear();
    editorScene.name = "";
    Octree@ octree = editorScene.CreateComponent("Octree");
    PhysicsWorld@ physicsWorld = editorScene.CreateComponent("PhysicsWorld");
    octree.Resize(BoundingBox(-1000.0, 1000.0), 8);
    editorScene.CreateComponent("DebugRenderer");

    UpdateSceneWindow();
    UpdateNodeWindow();
    
    suppressSceneChanges = false;

    runUpdate = false;
    sceneFileName = "";
    UpdateWindowTitle();
    ResetCamera();
    CreateGizmo();
}

void SetResourcePath(String newPath, bool usePreferredDir = true)
{
    if (newPath.empty)
        return;
    
    if (usePreferredDir)
        newPath = AddTrailingSlash(cache.GetPreferredResourceDir(newPath));
    else
        newPath = AddTrailingSlash(newPath);

    if (newPath == sceneResourcePath)
        return;

    cache.ReleaseAllResources(false);
    renderer.ReloadShaders();

    // Remove the old scene resource path if any. However make sure that the default data paths do not get removed
    if (!sceneResourcePath.empty && !sceneResourcePath.Contains(fileSystem.programDir))
        cache.RemoveResourceDir(sceneResourcePath);

    cache.AddResourceDir(newPath);
    sceneResourcePath = newPath;
}

bool LoadScene(const String&in fileName)
{
    if (fileName.empty)
        return false;

    ui.cursor.shape = CS_BUSY;
    
    // Always load the scene from the filesystem, not from resource paths
    if (!fileSystem.FileExists(fileName))
    {
        log.Error("No such scene: " + fileName);
        return false;
    }

    File file(fileName, FILE_READ);
    if (!file.open)
        return false;

    // Clear the old scene
    suppressSceneChanges = true;

    ClearSelection();
    editorScene.Clear();

    // Add the new resource path
    SetResourcePath(GetPath(fileName));

    String extension = GetExtension(fileName);
    bool loaded;
    if (extension != ".xml")
        loaded = editorScene.Load(file);
    else
        loaded = editorScene.LoadXML(file);

    // Always pause the scene, and do updates manually
    editorScene.active = false;

    sceneFileName = fileName;
    sceneModified = false;
    runUpdate = false;
    UpdateWindowTitle();
    UpdateSceneWindow();
    UpdateNodeWindow();

    suppressSceneChanges = false;

    ResetCamera();
    CreateGizmo();

    return loaded;
}

void SaveScene(const String&in fileName)
{
    if (fileName.empty || GetFileName(fileName).empty)
        return;

    // Unpause when saving so that the scene will work properly when loaded outside the editor
    editorScene.active = true;

    File file(fileName, FILE_WRITE);
    String extension = GetExtension(fileName);
    if (extension != ".xml")
        editorScene.Save(file);
    else
        editorScene.SaveXML(file);

    editorScene.active = false;

    sceneFileName = fileName;
    sceneModified = false;
    UpdateWindowTitle();
}

void LoadNode(const String&in fileName)
{
    if (fileName.empty)
        return;
    
    if (!fileSystem.FileExists(fileName))
    {
        log.Error("No such node file " + fileName);
        return;
    }

    File file(fileName, FILE_READ);
    if (!file.open)
        return;

    // Before instantiating, set resource path if empty
    if (sceneResourcePath.empty)
        SetResourcePath(GetPath(fileName));

    Vector3 position = GetNewNodePosition();
    Node@ newNode;

    String extension = GetExtension(fileName);
    if (extension != ".xml")
        newNode = editorScene.Instantiate(file, position, Quaternion(), instantiateMode);
    else
        newNode = editorScene.InstantiateXML(file, position, Quaternion(), instantiateMode);

    if (newNode !is null)
    {
        FocusNode(newNode);
        instantiateFileName = fileName;
    }
}

void SaveNode(const String&in fileName)
{
    if (fileName.empty || GetFileName(fileName).empty)
        return;

    if (selectedNodes.length == 1)
    {
        File file(fileName, FILE_WRITE);
        if (!file.open)
            return;

        String extension = GetExtension(fileName);
        if (extension != ".xml")
            selectedNodes[0].Save(file);
        else
            selectedNodes[0].SaveXML(file);

        instantiateFileName = fileName;
    }
}

void UpdateScene(float timeStep)
{
    if (runUpdate)
        editorScene.Update(timeStep);
}

void BeginModify(uint nodeID)
{
    // Undo/Redo can be implemented here
}

void EndModify(uint nodeID)
{
    // Undo/Redo can be implemented here

    if (!sceneModified)
    {
        sceneModified = true;
        UpdateWindowTitle();
    }
}

void BeginSelectionModify()
{
    // A large operation on selected nodes is about to begin. Disable intermediate selection updates
    inSelectionModify = true;
}

void EndSelectionModify()
{
    // The large operation on selected nodes has ended. Update node/component selection now
    inSelectionModify = false;
    HandleSceneWindowSelectionChange();
}

bool SceneDelete()
{
    if (!CheckSceneWindowFocus() || (selectedComponents.empty && selectedNodes.empty))
        return false;

    BeginSelectionModify();
    
    // Clear the selection now to prevent repopulation of selectedNodes and selectedComponents combo
    hierarchyList.ClearSelection();

    // Remove nodes
    for (uint i = 0; i < selectedNodes.length; ++i)
    {
        Node@ node = selectedNodes[i];
        if (node.parent is null || node.scene is null)
            continue; // Root or already deleted

        uint id = node.id;
        uint nodeIndex = GetNodeListIndex(node);

        BeginModify(id);
        node.Remove();
        EndModify(id);

        // If deleting only one node, select the next item in the same index
        if (selectedNodes.length == 1 && selectedComponents.empty)
            hierarchyList.selection = nodeIndex;
    }

    // Then remove components, if they still remain
    for (uint i = 0; i < selectedComponents.length; ++i)
    {
        Component@ component = selectedComponents[i];
        Node@ node = component.node;
        if (node is null)
            continue; // Already deleted

        uint index = GetComponentListIndex(component);
        uint nodeIndex = GetNodeListIndex(node);
        if (index == NO_ITEM || nodeIndex == NO_ITEM)
            continue;

        // Do not allow to remove the Octree, PhysicsWorld or DebugRenderer from the root node
        if (node is editorScene && (component.typeName == "Octree" || component.typeName == "PhysicsWorld" ||
            component.typeName == "DebugRenderer"))
            continue;

        uint id = node.id;
        BeginModify(id);
        node.RemoveComponent(component);
        EndModify(id);

        // If deleting only one component, select the next item in the same index
        if (selectedComponents.length == 1 && selectedNodes.empty)
            hierarchyList.selection = index;
    }

    EndSelectionModify();
    return true;
}

bool SceneCut()
{
    if (SceneCopy())
        return SceneDelete();
    else
        return false;
}

bool SceneCopy()
{
    if ((selectedNodes.empty && selectedComponents.empty) || !CheckSceneWindowFocus())
        return false;

    // Must have either only components, or only nodes
    if (!selectedNodes.empty && !selectedComponents.empty)
        return false;

    copyBuffer.Clear();

    // Copy components
    if (!selectedComponents.empty)
    {
        for (uint i = 0; i < selectedComponents.length; ++i)
        {
            XMLFile@ xml = XMLFile();
            XMLElement rootElem = xml.CreateRoot("component");
            selectedComponents[i].SaveXML(rootElem);
            rootElem.SetBool("local", selectedComponents[i].id >= FIRST_LOCAL_ID);
            copyBuffer.Push(xml);
        }
        return true;
    }
    // Copy node. The root node can not be copied
    else
    {
        for (uint i = 0; i < selectedNodes.length; ++i)
        {
            if (selectedNodes[i] is editorScene)
                return false;
        }

        for (uint i = 0; i < selectedNodes.length; ++i)
        {
            XMLFile@ xml = XMLFile();
            XMLElement rootElem = xml.CreateRoot("node");
            selectedNodes[i].SaveXML(rootElem);
            rootElem.SetBool("local", selectedNodes[i].id >= FIRST_LOCAL_ID);
            copyBuffer.Push(xml);
        }

        return true;
    }
}

bool ScenePaste()
{
    if (editNode is null || !CheckSceneWindowFocus() || copyBuffer.empty)
        return false;

    bool pasteComponents = false;

    for (uint i = 0; i < copyBuffer.length; ++i)
    {
        XMLElement rootElem = copyBuffer[i].root;
        String mode = rootElem.name;
        if (mode == "component")
        {
            pasteComponents = true;

            // If this is the root node, do not allow to create duplicate scene-global components
            if (editNode is editorScene && CheckForExistingGlobalComponent(editNode, rootElem.GetAttribute("type")))
                return false;

            BeginModify(editNode.id);

            // If copied component was local, make the new local too
            Component@ newComponent = editNode.CreateComponent(rootElem.GetAttribute("type"), rootElem.GetBool("local") ? LOCAL :
                REPLICATED);
            if (newComponent is null)
            {
                EndModify(editNode.id);
                return false;
            }
            newComponent.LoadXML(rootElem);
            newComponent.ApplyAttributes();
            EndModify(editNode.id);
        }
        else if (mode == "node")
        {
            // Make the paste go always to the root node, no matter of the selected node
            BeginModify(editorScene.id);
            // If copied node was local, make the new local too
            Node@ newNode = editorScene.CreateChild("", rootElem.GetBool("local") ? LOCAL : REPLICATED);
            BeginModify(newNode.id);
            newNode.LoadXML(rootElem);
            newNode.ApplyAttributes();
            EndModify(newNode.id);
            EndModify(editorScene.id);
        }
    }

    return true;
}

void SceneUnparent()
{
    if (!CheckSceneWindowFocus() || !selectedComponents.empty || selectedNodes.empty)
        return;

    // Parent selected nodes to root
    for (uint i = 0; i < selectedNodes.length; ++i)
    {
        Node@ sourceNode = selectedNodes[i];
        if (sourceNode.parent is null || sourceNode.parent is editorScene)
            continue; // Root or already parented to root

        Node@ targetNode = editorScene;

        // Perform the reparenting
        BeginModify(targetNode.id);
        BeginModify(sourceNode.id);
        sourceNode.parent = targetNode;
        EndModify(sourceNode.id);
        EndModify(targetNode.id);
    }
}

void SceneResetPosition()
{
    if (editNode !is null)
    {
        editNode.position = Vector3(0.0, 0.0, 0.0);
        UpdateNodeAttributes();
    }
}

void SceneResetRotation()
{
    if (editNode !is null)
    {
        editNode.rotation = Quaternion();
        UpdateNodeAttributes();
    }
}

void SceneResetScale()
{
    if (editNode !is null)
    {
        editNode.scale = Vector3(1.0, 1.0, 1.0);
        UpdateNodeAttributes();
    }
}

void SceneSelectAll()
{
    if (!hierarchyList.selections.empty)
    {
        BeginSelectionModify();
        hierarchyList.ClearSelection();
        EndSelectionModify();
    }
    else
    {
        BeginSelectionModify();
        Array<Node@> rootLevelNodes = editorScene.GetChildren();
        Array<uint> indices;
        for (uint i = 0; i < rootLevelNodes.length; ++i)
            indices.Push(GetNodeListIndex(rootLevelNodes[i]));
        hierarchyList.SetSelections(indices);
        EndSelectionModify();
    }
}
