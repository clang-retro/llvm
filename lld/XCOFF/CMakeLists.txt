add_lld_library(lldXCOFF
  Driver.cpp
  InputFiles.cpp
  Writer.cpp

  LINK_COMPONENTS
  BinaryFormat
  Object
  Option
  Support
  TargetParser

  LINK_LIBS
  lldCommon
  )
