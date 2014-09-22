/*
 * Copyright (c) 2012-14, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 *
 *
 *
 *
 *
 *
 *
 *
 */

#pragma once

#include <zlib/zlib.h>

#include <stdio.h>
#include <string.h>
#include <vector>
#include <assert.h>

#include <stdexcept>

// Generic I/O class for consuming data from a text file delimited by a single record separator
// handles gzip-compressed files transparently
class BufferedTextFile
{
    const char record_separator;

    gzFile fp;
    bool eof;

    std::vector<char> buffer;
    size_t read_ptr, valid_size;

public:
    BufferedTextFile(const char *fname, char record_separator = '\n', size_t buffer_size = 256 * 1024)
        : record_separator(record_separator), eof(false), read_ptr(0), valid_size(0)
    {
        fp = gzopen(fname, "r");
        if (fp == NULL)
        {
            throw std::runtime_error("unable to open file for reading");
        }

        buffer.resize(buffer_size + 1);
        buffer[buffer_size] = 0;
    };

    ~BufferedTextFile()
    {
        if (fp)
        {
            gzclose(fp);
        }
    }

    // refills buffer by reading from file
    // preserves all unprocessed bytes (i.e., anything after read_ptr is moved to the front of buffer prior to reading)
    bool fill_buffer(void)
    {
        if (eof)
        {
            return false;
        }

        if (read_ptr != valid_size && read_ptr != 0)
        {
            memmove(&buffer[0], &buffer[read_ptr], valid_size - read_ptr);
        }

        valid_size -= read_ptr;
        read_ptr = 0;

        size_t bytes_read = gzread(fp, &buffer[valid_size], buffer.size() - valid_size - 1);
        if (bytes_read == 0)
        {
            // end of file reached
            eof = true;
            return false;
        }

        valid_size += bytes_read;
        assert(valid_size < buffer.size() - 1);
        buffer[valid_size] = 0;

        return true;
    }

    bool buffer_full(void)
    {
        return (read_ptr == 0 && valid_size == buffer.size() - 1);
    }

    bool buffer_empty(void)
    {
        return (valid_size == 0 || read_ptr == valid_size);
    }

public:
    char *next_record(char **line_end)
    {
        char *start, *end;

        do
        {
            // search for the next record separator in memory
            start = &buffer[read_ptr];
            end = (char *) memchr(start, record_separator, valid_size - read_ptr);

            if (end == NULL)
            {
                // not found
                if (eof)
                {
                    // we're at the end of the file, so just return what's left in memory
                    if (buffer_empty())
                    {
                        return NULL;
                    } else {
                        NVBIO_CUDA_ASSERT(valid_size <= buffer.size());
                        end = &buffer[valid_size];
                    }
                } else {
                    // need to read more data from the file
                    // first check if the buffer is completely full
                    if (buffer_full())
                    {
                        // resize the buffer
                        buffer.resize(buffer.size() * 2);
                    }

                    // read more data from the file and try again
                    fill_buffer();
                }
            }
        } while(end == NULL);

        if (line_end)
            *line_end = end;

        read_ptr += end - start + 1;
        return start;
    }
};
