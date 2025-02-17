#pragma once

#include "targetver.h"
#define NOMINMAX
#include <windows.h>
#include <Shlwapi.h>
#include <TlHelp32.h>
#include <stdio.h>
#include <string>
#include <memory>
#include <limits>
#include <vector>
#include "NktHookLib.h"

typedef enum {
  atOther,
  atRunDll32,
  atIisService,
  atIisWorker
} eApplicationType;

//#define USE_VORNEX_DLL
