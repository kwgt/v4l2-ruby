# V4L2 for Ruby
A V4L2 (Video4Linux2) interfae library for Ruby.

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'v4l2'
```

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install v4l2

## Usage

```
#! /usr/bin/env ruby
# coding: utf-8

require 'v4l2'
require 'json'
require 'pp'

cam = Video4Linux2::Camera.open('/dev/video0')

#pp cam.support_formats
#pp cam.frame_capabilities(:MJPEG)

p cam.name
p cam.driver
p cam.bus

if not cam.busy?
  cam.image_width  = 640
  cam.image_height = 480
  cam.framerate    = 10/1r
  cam.format       = :MJPEG

  # pp cam.controls
  # camera.set_control(id, val)

  cam.start

  10.times {|i| IO.binwrite("%02d.jpg" % i, cam.capture)}

  cam.stop
else
  printf("camera is busy\n")
end

cam.close
```

