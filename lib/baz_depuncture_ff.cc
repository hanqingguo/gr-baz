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

#include <baz_depuncture_ff.h>
#include <gnuradio/io_signature.h>
#include <stdio.h>

/*
 * Create a new instance of baz_depuncture_ff and return
 * a boost shared_ptr.  This is effectively the public constructor.
 */
baz_depuncture_ff_sptr 
baz_make_depuncture_ff (const std::vector<int>& matrix)
{
  return baz_depuncture_ff_sptr (new baz_depuncture_ff (matrix));
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
baz_depuncture_ff::baz_depuncture_ff (const std::vector<int> matrix)
  : gr::block ("depuncture_ff",
	      gr::io_signature::make (MIN_IN, MAX_IN, sizeof (float)),
	      gr::io_signature::make (MIN_OUT, MAX_OUT, sizeof (float)))
  , m_iLength(0)
  , m_pMatrix(NULL)
  , m_iIndex(0)
{
  set_matrix(matrix);
}

/*
 * Our virtual destructor.
 */
baz_depuncture_ff::~baz_depuncture_ff ()
{
  delete [] m_pMatrix;
}

void baz_depuncture_ff::forecast(int noutput_items, gr_vector_int &ninput_items_required)
{
  //ninput_items_required[0] = (int)ceil((double)noutput_items * relative_rate());
  gr::block::forecast(noutput_items, ninput_items_required);
}

void baz_depuncture_ff::set_matrix(const std::vector<int>& matrix)
{
  if (matrix.empty())
	return;
  
  boost::mutex::scoped_lock guard(d_mutex);
  
  if (m_pMatrix)
	delete [] m_pMatrix;
  
  m_iLength = matrix.size();
  m_pMatrix = new char[m_iLength];
  int iOne = 0;
  for (int i = 0; i < m_iLength; ++i)
  {
	m_pMatrix[i] = (char)matrix[i];
	if (matrix[i])
	  iOne++;
  }
  
  double dRate = (double)matrix.size() / (double)iOne;
  set_relative_rate(dRate);
fprintf(stderr, "De-puncturer relative rate: %f\n", dRate);

  m_iIndex = 0;
}

int 
baz_depuncture_ff::general_work (int noutput_items,
			       gr_vector_int &ninput_items,
			       gr_vector_const_void_star &input_items,
			       gr_vector_void_star &output_items)
{
  const float *in = (const float *) input_items[0];
  float *out = (float *) output_items[0];
  
  boost::mutex::scoped_lock guard(d_mutex);

  int iIn = 0;
  for (int i = 0; i < noutput_items; i++) {
	//assert(iIn < ninput_items[0]);
	char b = (m_pMatrix ? m_pMatrix[m_iIndex] : 1);
	if (b)
	{
	  *out++ = *in++;
	  ++iIn;
	}
	else
	  *out++ = 0.0;	// ERASURE
	m_iIndex = (m_iIndex + 1) % m_iLength;
  }

  // Tell runtime system how many input items we consumed on each input stream.
  consume_each (iIn);

  // Tell runtime system how many output items we produced.
  return noutput_items;
}
