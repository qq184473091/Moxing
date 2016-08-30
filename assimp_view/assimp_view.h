/*
---------------------------------------------------------------------------
Open Asset Import Library (assimp)
---------------------------------------------------------------------------

Copyright (c) 2006-2016, assimp team

All rights reserved.

Redistribution and use of this software in source and binary forms,
with or without modification, are permitted provided that the following
conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the assimp team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the assimp team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
---------------------------------------------------------------------------
*/

#if (!defined AV_MAIN_H_INCLUDED)
#define AV_MAIN_H_INCLUDED

#define AI_SHADER_COMPILE_FLAGS D3DXSHADER_USE_LEGACY_D3DX9_31_DLL

// include resource definitions
//#include "resource.h"

#include <assert.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <stdio.h>
#include <time.h>

// Include ASSIMP headers (XXX: do we really need all of them?)
#include <assimp/cimport.h>
#include <assimp/Importer.hpp>
#include <assimp/ai_assert.h>
#include <assimp/cfileio.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/DefaultLogger.hpp>


//#include "../../code/MaterialSystem.h"   // aiMaterial class
//#include "../../code/StringComparison.h" // ASSIMP_stricmp and ASSIMP_strincmp

#include <time.h>

// default movement speed
#define MOVE_SPEED 3.f

#include "AssetHelper.h"
#include "Camera.h"
#include "RenderOptions.h"
#include "Shaders.h"
//#include "Background.h"
//#include "LogDisplay.h"
//#include "LogWindow.h"
//#include "Display.h"
#include "MeshRenderer.h"
#include "MaterialManager.h"


// outside of namespace, to help Intellisense and solve boost::metatype_stuff_miracle
#include "AnimEvaluator.h"
#include "SceneAnimator.h"




//-------------------------------------------------------------------------------
// Position of the cursor relative to the 3ds max' like control circle
//-------------------------------------------------------------------------------
enum EClickPos
{
	// The click was inside the inner circle (x,y axis)
	EClickPos_Circle,
	// The click was inside one of the vertical snap-ins
	EClickPos_CircleVert,
	// The click was inside onf of the horizontal snap-ins
	EClickPos_CircleHor,
	// the cklick was outside the circle (z-axis)
	EClickPos_Outside
};

#if (!defined AI_VIEW_CAPTION_BASE)
#   define AI_VIEW_CAPTION_BASE "Open Asset Import Library : Viewer "
#endif // !! AI_VIEW_CAPTION_BASE


#endif // !! AV_MAIN_H_INCLUDED
