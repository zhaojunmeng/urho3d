// Attribute editor
//
// Functions that caller must implement:
// - void SetAttributeEditorID(UIElement@ attrEdit, Array<Serializable@>@ serializables);
// - bool PreEditAttribute(Array<Serializable@>@ serializables, uint index);
// - void PostEditAttribute(Array<Serializable@>@ serializables, uint index, const Array<Variant>& oldValues);
// - Array<Serializable@>@ GetAttributeEditorTargets(UIElement@ attrEdit);
// - String GetVariableName(ShortStringHash hash);

const uint MIN_NODE_ATTRIBUTES = 4;
const uint MAX_NODE_ATTRIBUTES = 8;
const int ATTRNAME_WIDTH = 150;
const int ATTR_HEIGHT = 19;
const StringHash TEXT_CHANGED_EVENT_TYPE("TextChanged");

bool inLoadAttributeEditor = false;
bool inEditAttribute = false;
bool showNonEditableAttribute = false;

Color normalTextColor(1.0f, 1.0f, 1.0f);
Color modifiedTextColor(1.0f, 0.8f, 0.5f);
Color nonEditableTextColor(0.7f, 0.7f, 0.7f);

String sceneResourcePath;

UIElement@ SetEditable(UIElement@ element, bool editable)
{
    element.enabled = editable;
    element.colors[C_TOPLEFT] = editable ? element.colors[C_BOTTOMRIGHT] : nonEditableTextColor;
    element.colors[C_BOTTOMLEFT] = element.colors[C_TOPLEFT];
    element.colors[C_TOPRIGHT] = element.colors[C_TOPLEFT];
    return element;
}

UIElement@ SetValue(LineEdit@ element, const String&in value, bool sameValue)
{
    element.text = sameValue ? value : STRIKED_OUT;
    element.cursorPosition = 0;
    return element;
}

UIElement@ SetValue(CheckBox@ element, bool value, bool sameValue)
{
    element.checked = sameValue ? value : false;
    return element;
}

UIElement@ SetValue(DropDownList@ element, int value, bool sameValue)
{
    element.selection = sameValue ? value : M_MAX_UNSIGNED;
    return element;
}

UIElement@ CreateAttributeEditorParentWithSeparatedLabel(ListView@ list, const String&in name, uint index, uint subIndex, bool suppressedSeparatedLabel = false)
{
    UIElement@ editorParent = UIElement("Edit" + String(index) + "_" + String(subIndex));
    editorParent.vars["Index"] = index;
    editorParent.vars["SubIndex"] = subIndex;
    editorParent.SetLayout(LM_VERTICAL, 2);
    list.AddItem(editorParent);

    if (suppressedSeparatedLabel)
    {
        UIElement@ placeHolder = UIElement(name);
        editorParent.AddChild(placeHolder);
    }
    else
    {
        Text@ attrNameText = Text();
        editorParent.AddChild(attrNameText);
        attrNameText.style = "EditorAttributeText";
        attrNameText.text = name;
    }

    return editorParent;
}

UIElement@ CreateAttributeEditorParentAsListChild(ListView@ list, const String&in name, uint index, uint subIndex)
{
    UIElement@ editorParent = UIElement("Edit" + String(index) + "_" + String(subIndex));
    editorParent.vars["Index"] = index;
    editorParent.vars["SubIndex"] = subIndex;
    editorParent.SetLayout(LM_HORIZONTAL);
    list.AddChild(editorParent);

    UIElement@ placeHolder = UIElement(name);
    editorParent.AddChild(placeHolder);

    return editorParent;
}

UIElement@ CreateAttributeEditorParent(ListView@ list, const String&in name, uint index, uint subIndex)
{
    UIElement@ editorParent = UIElement("Edit" + String(index) + "_" + String(subIndex));
    editorParent.vars["Index"] = index;
    editorParent.vars["SubIndex"] = subIndex;
    editorParent.SetLayout(LM_HORIZONTAL);
    editorParent.SetFixedHeight(ATTR_HEIGHT);
    list.AddItem(editorParent);

    Text@ attrNameText = Text();
    editorParent.AddChild(attrNameText);
    attrNameText.style = "EditorAttributeText";
    attrNameText.text = name;
    attrNameText.SetFixedWidth(ATTRNAME_WIDTH);

    return editorParent;
}

LineEdit@ CreateAttributeLineEdit(UIElement@ parent, Array<Serializable@>@ serializables, uint index, uint subIndex)
{
    LineEdit@ attrEdit = LineEdit();
    parent.AddChild(attrEdit);
    attrEdit.style = "EditorAttributeEdit";
    attrEdit.SetFixedHeight(ATTR_HEIGHT - 2);
    attrEdit.vars["Index"] = index;
    attrEdit.vars["SubIndex"] = subIndex;
    SetAttributeEditorID(attrEdit, serializables);

    return attrEdit;
}

UIElement@ CreateStringAttributeEditor(ListView@ list, Array<Serializable@>@ serializables, const AttributeInfo&in info, uint index, uint subIndex)
{
    UIElement@ parent = CreateAttributeEditorParent(list, info.name, index, subIndex);
    LineEdit@ attrEdit = CreateAttributeLineEdit(parent, serializables, index, subIndex);
    attrEdit.dragDropMode = DD_TARGET;
    SubscribeToEvent(attrEdit, "TextChanged", "EditAttribute");
    SubscribeToEvent(attrEdit, "TextFinished", "EditAttribute");

    return parent;
}

UIElement@ CreateBoolAttributeEditor(ListView@ list, Array<Serializable@>@ serializables, const AttributeInfo&in info, uint index, uint subIndex)
{
    bool isUIElement = cast<UIElement>(serializables[0]) !is null;
    UIElement@ parent;
    if (info.name == (isUIElement ? "Is Visible" : "Is Enabled"))
        parent = CreateAttributeEditorParentAsListChild(list, info.name, index, subIndex);
    else
        parent = CreateAttributeEditorParent(list, info.name, index, subIndex);

    CheckBox@ attrEdit = CheckBox();
    parent.AddChild(attrEdit);
    attrEdit.style = AUTO_STYLE;
    attrEdit.vars["Index"] = index;
    attrEdit.vars["SubIndex"] = subIndex;
    SetAttributeEditorID(attrEdit, serializables);
    SubscribeToEvent(attrEdit, "Toggled", "EditAttribute");

    return parent;
}

UIElement@ CreateNumAttributeEditor(ListView@ list, Array<Serializable@>@ serializables, const AttributeInfo&in info, uint index, uint subIndex)
{
    UIElement@ parent = CreateAttributeEditorParent(list, info.name, index, subIndex);
    VariantType type = info.type;
    uint numCoords = type - VAR_FLOAT + 1;
    if (type == VAR_QUATERNION)
        numCoords = 3;
    else if (type == VAR_COLOR || type == VAR_INTRECT)
        numCoords = 4;
    else if (type == VAR_INTVECTOR2)
        numCoords = 2;

    for (uint i = 0; i < numCoords; ++i)
    {
        LineEdit@ attrEdit = CreateAttributeLineEdit(parent, serializables, index, subIndex);
        attrEdit.vars["Coordinate"] = i;
        SubscribeToEvent(attrEdit, "TextChanged", "EditAttribute");
        SubscribeToEvent(attrEdit, "TextFinished", "EditAttribute");
    }

    return parent;
}

UIElement@ CreateIntAttributeEditor(ListView@ list, Array<Serializable@>@ serializables, const AttributeInfo&in info, uint index, uint subIndex)
{
    UIElement@ parent = CreateAttributeEditorParent(list, info.name, index, subIndex);
    // Check for enums
    if (info.enumNames is null || info.enumNames.empty)
    {
        // No enums, create a numeric editor
        LineEdit@ attrEdit = CreateAttributeLineEdit(parent, serializables, index, subIndex);
        SubscribeToEvent(attrEdit, "TextChanged", "EditAttribute");
        SubscribeToEvent(attrEdit, "TextFinished", "EditAttribute");
    }
    else
    {
        DropDownList@ attrEdit = DropDownList();
        parent.AddChild(attrEdit);
        attrEdit.style = AUTO_STYLE;
        attrEdit.SetFixedHeight(ATTR_HEIGHT - 2);
        attrEdit.resizePopup = true;
        attrEdit.placeholderText = STRIKED_OUT;
        attrEdit.vars["Index"] = index;
        attrEdit.vars["SubIndex"] = subIndex;
        attrEdit.SetLayout(LM_HORIZONTAL, 0, IntRect(4, 1, 4, 1));
        SetAttributeEditorID(attrEdit, serializables);

        for (uint i = 0; i < info.enumNames.length; ++i)
        {
            Text@ choice = Text();
            attrEdit.AddItem(choice);
            choice.style = "EditorEnumAttributeText";
            choice.text = info.enumNames[i];
        }
        SubscribeToEvent(attrEdit, "ItemSelected", "EditAttribute");
    }

    return parent;
}

UIElement@ CreateResourceRefAttributeEditor(ListView@ list, Array<Serializable@>@ serializables, const AttributeInfo&in info, uint index, uint subIndex, bool suppressedSeparatedLabel = false)
{
    UIElement@ parent;
    ShortStringHash resourceType;
    VariantType attrType = info.type;
    if (attrType == VAR_RESOURCEREF)
        resourceType = serializables[0].attributes[index].GetResourceRef().type;
    else if (attrType == VAR_RESOURCEREFLIST)
        resourceType = serializables[0].attributes[index].GetResourceRefList().type;
    else if (attrType == VAR_VARIANTVECTOR)
        resourceType = serializables[0].attributes[index].GetVariantVector()[subIndex].GetResourceRef().type;

     // Create the attribute name on a separate non-interactive line to allow for more space
    parent = CreateAttributeEditorParentWithSeparatedLabel(list, info.name, index, subIndex, suppressedSeparatedLabel);

    UIElement@ container = UIElement();
    container.SetLayout(LM_HORIZONTAL, 4, IntRect(info.name.StartsWith("   ") ? 20 : 10, 0, 4, 0));    // Left margin is indented more when the name is so
    container.SetFixedHeight(ATTR_HEIGHT);
    parent.AddChild(container);

    LineEdit@ attrEdit = CreateAttributeLineEdit(container, serializables, index, subIndex);
    attrEdit.vars[TYPE_VAR] = resourceType.value;
    SubscribeToEvent(attrEdit, "TextFinished", "EditAttribute");

    Button@ pickButton = Button();
    container.AddChild(pickButton);
    pickButton.style = AUTO_STYLE;
    pickButton.SetFixedSize(36, ATTR_HEIGHT - 2);
    pickButton.vars["Index"] = index;
    pickButton.vars["SubIndex"] = subIndex;
    SetAttributeEditorID(pickButton, serializables);

    Text@ pickButtonText = Text();
    pickButton.AddChild(pickButtonText);
    pickButtonText.style = "EditorAttributeText";
    pickButtonText.SetAlignment(HA_CENTER, VA_CENTER);
    pickButtonText.text = "Pick";
    SubscribeToEvent(pickButton, "Released", "PickResource");

    Button@ openButton = Button();
    container.AddChild(openButton);
    openButton.style = AUTO_STYLE;
    openButton.SetFixedSize(36, ATTR_HEIGHT - 2);
    openButton.vars["Index"] = index;
    openButton.vars["SubIndex"] = subIndex;
    SetAttributeEditorID(openButton, serializables);

    Text@ openButtonText = Text();
    openButton.AddChild(openButtonText);
    openButtonText.style = "EditorAttributeText";
    openButtonText.SetAlignment(HA_CENTER, VA_CENTER);
    openButtonText.text = "Open";
    SubscribeToEvent(openButton, "Released", "OpenResource");

    return parent;
}

UIElement@ CreateAttributeEditor(ListView@ list, Array<Serializable@>@ serializables, const AttributeInfo&in info, uint index, uint subIndex, bool suppressedSeparatedLabel = false)
{
    UIElement@ parent;

    VariantType type = info.type;
    if (type == VAR_STRING || type == VAR_BUFFER)
        parent = CreateStringAttributeEditor(list, serializables, info, index, subIndex);
    else if (type == VAR_BOOL)
        parent = CreateBoolAttributeEditor(list, serializables, info, index, subIndex);
    else if ((type >= VAR_FLOAT && type <= VAR_VECTOR4) || type == VAR_QUATERNION || type == VAR_COLOR || type == VAR_INTVECTOR2 || type == VAR_INTRECT)
        parent = CreateNumAttributeEditor(list, serializables, info, index, subIndex);
    else if (type == VAR_INT)
        parent = CreateIntAttributeEditor(list, serializables, info, index, subIndex);
    else if (type == VAR_RESOURCEREF)
        parent = CreateResourceRefAttributeEditor(list, serializables, info, index, subIndex, suppressedSeparatedLabel);
    else if (type == VAR_RESOURCEREFLIST)
    {
        uint numRefs = serializables[0].attributes[index].GetResourceRefList().length;

        // Straightly speaking the individual resource reference in the list is not an attribute of the serializable
        // However, the AttributeInfo structure is used here to reduce the number of parameters being passed in the function
        AttributeInfo refInfo;
        refInfo.name = info.name;
        refInfo.type = VAR_RESOURCEREF;
        for (uint i = 0; i < numRefs; ++i)
            CreateAttributeEditor(list, serializables, refInfo, index, i, i > 0);
    }
    else if (type == VAR_VARIANTVECTOR)
    {
        VectorStruct@ vectorStruct = GetVectorStruct(serializables, index);
        if (vectorStruct is null)
            return null;
        uint nameIndex = 0;

        Array<Variant>@ vector = serializables[0].attributes[index].GetVariantVector();
        for (uint i = 0; i < vector.length; ++i)
        {
            // The individual variant in the vector is not an attribute of the serializable, the structure is reused for convenience
            AttributeInfo vectorInfo;
            vectorInfo.name = vectorStruct.variableNames[nameIndex];
            vectorInfo.type = vector[i].type;
            CreateAttributeEditor(list, serializables, vectorInfo, index, i);
            ++nameIndex;
            if (nameIndex >= vectorStruct.variableNames.length)
                nameIndex = vectorStruct.restartIndex;
        }
    }
    else if (type == VAR_VARIANTMAP)
    {
        VariantMap map = serializables[0].attributes[index].GetVariantMap();
        Array<ShortStringHash>@ keys = map.keys;
        for (uint i = 0; i < keys.length; ++i)
        {
            String varName = GetVariableName(keys[i]);
            Variant value = map[keys[i]];

            // The individual variant in the map is not an attribute of the serializable, the structure is reused for convenience
            AttributeInfo mapInfo;
            mapInfo.name = varName + " (Var)";
            mapInfo.type = value.type;
            parent = CreateAttributeEditor(list, serializables, mapInfo, index, i);
            // Add the variant key to the parent
            parent.vars["Key"] = keys[i].value;
            // If variable name is not registered (i.e. it is an editor internal variable) then hide it
            if (varName.empty)
                parent.visible = false;
        }
    }

    return parent;
}

uint GetAttributeEditorCount(Array<Serializable@>@ serializables)
{
    uint count = 0;

    if (!serializables.empty)
    {
        /// \todo When multi-editing, this only counts the editor count of the first serializable
        bool isUIElement = cast<UIElement>(serializables[0]) !is null;
        for (uint i = 0; i < serializables[0].numAttributes; ++i)
        {
            AttributeInfo info = serializables[0].attributeInfos[i];
            if (!showNonEditableAttribute && info.mode & AM_NOEDIT != 0)
                continue;
            // "Is Enabled" is not inserted into the main attribute list, so do not count
            // Similarly, for UIElement, "Is Visible" is not inserted
            if (info.name == (isUIElement ? "Is Visible" : "Is Enabled"))
                continue;
            if (info.type == VAR_RESOURCEREFLIST)
                count += serializables[0].attributes[i].GetResourceRefList().length;
            else if (info.type == VAR_VARIANTVECTOR && GetVectorStruct(serializables, i) !is null)
                count += serializables[0].attributes[i].GetVariantVector().length;
            else if (info.type == VAR_VARIANTMAP)
                count += serializables[0].attributes[i].GetVariantMap().length;
            else
                ++count;
        }
    }

    return count;
}

UIElement@ GetAttributeEditorParent(UIElement@ parent, uint index, uint subIndex)
{
    return parent.GetChild("Edit" + String(index) + "_" + String(subIndex), true);
}

void LoadAttributeEditor(ListView@ list, Array<Serializable@>@ serializables, const AttributeInfo&in info, uint index)
{
    bool editable = info.mode & AM_NOEDIT == 0;

    UIElement@ parent = GetAttributeEditorParent(list, index, 0);
    if (parent is null)
        return;

    inLoadAttributeEditor = true;

    bool sameName = true;
    bool sameValue = true;
    Variant value = serializables[0].attributes[index];
    Array<Variant> values;
    for (uint i = 0; i < serializables.length; ++i)
    {
        if (index >= serializables[i].numAttributes || serializables[i].attributeInfos[index].name != info.name)
        {
            sameName = false;
            break;
        }

        Variant val = serializables[i].attributes[index];
        if (val != value)
            sameValue = false;
        values.Push(val);
    }

    // Attribute with different values from multiple-select is loaded with default/empty value and non-editable
    if (sameName)
        LoadAttributeEditor(parent, value, info, editable, sameValue, values);
    else
        parent.visible = false;

    inLoadAttributeEditor = false;
}

void LoadAttributeEditor(UIElement@ parent, const Variant&in value, const AttributeInfo&in info, bool editable, bool sameValue, const Array<Variant>&in values)
{
    uint index = parent.vars["Index"].GetUInt();

    // Assume the first child is always a text label element or a container that containing a text label element
    UIElement@ label = parent.children[0];
    if (label.type == UI_ELEMENT_TYPE && label.numChildren > 0)
        label = label.children[0];
    if (label.type == TEXT_TYPE)
    {
        bool modified;
        if (info.defaultValue.type == VAR_NONE || info.defaultValue.type == VAR_RESOURCEREFLIST)
            modified = !value.zero;
        else
            modified = value != info.defaultValue;
        cast<Text>(label).color = (editable ? (modified ? modifiedTextColor : normalTextColor) : nonEditableTextColor);
    }

    VariantType type = info.type;
    if (type == VAR_FLOAT || type == VAR_STRING || type == VAR_BUFFER)
        SetEditable(SetValue(parent.children[1], value.ToString(), sameValue), editable && sameValue);
    else if (type == VAR_BOOL)
        SetEditable(SetValue(parent.children[1], value.GetBool(), sameValue), editable && sameValue);
    else if (type == VAR_INT)
    {
        if (info.enumNames is null || info.enumNames.empty)
            SetEditable(SetValue(parent.children[1], value.ToString(), sameValue), editable && sameValue);
        else
            SetEditable(SetValue(parent.children[1], value.GetInt(), sameValue), editable && sameValue);
    }
    else if (type == VAR_RESOURCEREF)
    {
        SetEditable(SetValue(parent.children[1].children[0], cache.GetResourceName(value.GetResourceRef().id), sameValue), editable && sameValue);
        SetEditable(parent.children[1].children[1], editable && sameValue);  // If editable then can pick
        SetEditable(parent.children[1].children[2], sameValue); // If same value then can open
    }
    else if (type == VAR_RESOURCEREFLIST)
    {
        UIElement@ list = parent.parent;
        ResourceRefList refList = value.GetResourceRefList();
        for (uint subIndex = 0; subIndex < refList.length; ++subIndex)
        {
            parent = GetAttributeEditorParent(list, index, subIndex);
            if (parent is null)
                break;

            StringHash firstID = refList.ids[subIndex];
            bool idSameValue = true;
            if (!sameValue)
            {
                // Reevaluate each ID in the list
                for (uint i = 0; i < values.length; ++i)
                {
                    ResourceRefList refList = values[i].GetResourceRefList();
                    if (subIndex >= refList.length || refList.ids[subIndex] != firstID)
                    {
                        idSameValue = false;
                        break;
                    }
                }
            }
            SetEditable(SetValue(parent.children[1].children[0], cache.GetResourceName(firstID), idSameValue), editable && idSameValue);
        }
    }
    else if (type == VAR_VARIANTVECTOR)
    {
        UIElement@ list = parent.parent;
        Array<Variant>@ vector = value.GetVariantVector();
        for (uint subIndex = 0; subIndex < vector.length; ++subIndex)
        {
            parent = GetAttributeEditorParent(list, index, subIndex);
            if (parent is null)
                break;

            Variant firstValue = vector[subIndex];
            bool sameValue = true;
            Array<Variant> varValues;

            // Reevaluate aach variant in the vector
            for (uint i = 0; i < values.length; ++i)
            {
                Array<Variant>@ vector = values[i].GetVariantVector();
                if (subIndex < vector.length)
                {
                    Variant value = vector[subIndex];
                    varValues.Push(value);
                    if (value != firstValue)
                        sameValue = false;
                }
                else
                    sameValue = false;
            }

            // The individual variant in the list is not an attribute of the serializable, the structure is reused for convenience
            AttributeInfo info;
            info.type = firstValue.type;
            LoadAttributeEditor(parent, firstValue, info, editable, sameValue, varValues);
        }
    }
    else if (type == VAR_VARIANTMAP)
    {
        UIElement@ list = parent.parent;
        VariantMap map = value.GetVariantMap();
        Array<ShortStringHash>@ keys = map.keys;
        for (uint subIndex = 0; subIndex < keys.length; ++subIndex)
        {
            parent = GetAttributeEditorParent(list, index, subIndex);
            if (parent is null)
                break;

            String varName = GetVariableName(keys[subIndex]);
            if (varName.empty)
                continue;

            Variant firstValue = map[keys[subIndex]];
            bool sameValue = true;
            Array<Variant> varValues;

            // Reevaluate each variant in the map
            for (uint i = 0; i < values.length; ++i)
            {
                VariantMap map = values[i].GetVariantMap();
                if (map.Contains(keys[subIndex]))
                {
                    Variant value = map[keys[subIndex]];
                    varValues.Push(value);
                    if (value != firstValue)
                       sameValue = false;
                }
                else
                    sameValue = false;
            }

            // The individual variant in the map is not an attribute of the serializable, the structure is reused for convenience
            AttributeInfo info;
            info.type = firstValue.type;
            LoadAttributeEditor(parent, firstValue, info, editable, sameValue, varValues);
        }
    }
    else
    {
        Array<Array<String> > coordinates;
        for (uint i = 0; i < values.length; ++i)
        {
            Variant value = values[i];

            // Convert Quaternion value to Vector3 value first
            if (type == VAR_QUATERNION)
                value = value.GetQuaternion().eulerAngles;

            coordinates.Push(value.ToString().Split(' '));
        }
        for (uint i = 0; i < coordinates[0].length; ++i)
        {
            String value = coordinates[0][i];
            bool coordinateSameValue = true;
            if (!sameValue)
            {
                // Reevaluate each coordinate
                for (uint j = 1; j < coordinates.length; ++j)
                {
                    if (coordinates[j][i] != value)
                    {
                        coordinateSameValue = false;
                        break;
                    }
                }
            }
            SetEditable(SetValue(parent.children[i + 1], value, coordinateSameValue), editable && coordinateSameValue);
        }
    }
}

void StoreAttributeEditor(UIElement@ parent, Array<Serializable@>@ serializables, uint index, uint subIndex, uint coordinate)
{
    AttributeInfo info = serializables[0].attributeInfos[index];

    if (info.type == VAR_RESOURCEREFLIST)
    {
        for (uint i = 0; i < serializables.length; ++i)
        {
            ResourceRefList refList = serializables[i].attributes[index].GetResourceRefList();
            Variant[] values(1);
            GetEditorValue(parent, VAR_RESOURCEREF, null, coordinate, values);
            ResourceRef ref = values[0].GetResourceRef();
            refList.ids[subIndex] = ref.id;
            serializables[i].attributes[index] = Variant(refList);
        }
    }
    else if (info.type == VAR_VARIANTVECTOR)
    {
        for (uint i = 0; i < serializables.length; ++i)
        {
            Array<Variant>@ vector = serializables[i].attributes[index].GetVariantVector();
            Variant[] values;
            values.Push(vector[subIndex]);  // Each individual variant may have multiple coordinates itself
            GetEditorValue(parent, vector[subIndex].type, null, coordinate, values);
            vector[subIndex] = values[0];
            serializables[i].attributes[index] = Variant(vector);
        }
    }
    else if (info.type == VAR_VARIANTMAP)
    {
        VariantMap map = serializables[0].attributes[index].GetVariantMap();
        ShortStringHash key(parent.vars["Key"].GetUInt());
        for (uint i = 0; i < serializables.length; ++i)
        {
            VariantMap map = serializables[i].attributes[index].GetVariantMap();
            Variant[] values;
            values.Push(map[key]);  // Each individual variant may have multiple coordinates itself
            GetEditorValue(parent, map[key].type, null, coordinate, values);
            map[key] = values[0];
            serializables[i].attributes[index] = Variant(map);
        }
    }
    else
    {
        Array<Variant> values;
        for (uint i = 0; i < serializables.length; ++i)
            values.Push(serializables[i].attributes[index]);
        GetEditorValue(parent, info.type, info.enumNames, coordinate, values);
        for (uint i = 0; i < serializables.length; ++i)
            serializables[i].attributes[index] = values[i];
    }
}

void FillValue(Array<Variant>& values, const Variant&in value)
{
    for (uint i = 0; i < values.length; ++i)
        values[i] = value;
}

void SanitizeNumericalValue(VariantType type, String& value)
{
    if (type >= VAR_FLOAT && type <= VAR_COLOR)
        value = String(value.ToFloat());
    else if (type == VAR_INT || type == VAR_INTRECT || type == VAR_INTVECTOR2)
        value = String(value.ToInt());
}

void GetEditorValue(UIElement@ parent, VariantType type, Array<String>@ enumNames, uint coordinate, Array<Variant>& values)
{
    LineEdit@ attrEdit = parent.children[coordinate + 1];
    if (type == VAR_STRING)
        FillValue(values, Variant(attrEdit.text.Trimmed()));
    else if (type == VAR_BOOL)
    {
        CheckBox@ attrEdit = parent.children[1];
        FillValue(values, Variant(attrEdit.checked));
    }
    else if (type == VAR_FLOAT)
        FillValue(values, Variant(attrEdit.text.ToFloat()));
    else if (type == VAR_QUATERNION)
    {
        float value = attrEdit.text.ToFloat();
        for (uint i = 0; i < values.length; ++i)
        {
            float[] data = values[i].GetQuaternion().eulerAngles.data;
            data[coordinate] = value;
            values[i] = Quaternion(Vector3(data));
        }
    }
    else if (type == VAR_INT)
    {
        if (enumNames is null || enumNames.empty)
            FillValue(values, Variant(attrEdit.text.ToInt()));
        else
        {
            DropDownList@ attrEdit = parent.children[1];
            FillValue(values, Variant(attrEdit.selection));
        }
    }
    else if (type == VAR_RESOURCEREF)
    {
        LineEdit@ attrEdit = parent.children[0];
        ResourceRef ref;
        ref.id = StringHash(attrEdit.text.Trimmed());
        ref.type = ShortStringHash(attrEdit.vars[TYPE_VAR].GetUInt());
        FillValue(values, Variant(ref));
    }
    else
    {
        String value = attrEdit.text;
        SanitizeNumericalValue(type, value);
        for (uint i = 0; i < values.length; ++i)
        {
            String[] data = values[i].ToString().Split(' ');
            data[coordinate] = value;
            values[i] = Variant(type, Join(data, " "));
        }
    }
}

void UpdateAttributes(Array<Serializable@>@ serializables, ListView@ list, bool& fullUpdate)
{
    // If attributes have changed structurally, do a full update
    uint count = GetAttributeEditorCount(serializables);
    if (fullUpdate == false)
    {
        if (list.contentElement.numChildren != count)
            fullUpdate = true;
    }

    // Remember the old scroll position so that a full update does not feel as jarring
    IntVector2 oldViewPos = list.viewPosition;

    if (fullUpdate)
    {
        list.RemoveAllItems();
        Array<UIElement@> children = list.GetChildren();
        for (uint i = 0; i < children.length; ++i)
        {
            if (!children[i].internal)
                children[i].Remove();
        }
    }

    if (serializables.empty)
        return;

    // If there are many serializables, they must share same attribute structure (up to certain number if not all)
    for (uint i = 0; i < serializables[0].numAttributes; ++i)
    {
        AttributeInfo info = serializables[0].attributeInfos[i];
        if (!showNonEditableAttribute && info.mode & AM_NOEDIT != 0)
            continue;

        // Use the default value (could be instance's default value) of the first serializable as the default for all
        info.defaultValue = serializables[0].attributeDefaults[i];

        if (fullUpdate)
            CreateAttributeEditor(list, serializables, info, i, 0);

        LoadAttributeEditor(list, serializables, info, i);
    }

    if (fullUpdate)
        list.viewPosition = oldViewPos;
}

void EditAttribute(StringHash eventType, VariantMap& eventData)
{
    // Changing elements programmatically may cause events to be sent. Stop possible infinite loop in that case.
    if (inLoadAttributeEditor)
        return;

    UIElement@ attrEdit = eventData["Element"].GetUIElement();
    UIElement@ parent = attrEdit.parent;
    Array<Serializable@>@ serializables = GetAttributeEditorTargets(attrEdit);
    if (serializables.empty)
        return;

    uint index = attrEdit.vars["Index"].GetUInt();
    uint subIndex = attrEdit.vars["SubIndex"].GetUInt();
    uint coordinate = attrEdit.vars["Coordinate"].GetUInt();
    bool intermediateEdit = eventType == TEXT_CHANGED_EVENT_TYPE;

    // Do the editor pre logic before attribute is being modified
    if (!PreEditAttribute(serializables, index))
        return;

    inEditAttribute = true;

    // Store old values so that PostEditAttribute can create undo actions
    Array<Variant> oldValues;
    for (uint i = 0; i < serializables.length; ++i)
        oldValues.Push(serializables[i].attributes[index]);

    StoreAttributeEditor(parent, serializables, index, subIndex, coordinate);
    for (uint i = 0; i < serializables.length; ++i)
        serializables[i].ApplyAttributes();

    // Do the editor post logic after attribute has been modified.
    PostEditAttribute(serializables, index, oldValues);

    inEditAttribute = false;

    // If not an intermediate edit, reload the editor fields with validated values
    // (attributes may have interactions; therefore we load everything, not just the value being edited)
    if (!intermediateEdit)
        attributesDirty = true;
}

// Resource picker functionality

class ResourcePicker
{
    String resourceType;
    String lastPath;
    uint lastFilter;
    Array<String> filters;

    ResourcePicker(const String&in resourceType_, const String&in filter_)
    {
        resourceType = resourceType_;
        filters.Push(filter_);
        filters.Push("*.*");
        lastFilter = 0;
    }

    ResourcePicker(const String&in resourceType_, const Array<String>@ filters_)
    {
        resourceType = resourceType_;
        filters = filters_;
        filters.Push("*.*");
        lastFilter = 0;
    }
};

Array<ResourcePicker@> resourcePickers;
Array<Serializable@> resourceTargets;
uint resourcePickIndex = 0;
uint resourcePickSubIndex = 0;
ResourcePicker@ resourcePicker = null;

void InitResourcePicker()
{
    // Fill resource picker data
    Array<String> fontFilters = {"*.ttf", "*.fnt", "*.xml"};
    Array<String> imageFilters = {"*.png", "*.jpg"};
    Array<String> textureFilters = {"*.dds", "*.png", "*.jpg", "*.bmp", "*.ktx", "*.pvr"};
    Array<String> soundFilters = {"*.wav","*.ogg"};
    Array<String> scriptFilters = {"*.as", "*.asc"};
    Array<String> materialFilters = {"*.xml", "*.material"};
    resourcePickers.Push(ResourcePicker("Animation", "*.ani"));
    resourcePickers.Push(ResourcePicker("Font", fontFilters));
    resourcePickers.Push(ResourcePicker("Image", imageFilters));
    resourcePickers.Push(ResourcePicker("Model", "*.mdl"));
    resourcePickers.Push(ResourcePicker("Material", materialFilters));
    resourcePickers.Push(ResourcePicker("Texture2D", textureFilters));
    resourcePickers.Push(ResourcePicker("TextureCube", "*.xml"));
    resourcePickers.Push(ResourcePicker("ScriptFile", scriptFilters));
    resourcePickers.Push(ResourcePicker("XMLFile", "*.xml"));
    resourcePickers.Push(ResourcePicker("Sound", soundFilters));
    sceneResourcePath = AddTrailingSlash(fileSystem.programDir + "Data");
}

ResourcePicker@ GetResourcePicker(const String&in resourceType)
{
    for (uint i = 0; i < resourcePickers.length; ++i)
    {
        if (resourcePickers[i].resourceType.Compare(resourceType, false) == 0)
            return resourcePickers[i];
    }
    return null;
}

void PickResource(StringHash eventType, VariantMap& eventData)
{
    UIElement@ button = eventData["Element"].GetUIElement();
    LineEdit@ attrEdit = button.parent.children[0];

    Array<Serializable@>@ targets = GetAttributeEditorTargets(attrEdit);
    if (targets.empty)
        return;

    resourcePickIndex = attrEdit.vars["Index"].GetUInt();
    resourcePickSubIndex = attrEdit.vars["SubIndex"].GetUInt();
    AttributeInfo info = targets[0].attributeInfos[resourcePickIndex];

    if (info.type == VAR_RESOURCEREF)
    {
        String resourceType = GetTypeName(targets[0].attributes[resourcePickIndex].GetResourceRef().type);
        @resourcePicker = GetResourcePicker(resourceType);
    }
    else if (info.type == VAR_RESOURCEREFLIST)
    {
        String resourceType = GetTypeName(targets[0].attributes[resourcePickIndex].GetResourceRefList().type);
        @resourcePicker = GetResourcePicker(resourceType);
    }
    else if (info.type == VAR_VARIANTVECTOR)
    {
        String resourceType = GetTypeName(targets[0].attributes[resourcePickIndex].GetVariantVector()
            [resourcePickSubIndex].GetResourceRef().type);
        @resourcePicker = GetResourcePicker(resourceType);
    }
    else
        @resourcePicker = null;

    if (resourcePicker is null)
        return;

    resourceTargets.Clear();
    for (uint i = 0; i < targets.length; ++i)
        resourceTargets.Push(targets[i]);

    String lastPath = resourcePicker.lastPath;
    if (lastPath.empty)
        lastPath = sceneResourcePath;
    CreateFileSelector("Pick " + resourcePicker.resourceType, "OK", "Cancel", lastPath, resourcePicker.filters, resourcePicker.lastFilter);
    SubscribeToEvent(uiFileSelector, "FileSelected", "PickResourceDone");
}

void PickResourceDone(StringHash eventType, VariantMap& eventData)
{
    // Store filter and directory for next time
    if (resourcePicker !is null)
    {
        resourcePicker.lastPath = uiFileSelector.path;
        resourcePicker.lastFilter = uiFileSelector.filterIndex;
    }

    CloseFileSelector();

    if (!eventData["OK"].GetBool())
    {
        resourceTargets.Clear();
        @resourcePicker = null;
        return;
    }

    if (resourcePicker is null)
        return;

    // Validate the resource. It must come from within a registered resource directory, and be loaded successfully
    String resourceName = eventData["FileName"].GetString();
    Array<String>@ resourceDirs = cache.resourceDirs;
    Resource@ res;

    for (uint i = 0; i < resourceDirs.length; ++i)
    {
        if (!resourceName.ToLower().StartsWith(resourceDirs[i].ToLower()))
            continue;
        resourceName = resourceName.Substring(resourceDirs[i].length);
        res = cache.GetResource(resourcePicker.resourceType, resourceName);
        break;
    }
    if (res is null) {
        log.Warning("Cannot find resource type: " + resourcePicker.resourceType + " Name:" +resourceName);
        return;
    }

    // Store old values so that PostEditAttribute can create undo actions
    Array<Variant> oldValues;
    for (uint i = 0; i < resourceTargets.length; ++i)
        oldValues.Push(resourceTargets[i].attributes[resourcePickIndex]);

    for (uint i = 0; i < resourceTargets.length; ++i)
    {
        Serializable@ target = resourceTargets[i];

        AttributeInfo info = target.attributeInfos[resourcePickIndex];
        if (info.type == VAR_RESOURCEREF)
        {
            ResourceRef ref = target.attributes[resourcePickIndex].GetResourceRef();
            ref.type = ShortStringHash(resourcePicker.resourceType);
            ref.id = StringHash(resourceName);
            target.attributes[resourcePickIndex] = Variant(ref);
            target.ApplyAttributes();
        }
        else if (info.type == VAR_RESOURCEREFLIST)
        {
            ResourceRefList refList = target.attributes[resourcePickIndex].GetResourceRefList();
            if (resourcePickSubIndex < refList.length)
            {
                refList.ids[resourcePickSubIndex] = StringHash(resourceName);
                target.attributes[resourcePickIndex] = Variant(refList);
                target.ApplyAttributes();
            }
        }
        else if (info.type == VAR_VARIANTVECTOR)
        {
            Array<Variant>@ attrs = target.attributes[resourcePickIndex].GetVariantVector();
            ResourceRef ref = attrs[resourcePickSubIndex].GetResourceRef();
            ref.type = ShortStringHash(resourcePicker.resourceType);
            ref.id = StringHash(resourceName);
            attrs[resourcePickSubIndex] = ref;
            target.attributes[resourcePickIndex] = Variant(attrs);
            target.ApplyAttributes();
        }
    }

    PostEditAttribute(resourceTargets, resourcePickIndex, oldValues);
    UpdateAttributeInspector(false);

    resourceTargets.Clear();
    @resourcePicker = null;
}

void OpenResource(StringHash eventType, VariantMap& eventData)
{
    UIElement@ button = eventData["Element"].GetUIElement();
    LineEdit@ attrEdit = button.parent.children[0];

    String fileName = attrEdit.text.Trimmed();
    if (fileName.empty)
        return;

    Array<String>@ resourceDirs = cache.resourceDirs;
    for (uint i = 0; i < resourceDirs.length; ++i)
    {
        String fullPath = resourceDirs[i] + fileName;
        if (fileSystem.FileExists(fullPath))
        {
            fileSystem.SystemOpen(fullPath, "");
            return;
        }
    }
}

// VariantVector decoding & editing for certain components

class VectorStruct
{
    String componentTypeName;
    String attributeName;
    Array<String> variableNames;
    uint restartIndex;

    VectorStruct(const String&in componentTypeName_, const String&in attributeName_, const Array<String>@ variableNames_, uint restartIndex_)
    {
        componentTypeName = componentTypeName_;
        attributeName = attributeName_;
        variableNames = variableNames_;
        restartIndex = restartIndex_;
    }
};

Array<VectorStruct@> vectorStructs;

void InitVectorStructs()
{
    // Fill vector structure data
    Array<String> billboardVariables = {
        "Billboard Count",
        "   Position",
        "   Size",
        "   UV Coordinates",
        "   Color",
        "   Rotation",
        "   Is Enabled"
    };
    vectorStructs.Push(VectorStruct("BillboardSet", "Billboards", billboardVariables, 1));

    Array<String> animationStateVariables = {
        "Anim State Count",
        "   Animation",
        "   Start Bone",
        "   Is Looped",
        "   Weight",
        "   Time",
        "   Layer"
    };
    vectorStructs.Push(VectorStruct("AnimatedModel", "Animation States", animationStateVariables, 1));

    Array<String> particleColorVariables = {
        "Color Animation Frames",
        "   Color",
        "   Time"
    };
    vectorStructs.Push(VectorStruct("ParticleEmitter", "Particle Colors", particleColorVariables, 1));

    Array<String> particleUVAnimVariables = {
        "UV Animation Frames",
        "   UV Coords",
        "   Time"
    };
    vectorStructs.Push(VectorStruct("ParticleEmitter", "UV Animation", particleUVAnimVariables, 1));
}

VectorStruct@ GetVectorStruct(Array<Serializable@>@ serializables, uint index)
{
    AttributeInfo info = serializables[0].attributeInfos[index];
    for (uint i = 0; i < vectorStructs.length; ++i)
    {
        if (vectorStructs[i].componentTypeName == serializables[0].typeName && vectorStructs[i].attributeName == info.name)
            return vectorStructs[i];
    }
    return null;
}
