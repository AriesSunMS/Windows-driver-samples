/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    ToneGenerator.h

Abstract:

    Declaration of Virtual Audio Device sine wave generator.
--*/
#ifndef _VIRTUALAUDIODEVICE_TONEGENERATOR_H
#define _VIRTUALAUDIODEVICE_TONEGENERATOR_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <limits.h>

typedef struct _CHUNK_HEADER
{
    CHAR szId[4] = { 0 };
    LONG lSize = 0;
} CHUNK_HEADER;

typedef struct _FMT_CHUNK
{
    SHORT sAudioFormat = 0;
    SHORT sNumChannels = 0;
    LONG lSampleRate = 0;
    LONG lByteRate = 0;
    SHORT sBlockAlign = 0;
    SHORT sBitsPerSample = 0;
} FMT_CHUNK;

class ToneGenerator
{
public:
    DWORD           m_Frequency; 
    WORD            m_ChannelCount; 
    WORD            m_BitsPerSample;
    DWORD           m_SamplesPerSecond;
    double          m_Theta;
    double          m_SampleIncrement;  
    bool            m_Mute;
    BYTE*           m_PartialFrame;
    DWORD           m_PartialFrameBytes;
    DWORD           m_FrameSize;
    double          m_ToneAmplitude;
    double          m_ToneDCOffset;
    PETHREAD        m_ThreadObject = nullptr;
    bool            m_Stop = false;
    KSPIN_LOCK      m_BufferSpinLock;
    
    BYTE*           m_RingBuffer = nullptr;
    DWORD           m_WritePos = 0;
    DWORD           m_ReadPos = 0;
    DWORD           m_DataSize = 0;
    DWORD           m_BufferSize = 0;

public:
    ToneGenerator();
    ~ToneGenerator();
    
    NTSTATUS Init(_In_ DWORD ToneFrequency, _In_ double ToneAmplitude, _In_ double ToneDCOffset, _In_ double ToneInitialPhase, _In_ PWAVEFORMATEXTENSIBLE WfExt);
    
    VOID GenerateSine(_Out_writes_bytes_(BufferLength) BYTE* Buffer, _In_ size_t BufferLength);

    VOID SetMute(_In_ bool Value)
    {
        m_Mute = Value;
    }

    BOOL isBufferEmpty();
    BOOL isBufferFull();
    DWORD getUsedSize();
    DWORD GetFrameAvailableCount();
    DWORD GetFreeFrameCount();
    DWORD AddFramesToBuffer(FMT_CHUNK* fmtHeader, BYTE* data, ULONG length);
    DWORD GetFramesFromBuffer(_Out_writes_bytes_(BufferLength) BYTE* Buffer, _In_ size_t BufferLength);

private:
    VOID InitNewFrame(_Out_writes_bytes_(FrameSize) BYTE* Frame, _In_ DWORD FrameSize);
};

#endif // _VIRTUALAUDIODEVICE_TONEGENERATOR_H
