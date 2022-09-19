#! /usr/bin/env ruby
# coding: utf-8

require 'test/unit'
require 'v4l2'

using TestUtil

class TestFramerate < Test::Unit::TestCase
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

  test "set framerate from capability" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    assert_nothing_raised {
      cam.support_formats.each { |fmt|
        cam.frame_capabilities(fmt.fcc).each { |cap|
          cap.rate.each { |rate| cam.framerate = rate}
        }
      }
    }

  ensure
    cam&.close if defined? cam
  end

  test "set frame rate" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    assert_nothing_raised {cam.framerate = 10}
    assert_equal(1/10r, cam.framerate)

    assert_nothing_raised {cam.framerate = 5.0}
    assert_equal(1/5r, cam.framerate)

    assert_nothing_raised {cam.framerate = 10/1r}
    assert_equal(1/10r, cam.framerate)

    assert_raise_kind_of(TypeError) {cam.image_width = "10"}
    assert_raise_kind_of(TypeError) {cam.image_width = :"10"}

  ensure
    cam&.close if defined? cam
  end
end
