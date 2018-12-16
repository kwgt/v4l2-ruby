require 'mkmf'

$CFLAGS="-DRUBY_EXTLIB"

create_makefile( "v4l2/v4l2")
