set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/sctp-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME sctp
    SOURCE_FILES model/sctp.cc
                 model/sctp-header.cc
                 model/sctp-association.cc
                 model/sctp-socket.cc
                 model/sctp-l4-protocol.cc
                 helper/sctp-helper.cc
    HEADER_FILES model/sctp.h
                 model/sctp-header.h
                 model/sctp-association.h
                 model/sctp-socket.h
                 model/sctp-l4-protocol.h
                 helper/sctp-helper.h
    LIBRARIES_TO_LINK ${libinternet}
    TEST_SOURCES test/sctp-test-suite.cc
                 ${examples_as_tests_sources}
)
