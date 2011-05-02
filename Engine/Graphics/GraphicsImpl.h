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

#pragma once

#include "Color.h"

#include <windows.h>
#include <d3d9.h>

/// Graphics implementation. Contains the main Direct3D9 objects
class GraphicsImpl
{
    friend class Graphics;
    
public:
    /// Construct
    GraphicsImpl();
    
    /// Return Direct3D device
    IDirect3DDevice9* GetDevice() const { return device_; }
    /// Return device capabilities
    const D3DCAPS9& GetDeviceCaps() const { return deviceCaps_; }
    /// Return window handle
    HWND GetWindowHandle() const { return window_; }
    /// Return adapter identifier
    const D3DADAPTER_IDENTIFIER9& GetAdapterIdentifier() const { return adapterIdentifier_; }
    /// Return whether a texture format and usage is supported
    bool CheckFormatSupport(D3DFORMAT format, DWORD usage, D3DRESOURCETYPE type);
    /// Return desktop texture format
    D3DFORMAT GetDesktopFormat();
    /// Return desktop width/height
    IntVector2 GetDesktopResolution();
    
private:
    /// Direct3D interface
    IDirect3D9* interface_;
    /// Direct3D device
    IDirect3DDevice9* device_;
    /// Default color surface
    IDirect3DSurface9* defaultColorSurface_;
    /// Default depth stencil surfac
    IDirect3DSurface9* defaultDepthStencilSurface_;
    /// Frame query for flushing the GPU command queue
    IDirect3DQuery9* frameQuery_;
    /// Adapter number
    DWORD adapter_;
    /// Device type
    D3DDEVTYPE deviceType_;
    /// Device capabilities
    D3DCAPS9 deviceCaps_;
    /// Adapter identifier
    D3DADAPTER_IDENTIFIER9 adapterIdentifier_;
    /// Application instance
    HINSTANCE instance_;
    /// Application window
    HWND window_;
    /// Direct3D presentation parameters
    D3DPRESENT_PARAMETERS presentParams_;
    /// Texture min/mag filter modes in use
    D3DTEXTUREFILTERTYPE minMagFilter_[MAX_TEXTURE_UNITS];
    /// Texture mip filter modes in use
    D3DTEXTUREFILTERTYPE mipFilter_[MAX_TEXTURE_UNITS];
    /// Texture U coordinate addressing modes in use
    D3DTEXTUREADDRESS uAddressMode_[MAX_TEXTURE_UNITS];
    /// Texture V coordinate addressing modes in use
    D3DTEXTUREADDRESS vAddressMode_[MAX_TEXTURE_UNITS];
    /// Texture border colors in use
    Color borderColor_[MAX_TEXTURE_UNITS];
    /// Color surfaces in use
    IDirect3DSurface9* colorSurface_[MAX_RENDERTARGETS];
    /// Depth stencil buffer in use
    IDirect3DSurface9* depthStencilSurface_;
    /// Blending enabled flag
    DWORD blendEnable_;
    /// Source blend mode
    D3DBLEND srcBlend_;
    /// Destination blend mode
    D3DBLEND destBlend_;
};