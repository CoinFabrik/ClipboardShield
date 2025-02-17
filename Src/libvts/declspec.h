#pragma once

#ifdef BUILDING_LIBVTS
#define LIBVTS_EXPORT __declspec(dllexport)
#else
//#define LIBVTS_EXPORT __declspec(dllimport)
#define LIBVTS_EXPORT
#endif
