/* -*- c++ -*- */
/*
 * Copyright 2004 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * gr-baz by Balint Seeber (http://spench.net/contact)
 * Information, documentation & samples: http://wiki.spench.net/wiki/gr-baz
 */

/*
 * config.h is generated by configure.  It contains the results
 * of probing for features, options etc.  It should be the first
 * file included in your .cc file.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <baz_time_keeper.h>
#include <gnuradio/io_signature.h>
#include <pmt/pmt.h>

#include <stdio.h>

/*
 * Create a new instance of baz_time_keeper and return
 * a boost shared_ptr.  This is effectively the public constructor.
 */
baz_time_keeper_sptr
baz_make_time_keeper (int item_size, float sample_rate)
{
	return baz_time_keeper_sptr (new baz_time_keeper (item_size, sample_rate));
}

/*
 * The private constructor
 */
baz_time_keeper::baz_time_keeper (int item_size, float sample_rate)
	: gr::sync_block ("baz_time_keeper",
		gr::io_signature::make (1, 1, item_size),
		gr::io_signature::make (0, 0, 0))
	, d_item_size(item_size), d_sample_rate(sample_rate)
	, d_last_time_seconds(0), d_first_time_seconds(0), d_last_time_fractional_seconds(0), d_first_time_fractional_seconds(0)
	, d_time_offset(-1), d_seen_time(false), d_update_count(0), d_ignore_next(true)	// Ignore first update
{
	//memset(&d_last_time, 0x00, sizeof(uhd::time_spec_t));

	fprintf(stderr, "[%s<%i>] item size: %d, sample rate: %f\n", name().c_str(), unique_id(), item_size, sample_rate);

	d_status_port_id = pmt::mp("status");
	message_port_register_out(d_status_port_id);
}

/*
 * Our virtual destructor.
 */
baz_time_keeper::~baz_time_keeper ()
{
}

static const pmt::pmt_t RX_TIME_KEY = pmt::string_to_symbol("rx_time");

double baz_time_keeper::time(bool relative /*= false*/)
{
	gr::thread::scoped_lock guard(d_mutex);
	
	double d = ((double)d_last_time_seconds + d_last_time_fractional_seconds) + ((double)d_time_offset / (double)d_sample_rate);
	if (relative)
		d -= ((double)d_first_time_seconds + d_first_time_fractional_seconds);
	
	return d;
}

void baz_time_keeper::ignore_next(bool ignore /*= true*/)
{
	gr::thread::scoped_lock guard(d_mutex);
	
	d_ignore_next = ignore;
	
	//fprintf(stderr, "[%s<%i>] ignoring next: %s\n", name().c_str(), unique_id(), (ignore ? "yes" : "no"));
}

int baz_time_keeper::update_count(void)
{
	gr::thread::scoped_lock guard(d_mutex);
	
	return d_update_count;
}

int baz_time_keeper::work (int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
	gr::thread::scoped_lock guard(d_mutex);
	
	//fprintf(stderr, "[%s] Work %d\n", name().c_str(), noutput_items);
	
	//const char *in = (char*)input_items[0];
	
	const int tag_channel = 0;
	const uint64_t nread = nitems_read(tag_channel); //number of items read on port 0
	std::vector<gr::tag_t> tags;
	
	get_tags_in_range(tags, tag_channel, nread, (nread + noutput_items), RX_TIME_KEY);
	
	// 'd_ignore_next' can either be the very next tag, or all tags found in the current work
	// [1] Next tag: expecting frequent re-tunes & less overruns
	// [2] All tags: expecting infrequent re-tunes & more overruns
	if (tags.size() > 0) {
		if (d_ignore_next == false)	// [1]
			d_update_count += tags.size() - 1;
		//else if (tags.size() > 1)	// [1]
		//	d_ignore_next = false;
	}
	
	int offset = 0;
	for (int i = /*0*/(tags.size() - 1); i < tags.size(); ++i) {
		const gr::tag_t& tag = tags[i];
		
		//fprintf(stderr, "[%s<%i>] Tag #%d %s\n", name().c_str(), unique_id(), i, pmt::write_string(tag.key).c_str());
		
		d_time_offset = 0;
		offset = tags[i].offset - nread;
		
		d_last_time_seconds = pmt::to_uint64(pmt::tuple_ref(tag.value, 0));
		d_last_time_fractional_seconds = pmt::to_double(pmt::tuple_ref(tag.value, 1));
		
		if (d_seen_time == false) {
			d_first_time_seconds = d_last_time_seconds;
			d_first_time_fractional_seconds = d_last_time_fractional_seconds;
		}
		
		if (d_ignore_next == false)
		{
			++d_update_count;

			message_port_pub(d_status_port_id, pmt::string_to_symbol("update"));
		}
		
		d_seen_time = true;
		//d_ignore_next = false;	// [1]
	}
	
	if (tags.size() > 0)	// [2]
		d_ignore_next = false;
	
	d_time_offset += (noutput_items - offset);
	
	return noutput_items;
}
