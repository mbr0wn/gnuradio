/* -*- c++ -*- */
/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_QTGUI_FOSPHOR_FORMATTER_IMPL_H
#define INCLUDED_QTGUI_FOSPHOR_FORMATTER_IMPL_H

#include <gnuradio/blocks/complex_to_mag_squared.h>
#include <gnuradio/blocks/float_to_uchar.h>
#include <gnuradio/blocks/keep_one_in_n.h>
#include <gnuradio/blocks/lambda_block.h>
#include <gnuradio/blocks/nlog10_ff.h>
#include <gnuradio/blocks/stream_to_vector.h>
#include <gnuradio/fft/fft_v.h>
#include <gnuradio/filter/single_pole_iir_filter_ff.h>
#include <gnuradio/qtgui/fosphor_formatter.h>
#include <volk/volk_alloc.hh>

namespace gr {
namespace qtgui {

class fosphor_formatter_impl : public fosphor_formatter
{
private:
    //! The general_work() function of d_histo_proc
    int _process_histogram(gr::blocks::lambda_block* self,
                           int noutput_items,
                           gr_vector_int& ninput_items,
                           gr_vector_const_void_star& input_items,
                           gr_vector_void_star& output_items);


    const int d_fftsize;
    const int d_num_bins;
    const int d_histo_decim;
    //! Count processed vectors for the histogram, resets to 0 when we have
    // processed d_histo_decim vectors.
    int d_histo_count = 0;
    float d_epsilon;
    double d_trise;
    double d_tdecay;
    volk::vector<float> d_maxhold_buf;
    volk::vector<float> d_histo_buf_f;
    volk::vector<int16_t> d_hit_count;

    // All the internal blocks
    gr::blocks::stream_to_vector::sptr d_s2v;
    gr::blocks::keep_one_in_n::sptr d_input_decim;
    gr::fft::fft_v<gr_complex, true>::sptr d_fft;
    gr::blocks::complex_to_mag_squared::sptr d_c2m;
    gr::blocks::nlog10_ff::sptr d_log;
    gr::blocks::lambda_block::sptr d_f2byte; // float_to_uchar does not support vectors
    gr::filter::single_pole_iir_filter_ff::sptr d_avg;
    gr::blocks::lambda_block::sptr d_histo_proc;
    gr::blocks::keep_one_in_n::sptr d_wf_decim;


public:
    fosphor_formatter_impl(int fft_size,
                           int num_bins,
                           int input_decim,
                           int waterfall_decim,
                           int histo_decim,
                           double scale,
                           double alpha,
                           double epsilon,
                           double trise,
                           double tdecay);

    ~fosphor_formatter_impl() override;
};

} // namespace qtgui
} /* namespace gr */

#endif /* INCLUDED_QTGUI_FOSPHOR_FORMATTER_IMPL_H */
