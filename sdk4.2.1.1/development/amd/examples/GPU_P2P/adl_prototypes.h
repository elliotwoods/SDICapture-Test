

#pragma once

#include "ADL/adl_sdk.h"

typedef int ( *ADL_MAIN_CONTROL_CREATE )(ADL_MAIN_MALLOC_CALLBACK, int );
typedef int ( *ADL_MAIN_CONTROL_DESTROY )();
typedef int ( *ADL_ADAPTER_NUMBEROFADAPTERS_GET ) ( int* );
typedef int ( *ADL_ADAPTER_ACTIVE_GET ) ( int, int* );
typedef int ( *ADL_ADAPTER_ADAPTERINFO_GET ) ( LPAdapterInfo, int );
typedef int ( *ADL_ADAPTER_ACTIVE_GET ) ( int, int* );
typedef int ( *ADL_DISPLAY_DISPLAYINFO_GET ) ( int, int *, ADLDisplayInfo **, int );
typedef int ( *ADL_DISPLAY_POSITION_GET ) (int, int, int*, int*, int*,int*, int*, int*,int*, int*, int*,int*);
typedef int ( *ADL_DISPLAY_PROPERTY_GET ) ( int, int, ADLDisplayProperty* );


static ADL_MAIN_CONTROL_CREATE          ADL_Main_Control_Create;
static ADL_MAIN_CONTROL_DESTROY         ADL_Main_Control_Destroy;
static ADL_ADAPTER_NUMBEROFADAPTERS_GET ADL_Adapter_NumberOfAdapters_Get;
static ADL_ADAPTER_ACTIVE_GET           ADL_Adapter_Active_Get;
static ADL_ADAPTER_ADAPTERINFO_GET      ADL_Adapter_AdapterInfo_Get;
static ADL_DISPLAY_DISPLAYINFO_GET      ADL_Display_DisplayInfo_Get;
static ADL_DISPLAY_POSITION_GET		 ADL_Display_Position_Get;
static ADL_DISPLAY_PROPERTY_GET		 ADL_Display_Property_Get;

