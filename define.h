#pragma once

#ifdef ASSIMP_VIEW_DLL
#ifdef ASSIMP_VIEW_EXPORT
#define ASSIMP_VIEW_API __declspec(dllexport)
#else
#define ASSIMP_VIEW_API __declspec(dllimport)
#endif	//ASSIMP_VIEW_EXPORT
#else
#define ASSIMP_VIEW_API 
#endif	//ASSIMP_VIEW_DLL
