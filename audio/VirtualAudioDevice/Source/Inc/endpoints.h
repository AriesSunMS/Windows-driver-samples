/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    endpoints.h

Abstract:

    Node and Pin numbers and other common definitions for virtual audio device.
--*/

#ifndef _VIRTUALAUDIODEVICE_ENDPOINTS_H_
#define _VIRTUALAUDIODEVICE_ENDPOINTS_H_

// Name Guid
// {0104947F-82AE-4291-A6F3-5E2DE1AD7DC2}
#define STATIC_NAME_VIRTUAL_AUDIO_DEVICE\
    0xc0356e10, 0x9c55, 0x4f81, 0xb5, 0xc7, 0x46, 0xf1, 0xbd, 0x4a, 0x72, 0x80
DEFINE_GUIDSTRUCT("C0356E10-9C55-4F81-B5C7-46F1BD4A7280", NAME_VIRTUAL_AUDIO_DEVICE);
#define NAME_VIRTUAL_AUDIO_DEVICE DEFINE_GUIDNAMED(NAME_VIRTUAL_AUDIO_DEVICE)

//----------------------------------------------------
// New defines for the render endpoints.
//----------------------------------------------------

// Default pin instances.
#define MAX_INPUT_SYSTEM_STREAMS        1

// Wave pins - no mix, no offload
enum 
{
    KSPIN_WAVE_RENDER3_SINK_SYSTEM = 0, 
    KSPIN_WAVE_RENDER3_SOURCE
};

// Wave pins - offloading is NOT supported.
enum 
{
    KSPIN_WAVE_RENDER2_SINK_SYSTEM = 0, 
    KSPIN_WAVE_RENDER2_SINK_LOOPBACK, 
    KSPIN_WAVE_RENDER2_SOURCE
};

// Wave Topology nodes - offloading is NOT supported.
enum 
{
    KSNODE_WAVE_SUM = 0,
    KSNODE_WAVE_VOLUME,
    KSNODE_WAVE_MUTE,
    KSNODE_WAVE_PEAKMETER
};

// Topology pins.
enum
{
    KSPIN_TOPO_WAVEOUT_SOURCE = 0,
    KSPIN_TOPO_LINEOUT_DEST,
};

// Topology nodes.
enum
{
    KSNODE_TOPO_WAVEOUT_VOLUME = 0,
    KSNODE_TOPO_WAVEOUT_MUTE,
    KSNODE_TOPO_WAVEOUT_PEAKMETER
};

//----------------------------------------------------
// New defines for the capture endpoints.
//----------------------------------------------------

// Default pin instances.
#define MAX_INPUT_STREAMS           1       // Number of capture streams.

// Wave pins
enum 
{
    KSPIN_WAVE_BRIDGE = 0,
    KSPIN_WAVEIN_HOST,
};

// Wave Topology nodes.
enum 
{
    KSNODE_WAVE_ADC = 0
};

// Wave Topology nodes.
enum 
{
    KSNODE_WAVE_DAC = 0
};

// topology pins.
enum
{
    KSPIN_TOPO_MIC_ELEMENTS,
    KSPIN_TOPO_BRIDGE
};

// topology nodes.
enum
{
    KSNODE_TOPO_VOLUME,
    KSNODE_TOPO_MUTE,
    KSNODE_TOPO_PEAKMETER
};

// data format attribute range definitions.
static
KSATTRIBUTE PinDataRangeSignalProcessingModeAttribute =
{
    sizeof(KSATTRIBUTE),
    0,
    STATICGUIDOF(KSATTRIBUTEID_AUDIOSIGNALPROCESSING_MODE),
};

static
PKSATTRIBUTE PinDataRangeAttributes[] =
{
    &PinDataRangeSignalProcessingModeAttribute,
};

static
KSATTRIBUTE_LIST PinDataRangeAttributeList =
{
    ARRAYSIZE(PinDataRangeAttributes),
    PinDataRangeAttributes,
};

#endif // _VIRTUALAUDIODEVICE_ENDPOINTS_H_
