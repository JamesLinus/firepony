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
#include "variants.h"
#include "covariates.h"
#include "cigar.h"
#include "bam_loader.h"
#include "baq.h"
#include "reference.h"

using namespace nvbio;

struct bqsr_statistics // host-only
{
    uint32 total_reads;        // total number of reads processed
    uint32 filtered_reads;     // number of reads filtered out in pre-processing
    uint32 baq_reads;          // number of reads for which BAQ was computed

    bqsr_statistics()
        : total_reads(0),
          filtered_reads(0),
          baq_reads(0)
    { }
};

struct bqsr_context
{
    BAM_header& bam_header;
    const DeviceSNPDatabase& db;
    const reference_genome& reference;

    // sorted list of active reads
    D_VectorU32 active_read_list;
    // alignment windows for each read in reference coordinates
    D_VectorU32_2 alignment_windows;
    // alignment windows for each read in local sequence coordinates
    D_VectorU16_2 sequence_alignment_windows;

    // list of active BP locations
    D_ActiveLocationList active_location_list;
    // list of read offsets in the reference for each BP (relative to the alignment start position)
    D_VectorU16 read_offset_list;

    // temporary storage for CUB calls
    D_VectorU8 temp_storage;

    // and more temporary storage
    D_VectorU32 temp_u32;
    D_VectorU32 temp_u32_2;
    D_VectorU32 temp_u32_3;
    D_VectorU32 temp_u32_4;

    // various pipeline states go here
    snp_filter_context snp_filter;
    cigar_context cigar;
    baq_context baq;
    covariates_context covariates;

    // --- everything below this line is host-only and not available on the device
    bqsr_statistics stats;

    bqsr_context(BAM_header& bam_header,
                 const DeviceSNPDatabase& db,
                 const reference_genome& reference)
        : bam_header(bam_header),
          db(db),
          reference(reference)
    { }

    struct view
    {
        BAM_header::view                        bam_header;
        DeviceSNPDatabase::const_view           db;
        reference_genome_device::const_view     reference;
        D_VectorU32::plain_view_type            active_read_list;
        D_VectorU32_2::plain_view_type          alignment_windows;
        D_VectorU16_2::plain_view_type          sequence_alignment_windows;
        D_ActiveLocationList::plain_view_type   active_location_list;
        D_VectorU16::plain_view_type            read_offset_list;
        D_VectorU8::plain_view_type             temp_storage;
        D_VectorU32::plain_view_type            temp_u32;
        D_VectorU32::plain_view_type            temp_u32_2;
        D_VectorU32::plain_view_type            temp_u32_3;
        D_VectorU32::plain_view_type            temp_u32_4;
        snp_filter_context::view                snp_filter;
        cigar_context::view                     cigar;
        baq_context::view                       baq;
        covariates_context::view                covariates;
    };

    operator view()
    {
        view v = {
            bam_header,
            db,
            reference.device,
            plain_view(active_read_list),
            plain_view(alignment_windows),
            plain_view(sequence_alignment_windows),
            plain_view(active_location_list),
            plain_view(read_offset_list),
            plain_view(temp_storage),
            plain_view(temp_u32),
            plain_view(temp_u32_2),
            plain_view(temp_u32_3),
            plain_view(temp_u32_4),
            snp_filter,
            cigar,
            baq,
            covariates,
        };

        return v;
    }

    void start_batch(BAM_alignment_batch& batch);
#if 0
    void compact_active_read_list(void);
#endif
};

// encapsulates common state for our thrust functors to save a little typing
struct bqsr_lambda
{
    bqsr_context::view ctx;
    const BAM_alignment_batch_device::const_view batch;

    bqsr_lambda(bqsr_context::view ctx,
                const BAM_alignment_batch_device::const_view batch)
        : ctx(ctx),
          batch(batch)
    { }
};

void debug_read(bqsr_context *context, const BAM_alignment_batch& batch, int read_id);
