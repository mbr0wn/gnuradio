# Copyright 2011-2013,2019 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Setup library
########################################################################
set(LIB_GR_UHD_SOURCES amsg_source_impl.cc usrp_block_impl.cc usrp_sink_impl.cc
                       usrp_source_impl.cc)

if(ENABLE_GR_UHD_OPENGL)
    qt5_add_resources(FOSPHOR_RC_SRCS fosphor.qrc)
    set(CMAKE_AUTOMOC ON)
    include_directories(${Qt5Widgets_INCLUDE_DIRS}
        ${Qt5OpenGL_INCLUDE_DIRS})
endif(ENABLE_GR_UHD_OPENGL)

if(ENABLE_UHD_RFNOC)
    list(
        APPEND
        LIB_GR_UHD_SOURCES
        rfnoc_block.cc
        rfnoc_block_generic_impl.cc
        rfnoc_ddc_impl.cc
        rfnoc_duc_impl.cc
        rfnoc_fir_filter_impl.cc
        rfnoc_graph_impl.cc
        rfnoc_replay_impl.cc
        rfnoc_rx_radio_impl.cc
        rfnoc_rx_streamer_impl.cc
        rfnoc_siggen_impl.cc
        rfnoc_tx_radio_impl.cc
        rfnoc_tx_streamer_impl.cc
        rfnoc_window_impl.cc)
endif(ENABLE_UHD_RFNOC)

if(ENABLE_GR_UHD_OPENGL)
    list(
        APPEND
        LIB_GR_UHD_SOURCES
        axis.c
        fosphor_display_impl.cc
        fosphor_formatter_impl.cc
        QFosphorColorMapper.cc
        QFosphorSurface.cc
        ${FOSPHOR_RC_SRCS}
    )
    find_package(OpenGL)
    list(APPEND PRIVATE_LIBS
        Qt5::OpenGL
        OpenGL::GL
    )
endif()

add_library(gnuradio-uhd ${LIB_GR_UHD_SOURCES})

if(ENABLE_GR_UHD_OPENGL)
    find_package(OpenGL)
    target_link_libraries(gnuradio-uhd
        PUBLIC
            gnuradio-blocks
            gnuradio-fft
            gnuradio-filter
            gnuradio-runtime
            qwt::qwt
            Qt5::Core
            Qt5::Gui
            Qt5::OpenGL
            ${UHD_LIBRARIES}
        PRIVATE
            OpenGL::GL
    )
else(ENABLE_GR_UHD_OPENGL)
    target_link_libraries(gnuradio-uhd
        PUBLIC
            gnuradio-runtime
            ${UHD_LIBRARIES}
    )
endif(ENABLE_GR_UHD_OPENGL)

target_include_directories(
    gnuradio-uhd PUBLIC $<INSTALL_INTERFACE:include>
                        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)

#Add Windows DLL resource file if using MSVC
if(MSVC)
    include(${PROJECT_SOURCE_DIR}/cmake/Modules/GrVersion.cmake)

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gnuradio-uhd.rc.in
                   ${CMAKE_CURRENT_BINARY_DIR}/gnuradio-uhd.rc @ONLY)

    target_sources(gnuradio-uhd PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gnuradio-uhd.rc)
endif(MSVC)

if(BUILD_SHARED_LIBS)
    gr_library_foo(gnuradio-uhd UHD)
endif()
