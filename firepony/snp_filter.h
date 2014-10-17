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

#include "bqsr_types.h"
#include "alignment_data.h"
#include "sequence_data.h"
#include "variant_data.h"
#include "util.h"

struct snp_filter_context
{
    // the window in the reference for each sequence in the database
    D_VectorU32_2 sequence_windows;

    // active reads for the VCF search
    D_VectorU32 active_read_ids;
    // active VCF range for each read
    D_VectorU32_2 active_vcf_ranges;

    struct view
    {
        D_VectorU32::view active_read_ids;
        D_VectorU32_2::view active_vcf_ranges;
    };

    operator view()
    {
        view v = {
            active_read_ids,
            active_vcf_ranges
        };
        return v;
    }
};

void build_read_offset_list(bqsr_context *context,
                            const alignment_batch& batch);

void build_alignment_windows(bqsr_context *ctx,
                             const alignment_batch& batch);

void filter_known_snps(bqsr_context *context,
                       const alignment_batch& batch);