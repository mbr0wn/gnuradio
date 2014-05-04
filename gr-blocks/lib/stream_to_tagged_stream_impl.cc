/* -*- c++ -*- */
/*
 * Copyright 2013-2014 Free Software Foundation, Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <gnuradio/io_signature.h>
#include "stream_to_tagged_stream_impl.h"

namespace gr {
  namespace blocks {

    stream_to_tagged_stream::sptr
    stream_to_tagged_stream::make(size_t itemsize, int vlen, unsigned packet_len, const std::string &tsb_key)
    {
      return gnuradio::get_initial_sptr
        (new stream_to_tagged_stream_impl(itemsize, vlen, packet_len, tsb_key));
    }

    stream_to_tagged_stream_impl::stream_to_tagged_stream_impl(size_t itemsize, int vlen, unsigned packet_len, const std::string &tsb_key)
      : gr::sync_block("stream_to_tagged_stream",
              gr::io_signature::make(1, 1, itemsize * vlen),
              gr::io_signature::make(1, 1, itemsize * vlen)),
      d_itemsize(itemsize * vlen),
      d_packet_len(packet_len),
      d_tsb_key(pmt::string_to_symbol(tsb_key)),
      d_next_tag_pos(packet_len-1)
    {
      if (packet_len < 1) {
        throw std::invalid_argument("packet length must be at least 1");
      }
    }

    stream_to_tagged_stream_impl::~stream_to_tagged_stream_impl()
    {
    }

    int
    stream_to_tagged_stream_impl::work(int noutput_items,
			  gr_vector_const_void_star &input_items,
			  gr_vector_void_star &output_items)
    {
        const unsigned char *in = (const unsigned char *) input_items[0];
        unsigned char *out = (unsigned char *) output_items[0];
	// Copy data
	memcpy(out, in, noutput_items * d_itemsize);
	// Add tags every d_packet_len
	while(d_next_tag_pos < nitems_written(0) + noutput_items) {
	  add_item_tag(0, d_next_tag_pos, d_tsb_key, pmt::PMT_T);
	  d_next_tag_pos += d_packet_len;
	}

        return noutput_items;
    }

  } /* namespace blocks */
} /* namespace gr */

