build_lib(
    LIBNAME ospf
    SOURCE_FILES
        model/ospf-header.cc
        model/ospf-lsa.cc
        model/ospf-neighbor.cc
        model/ospf-routing-protocol.cc
        helper/ospf-helper.cc
    HEADER_FILES
        model/ospf-header.h
        model/ospf-lsa.h
        model/ospf-neighbor.h
        model/ospf-routing-protocol.h
        helper/ospf-helper.h
    LIBRARIES_TO_LINK ${libinternet}
    TEST_SOURCES
        test/ospf-test-suite.cc
)
