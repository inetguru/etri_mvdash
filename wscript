# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
#    module = bld.create_ns3_module('etri_mvdash', ['internet', 'dash', 'quic'])
    module = bld.create_ns3_module('etri_mvdash', ['internet', 'applications', 'point-to-point'])
    module.source = [
        'model/mvdash_client.cc',
        'model/mvdash_server.cc',
        'model/multiview-model.cc',
        'model/free_viewpoint_model.cc',
        'model/markovian_viewpoint_model.cc',
        'model/mvdash_adaptation_algorithm.cc',
        'model/maximize_current_adaptation.cc',
        'model/adaptation_test.cc', #TEST
        'helper/mvdash-helper.cc',
        ]

    module_test = bld.create_ns3_module_test_library('etri_mvdash')
    module_test.source = [
        #'test/etri_mvdash-examples-test-suite.cc',
        ]
    # Tests encapsulating example programs should be listed here
    if (bld.env['ENABLE_EXAMPLES']):
        module_test.source.extend([
#        'example/etri_mvdash-example.cc',
        'example/mvdash-v1.cc',
        'example/viewpoint_test.cc'
             ])

    headers = bld(features='ns3header')
    headers.module = 'etri_mvdash'
    headers.source = [
        'model/mvdash.h',
        'model/mvdash_client.h',
        'model/mvdash_server.h',
        'model/multiview-model.h',
        'model/free_viewpoint_model.h',
        'model/markovian_viewpoint_model.h',
        'model/mvdash_adaptation_algorithm.h',
        'model/maximize_current_adaptation.h',    
        'model/adaptation_test.h', #TEST   
        'helper/mvdash-helper.h',
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()

