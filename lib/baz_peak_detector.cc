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

#include <baz_peak_detector.h>
#include <gnuradio/io_signature.h>
#include <stdio.h>

/*
 * Create a new instance of baz_peak_detector and return
 * a boost shared_ptr.  This is effectively the public constructor.
 */
baz_peak_detector_sptr  baz_make_peak_detector (float min_diff /*= 0.0*/, int min_len /*= 1*/, int lockout /*= 0*/, float drop /*= 0.0*/, float alpha /*= 1.0*/, int look_ahead /*= 0*/)
{
	return baz_peak_detector_sptr (new baz_peak_detector (min_diff, min_len, lockout, drop, alpha, look_ahead));
}

/*
 * Specify constraints on number of input and output streams.
 * This info is used to construct the input and output signatures
 * (2nd & 3rd args to gr::block's constructor).  The input and
 * output signatures are used by the runtime system to
 * check that a valid number and type of inputs and outputs
 * are connected to this block.  In this case, we accept
 * only 1 input and 1 output.
 */
static const int MIN_IN = 1;	// mininum number of input streams
static const int MAX_IN = 1;	// maximum number of input streams
static const int MIN_OUT = 1;	// minimum number of output streams
static const int MAX_OUT = 1;	// maximum number of output streams

/*
 * The private constructor
 */
baz_peak_detector::baz_peak_detector (float min_diff /*= 0.0*/, int min_len /*= 1*/, int lockout /*= 0*/, float drop /*= 0.0*/, float alpha /*= 1.0*/, int look_ahead /*= 0*/)
  : gr::sync_block ("peak_detector",
		gr::io_signature::make (MIN_IN,  MAX_IN,  sizeof (float)),
		gr::io_signature::make (MIN_OUT, MAX_OUT, sizeof (float)))
	, d_min_diff(min_diff)
	, d_min_len(min_len)
	, d_lockout(lockout)
	, d_drop(drop)
	, d_alpha(alpha)
	, d_look_ahead(look_ahead)
	, d_rising(false)
	, d_rise_count(0)
	, d_lockout_count(1)
	, d_first(0.0f)
	, d_ave(0.0f)
	, d_peak(0.0f)
	, d_look_ahead_count(0)
	, d_peak_idx(-1)
{
	fprintf(stderr, "[%s<%i>] min diff: %f, min len: %d, lockout: %d, drop: %f, alpha: %f, look ahead: %d\n", name().c_str(), unique_id(), min_diff, min_len, lockout, drop, alpha, look_ahead);
	
	set_history(1+1);
}

baz_peak_detector::~baz_peak_detector ()
{
}
/*
void baz_peak_detector::set_exponent(float exponent)
{
  d_exponent = exponent;
}
*/
int baz_peak_detector::work (int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
	const float *in = (const float *) input_items[0];
	float *out = (float *) output_items[0];
	
	memset(out, 0x00, sizeof(float) * noutput_items);
	
	const int offset = 1;
	for (int i = (0+offset); i < (noutput_items+offset); i++)
	{
		d_ave = d_alpha * in[i-1] + (1.0f - d_alpha) * d_ave;
		
		if (d_lockout_count > 0)
		{
			--d_lockout_count;
			
			if (d_lockout_count > 0)
				continue;
		}
		
		if (in[i] > (/*in[i-1]*/d_ave - (/*in[i-1]*/d_ave * d_drop)))
		{
			bool new_peak = false;
			
			if (d_rising == false)
			{
				d_rising = true;
				d_rise_count = 0;
				d_first = in[i];
				
				new_peak = true;
			}
			else
			{
				if (in[i] > d_peak)
				{
					new_peak = true;
				}
			}
			
			if (new_peak)
			{
				d_peak = in[i];
				d_look_ahead_count = d_look_ahead;
				d_peak_idx = i - offset;
				
				if (d_look_ahead_count > (noutput_items - (i - offset) + 1))
				{
					if ((i - offset) == 0)
					{
						fprintf(stderr, "Too few items!\n");
					}
					
					return (i - offset) + 1;
				}
			}
			
			++d_rise_count;
			
			continue;
		}
		
		if (d_look_ahead_count > 0)
		{
			--d_look_ahead_count;
			
			if (d_look_ahead_count)
			{
				continue;
			}
		}
		
		if (d_rising)
		{
			if (d_rise_count >= d_min_len)
			{
				//float diff = in[i] - d_first;
				float diff = 0.0f;
				if (d_first > 0.0f)
					diff = in[i] / d_first;
				
				if ((d_min_diff == 0.0f) || (diff >= d_min_diff))
				{
					out[d_peak_idx] = 1.0f;
					
					d_lockout_count = d_lockout;
				}
			}
			
			d_rising = false;
		}
	}

	return noutput_items;
}