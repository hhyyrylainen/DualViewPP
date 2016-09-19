// Leviathan Game Engine
// Copyright (c) 2012-2016 Henri Hyyryläinen
#pragma once
//
// File configured by CMake do not edit Include.h //
//

// UE4 Plugin version
#if 0 == 1

#ifndef LEVIATHAN_BUILD
#define LEVIATHAN_ASSERT(x, msg) ensureMsgf(x, TEXT(msg))
#endif //LEVIATHAN_BUILD

#define DEBUG_BREAK LEVIATHAN_ASSERT(false, "Debug Break");

#define LEVIATHAN_UE_PLUGIN
#define ALTERNATIVE_EXCEPTIONS_FATAL
#define ALLOW_INTERNAL_EXCEPTIONS
#define NO_DEFAULT_DATAINDEX

#else

// If defined some select classes are leaked into global namespace
#define LEAK_INTO_GLOBAL
#define ALLOW_INTERNAL_EXCEPTIONS
#define LEVIATHAN_FULL
#endif

#define LEVIATHAN_NO_DEBUG 0
#if LEVIATHAN_NO_DEBUG == 0
#undef LEVIATHAN_NO_DEBUG
#define LEVIATHAN_DEBUG
#endif //LEVIATHAN_NO_DEBUG

#define LEVIATHAN_USING_ANGELSCRIPT 0
#if LEVIATHAN_USING_ANGELSCRIPT == 0
#undef LEVIATHAN_USING_ANGELSCRIPT
#endif //LEVIATHAN_USING_ANGELSCRIPT

#define LEVIATHAN_USING_BOOST 1
#if LEVIATHAN_USING_BOOST == 0
#undef LEVIATHAN_USING_BOOST
#endif //LEVIATHAN_USING_BOOST

#define LEVIATHAN_USING_OGRE 0
#if LEVIATHAN_USING_OGRE == 0
#undef LEVIATHAN_USING_OGRE
#endif //LEVIATHAN_USING_OGRE

#define LEVIATHAN_USING_NEWTON 0
#if LEVIATHAN_USING_NEWTON == 0
#undef LEVIATHAN_USING_NEWTON
#endif //LEVIATHAN_USING_NEWTON

#define LEVIATHAN_USING_SFML 0
#if LEVIATHAN_USING_SFML == 0
#undef LEVIATHAN_USING_SFML
#else
#define SFML_PACKETS
#endif //LEVIATHAN_USING_SFML

#define LEVIATHAN_USING_LEAP 0
#if LEVIATHAN_USING_LEAP == 0
#undef LEVIATHAN_USING_LEAP
#endif

#define LEVIATHAN_USING_BREAKPAD 0
#if LEVIATHAN_USING_BREAKPAD == 0
#undef LEVIATHAN_USING_BREAKPAD
#endif

#define LEVIATHAN_VERSION 0.800
#define LEVIATHAN_VERSIONS L"0.8.0.0"
#define LEVIATHAN_VERSION_ANSIS "0.8.0.0"

#define LEVIATHAN_VERSION_STABLE    0
#define LEVIATHAN_VERSION_MAJOR     8
#define LEVIATHAN_VERSION_MINOR     0
#define LEVIATHAN_VERSION_PATCH     0

#define LEVIATHAN

#ifdef __GNUC__
#define __FUNCSIG__ __PRETTY_FUNCTION__
#endif

#ifndef DLLEXPORT
#ifdef ENGINE_EXPORTS
#ifdef _WIN32
#define DLLEXPORT    __declspec( dllexport )
#else
// This might not be needed for gcc
#define DLLEXPORT   __attribute__ ((visibility ("default")))
#endif
// Json-cpp //
#define JSON_DLL_BUILD
#else

#ifdef _WIN32
#define DLLEXPORT __declspec( dllimport )
#else
#define DLLEXPORT 
#endif


#define JSON_DLL
#endif // ENGINE_EXPORTS
#endif

#ifndef FORCE_INLINE
#ifndef _WIN32

#define FORCE_INLINE __attribute__((always_inline))
    
#else
// Windows needs these //
#define FORCE_INLINE    __forceinline
#endif    
#endif //FORCE_INLINE

#ifndef NOT_UNUSED
#define NOT_UNUSED(x) (void)x;
#endif //NOT_UNUSED


