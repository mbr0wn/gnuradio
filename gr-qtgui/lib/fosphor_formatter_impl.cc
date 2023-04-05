/* -*- c++ -*- */
/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fosphor_formatter_impl.h"
#include <gnuradio/fft/window.h>
#include <gnuradio/io_signature.h>
#include <cmath>


namespace {

// Helper function for converting float to unsigned char
void float_array_to_uchar(const float* in,
                          unsigned char* out,
                          int nsamples,
                          const float scaling = 1.0f)
{
    constexpr int MIN_UCHAR = 0;
    constexpr int MAX_UCHAR = 255;
    for (int i = 0; i < nsamples; i++) {
        long int r = (long int)rint(in[i] * scaling);
        if (r < MIN_UCHAR)
            r = MIN_UCHAR;
        else if (r > MAX_UCHAR)
            r = MAX_UCHAR;
        out[i] = r;
    }
}

// Helper function to transform the scale factor to something we can put into
// the nlog10_ff block
float calculate_log10_scale(const double scale)
{
    return static_cast<float>(scale) * 10.0f;
}


} // namespace

using namespace gr::qtgui;

fosphor_formatter::sptr fosphor_formatter::make(int fft_size,
                                                int num_bins,
                                                int input_decim,
                                                int waterfall_decim,
                                                int histo_decim,
                                                double scale,
                                                double alpha,
                                                double epsilon,
                                                double trise,
                                                double tdecay)
{
    return gnuradio::make_block_sptr<fosphor_formatter_impl>(fft_size,
                                                             num_bins,
                                                             input_decim,
                                                             waterfall_decim,
                                                             histo_decim,
                                                             scale,
                                                             alpha,
                                                             epsilon,
                                                             trise,
                                                             tdecay);
}


fosphor_formatter_impl::fosphor_formatter_impl(int fft_size,
                                               int num_bins,
                                               int input_decim,
                                               int waterfall_decim,
                                               int histo_decim,
                                               double scale,
                                               double alpha,
                                               double epsilon,
                                               double trise,
                                               double tdecay)
    : hier_block2("fosphor_formatter",
                  io_signature::make(1, 1, sizeof(float) * fft_size),
                  io_signature::make(2, 2, sizeof(unsigned char) * fft_size)),
      // Init attributes
      d_fftsize(fft_size),
      d_num_bins(64),
      d_histo_decim(histo_decim),
      d_epsilon(epsilon),
      d_trise(trise),
      d_tdecay(tdecay),
      d_histo_buf_f(fft_size * d_num_bins, 0.0f),
      d_hit_count(fft_size * d_num_bins, 0),
      // Init sub-blocks
      d_s2v(gr::blocks::stream_to_vector::make(sizeof(float), fft_size)),
      d_input_decim(
          gr::blocks::keep_one_in_n::make(sizeof(float) * fft_size, input_decim)),
      d_fft(gr::fft::fft_v<float, true /* forward */>::make(
          fft_size, gr::fft::window::blackman_harris(fft_size), true /* shift */)),
      d_c2m(gr::blocks::complex_to_mag_squared::make(fft_size)),
      d_log(gr::blocks::nlog10_ff::make(calculate_log10_scale(scale),
                                        fft_size,
                                        20 * std::log10(static_cast<float>(fft_size)))),
      d_f2byte(gr::blocks::lambda_block::make(
          [fft_size](gr::blocks::lambda_block* self,
                     int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items) -> int {
              const float* in = static_cast<const float*>(input_items[0]);
              float* out = static_cast<float*>(output_items[0]);
              const int items_to_process = std::min(noutput_items, ninput_items[0]);
              for (int i; i < items_to_process; i++) {
                  float_array_to_uchar(in, out, fft_size);
                  in += fft_size;
                  out += fft_size;
              }
              self->consume_each(items_to_process);
              return items_to_process;
          },
          gr::io_signature::make(1, 1, sizeof(float) * fft_size),
          gr::io_signature::make(1, 1, sizeof(unsigned char) * fft_size),
          "float2byte")),
      d_avg(gr::filter::single_pole_iir_filter_ff::make(alpha, fft_size)),
      d_histo_proc(gr::blocks::lambda_block::make(
          [this](gr::blocks::lambda_block* self,
                 int noutput_items,
                 gr_vector_int& ninput_items,
                 gr_vector_const_void_star& input_items,
                 gr_vector_void_star& output_items) {
              return this->_process_histogram(
                  self, noutput_items, ninput_items, input_items, output_items);
          },
          gr::io_signature::make2(
              3, 3, sizeof(unsigned char) * fft_size, sizeof(float) * fft_size),
          gr::io_signature::make(1, 1, sizeof(unsigned char) * fft_size),
          "histo_proc")),
      d_wf_decim(gr::blocks::keep_one_in_n::make(sizeof(unsigned char) * fft_size,
                                                 waterfall_decim))
{
    // Ensure we can always produce a full histogram set
    d_histo_proc->set_output_multiple(d_num_bins + 2);

    // Common path
    connect(self(), 0, d_s2v, 0);
    connect(d_s2v, 0, d_input_decim, 0);
    connect(d_input_decim, 0, d_fft, 0);
    connect(d_fft, 0, d_c2m, 0);
    connect(d_c2m, 0, d_log, 0);
    connect(d_log, 0, d_f2byte, 0);
    // Histogram path
    connect(d_log, 0, d_avg, 0);
    connect(d_f2byte, 0, d_histo_proc, 0);
    connect(d_log, 0, d_histo_proc, 1);
    connect(d_avg, 0, d_histo_proc, 2);
    connect(d_histo_proc, 0, self(), 0);
    // Waterfall path
    connect(d_f2byte, 0, d_wf_decim, 0);
    connect(d_wf_decim, 0, self(), 1);
}

fosphor_formatter_impl::~fosphor_formatter_impl() {}

int fosphor_formatter_impl::_process_histogram(gr::blocks::lambda_block* self,
                                               int noutput_items,
                                               gr_vector_int& ninput_items,
                                               gr_vector_const_void_star& input_items,
                                               gr_vector_void_star& output_items)
{
    const int items_to_process = std::min<int>({ ninput_items[0],
                                                 ninput_items[1],
                                                 ninput_items[2],
                                                 d_histo_decim - d_histo_count });
    if (items_to_process == 0) {
        return 0;
    }
    // We are guaranteed to have space for at least one histogram output,
    // because we called set_output_multiple() in the ctor on the histo proc.

    const unsigned char* in_logfft_b = static_cast<const unsigned char*>(input_items[0]);
    const float* in_logfft_f = static_cast<const float*>(input_items[1]);
    const float* in_logfft_avg = static_cast<const float*>(input_items[2]);
    unsigned char* out = static_cast<unsigned char*>(output_items[0]);

    for (int i; i < items_to_process; i++) {
        //// Update max hold:
        // Decay previous value
        volk_32f_s32f_multiply_32f(d_maxhold_buf.data(), d_maxhold_buf.data(), d_epsilon);
        // Compare with current max
        volk_32f_x2_max_32f(
            d_maxhold_buf.data(), d_maxhold_buf.data(), in_logfft_f, d_fftsize);
        //// Update hit counter
        for (size_t i = 0; i < static_cast<size_t>(d_fftsize); i++) {
            // This >>2 assumes d_num_bins is 64
            const uint8_t bin_index = in_logfft_b[i] >> 2;
            d_hit_count[bin_index * d_fftsize + i]++;
        }
        d_histo_count++;
        in_logfft_f += d_fftsize;
        in_logfft_b += d_fftsize;
    }
    self->consume_each(items_to_process);
    if (d_histo_count < d_histo_decim) {
        return 0;
    }
    d_histo_count = 0;

    // Now update the output histogram buffer
    for (size_t i = 0; i < d_histo_buf_f.size(); ++i) {
        d_histo_buf_f[i] = _update_histo_val(d_histo_buf_f[i], d_hit_count[i]);
    }

    // Copy the histogram buffer
    float_array_to_uchar(d_histo_buf_f.data(), out, d_fftsize * d_num_bins, 256);
    // Copy average to out buffer
    const int avg_idx = d_num_bins * d_fftsize;
    const int maxhold_idx = avg_idx + d_fftsize;
    float_array_to_uchar(in_logfft_avg, out + avg_idx, d_fftsize);
    // Copy max hold to out buffer
    float_array_to_uchar(d_maxhold_buf.data(), out + maxhold_idx, d_fftsize);
    const int num_items_written = d_num_bins + 2;
    // Insert stream tag
    auto tag = gr::tag_t{};
    tag.key = pmt::string_to_symbol("rx_eob");
    tag.value = pmt::PMT_T;
    tag.offset = self->nitems_written(0) + num_items_written - 1;
    self->add_item_tag(0, tag);

    // Reset hit counter
    std::fill(d_hit_count.begin(), d_hit_count.end(), 0);
    return d_num_bins + 2;
}

void fosphor_formatter_impl::_update_histo_val(float& hv, const int16_t hc)
{
    // This is copied from gr-fosphor
    if (hv < 0.01f && hc == 0) {
        return;
    }

    const float a = static_cast<float>(hc) / d_histo_decim;
    const float b = a / d_trise;
    const float c = b + 1.0f / d_tdecay;
    const float d = b / c;
    const float e = std::pow(1.0f - c, d_histo_decim);

    hv = (hv - d) * e + d;
    hv = std::max(std::min(1.0f, hv), 0.0f);
}
