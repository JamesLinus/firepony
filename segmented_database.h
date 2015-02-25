/*
 * Firepony
 * Copyright (c) 2014-2015, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the NVIDIA CORPORATION nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NVIDIA CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "types.h"
#include "device/primitives/cuda.h"

namespace firepony {

template <typename T>
struct segmented_coordinate
{
    uint16 id;
    T coordinate;
};

// base struct for per-chromosome data
template <target_system system>
struct segmented_storage
{
    // sequence ID for this entry
    uint16 id;

    segmented_storage()
        : id(uint16(-1))
    { }

    struct const_view
    {
        uint16 id;
    };

    operator const_view() const
    {
        const_view v = {
            id,
        };

        return v;
    }
};

// represents a resident set for the segmented database
struct resident_segment_map
{
    vector<host, bool> set;

    resident_segment_map()
    { }

    resident_segment_map(uint16 size)
    {
        resize(size);
        clear();
    }

    // a resident segment set can never shrink
    // this is because any sequences that have been referenced before
    // will be present in the host-side memory database
    void resize(uint16 num_sequences)
    {
        if (set.size() < num_sequences)
        {
            size_t old_size = set.size();
            set.resize(num_sequences);
            for(size_t i = old_size; i < num_sequences; i++)
            {
                set[i] = false;
            }
        }
    }

    void mark_resident(uint16 segment)
    {
        resize(segment + 1);
        set[segment] = true;
    }

    void mark_evicted(uint16 segment)
    {
        resize(segment + 1);
        set[segment] = false;
    }

    void clear(void)
    {
        for(uint32 i = 0; i < set.size(); i++)
        {
            mark_evicted(i);
        }
    }

    uint32 size(void) const
    {
        return set.size();
    }

    bool is_resident(uint16 segment) const
    {
        return set[segment];
    }
};

// a generic database segmented by chromosome, used for both reference and variant data
// storage type is meant to be a structure derived from segmented_storage that holds the data
// for a given chromosome
template <target_system system,
          template <target_system __unused> class chromosome_storage>
struct segmented_database_storage
{
    // shorthand for the view type
    typedef typename chromosome_storage<system>::const_view chromosome_view;

    // per-chromosome data
    vector<system, chromosome_storage<system> *> storage;
    // vector with views that are used for device code
    vector<system, chromosome_view> views;

    // creates an entry for a given sequence ID
    // returns nullptr if the given ID already exists in the database
    chromosome_storage<system> *new_entry(uint16 id)
    {
        if (storage.size() < uint16(id + 1))
        {
            // resize and fill with nullptrs
            size_t old_size = storage.size();
            storage.resize(id + 1);
            for(size_t i = old_size; i <= id; i++)
            {
                storage[i] = nullptr;
            }
        }

        if (storage[id] != nullptr)
        {
            return nullptr;
        }

        chromosome_storage<system> *ret = new chromosome_storage<system>();
        storage[id] = ret;
        return ret;
    }

    // look up a sequence in the database and return a reference
    const chromosome_storage<system>& get_sequence(uint16 id)
    {
        return *storage[id];
    }

    // returns a resident segment map of the right size for the current databasewith all entries marked non-resident
    resident_segment_map empty_segment_map(void) const
    {
        return resident_segment_map(storage.size());
    }

private:
    // evict chromosome at index i
    void evict(uint16 i)
    {
        if (storage[i])
        {
            delete storage[i];
            storage[i] = nullptr;
            // this implicitly creates an invalid view (view.id == -1)
            views[i] = chromosome_view();
        }
    }

    // make chromosome i resident
    void download(const segmented_database_storage<host, chromosome_storage>& db,
                  uint16 i)
    {
        if (storage[i] == nullptr)
        {
            // alloc data
            storage[i] = new chromosome_storage<system>();
            // copy to device
            *storage[i] = *db.storage[i];
            // set up the corresponding view
            views[i] = *storage[i];
        }
    }

public:
    // make a set of chromosomes resident, evict any not marked as resident in the set
    void update_resident_set(const segmented_database_storage<host, chromosome_storage>& db,
                             const resident_segment_map& target_resident_set)
    {
        assert(db.storage.size() == target_resident_set.size());

        // make sure we have enough slots in the database
        if (storage.size() != db.storage.size())
        {
            // create new slots
            size_t old_size = storage.size();
            storage.resize(db.storage.size());
            views.resize(db.storage.size());

            for(size_t i = old_size; i < db.storage.size(); i++)
            {
                storage[i] = nullptr;
                views[i] = chromosome_view();
            }
        }

        for(uint32 i = 0; i < target_resident_set.size(); i++)
        {
            if (target_resident_set.is_resident(i))
            {
                download(db, i);
            } else {
                evict(i);
            }
        }
    }

    struct const_view
    {
        typename vector<system, chromosome_view>::const_view data;

        // grab a reference to a chromosome in the database
        CUDA_HOST_DEVICE
        const typename chromosome_storage<system>::const_view& get_chromosome(uint16 id)
        {
            return data[id];
        }

        template <typename T>
        CUDA_HOST_DEVICE
        const typename chromosome_storage<system>::const_view& get_chromosome(segmented_coordinate<T> coord)
        {
            return get_chromosome(coord.id);
        }
    };

    // explicit conversion to const_view
    const_view view() const
    {
        // note: initializer list construction doesn't work here because we don't know what the derived type constructor is
        const_view v;
        v.data = views;

        return v;
    }

    // implicit cast to const_view
    operator const_view() const
    {
        return view();
    }
};

} // namespace firepony
