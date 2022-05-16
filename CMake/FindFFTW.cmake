# - Find FFTW
# Find the native FFTW includes and library
# This module defines
#  FFTW_INCLUDE_DIR, where to find fftw3.h, etc.
#  FFTW_LIBRARIES, the libraries needed to use FFTW.
#  FFTW_FOUND, If false, do not try to use FFTW.
# also defined, but not for general use are
#  FFTW_LIBRARY, where to find the FFTW library.

FIND_PATH(FFTW_INCLUDE_DIR
  NAMES
    fftw3.h
  PATHS
    /usr/local/include
    /opt/local/include
    /usr/include
)

SET(FFTW_NAMES ${FFTW_NAMES} fftw3 fftw3f fftw3l fftw3-3)
FIND_LIBRARY(FFTW_LIBRARY
  NAMES
    ${FFTW_NAMES}
  PATHS
    /usr/local/lib64
    /opt/local/lib64
    /usr/lib64
    /usr/local/lib
    /opt/local/lib
    /usr/lib
  )


if (FFTW_INCLUDE_DIR AND FFTW_LIBRARY)
	  set (FFTW_FOUND TRUE)
	 
	  set(FFTW_INCLUDE_DIRS
			${FFTW_INCLUDE_DIR}
		)
		
	  set(FFTW_LIBRARIES
			${FFTW_LIBRARY}
		)
		
endif(FFTW_INCLUDE_DIR AND FFTW_LIBRARY)


 
