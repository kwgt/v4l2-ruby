#! /usr/bin/env ruby
# coding: utf-8

require 'test/unit'
require 'v4l2'

Warning[:experimental] = false

using TestUtil

class TestRactor < Test::Unit::TestCase
  class << self
    def startup
    end

    def shutdown
    end
  end

  def setup
  end

  def teardown
  end

  test "using Ractor" do
    r = Ractor.new(Config.device) { |device|
      begin
        cam = Video4Linux2::Camera.open(device)

        cam.image_width  = 640
        cam.image_height = 480
        cam.framerate    = 5/1r
        cam.format       = :MJPG

        cam.start {
          10.times {
            Ractor.yield(cam.capture)
          }
        }

      ensure
        cam&.close if defined? cam
      end

      :END
    }

    assert_nothing_raised {
      loop {
        data = r.take
        break if data == :END

        p data.bytesize if Config.show_data?
      }
    }
  end

  test "unshareble" do 
    cam = assert_nothing_raised {klass.open(Config.device)}

    assert_false(Ractor.shareable?(cam))

  ensure
    cam&.close if defined? cam
  end
end
