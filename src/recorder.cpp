/*
    cs3-runtime-sdl
    Copyright (C) 2025 Francois Blanchette

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <algorithm>
#include <cstring>
#include "recorder.h"

CRecorder::CRecorder()
{
    m_buffer = new uint8_t[MAX_ENTRIES];
    m_mode = MODE_CLOSED;
}

CRecorder::~CRecorder()
{
    delete[] m_buffer;
}

bool CRecorder::start(FILE *file, bool isWrite)
{
    auto readFile = [file](auto ptr, auto size)
    {
        return fread(ptr, size, 1, file) == 1;
    };
    auto writeFile = [file](auto ptr, auto size)
    {
        return fwrite(ptr, size, 1, file) == 1;
    };

    m_newInfo = true;
    m_index = 0;
    m_count = 0;
    m_size = 0;

    uint32_t version = VERSION;
    m_mode = isWrite ? MODE_WRITE : MODE_READ;
    m_file = file;
    if (m_file && m_mode == MODE_WRITE)
    {
        const uint8_t placeholder[] = {0, 0, 0, 0};
        writeFile("REC!", 4);
        writeFile(&version, 4);
        m_offset = ftell(file);
        writeFile(placeholder, 4); // placeholder for datasize
    }
    else if (m_file && m_mode == MODE_READ)
    {
        // TODO check signature
        char sig[5];
        readFile(sig, 4);
        if (memcmp(sig, "REC!", 4) != 0)
        {
            fprintf(stderr, "signature mismatch:\n");
        }
        readFile(&version, 4);
        if (version != VERSION)
        {
            fprintf(stderr, "version mismatch:\n");
        }
        readFile(&m_size, 4); // total datasize of data
        readNextBatch();
    }
    else
    {
        m_mode = MODE_CLOSED;
        fprintf(stderr, "invalid file handle\n");
        return false;
    }
    return true;
}

bool CRecorder::readNextBatch()
{
    m_batchSize = std::min(static_cast<uint32_t>(MAX_ENTRIES), m_size);
    m_size -= m_batchSize;
    fread(m_buffer, m_batchSize, 1, m_file);
    m_index = 0;
    m_newInfo = true;
    return true;
}

void CRecorder::append(const uint8_t *input)
{
    // encode input
    uint8_t data = 0;
    for (int i = 0; i < INPUTS; ++i)
    {
        data |= (input[i] != 0) << i;
    }
    if (m_newInfo)
    {
        m_current = data;
        m_count = 1;
        m_newInfo = false;
    }
    else if (m_current == data)
    {
        ++m_count;

        if (m_count == MAX_CPT)
        {
            storeData(false);
            m_newInfo = true;
        }
    }
    else
    {
        storeData(false);
        m_current = data;
        ++m_count;
    }
}

void CRecorder::storeData(bool finalize)
{
    if (m_count && !m_newInfo)
        m_buffer[m_index++] = m_current | (m_count << 4);
    m_count = 0;
    if ((m_index == MAX_ENTRIES) || finalize)
    {
        dump();
    }
}

void CRecorder::dump()
{
    if (m_index)
    {
        fwrite(m_buffer, m_index, 1, m_file);
        m_size += m_index;
    }
    m_index = 0;
}

void CRecorder::nextData()
{
    const uint8_t data = m_buffer[m_index++];
    m_current = data & MAX_CPT;
    m_count = data >> 4;
    // printf(">>>> %.2x x %.2x\n", m_count, m_current);
}

bool CRecorder::get(uint8_t *output)
{
    if (m_newInfo)
    {
        nextData();
        m_newInfo = false;
        --m_count;
    }
    else if (m_count)
    {
        --m_count;
    }
    else
    {
        // fetch next batch from disk
        if (m_index == m_batchSize)
        {
            if (m_size != 0)
                readNextBatch();
            else
                return false;
        }
        nextData();
        --m_count;
    }
    decode(output, m_current);
    return true;
}

void CRecorder::decode(uint8_t *output, uint8_t data)
{
    for (int i = 0; i < INPUTS; ++i)
    {
        output[i] = data & 1;
        data >>= 1;
    }
}

void CRecorder::stop()
{
    if (m_mode == MODE_WRITE)
    {
        storeData(true);
        fseek(m_file, m_offset, SEEK_SET);
        fwrite(&m_size, 4, 1, m_file);
    }
    m_mode = MODE_CLOSED;
    if (m_file)
        fclose(m_file);
    m_file = nullptr;
}

bool CRecorder::isRecording()
{
    return m_mode == MODE_WRITE;
}

bool CRecorder::isReading()
{
    return m_mode == MODE_READ;
}

bool CRecorder::isStopped()
{
    return m_mode == MODE_CLOSED;
}