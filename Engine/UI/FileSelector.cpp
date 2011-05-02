//
// Urho3D Engine
// Copyright (c) 2008-2011 Lasse ��rni
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "Precompiled.h"
#include "Context.h"
#include "DropDownList.h"
#include "File.h"
#include "FileSelector.h"
#include "FileSystem.h"
#include "InputEvents.h"
#include "LineEdit.h"
#include "ListView.h"
#include "Text.h"
#include "UI.h"
#include "UIEvents.h"
#include "Window.h"

#include <algorithm>

#include "DebugNew.h"

static const std::string emptyFilter;

static bool CompareEntries(const FileSelectorEntry& lhs, const FileSelectorEntry& rhs)
{
    if ((lhs.directory_) && (!rhs.directory_))
        return true;
    if ((!lhs.directory_) && (rhs.directory_))
        return false;
    return lhs.name_ < rhs.name_;
}

OBJECTTYPESTATIC(FileSelector);

FileSelector::FileSelector(Context* context) :
    Object(context),
    ignoreEvents_(false),
    directoryMode_(false)
{
    window_ = new Window(context_);
    window_->SetLayout(LM_VERTICAL);
    
    titleLayout = new UIElement(context_);
    titleLayout->SetLayout(LM_HORIZONTAL);
    window_->AddChild(titleLayout);
    
    titleText_ = new Text(context_);
    titleLayout->AddChild(titleText_);
    
    closeButton_ = new Button(context_);
    titleLayout->AddChild(closeButton_);
    
    pathEdit_ = new LineEdit(context_);
    window_->AddChild(pathEdit_);
    
    fileList_ = new ListView(context_);
    window_->AddChild(fileList_);
    
    fileNameLayout_ = new UIElement(context_);
    fileNameLayout_->SetLayout(LM_HORIZONTAL);
    
    fileNameEdit_ = new LineEdit(context_);
    fileNameLayout_->AddChild(fileNameEdit_);
    
    filterList_ = new DropDownList(context_);
    fileNameLayout_->AddChild(filterList_);
    
    window_->AddChild(fileNameLayout_);
    
    buttonLayout_ = new UIElement(context_);
    buttonLayout_->SetLayout(LM_HORIZONTAL);
    
    buttonLayout_->AddChild(new UIElement(context_)); // Add spacer
    
    okButton_ = new Button(context_);
    okButtonText_ = new Text(context_);
    okButtonText_->SetAlignment(HA_CENTER, VA_CENTER);
    okButton_->AddChild(okButtonText_);
    buttonLayout_->AddChild(okButton_);
    
    buttonLayout_->AddChild(new UIElement(context_)); // Add spacer
    
    cancelButton_ = new Button(context_);
    cancelButtonText_ = new Text(context_);
    cancelButtonText_->SetAlignment(HA_CENTER, VA_CENTER);
    cancelButton_->AddChild(cancelButtonText_);
    buttonLayout_->AddChild(cancelButton_);
    
    buttonLayout_->AddChild(new UIElement(context_)); // Add spacer
    
    window_->AddChild(buttonLayout_);
    
    UI* ui = GetSubsystem<UI>();
    if (ui)
        ui->GetRootElement()->AddChild(window_);
    
    std::vector<std::string> defaultFilters;
    defaultFilters.push_back("*.*");
    SetFilters(defaultFilters, 0);
    FileSystem* fileSystem = GetSubsystem<FileSystem>();
    if (fileSystem)
        SetPath(fileSystem->GetCurrentDir());
    
    // Focus the fileselector's filelist initially when created, and bring to front
    if (ui)
        ui->SetFocusElement(fileList_);
    window_->BringToFront();
    
    SubscribeToEvent(filterList_, E_ITEMSELECTED, HANDLER(FileSelector, HandleFilterChanged));
    SubscribeToEvent(pathEdit_, E_TEXTFINISHED, HANDLER(FileSelector, HandlePathChanged));
    SubscribeToEvent(fileNameEdit_, E_TEXTFINISHED, HANDLER(FileSelector, HandleOKPressed));
    SubscribeToEvent(fileList_, E_ITEMSELECTED, HANDLER(FileSelector, HandleFileSelected));
    SubscribeToEvent(fileList_, E_ITEMDOUBLECLICKED, HANDLER(FileSelector, HandleFileDoubleClicked));
    SubscribeToEvent(fileList_, E_UNHANDLEDKEY, HANDLER(FileSelector, HandleFileListKey));
    SubscribeToEvent(okButton_, E_RELEASED, HANDLER(FileSelector, HandleOKPressed));
    SubscribeToEvent(cancelButton_, E_RELEASED, HANDLER(FileSelector, HandleCancelPressed));
    SubscribeToEvent(closeButton_, E_RELEASED, HANDLER(FileSelector, HandleCancelPressed));
}

FileSelector::~FileSelector()
{
    UI* ui = GetSubsystem<UI>();
    if (ui)
        ui->GetRootElement()->RemoveChild(window_);
}

void FileSelector::RegisterObject(Context* context)
{
    context->RegisterFactory<FileSelector>();
}

void FileSelector::SetStyle(XMLFile* style)
{
    if (!style)
        return;
    
    style_ = style;
    
    window_->SetStyleAuto(style);
    window_->SetStyle(style, "FileSelector");
    
    titleText_->SetStyle(style, "FileSelectorTitleText");
    closeButton_->SetStyle(style, "CloseButton");
    
    okButtonText_->SetStyle(style, "FileSelectorButtonText");
    cancelButtonText_->SetStyle(style, "FileSelectorButtonText");
    
    titleLayout->SetStyle(style, "FileSelectorTitleLayout");
    fileNameLayout_->SetStyle(style, "FileSelectorLayout");
    buttonLayout_->SetStyle(style, "FileSelectorLayout");
    
    fileList_->SetStyleAuto(style);
    fileNameEdit_->SetStyleAuto(style);
    pathEdit_->SetStyleAuto(style);
    
    filterList_->SetStyleAuto(style);
    filterList_->SetStyle(style, "FileSelectorFilterList");
    
    okButton_->SetStyleAuto(style);
    cancelButton_->SetStyleAuto(style);
    okButton_->SetStyle(style, "FileSelectorButton");
    cancelButton_->SetStyle(style, "FileSelectorButton");
    
    std::vector<UIElement*> filterTexts = filterList_->GetListView()->GetContentElement()->GetChildren();
    for (unsigned i = 0; i < filterTexts.size(); ++i)
        filterTexts[i]->SetStyle(style, "FileSelectorFilterText");
    
    std::vector<UIElement*> listTexts = fileList_->GetContentElement()->GetChildren();
    for (unsigned i = 0; i < listTexts.size(); ++i)
        listTexts[i]->SetStyle(style, "FileSelectorListText");
    
    UpdateElements();
}

void FileSelector::SetTitle(const std::string& text)
{
    titleText_->SetText(text);
}

void FileSelector::SetButtonTexts(const std::string& okText, const std::string& cancelText)
{
    okButtonText_->SetText(okText);
    cancelButtonText_->SetText(cancelText);
}

void FileSelector::SetPath(const std::string& path)
{
    FileSystem* fileSystem = GetSubsystem<FileSystem>();
    if (!fileSystem)
        return;
    
    if (fileSystem->DirExists(path))
    {
        path_ = AddTrailingSlash(path);
        SetLineEditText(pathEdit_, path_);
        RefreshFiles();
    }
    else
    {
        // If path was invalid, restore the old path to the line edit
        if (pathEdit_->GetText() != path_)
            SetLineEditText(pathEdit_, path_);
    }
}

void FileSelector::SetFileName(const std::string& fileName)
{
    SetLineEditText(fileNameEdit_, fileName);
}

void FileSelector::SetFilters(const std::vector<std::string>& filters, unsigned defaultIndex)
{
    if (filters.empty())
        return;
    
    ignoreEvents_ = true;
    
    filters_ = filters;
    filterList_->RemoveAllItems();
    for (unsigned i = 0; i < filters_.size(); ++i)
    {
        Text* filterText = new Text(context_);
        filterText->SetText(filters_[i]);
        filterText->SetStyle(style_, "FileSelectorFilterText");
        filterList_->AddItem(filterText);
    }
    if (defaultIndex > filters.size())
        defaultIndex = 0;
    filterList_->SetSelection(defaultIndex);
    
    ignoreEvents_ = false;
    
    if (GetFilter() != lastUsedFilter_)
        RefreshFiles();
}

void FileSelector::SetDirectoryMode(bool enable)
{
    directoryMode_ = enable;
}

void FileSelector::UpdateElements()
{
    {
        const IntRect& clipBorder = pathEdit_->GetClipBorder();
        pathEdit_->SetFixedHeight(pathEdit_->GetTextElement()->GetRowHeight() + clipBorder.top_ + clipBorder.bottom_);
    }
    
    {
        const IntRect& clipBorder = fileNameEdit_->GetClipBorder();
        int fileNameHeight = fileNameEdit_->GetTextElement()->GetRowHeight() + clipBorder.top_ + clipBorder.bottom_;
        fileNameEdit_->SetFixedHeight(fileNameHeight);
        filterList_->SetFixedHeight(fileNameHeight);
        fileNameLayout_->SetFixedHeight(fileNameHeight);
    }
    
    buttonLayout_->SetFixedHeight(Max(okButton_->GetHeight(), cancelButton_->GetHeight()));
}

const std::string& FileSelector::GetFileName() const
{
    return fileNameEdit_->GetText();
}

const std::string& FileSelector::GetFilter() const
{
    Text* selectedFilter = static_cast<Text*>(filterList_->GetSelectedItem());
    if (selectedFilter)
        return selectedFilter->GetText();
    
    return emptyFilter;
}

unsigned FileSelector::GetFilterIndex() const
{
    return filterList_->GetSelection();
}

void FileSelector::SetLineEditText(LineEdit* edit, const std::string& text)
{
    ignoreEvents_ = true;
    edit->SetText(text);
    ignoreEvents_ = false;
}

void FileSelector::RefreshFiles()
{
    FileSystem* fileSystem = GetSubsystem<FileSystem>();
    if (!fileSystem)
        return;
    
    ignoreEvents_ = true;
    
    fileList_->RemoveAllItems();
    fileEntries_.clear();
    
    std::vector<std::string> directories;
    std::vector<std::string> files;
    fileSystem->ScanDir(directories, path_, "*.*", SCAN_DIRS, false);
    fileSystem->ScanDir(files, path_, GetFilter(), SCAN_FILES, false);
    
    for (unsigned i = 0; i < directories.size(); ++i)
    {
        FileSelectorEntry newEntry;
        newEntry.name_ = directories[i];
        newEntry.directory_ = true;
        fileEntries_.push_back(newEntry);
    }
    
    for (unsigned i = 0; i < files.size(); ++i)
    {
        FileSelectorEntry newEntry;
        newEntry.name_ = files[i];
        newEntry.directory_ = false;
        fileEntries_.push_back(newEntry);
    }
    
    // Sort and add to the list view
    // While items are being added, disable layout update for performance optimization
    std::sort(fileEntries_.begin(), fileEntries_.end(), CompareEntries);
    UIElement* listContent = fileList_->GetContentElement();
    listContent->DisableLayoutUpdate();
    for (unsigned i = 0; i < fileEntries_.size(); ++i)
    {
        std::string displayName;
        if (fileEntries_[i].directory_)
            displayName = "<DIR> " + fileEntries_[i].name_;
        else
            displayName = fileEntries_[i].name_;
        
        Text* entryText = new Text(context_);
        entryText->SetText(displayName);
        entryText->SetStyle(style_, "FileSelectorListText");
        fileList_->AddItem(entryText);
    }
    listContent->EnableLayoutUpdate();
    listContent->UpdateLayout();
    
    ignoreEvents_ = false;
    
    // Clear filename from the previous dir so that there is no confusion
    SetFileName(std::string());
    lastUsedFilter_ = GetFilter();
}

bool FileSelector::EnterFile()
{
    unsigned index = fileList_->GetSelection();
    if (index >= fileEntries_.size())
        return false;
    
    if (fileEntries_[index].directory_)
    {
        // If a directory doubleclicked, enter it. Recognize . and .. as a special case
        const std::string& newPath = fileEntries_[index].name_;
        if ((newPath != ".") &&  (newPath != ".."))
            SetPath(path_ + newPath);
        else if (newPath == "..")
        {
            std::string parentPath = GetParentPath(path_);
            SetPath(parentPath);
        }
        
        return true;
    }
    else
    {
        // Doubleclicking a file is the same as pressing OK
        if (!directoryMode_)
        {
            using namespace FileSelected;
        
            VariantMap eventData;
            eventData[P_FILENAME] = path_ + fileEntries_[index].name_;
            eventData[P_OK] = true;
            SendEvent(E_FILESELECTED, eventData);
        }
    }
    
    return false;
}

void FileSelector::HandleFilterChanged(StringHash eventType, VariantMap& eventData)
{
    if (ignoreEvents_)
        return;
    
    if (GetFilter() != lastUsedFilter_)
        RefreshFiles();
}

void FileSelector::HandlePathChanged(StringHash eventType, VariantMap& eventData)
{
    if (ignoreEvents_)
        return;
    
    // Attempt to set path. Restores old if does not exist
    SetPath(pathEdit_->GetText());
}

void FileSelector::HandleFileSelected(StringHash eventType, VariantMap& eventData)
{
    if (ignoreEvents_)
        return;
    
    unsigned index = fileList_->GetSelection();
    if (index >= fileEntries_.size())
        return;
    // If a file selected, update the filename edit field
    if (!fileEntries_[index].directory_)
        SetFileName(fileEntries_[index].name_);
}

void FileSelector::HandleFileDoubleClicked(StringHash eventType, VariantMap& eventData)
{
    if (ignoreEvents_)
        return;
    
    EnterFile();
}

void FileSelector::HandleFileListKey(StringHash eventType, VariantMap& eventData)
{
    if (ignoreEvents_)
        return;
    
    using namespace UnhandledKey;
    
    if (eventData[P_KEY].GetInt() == KEY_RETURN)
    {
        bool entered = EnterFile();
        // When a key is used to enter a directory, select the first file if no selection
        if ((entered) && (!fileList_->GetSelectedItem()))
            fileList_->SetSelection(0);
    }
}

void FileSelector::HandleOKPressed(StringHash eventType, VariantMap& eventData)
{
    if (ignoreEvents_)
        return;
    
    const std::string& fileName = GetFileName();
    
    if (!directoryMode_)
    {
        if (!fileName.empty())
        {
            using namespace FileSelected;
            
            VariantMap newEventData;
            newEventData[P_FILENAME] = path_ + GetFileName();
            newEventData[P_OK] = true;
            SendEvent(E_FILESELECTED, newEventData);
        }
    }
    else if ((eventType == E_RELEASED) && (!path_.empty()))
    {
        using namespace FileSelected;
        
        VariantMap newEventData;
        newEventData[P_FILENAME] = path_;
        newEventData[P_OK] = true;
        SendEvent(E_FILESELECTED, newEventData);
    }
}

void FileSelector::HandleCancelPressed(StringHash eventType, VariantMap& eventData)
{
    if (ignoreEvents_)
        return;
    
    using namespace FileSelected;
    
    VariantMap newEventData;
    newEventData[P_FILENAME] = std::string();
    newEventData[P_OK] = false;
    SendEvent(E_FILESELECTED, newEventData);
}