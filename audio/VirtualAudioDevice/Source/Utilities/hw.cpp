/*++

Copyright (c)  Microsoft Corporation All Rights Reserved

Module Name:

    hw.cpp

Abstract:

    Implementation of Virtual Audio Device HW class. 
    Virtual Audio Device HW has an array for storing mixer and volume settings
    for the topology.
--*/
#include "definitions.h"
#include "hw.h"

//=============================================================================
// CVirtualAudioDeviceHW
//=============================================================================

//=============================================================================
#pragma code_seg("PAGE")
CVirtualAudioDeviceHW::CVirtualAudioDeviceHW()
: m_ulMux(0),
    m_bDevSpecific(FALSE),
    m_iDevSpecific(0),
    m_uiDevSpecific(0)
/*++

Routine Description:

    Constructor for VirtualAudioDeviceHW. 

Arguments:

Return Value:

    void

--*/
{
    PAGED_CODE();
    
    MixerReset();
} // VirtualAudioDeviceHW
#pragma code_seg()


//=============================================================================
BOOL
CVirtualAudioDeviceHW::bGetDevSpecific()
/*++

Routine Description:

  Gets the HW (!) Device Specific info

Arguments:

  N/A

Return Value:

  True or False (in this example).

--*/
{
    return m_bDevSpecific;
} // bGetDevSpecific

//=============================================================================
void
CVirtualAudioDeviceHW::bSetDevSpecific
(
    _In_  BOOL                bDevSpecific
)
/*++

Routine Description:

  Sets the HW (!) Device Specific info

Arguments:

  fDevSpecific - true or false for this example.

Return Value:

    void

--*/
{
    m_bDevSpecific = bDevSpecific;
} // bSetDevSpecific

//=============================================================================
INT
CVirtualAudioDeviceHW::iGetDevSpecific()
/*++

Routine Description:

  Gets the HW (!) Device Specific info

Arguments:

  N/A

Return Value:

  int (in this example).

--*/
{
    return m_iDevSpecific;
} // iGetDevSpecific

//=============================================================================
void
CVirtualAudioDeviceHW::iSetDevSpecific
(
    _In_  INT                 iDevSpecific
)
/*++

Routine Description:

  Sets the HW (!) Device Specific info

Arguments:

  fDevSpecific - true or false for this example.

Return Value:

    void

--*/
{
    m_iDevSpecific = iDevSpecific;
} // iSetDevSpecific

//=============================================================================
UINT
CVirtualAudioDeviceHW::uiGetDevSpecific()
/*++

Routine Description:

  Gets the HW (!) Device Specific info

Arguments:

  N/A

Return Value:

  UINT (in this example).

--*/
{
    return m_uiDevSpecific;
} // uiGetDevSpecific

//=============================================================================
void
CVirtualAudioDeviceHW::uiSetDevSpecific
(
    _In_  UINT                uiDevSpecific
)
/*++

Routine Description:

  Sets the HW (!) Device Specific info

Arguments:

  uiDevSpecific - int for this example.

Return Value:

    void

--*/
{
    m_uiDevSpecific = uiDevSpecific;
} // uiSetDevSpecific


//=============================================================================
BOOL
CVirtualAudioDeviceHW::GetMixerMute
(
    _In_  ULONG                   ulNode,
    _In_  ULONG                   ulChannel
)
/*++

Routine Description:

  Gets the HW (!) mute levels for Virtual Audio Device

Arguments:

  ulNode - topology node id
  
  ulChannel - which channel are we reading?

Return Value:

  mute setting

--*/
{
    UNREFERENCED_PARAMETER(ulChannel);
    
    if (ulNode < MAX_TOPOLOGY_NODES)
    {
        return m_MuteControls[ulNode];
    }

    return 0;
} // GetMixerMute

//=============================================================================
ULONG                       
CVirtualAudioDeviceHW::GetMixerMux()
/*++

Routine Description:

  Return the current mux selection

Arguments:

Return Value:

  ULONG

--*/
{
    return m_ulMux;
} // GetMixerMux

//=============================================================================
LONG
CVirtualAudioDeviceHW::GetMixerVolume
(   
    _In_  ULONG                   ulNode,
    _In_  ULONG                   ulChannel
)
/*++

Routine Description:

  Gets the HW (!) volume for Virtual Audio Device.

Arguments:

  ulNode - topology node id

  ulChannel - which channel are we reading?

Return Value:

  LONG - volume level

--*/
{
    UNREFERENCED_PARAMETER(ulChannel);

    if (ulNode < MAX_TOPOLOGY_NODES)
    {
        return m_VolumeControls[ulNode];
    }

    return 0;
} // GetMixerVolume

//=============================================================================
LONG
CVirtualAudioDeviceHW::GetMixerPeakMeter
(   
    _In_  ULONG                   ulNode,
    _In_  ULONG                   ulChannel
)
/*++

Routine Description:

  Gets the HW (!) peak meter for Virtual Audio Device

Arguments:

  ulNode - topology node id

  ulChannel - which channel are we reading?

Return Value:

  LONG - sample peak meter level

--*/
{
    UNREFERENCED_PARAMETER(ulChannel);

    if (ulNode < MAX_TOPOLOGY_NODES)
    {
        return m_PeakMeterControls[ulNode];
    }

    return 0;
} // GetMixerVolume

//=============================================================================
#pragma code_seg("PAGE")
void 
CVirtualAudioDeviceHW::MixerReset()
/*++

Routine Description:

  Resets the mixer registers.

Arguments:

Return Value:

    void

--*/
{
    PAGED_CODE();
    
    RtlFillMemory(m_VolumeControls, sizeof(LONG) * MAX_TOPOLOGY_NODES, 0xFF);
    // Endpoints are not muted by default.
    RtlZeroMemory(m_MuteControls, sizeof(BOOL) * MAX_TOPOLOGY_NODES);

    for (ULONG i=0; i<MAX_TOPOLOGY_NODES; ++i)
    {
        m_PeakMeterControls[i] = PEAKMETER_SIGNED_MAXIMUM/2;
    }
    
    // BUGBUG change this depending on the topology
    m_ulMux = 2;
} // MixerReset
#pragma code_seg()

//=============================================================================
void
CVirtualAudioDeviceHW::SetMixerMute
(
    _In_  ULONG                   ulNode,
    _In_  ULONG                   ulChannel,
    _In_  BOOL                    fMute
)
/*++

Routine Description:

  Sets the HW (!) mute levels for Virtual Audio Device

Arguments:

  ulNode - topology node id

  ulChannel - which channel are we setting?
  
  fMute - mute flag

Return Value:

    void

--*/
{
    UNREFERENCED_PARAMETER(ulChannel);

    if (ulNode < MAX_TOPOLOGY_NODES)
    {
        m_MuteControls[ulNode] = fMute;
    }
} // SetMixerMute

//=============================================================================
void                        
CVirtualAudioDeviceHW::SetMixerMux
(
    _In_  ULONG                   ulNode
)
/*++

Routine Description:

  Sets the HW (!) mux selection

Arguments:

  ulNode - topology node id

Return Value:

    void

--*/
{
    m_ulMux = ulNode;
} // SetMixMux

//=============================================================================
void  
CVirtualAudioDeviceHW::SetMixerVolume
(   
    _In_  ULONG                   ulNode,
    _In_  ULONG                   ulChannel,
    _In_  LONG                    lVolume
)
/*++

Routine Description:

  Sets the HW (!) volume for Virtual Audio Device

Arguments:

  ulNode - topology node id

  ulChannel - which channel are we setting?

  lVolume - volume level

Return Value:

    void

--*/
{
    UNREFERENCED_PARAMETER(ulChannel);

    if (ulNode < MAX_TOPOLOGY_NODES)
    {
        m_VolumeControls[ulNode] = lVolume;
    }
} // SetMixerVolume
