/* -*- c++ -*- */
/*
 * Copyright 2010-2015 Free Software Foundation, Inc.
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

#include "usrp_source_impl.h"
#include "gr_uhd_common.h"
#include <boost/format.hpp>
#include <boost/thread/thread.hpp>
#include <boost/make_shared.hpp>
#include <stdexcept>
#include <iostream>

namespace gr {
  namespace uhd {

    usrp_source::sptr
    usrp_source::make(const ::uhd::device_addr_t &device_addr,
                      const ::uhd::io_type_t &io_type,
                      size_t num_channels)
    {
      //fill in the streamer args
      ::uhd::stream_args_t stream_args;
      switch(io_type.tid) {
      case ::uhd::io_type_t::COMPLEX_FLOAT32: stream_args.cpu_format = "fc32"; break;
      case ::uhd::io_type_t::COMPLEX_INT16: stream_args.cpu_format = "sc16"; break;
      default: throw std::runtime_error("only complex float and shorts known to work");
      }

      stream_args.otw_format = "sc16"; //only sc16 known to work
      for(size_t chan = 0; chan < num_channels; chan++)
        stream_args.channels.push_back(chan); //linear mapping

      return usrp_source::make(device_addr, stream_args);
    }

    usrp_source::sptr
    usrp_source::make(const ::uhd::device_addr_t &device_addr,
                      const ::uhd::stream_args_t &stream_args)
    {
      check_abi();
      return usrp_source::sptr
        (new usrp_source_impl(device_addr, stream_args_ensure(stream_args)));
    }

    usrp_source_impl::usrp_source_impl(const ::uhd::device_addr_t &device_addr,
                                       const ::uhd::stream_args_t &stream_args):
      usrp_block("gr uhd usrp source",
                    io_signature::make(0, 0, 0),
                    args_to_io_sig(stream_args)),
      usrp_block_impl(device_addr, stream_args, ""),
      _tag_now(false)
    {
      std::stringstream str;
      str << name() << unique_id();
      _id = pmt::string_to_symbol(str.str());

      _samp_rate = this->get_samp_rate();
      _center_freq = this->get_center_freq(0);
#ifdef GR_UHD_USE_STREAM_API
      _samps_per_packet = 1;
#endif
    }

    usrp_source_impl::~usrp_source_impl()
    {
    }

    ::uhd::dict<std::string, std::string>
    usrp_source_impl::get_usrp_info(size_t chan)
    {
      chan = _stream_args.channels[chan];
#ifdef UHD_USRP_MULTI_USRP_GET_USRP_INFO_API
      return _dev->get_usrp_rx_info(chan);
#else
      throw std::runtime_error("not implemented in this version");
#endif
    }

    void
    usrp_source_impl::set_subdev_spec(const std::string &spec, size_t mboard)
    {
      return _dev->set_rx_subdev_spec(spec, mboard);
    }

    std::string
    usrp_source_impl::get_subdev_spec(size_t mboard)
    {
      return _dev->get_rx_subdev_spec(mboard).to_string();
    }

    void
    usrp_source_impl::set_samp_rate(double rate)
    {
        BOOST_FOREACH(const size_t chan, _stream_args.channels)
        {
            _dev->set_rx_rate(rate, chan);
        }
      _samp_rate = this->get_samp_rate();
      _tag_now = true;
    }

    double
    usrp_source_impl::get_samp_rate(void)
    {
      return _dev->get_rx_rate(_stream_args.channels[0]);
    }

    ::uhd::meta_range_t
    usrp_source_impl::get_samp_rates(void)
    {
#ifdef UHD_USRP_MULTI_USRP_GET_RATES_API
      return _dev->get_rx_rates(_stream_args.channels[0]);
#else
      throw std::runtime_error("not implemented in this version");
#endif
    }

    ::uhd::tune_result_t
    usrp_source_impl::set_center_freq(const ::uhd::tune_request_t tune_request,
                                      size_t chan)
    {
      const size_t user_chan = chan;
      chan = _stream_args.channels[chan];
      const ::uhd::tune_result_t res = _dev->set_rx_freq(tune_request, chan);
      _center_freq = this->get_center_freq(user_chan);
      _tag_now = true;
      return res;
    }

    SET_CENTER_FREQ_FROM_INTERNALS(usrp_source_impl, set_rx_freq);

    double
    usrp_source_impl::get_center_freq(size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->get_rx_freq(chan);
    }

    ::uhd::freq_range_t
    usrp_source_impl::get_freq_range(size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->get_rx_freq_range(chan);
    }

    void
    usrp_source_impl::set_gain(double gain, size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->set_rx_gain(gain, chan);
    }

    void
    usrp_source_impl::set_gain(double gain, const std::string &name, size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->set_rx_gain(gain, name, chan);
    }

    void usrp_source_impl::set_normalized_gain(double norm_gain, size_t chan)
    {
      if (norm_gain > 1.0 || norm_gain < 0.0) {
        throw std::runtime_error("Normalized gain out of range, must be in [0, 1].");
      }
      ::uhd::gain_range_t gain_range = get_gain_range(chan);
      double abs_gain = (norm_gain * (gain_range.stop() - gain_range.start())) + gain_range.start();
      set_gain(abs_gain, chan);
    }

    double
    usrp_source_impl::get_gain(size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->get_rx_gain(chan);
    }

    double
    usrp_source_impl::get_gain(const std::string &name, size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->get_rx_gain(name, chan);
    }

    double
    usrp_source_impl::get_normalized_gain(size_t chan)
    {
      ::uhd::gain_range_t gain_range = get_gain_range(chan);
      double norm_gain =
        (get_gain(chan) - gain_range.start()) /
        (gain_range.stop() - gain_range.start());
      // Avoid rounding errors:
      if (norm_gain > 1.0) return 1.0;
      if (norm_gain < 0.0) return 0.0;
      return norm_gain;
    }

    std::vector<std::string>
    usrp_source_impl::get_gain_names(size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->get_rx_gain_names(chan);
    }

    ::uhd::gain_range_t
    usrp_source_impl::get_gain_range(size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->get_rx_gain_range(chan);
    }

    ::uhd::gain_range_t
    usrp_source_impl::get_gain_range(const std::string &name, size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->get_rx_gain_range(name, chan);
    }

    void
    usrp_source_impl::set_antenna(const std::string &ant, size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->set_rx_antenna(ant, chan);
    }

    std::string
    usrp_source_impl::get_antenna(size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->get_rx_antenna(chan);
    }

    std::vector<std::string>
    usrp_source_impl::get_antennas(size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->get_rx_antennas(chan);
    }

    void
    usrp_source_impl::set_bandwidth(double bandwidth, size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->set_rx_bandwidth(bandwidth, chan);
    }

    double
    usrp_source_impl::get_bandwidth(size_t chan)
    {
        chan = _stream_args.channels[chan];
        return _dev->get_rx_bandwidth(chan);
    }

    ::uhd::freq_range_t
    usrp_source_impl::get_bandwidth_range(size_t chan)
    {
        chan = _stream_args.channels[chan];
        return _dev->get_rx_bandwidth_range(chan);
    }

    void
    usrp_source_impl::set_auto_dc_offset(const bool enable, size_t chan)
    {
      chan = _stream_args.channels[chan];
#ifdef UHD_USRP_MULTI_USRP_FRONTEND_CAL_API
      return _dev->set_rx_dc_offset(enable, chan);
#else
      throw std::runtime_error("not implemented in this version");
#endif
    }

    void
    usrp_source_impl::set_dc_offset(const std::complex<double> &offset,
                                    size_t chan)
    {
      chan = _stream_args.channels[chan];
#ifdef UHD_USRP_MULTI_USRP_FRONTEND_CAL_API
      return _dev->set_rx_dc_offset(offset, chan);
#else
      throw std::runtime_error("not implemented in this version");
#endif
    }

    void
    usrp_source_impl::set_auto_iq_balance(const bool enable, size_t chan)
    {
      chan = _stream_args.channels[chan];
#ifdef UHD_USRP_MULTI_USRP_FRONTEND_IQ_AUTO_API
      return _dev->set_rx_iq_balance(enable, chan);
#else
      throw std::runtime_error("not implemented in this version");
#endif
    }


    void
    usrp_source_impl::set_iq_balance(const std::complex<double> &correction,
                                     size_t chan)
    {
      chan = _stream_args.channels[chan];
#ifdef UHD_USRP_MULTI_USRP_FRONTEND_CAL_API
      return _dev->set_rx_iq_balance(correction, chan);
#else
      throw std::runtime_error("not implemented in this version");
#endif
    }

    ::uhd::sensor_value_t
    usrp_source_impl::get_sensor(const std::string &name, size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->get_rx_sensor(name, chan);
    }

    std::vector<std::string>
    usrp_source_impl::get_sensor_names(size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->get_rx_sensor_names(chan);
    }

    ::uhd::usrp::dboard_iface::sptr
    usrp_source_impl::get_dboard_iface(size_t chan)
    {
      chan = _stream_args.channels[chan];
      return _dev->get_rx_dboard_iface(chan);
    }

    void
    usrp_source_impl::set_stream_args(const ::uhd::stream_args_t &stream_args)
    {
      _update_stream_args(stream_args);
#ifdef GR_UHD_USE_STREAM_API
      _rx_stream.reset();
#else
      throw std::runtime_error("not implemented in this version");
#endif
    }

    void
    usrp_source_impl::set_start_time(const ::uhd::time_spec_t &time)
    {
      _start_time = time;
      _start_time_set = true;
      _stream_now = false;
    }

    void
    usrp_source_impl::issue_stream_cmd(const ::uhd::stream_cmd_t &cmd)
    {
        for (size_t i = 0; i < _stream_args.channels.size(); i++)
        {
            _dev->issue_stream_cmd(cmd, _stream_args.channels[i]);
        }
    }

    bool
    usrp_source_impl::start(void)
    {
      boost::recursive_mutex::scoped_lock lock(d_mutex);
#ifdef GR_UHD_USE_STREAM_API
      if(not _rx_stream){
        _rx_stream = _dev->get_rx_stream(_stream_args);
        _samps_per_packet = _rx_stream->get_max_num_samps();
      }
#endif
      //setup a stream command that starts streaming slightly in the future
      static const double reasonable_delay = 0.1; //order of magnitude over RTT
      ::uhd::stream_cmd_t stream_cmd(::uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
      stream_cmd.stream_now = _stream_now;
      if(_start_time_set) {
        _start_time_set = false; //cleared for next run
        stream_cmd.time_spec = _start_time;
      }
      else {
        stream_cmd.time_spec = get_time_now() + ::uhd::time_spec_t(reasonable_delay);
      }
      this->issue_stream_cmd(stream_cmd);
      _tag_now = true;
      return true;
    }

    void
    usrp_source_impl::flush(void)
    {
      const size_t nbytes = 4096;
      gr_vector_void_star outputs;
      std::vector<std::vector<char> > buffs(_nchan, std::vector<char>(nbytes));
      for(size_t i = 0; i < _nchan; i++) {
        outputs.push_back(&buffs[i].front());
      }
      while(true) {
#ifdef GR_UHD_USE_STREAM_API
        const size_t bpi = ::uhd::convert::get_bytes_per_item(_stream_args.cpu_format);
        _rx_stream->recv(outputs, nbytes/bpi, _metadata, 0.0);
#else
        _dev->get_device()->recv
          (outputs, nbytes/_type->size, _metadata, *_type,
           ::uhd::device::RECV_MODE_FULL_BUFF, 0.0);
#endif
        if(_metadata.error_code == ::uhd::rx_metadata_t::ERROR_CODE_TIMEOUT)
          break;
      }
    }

    bool
    usrp_source_impl::stop(void)
    {
      boost::recursive_mutex::scoped_lock lock(d_mutex);
      this->issue_stream_cmd(::uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
      this->flush();

      return true;
    }

    std::vector<std::complex<float> >
    usrp_source_impl::finite_acquisition(const size_t nsamps)
    {
      if(_nchan != 1)
        throw std::runtime_error("finite_acquisition: usrp source has multiple channels, call finite_acquisition_v");
      return finite_acquisition_v(nsamps).front();
    }

    std::vector<std::vector<std::complex<float> > >
    usrp_source_impl::finite_acquisition_v(const size_t nsamps)
    {
#ifdef GR_UHD_USE_STREAM_API
      //kludgy way to ensure rx streamer exsists
      if(!_rx_stream) {
        this->start();
        this->stop();
      }

      //flush so there is no queued-up data
      this->flush();

      //create a multi-dimensional container to hold an array of sample buffers
      std::vector<std::vector<std::complex<float> > >
        samps(_nchan, std::vector<std::complex<float> >(nsamps));

      //load the void* vector of buffer pointers
      std::vector<void *> buffs(_nchan);
      for(size_t i = 0; i < _nchan; i++) {
        buffs[i] = &samps[i].front();
      }

      //tell the device to stream a finite amount
      ::uhd::stream_cmd_t cmd(::uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
      cmd.num_samps = nsamps;
      cmd.stream_now = _stream_now;
      static const double reasonable_delay = 0.1; //order of magnitude over RTT
      cmd.time_spec = get_time_now() + ::uhd::time_spec_t(reasonable_delay);
      this->issue_stream_cmd(cmd);

      //receive samples until timeout
      const size_t actual_num_samps = _rx_stream->recv
        (buffs, nsamps, _metadata, 1.0);

      //resize the resulting sample buffers
      for(size_t i = 0; i < _nchan; i++) {
        samps[i].resize(actual_num_samps);
      }

      return samps;
#else
      throw std::runtime_error("not implemented in this version");
#endif
    }

    int
    usrp_source_impl::work(int noutput_items,
                           gr_vector_const_void_star &input_items,
                           gr_vector_void_star &output_items)
    {
      boost::recursive_mutex::scoped_lock lock(d_mutex);
#ifdef GR_UHD_USE_STREAM_API
      //In order to allow for low-latency:
      //We receive all available packets without timeout.
      //This call can timeout under regular operation...
      size_t num_samps = _rx_stream->recv
        (output_items, noutput_items, _metadata, 0.0);

      //If receive resulted in a timeout condition:
      //We now receive a single packet with a large timeout.
      if(_metadata.error_code == ::uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
        num_samps = _rx_stream->recv
          (output_items, noutput_items, _metadata, 0.1, true/*one pkt*/);
      }
#else
      size_t num_samps = _dev->get_device()->recv
        (output_items, noutput_items, _metadata,
         *_type, ::uhd::device::RECV_MODE_FULL_BUFF, 0.0);

      if(_metadata.error_code == ::uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
        num_samps = _dev->get_device()->recv
          (output_items, noutput_items, _metadata, *_type,
           ::uhd::device::RECV_MODE_ONE_PACKET, 1.0);
      }
#endif

      //handle possible errors conditions
      switch(_metadata.error_code) {
      case ::uhd::rx_metadata_t::ERROR_CODE_NONE:
        if(_tag_now) {
          _tag_now = false;
          //create a timestamp pmt for the first sample
          const pmt::pmt_t val = pmt::make_tuple
            (pmt::from_uint64(_metadata.time_spec.get_full_secs()),
             pmt::from_double(_metadata.time_spec.get_frac_secs()));
          //create a tag set for each channel
          for(size_t i = 0; i < _nchan; i++) {
            this->add_item_tag(i, nitems_written(0), TIME_KEY, val, _id);
            this->add_item_tag(i, nitems_written(0), RATE_KEY,
                               pmt::from_double(_samp_rate), _id);
            this->add_item_tag(i, nitems_written(0), FREQ_KEY,
                               pmt::from_double(_center_freq), _id);
          }
        }
        break;

      case ::uhd::rx_metadata_t::ERROR_CODE_TIMEOUT:
        //its ok to timeout, perhaps the user is doing finite streaming
        return 0;

      case ::uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
        _tag_now = true;
        //ignore overflows and try work again
        return work(noutput_items, input_items, output_items);

      default:
        //GR_LOG_WARN(d_logger, boost::format("USRP Source Block caught rx error: %s") % _metadata.strerror());
        GR_LOG_WARN(d_logger, boost::format("USRP Source Block caught rx error code: %d") % _metadata.error_code);
        break;
      }

      if (_metadata.end_of_burst) {
        for (size_t i = 0; i < _nchan; i++) {
          this->add_item_tag(i,
              nitems_written(i) + num_samps - 1,
              EOB_KEY, pmt::PMT_T,
              _id
          );
        }
        _tag_now = true;
      }

      return num_samps;
    }

  } /* namespace uhd */
} /* namespace gr */
