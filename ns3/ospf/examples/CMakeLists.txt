build_lib_example(
    NAME ospf-example
    SOURCE_FILES ospf-example.cc
    LIBRARIES_TO_LINK ${libospf} ${libpoint-to-point} ${libinternet-apps}
)
