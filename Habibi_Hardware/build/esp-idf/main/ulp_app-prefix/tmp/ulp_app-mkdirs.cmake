# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Espressif/frameworks/esp-idf-v5.3.1/components/ulp/cmake"
  "C:/Users/moham/Habibi_Hardware/Habibi_Hardware/build/esp-idf/main/ulp_app"
  "C:/Users/moham/Habibi_Hardware/Habibi_Hardware/build/esp-idf/main/ulp_app-prefix"
  "C:/Users/moham/Habibi_Hardware/Habibi_Hardware/build/esp-idf/main/ulp_app-prefix/tmp"
  "C:/Users/moham/Habibi_Hardware/Habibi_Hardware/build/esp-idf/main/ulp_app-prefix/src/ulp_app-stamp"
  "C:/Users/moham/Habibi_Hardware/Habibi_Hardware/build/esp-idf/main/ulp_app-prefix/src"
  "C:/Users/moham/Habibi_Hardware/Habibi_Hardware/build/esp-idf/main/ulp_app-prefix/src/ulp_app-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/moham/Habibi_Hardware/Habibi_Hardware/build/esp-idf/main/ulp_app-prefix/src/ulp_app-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/moham/Habibi_Hardware/Habibi_Hardware/build/esp-idf/main/ulp_app-prefix/src/ulp_app-stamp${cfgdir}") # cfgdir has leading slash
endif()
