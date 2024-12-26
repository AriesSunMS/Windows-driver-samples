/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    ToneGenerator

Abstract:

    Implementation of Simple Audio Sample sine wave generator

--*/
#pragma warning (disable : 4127)

#include "definitions.h"
#include "ToneGenerator.h"

#define DEFAULT_READ_FRAME_COUNT         1024

const double TWO_PI = M_PI * 2;

extern DWORD g_DisableToneGenerator;

//
// Double to long conversion.
//
long ConvertToLong(double Value)
{
    return (long)(Value * _I32_MAX);
};

//
// Double to short conversion.
//
short ConvertToShort(double Value)
{
    return (short)(Value * _I16_MAX);
};

//
// Double to char conversion.
//
unsigned char ConvertToUChar(double Value)
{
    const double F_127_5 = 127.5;
    return (unsigned char)(Value * F_127_5 + F_127_5);
};

VOID ReadWaveFile(PVOID context)
{
    ToneGenerator* generator = reinterpret_cast<ToneGenerator*>(context);

    HANDLE fileHandle = nullptr;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK ioStatusBlock;
    UNICODE_STRING fileName;
    NTSTATUS status;

    RtlInitUnicodeString(&fileName, L"\\DosDevices\\C:\\1.wav");
    InitializeObjectAttributes(&objectAttributes, &fileName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwCreateFile(&fileHandle, GENERIC_READ, &objectAttributes, &ioStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    if (!NT_SUCCESS(status))
    {
        DPF(D_TERSE, ("[ReadWaveFile], file not found"));
        PsTerminateSystemThread(STATUS_SUCCESS);
        return;
    }
    
    ULONG headError = 0;
    FMT_CHUNK fmtChunk = { 0 };
    LARGE_INTEGER offset = { 0 };
    while (1)
    {
        CHUNK_HEADER chunkHeader = { 0 };
        status = ZwReadFile(fileHandle, NULL, NULL, NULL, &ioStatusBlock, &chunkHeader, sizeof(chunkHeader), &offset, NULL);
        if (!NT_SUCCESS(status) || ioStatusBlock.Information != sizeof(chunkHeader))
        {
            headError = TRUE;
            break;
        }
        offset.QuadPart += (ULONG)ioStatusBlock.Information;
        
        if (memcmp(chunkHeader.szId, "RIFF", 4) == 0)
        {
            offset.QuadPart += 4;
        }
        else if (memcmp(chunkHeader.szId, "fmt ", 4) == 0)
        {
            status = ZwReadFile(fileHandle, NULL, NULL, NULL, &ioStatusBlock, &fmtChunk, sizeof(fmtChunk), &offset, NULL);
            if (!NT_SUCCESS(status) || ioStatusBlock.Information != sizeof(fmtChunk))
            {
                headError = TRUE;
                break;
            }
            offset.QuadPart += (ULONG)chunkHeader.lSize;
        }
        else if (memcmp(chunkHeader.szId, "data", 4) == 0)
        {
            break;
        }
        else
        {
            offset.QuadPart += (ULONG)chunkHeader.lSize;
        }
    }
    
    if (headError)
    {
        DPF(D_TERSE, ("[ReadWaveFile], read header error"));
        ZwClose(fileHandle);
        PsTerminateSystemThread(STATUS_SUCCESS);
        return;
    }

    DPF(D_TERSE, ("[ReadWaveFile], sampleRate:%d, channel:%d, bits:%d", fmtChunk.lSampleRate, fmtChunk.sNumChannels, fmtChunk.sBitsPerSample));

    ULONG frameSize = fmtChunk.sNumChannels * fmtChunk.sBitsPerSample / 8;
    ULONG bufLen = DEFAULT_READ_FRAME_COUNT * frameSize;
    PBYTE pDataBuffer = (PBYTE)ExAllocatePool2(POOL_FLAG_NON_PAGED, bufLen, VIRTUALAUDIODEVICE_POOLTAG);
    if (!pDataBuffer)
    {
        DPF(D_TERSE, ("[Could not allocate memory for Reading Data]"));
        ZwClose(fileHandle);
        PsTerminateSystemThread(STATUS_SUCCESS);
        return;
    }

    LONGLONG headLength = offset.QuadPart;
    while (!generator->m_Stop)
    {
        if (generator->GetFreeFrameCount() >= DEFAULT_READ_FRAME_COUNT)
        {
            status = ZwReadFile(fileHandle, NULL, NULL, NULL, &ioStatusBlock, pDataBuffer, bufLen, &offset, NULL);
            if (status == STATUS_END_OF_FILE)
            {
                offset.QuadPart = headLength;
                continue;
            }
            else if (!NT_SUCCESS(status))
            {
                DPF(D_TERSE, ("[ReadWaveFile], read failed, bytes:%d", (ULONG)ioStatusBlock.Information));
                break;
            }

            ULONG bytesRead = (ULONG)ioStatusBlock.Information;
            if (bytesRead == 0)
            {
                DPF(D_TERSE, ("[ReadWaveFile], read failed, 0 bytes"));
                break;
            }

            if (bytesRead % frameSize)
            {
                bytesRead = bytesRead / frameSize * frameSize;
            }

            offset.QuadPart += bytesRead;

            // indicate the float value
            if (fmtChunk.sAudioFormat == 3)
            {
                for (ULONG i = 0; i < bytesRead; i += 4)
                {
                    FLOAT fValue = 0.0;
                    RtlCopyMemory(&fValue, pDataBuffer + i, 4);
                    LONG iValue = (LONG)(fValue * _I32_MAX);
                    RtlCopyMemory(pDataBuffer + i, &iValue, 4);
                }
            }

            generator->AddFramesToBuffer(&fmtChunk, pDataBuffer, bytesRead);
        }

        LARGE_INTEGER Interval;
        NTSTATUS Status;
        // Sleep a little bit (100 nanoseconds unit)
        Interval.QuadPart = -100000;

        // Put the thread to sleep
        Status = KeDelayExecutionThread(KernelMode, FALSE, &Interval);

        if (NT_SUCCESS(Status))
        {
            // The thread successfully slept for the specified interval
        }
        else
        {
            // Handle the error
        }
    }

    ZwClose(fileHandle);

    ExFreePoolWithTag(pDataBuffer, VIRTUALAUDIODEVICE_POOLTAG);
    pDataBuffer = NULL;

    // Terminate the thread when done
    PsTerminateSystemThread(STATUS_SUCCESS);
}

//
// Ctor: basic init.
//
ToneGenerator::ToneGenerator()
    : m_Frequency(0),
    m_ChannelCount(0),
    m_BitsPerSample(0),
    m_SamplesPerSecond(0),
    m_Mute(false),
    m_PartialFrame(NULL),
    m_PartialFrameBytes(0),
    m_FrameSize(0)
{
    // Theta (double) and SampleIncrement (double) are init in the Init() method 
    // after saving the floating point state. 

    KeInitializeSpinLock(&m_BufferSpinLock);
}

//
// Dtor: free resources.
//
ToneGenerator::~ToneGenerator()
{
    if (m_PartialFrame)
    {
        ExFreePoolWithTag(m_PartialFrame, VIRTUALAUDIODEVICE_POOLTAG);
        m_PartialFrame = NULL;
        m_PartialFrameBytes = 0;
    }

    m_Stop = true;
    if (m_ThreadObject)
    {
        // Wait for the thread to terminate
        KeWaitForSingleObject(m_ThreadObject, Executive, KernelMode, FALSE, NULL);

        // Dereference the thread object
        ObDereferenceObject(m_ThreadObject);
    }
}

// 
// Init a new frame. 
// Note: caller will save and restore the floatingpoint state.
//
#pragma warning(push)
// Caller wraps this routine between KeSaveFloatingPointState/KeRestoreFloatingPointState calls.
#pragma warning(disable: 28110)

VOID ToneGenerator::InitNewFrame(_Out_writes_bytes_(FrameSize)BYTE* Frame, _In_ DWORD FrameSize)
{
    double sinValue = m_ToneDCOffset + m_ToneAmplitude * sin(m_Theta);

    if (FrameSize != (DWORD)m_ChannelCount * m_BitsPerSample / 8)
    {
        ASSERT(FALSE);
        RtlZeroMemory(Frame, FrameSize);
        return;
    }

    /* Use __analysis_assume to suppress the reports of a false OACR error. */
    for (ULONG i = 0; i < m_ChannelCount; ++i)
    {
        if (m_BitsPerSample == 8)
        {
            __analysis_assume((DWORD)m_ChannelCount == FrameSize);
            unsigned char* dataBuffer = reinterpret_cast<unsigned char*>(Frame);
            dataBuffer[i] = ConvertToUChar(sinValue);
        }
        else if (m_BitsPerSample == 16)
        {
            __analysis_assume((DWORD)(m_ChannelCount) * 2 == FrameSize);
            short* dataBuffer = reinterpret_cast<short*>(Frame);
            dataBuffer[i] = ConvertToShort(sinValue);
        }
        else if (m_BitsPerSample == 24)
        {
            __analysis_assume((DWORD)(m_ChannelCount) * 3 == FrameSize);
            BYTE* dataBuffer = Frame;
            long val = ConvertToLong(sinValue);
            val = val >> 8;
            RtlCopyMemory(dataBuffer, &val, 3);
        }
        else if (m_BitsPerSample == 32)
        {
            __analysis_assume((DWORD)(m_ChannelCount) * 4 == FrameSize);
            long* dataBuffer = reinterpret_cast<long*>(Frame);
            dataBuffer[i] = ConvertToLong(sinValue);
        }
    }

    m_Theta += m_SampleIncrement;
    if (m_Theta >= TWO_PI)
    {
        m_Theta -= TWO_PI;
    }
}
#pragma warning(pop)

//
// GenerateSamples()
//
//  Generate a sine wave that fits into the specified buffer.
//
//  Buffer - Buffer to hold the samples
//  BufferLength - Length of the buffer.
//
//
void ToneGenerator::GenerateSine(_Out_writes_bytes_(BufferLength) BYTE* Buffer, _In_ size_t BufferLength)
{
    NTSTATUS        status;
    KFLOATING_SAVE  saveData;
    BYTE* buffer;
    size_t          length;
    size_t          copyBytes;

    // if muted, or tone generator disabled via registry,
    // we deliver silence.
    if (m_Mute || g_DisableToneGenerator)
    {
        goto ZeroBuffer;
    }

    status = KeSaveFloatingPointState(&saveData);
    if (!NT_SUCCESS(status))
    {
        goto ZeroBuffer;
    }

    buffer = Buffer;
    length = BufferLength;

    //
    // Check if we have any residual frame bytes from the last time.
    //
    if (m_PartialFrameBytes)
    {
        ASSERT(m_FrameSize > m_PartialFrameBytes);
        DWORD offset = m_FrameSize - m_PartialFrameBytes;
        copyBytes = MIN(m_PartialFrameBytes, length);
        RtlCopyMemory(buffer, m_PartialFrame + offset, copyBytes);
        RtlZeroMemory(m_PartialFrame + offset, copyBytes);
        length -= copyBytes;
        buffer += copyBytes;
        m_PartialFrameBytes = 0;
    }

    IF_TRUE_JUMP(length == 0, Done);

    //
    // Copy all the aligned frames.
    // 

    size_t frames = length / m_FrameSize;

    for (size_t i = 0; i < frames; ++i)
    {
        InitNewFrame(buffer, m_FrameSize);
        buffer += m_FrameSize;
        length -= m_FrameSize;
    }

    IF_TRUE_JUMP(length == 0, Done);

    //
    // Copy any partial frame at the end.
    //
    ASSERT(m_FrameSize > length);
    InitNewFrame(m_PartialFrame, m_FrameSize);
    RtlCopyMemory(buffer, m_PartialFrame, length);
    RtlZeroMemory(m_PartialFrame, length);
    m_PartialFrameBytes = m_FrameSize - (DWORD)length;

Done:
    KeRestoreFloatingPointState(&saveData);
    return;

ZeroBuffer:
    RtlZeroMemory(Buffer, BufferLength);
    return;
}

NTSTATUS ToneGenerator::Init(_In_ DWORD ToneFrequency, _In_ double ToneAmplitude,
    _In_ double ToneDCOffset, _In_ double ToneInitialPhase, _In_ PWAVEFORMATEXTENSIBLE WfExt)
{
    NTSTATUS        status = STATUS_SUCCESS;
    KFLOATING_SAVE  saveData;

    //
    // This sample supports PCM formats only. 
    //
    if ((WfExt->Format.wFormatTag != WAVE_FORMAT_PCM &&
        !(WfExt->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
            IsEqualGUIDAligned(WfExt->SubFormat, KSDATAFORMAT_SUBTYPE_PCM))))
    {
        status = STATUS_NOT_SUPPORTED;
    }
    IF_FAILED_JUMP(status, Done);

    //
    // Save floating state (just in case).
    //
    status = KeSaveFloatingPointState(&saveData);
    IF_FAILED_JUMP(status, Done);

    //
    // Basic init.
    //
    m_Theta = ToneInitialPhase;
    m_Frequency = ToneFrequency;
    m_ToneAmplitude = ToneAmplitude;
    m_ToneDCOffset = ToneDCOffset;

    m_ChannelCount = WfExt->Format.nChannels;      // # channels.
    m_BitsPerSample = WfExt->Format.wBitsPerSample; // bits per sample.
    m_SamplesPerSecond = WfExt->Format.nSamplesPerSec; // samples per sec.
    m_Mute = false;
    m_SampleIncrement = (m_Frequency * TWO_PI) / (double)m_SamplesPerSecond;
    m_FrameSize = (DWORD)m_ChannelCount * m_BitsPerSample / 8;
    ASSERT(m_FrameSize == WfExt->Format.nBlockAlign);

    DPF(D_TERSE, ("[ToneGenerator::Init], sampleRate:%d, channel:%d, bits:%d", m_SamplesPerSecond, m_ChannelCount, m_BitsPerSample));

    //
    // Restore floating state.
    //
    KeRestoreFloatingPointState(&saveData);

    // 
    // Allocate a buffer to hold a partial frame.
    //
    m_PartialFrame = (BYTE*)ExAllocatePool2(POOL_FLAG_NON_PAGED, m_FrameSize, VIRTUALAUDIODEVICE_POOLTAG);
    IF_TRUE_ACTION_JUMP(m_PartialFrame == NULL, status = STATUS_INSUFFICIENT_RESOURCES, Done);

    m_BufferSize = 4 * DEFAULT_READ_FRAME_COUNT * m_ChannelCount * m_BitsPerSample / 8;
    m_RingBuffer = (BYTE*)ExAllocatePool2(POOL_FLAG_NON_PAGED, m_BufferSize, VIRTUALAUDIODEVICE_POOLTAG);
    IF_TRUE_ACTION_JUMP(m_RingBuffer == NULL, status = STATUS_INSUFFICIENT_RESOURCES, Done);

    HANDLE threadHandle = nullptr;
    status = PsCreateSystemThread(&threadHandle, THREAD_ALL_ACCESS, NULL, NULL, NULL, ReadWaveFile, this);
    if (NT_SUCCESS(status))
    {
        ObReferenceObjectByHandle(threadHandle, THREAD_ALL_ACCESS, *PsThreadType, KernelMode, (PVOID*)&m_ThreadObject, NULL);
        ZwClose(threadHandle);
    }
    else
    {
        DPF(D_TERSE, ("[ToneGenerator::Init], PsCreateSystemThread failed"));
    }

Done:
    return status;
}

BOOL ToneGenerator::isBufferEmpty()
{
    BOOL ret = FALSE;
    KIRQL oldIrql = 0;
    KeAcquireSpinLock(&m_BufferSpinLock, &oldIrql);
    if (m_DataSize == 0)
    {
        ret = TRUE;
    }
    KeReleaseSpinLock(&m_BufferSpinLock, oldIrql);
    return ret;
}

BOOL ToneGenerator::isBufferFull()
{
    BOOL ret = FALSE;
    KIRQL oldIrql = 0;
    KeAcquireSpinLock(&m_BufferSpinLock, &oldIrql);
    if (m_DataSize == m_BufferSize)
    {
        ret = TRUE;
    }
    KeReleaseSpinLock(&m_BufferSpinLock, oldIrql);
    return ret;
}

//void addToBuffer(RingBuffer* rb, unsigned char data) {
//    if (!isBufferFull(rb)) {
//        rb->buffer[rb->head] = data;
//        rb->head = (rb->head + 1) % BUFFER_SIZE;
//        rb->size++;
//    }
//    else {
//        printf("Buffer is full\n");
//    }
//}

//unsigned char removeFromBuffer(RingBuffer* rb) {
//    unsigned char data = 0;
//    if (!isBufferEmpty(rb)) {
//        data = rb->buffer[rb->tail];
//        rb->tail = (rb->tail + 1) % BUFFER_SIZE;
//        rb->size--;
//    }
//    else {
//        printf("Buffer is empty\n");
//    }
//    return data;
//}

DWORD ToneGenerator::getUsedSize()
{
    return m_DataSize;
}

DWORD ToneGenerator::GetFrameAvailableCount()
{
    if (m_ChannelCount == 0 || m_BitsPerSample == 0)
    {
        return 0;
    }

    KIRQL oldIrql = 0;
    KeAcquireSpinLock(&m_BufferSpinLock, &oldIrql);
    DWORD availableCount = m_DataSize / m_ChannelCount / (m_BitsPerSample / 8);;
    KeReleaseSpinLock(&m_BufferSpinLock, oldIrql);

    return availableCount;
}

DWORD ToneGenerator::GetFreeFrameCount()
{
    if (m_ChannelCount == 0 || m_BitsPerSample == 0)
    {
        return 0;
    }

    KIRQL oldIrql = 0;
    KeAcquireSpinLock(&m_BufferSpinLock, &oldIrql);
    DWORD freeCount = (m_BufferSize - m_DataSize) / m_ChannelCount / (m_BitsPerSample / 8);
    KeReleaseSpinLock(&m_BufferSpinLock, oldIrql);

    return freeCount;
}

DWORD ToneGenerator::AddFramesToBuffer(FMT_CHUNK* fmtHeader, BYTE* data, ULONG length)
{
    if (fmtHeader->lSampleRate != (LONG)m_SamplesPerSecond)
    {
        DPF(D_TERSE, ("[ToneGenerator::AddFramesToBuffer], sample rate not equal, wav file:%d, required:%d",
            fmtHeader->lSampleRate, m_SamplesPerSecond));
        return 0;
    }

    KIRQL oldIrql = 0;
    KeAcquireSpinLock(&m_BufferSpinLock, &oldIrql);
    DWORD readPos = m_ReadPos;
    DWORD writePos = m_WritePos;
    DWORD dataSize = m_DataSize;
    KeReleaseSpinLock(&m_BufferSpinLock, oldIrql);

    if (fmtHeader->sNumChannels == m_ChannelCount && fmtHeader->sBitsPerSample == m_BitsPerSample)
    {
        DWORD copiedLength = 0;
        
        /*                                                                  R
        *         R         W                                               W
        * |-------**********-------------|          * |------------------------------|
        * |<--------m_BufferSize-------->|            |<--------m_BufferSize-------->|
        */
        if (writePos > readPos || (writePos == readPos && dataSize == 0))
        {
            if (m_BufferSize - writePos >= length)
            {
                RtlCopyMemory(m_RingBuffer + writePos, data, length);
                writePos += length;
                writePos %= m_BufferSize;
                dataSize += length;
                copiedLength = length;
            }
            else
            {
                DWORD copy1 = m_BufferSize - writePos;
                DWORD copy2 = length - copy1;
                RtlCopyMemory(m_RingBuffer + writePos, data, copy1);
                copy2 = readPos < copy2 ? readPos : copy2;
                RtlCopyMemory(m_RingBuffer, data + copy1, copy2);
                writePos = copy2;
                dataSize += (copy1 + copy2);
                copiedLength = copy1 + copy2;
            }
        }
        /*
        *         W         R
        * |*******----------*************|
        * |<--------m_BufferSize-------->|
        */
        else if (writePos < readPos)
        {
            if (readPos - writePos >= length)
            {
                RtlCopyMemory(m_RingBuffer + writePos, data, length);
                writePos += length;
                dataSize += length;
                copiedLength = length;
            }
            else
            {
                RtlCopyMemory(m_RingBuffer + writePos, data, readPos - writePos);
                writePos += (readPos - writePos);
                dataSize += (readPos - writePos);
                copiedLength = readPos - writePos;
            }
        }

        //DPF(D_TERSE, ("[ToneGenerator::AddFramesToBuffer], bytes:%d, read pos:%d, write pos:%d, data size:%d", copiedLength, readPos, writePos, dataSize));

        KeAcquireSpinLock(&m_BufferSpinLock, &oldIrql);
        m_WritePos = writePos;
        m_DataSize = dataSize;
        KeReleaseSpinLock(&m_BufferSpinLock, oldIrql);
        return copiedLength;
    }
    else
    {
        BYTE* srcByte = nullptr;
        SHORT* srcShort = nullptr;
        LONG* srcLong = nullptr;
        switch (fmtHeader->sBitsPerSample)
        {
        case 8:
            srcByte = data;
            break;
        case 16:
            srcShort = reinterpret_cast<SHORT*>(data);
            break;
        case 24:
            break;
        case 32:
            srcLong = reinterpret_cast<LONG*>(data);
            break;
        default:
            return 0;
        }

        BYTE* dstByte = nullptr;
        SHORT* dstShort = nullptr;
        LONG* dstLong = nullptr;
        switch (m_BitsPerSample)
        {
        case 8:
            dstByte = m_RingBuffer + writePos;
            break;
        case 16:
            dstShort = reinterpret_cast<SHORT*>(m_RingBuffer + writePos);
            break;
        case 24:
            break;
        case 32:
            dstLong = reinterpret_cast<LONG*>(m_RingBuffer + writePos);
            break;
        default:
            return 0;
        }

        DWORD copiedLength = 0;
        BOOL bufferFull = FALSE;
        ULONG frameCount = length / fmtHeader->sNumChannels / (fmtHeader->sBitsPerSample / 8);
        for (ULONG i = 0; i < frameCount; i++)
        {
            LONGLONG totalValue = 0;
            for (SHORT j = 0; j < fmtHeader->sNumChannels; j++)
            {
                switch (fmtHeader->sBitsPerSample)
                {
                case 8:
                    totalValue += ((LONGLONG)(*srcByte) - 128) << 24;
                    srcByte++;
                    copiedLength += sizeof(BYTE);
                    break;
                case 16:
                    totalValue += ((LONGLONG)*srcShort) << 16;
                    srcShort++;
                    copiedLength += sizeof(SHORT);
                    break;
                case 32:
                    totalValue += (LONGLONG)*srcLong;
                    srcLong++;
                    copiedLength += sizeof(LONG);
                    break;
                default:
                    break;
                }
            }

            LONG valueLong = (LONG)(totalValue / fmtHeader->sNumChannels);
            for (WORD j = 0; j < m_ChannelCount; j++)
            {
                switch (m_BitsPerSample)
                {
                case 8:
                    *dstByte = BYTE((valueLong >> 24) + 128);
                    ++dstByte;
                    dataSize += sizeof(BYTE);
                    if (dstByte == m_RingBuffer + m_BufferSize)
                    {
                        dstByte = m_RingBuffer;
                    }
                    writePos = DWORD(dstByte - m_RingBuffer);
                    if (dstByte == m_RingBuffer + readPos)
                    {
                        bufferFull = TRUE;
                    }
                    break;
                case 16:
                    *dstShort = SHORT(valueLong >> 16);
                    ++dstShort;
                    dataSize += sizeof(SHORT);
                    if (dstShort == reinterpret_cast<SHORT*>(m_RingBuffer + m_BufferSize))
                    {
                        dstShort = reinterpret_cast<SHORT*>(m_RingBuffer);
                    }
                    writePos = DWORD((BYTE*)dstShort - m_RingBuffer);
                    if (dstShort == (SHORT*)(m_RingBuffer + readPos))
                    {
                        bufferFull = TRUE;
                    }
                    break;
                case 32:
                    *dstLong = valueLong;
                    ++dstLong;
                    dataSize += sizeof(LONG);
                    if (dstLong == reinterpret_cast<LONG*>(m_RingBuffer + m_BufferSize))
                    {
                        dstLong = reinterpret_cast<LONG*>(m_RingBuffer);
                    }
                    writePos = DWORD((BYTE*)dstLong - m_RingBuffer);
                    if (dstLong == (LONG*)(m_RingBuffer + readPos))
                    {
                        bufferFull = TRUE;
                    }
                    break;
                default:
                    break;
                }
                if (bufferFull)
                {
                    break;
                }
            }
            if (bufferFull)
            {
                break;
            }
        }

        //DPF(D_TERSE, ("[ToneGenerator::AddFramesToBuffer], bytes:%d, read pos:%d, write pos:%d, data size:%d", copiedLength, readPos, writePos, dataSize));

        KeAcquireSpinLock(&m_BufferSpinLock, &oldIrql);
        m_WritePos = writePos;
        m_DataSize = dataSize;
        KeReleaseSpinLock(&m_BufferSpinLock, oldIrql);

        return copiedLength;
    }
}

DWORD ToneGenerator::GetFramesFromBuffer(_Out_writes_bytes_(BufferLength) BYTE* Buffer, _In_ size_t BufferLength)
{
    if (m_ChannelCount == 0 || m_BitsPerSample / 8 == 0)
    {
        RtlZeroMemory(Buffer, BufferLength);
        return 0;
    }

    DWORD length = (DWORD)BufferLength;
    DWORD copiedLength = 0;
    KIRQL oldIrql = 0;

    KeAcquireSpinLock(&m_BufferSpinLock, &oldIrql);
    DWORD readPos = m_ReadPos;
    DWORD writePos = m_WritePos;
    DWORD dataSize = m_DataSize;
    KeReleaseSpinLock(&m_BufferSpinLock, oldIrql);

    /*
    *         R         W
    * |-------**********-------------|
    * |<--------m_BufferSize-------->|
    */
    if (writePos > readPos)
    {
        if (writePos - readPos >= length)
        {
            RtlCopyMemory(Buffer, m_RingBuffer + readPos, length);
            readPos += length;
            dataSize -= length;
            copiedLength = length;
        }
        else
        {
            RtlCopyMemory(Buffer, m_RingBuffer + readPos, writePos - readPos);
            readPos += (writePos - readPos);
            dataSize -= (writePos - readPos);
            copiedLength = writePos - readPos;
        }
    }
    /*                                                                  R
    *         W         R                                               W
    * |*******----------*************|          * |******************************|
    * |<--------m_BufferSize-------->|            |<--------m_BufferSize-------->|
    */
    else if (writePos < readPos || (writePos == readPos && dataSize == m_BufferSize))
    {
        if (m_BufferSize - readPos >= length)
        {
            RtlCopyMemory(Buffer, m_RingBuffer + readPos, length);
            readPos += length;
            dataSize -= length;
            copiedLength = length;
        }
        else
        {
            DWORD copy1 = m_BufferSize - readPos;
            DWORD copy2 = length - copy1;
            RtlCopyMemory(Buffer, m_RingBuffer + readPos, copy1);
            copy2 = writePos < copy2 ? writePos : copy2;
            RtlCopyMemory(Buffer + copy1, m_RingBuffer, copy2);
            readPos = copy2;
            dataSize -= (copy1 + copy2);
            copiedLength = copy1 + copy2;
        }
    }
    //DPF(D_TERSE, ("[ToneGenerator::GetFramesFromBuffer], bytes:%d, read pos:%d, write pos:%d, data size:%d", copiedLength, readPos, writePos, dataSize));
    
    KeAcquireSpinLock(&m_BufferSpinLock, &oldIrql);
    m_ReadPos = readPos;
    m_DataSize = dataSize;
    KeReleaseSpinLock(&m_BufferSpinLock, oldIrql);

    return copiedLength;
}
