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

#include <gamgee/sam_reader.h>
#include "alignment_data.h"
#include "sequence_data.h"
#include "variant_data.h"

namespace firepony {

struct gamgee_alignment_file
{
private:
    gamgee::SamReader<gamgee::SamIterator> file;
    gamgee::SamHeader gamgee_header;
    gamgee::SamIterator iterator;

    // map read group identifiers in tag data to read group names from the header
    // the read group name is either taken from the platform unit string if present, or else it's just the identifier itself
    std::map<std::string, std::string> read_group_id_to_name;

public:
    alignment_header_host header;

    gamgee_alignment_file(const char *fname);
    ~gamgee_alignment_file();

    bool next_batch(alignment_batch_host *batch, uint32 data_mask, const uint32 batch_size = 100000);
    const char *get_sequence_name(uint32 id);
};

bool gamgee_load_sequences(sequence_data_host *output, const char *filename, uint32 data_mask, bool try_mmap = true);
bool gamgee_load_vcf(variant_database_host *output, const sequence_data_host& reference, const char *filename, uint32 data_mask, bool try_mmap = true);

} // namespace firepony

