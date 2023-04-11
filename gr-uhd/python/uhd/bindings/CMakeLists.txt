include(GrPybind)

########################################################################
# Python Bindings
########################################################################

list(
    APPEND
    uhd_python_files
    amsg_source_python.cc
    uhd_types_python.cc
    usrp_block_python.cc
    usrp_sink_python.cc
    usrp_source_python.cc
    python_bindings.cc)

if(ENABLE_UHD_RFNOC)
    add_definitions(-DGR_ENABLE_UHD_RFNOC)
    list(
        APPEND
        uhd_python_files
        rfnoc_block_generic_python.cc
        rfnoc_block_python.cc
        rfnoc_ddc_python.cc
        rfnoc_duc_python.cc
        rfnoc_fir_filter_python.cc
        rfnoc_graph_python.cc
        rfnoc_replay_python.cc
        rfnoc_rx_radio_python.cc
        rfnoc_rx_streamer_python.cc
        rfnoc_siggen_python.cc
        rfnoc_tx_radio_python.cc
        rfnoc_tx_streamer_python.cc
        rfnoc_window_python.cc)
endif()

if(ENABLE_GR_UHD_OPENGL)
    list(
        APPEND
        uhd_python_files
        fosphor_display_python.cc
        fosphor_formatter_python.cc)
endif(ENABLE_GR_UHD_OPENGL)

set(include_dirs ${Qt5Widgets_INCLUDE_DIRS})
if(QWT_FOUND)
    list(APPEND include_dirs ${QWT_INCLUDE_DIRS})
endif(QWT_FOUND)
list(JOIN include_dirs "," extra_includes)

gr_pybind_make(uhd ../../.. gr::uhd "${uhd_python_files}")

if(ENABLE_GR_UHD_OPENGL)
    target_compile_definitions(uhd_python PRIVATE OPENGL_FOUND)
endif(ENABLE_GR_UHD_OPENGL)

install(
    TARGETS uhd_python
    DESTINATION ${GR_PYTHON_DIR}/gnuradio/uhd
    COMPONENT pythonapi)
