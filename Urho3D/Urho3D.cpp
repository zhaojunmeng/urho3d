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

#include "Context.h"
#include "Engine.h"
#include "FileSystem.h"
#include "Log.h"
#include "ProcessUtils.h"
#include "ResourceCache.h"
#include "ScriptFile.h"
#include "Time.h"

#include <stdexcept>
#include <windows.h>

#include "DebugNew.h"

void Run(const char* cmdLine);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
    #if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    #endif
    
    #if defined(ENABLE_MINIDUMPS) && !defined(_DEBUG)
    __try
    {
        Run(cmdLine);
    }
    __except(WriteMiniDump("Urho3D", GetExceptionInformation())) {}
    #else
    Run(cmdLine);
    #endif
    
    return 0;
}

void Run(const char* cmdLine)
{
    // Check for script file name
    std::vector<std::string> arguments = ParseArguments(cmdLine);
    std::string scriptFileName;
    for (unsigned i = 0; i < arguments.size(); ++i)
    {
        if (arguments[i][0] != '-')
        {
            scriptFileName = arguments[i];
            break;
        }
    }
    
    // Show usage if not found
    if (scriptFileName.empty())
    {
        ErrorDialog("Urho3D", "Usage: Urho3D <scriptfile> [options]\n\n"
            "The script file should implement the function void Start(), which should create the scene content and subscribe to "
            "all necessary events, such as the application update."
        );
        return;
    }
    
    // Create the execution context and the engine
    SharedPtr<Context> context(new Context());
    SharedPtr<Engine> engine(new Engine(context));
    if (!engine->Initialize("Urho3D", "Urho3D.log", arguments))
    {
        ErrorDialog("Urho3D", context->GetSubsystem<Log>()->GetLastMessage().c_str());
        return;
    }
    
    // Set 5 ms timer period to allow accurate FPS limiting up to 200 FPS
    context->GetSubsystem<Time>()->SetTimerPeriod(5);
    
    // Execute the Start function from the script file, then run the engine loop until exited
    ScriptFile* scriptFile = context->GetSubsystem<ResourceCache>()->GetResource<ScriptFile>(scriptFileName);
    if ((scriptFile) && (scriptFile->Execute("void Start()")))
    {
        while (!engine->IsExiting())
            engine->RunFrame();
    }
    else
    {
        engine->Exit(); // Close the rendering window
        ErrorDialog("Urho3D", context->GetSubsystem<Log>()->GetLastMessage().c_str());
    }
}