#! /usr/bin/env ruby
# coding: utf-8

require 'test/unit'
require 'v4l2'

using TestUtil

class TestImageSize < Test::Unit::TestCase
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

  test "set image size from capability list" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    assert_nothing_raised {
      cam.support_formats.each { |fmt|
        cam.frame_capabilities(:MJPEG).each { |cap|
          cam.image_width  = cap.width
          cam.image_height = cap.height
        }
      }
    }

  ensure
    cam&.close if defined? cam
  end

  test "set width" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    assert_nothing_raised {cam.image_width = 640}
    assert_equal(640, cam.image_width);

    assert_nothing_raised {cam.image_width = 320.0}
    assert_equal(320, cam.image_width);

    assert_nothing_raised {cam.image_width = 1280/2r}
    assert_equal(640, cam.image_width);

    assert_nothing_raised {cam.image_width = 320 + 0i}
    assert_equal(320, cam.image_width);

    assert_raise_kind_of(TypeError) {cam.image_width = "123"}
    assert_raise_kind_of(TypeError) {cam.image_width = :A123}
    assert_raise_kind_of(RangeError) {cam.image_width = 640i}

  ensure
    cam&.close if defined? cam
  end

  test "set height" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    assert_nothing_raised {cam.image_height = 480}
    assert_equal(480, cam.image_height);

    assert_nothing_raised {cam.image_height = 240.0}
    assert_equal(240, cam.image_height);

    assert_nothing_raised {cam.image_height = 960/2r}
    assert_equal(480, cam.image_height);

    assert_nothing_raised {cam.image_height = 240 + 0i}
    assert_equal(240, cam.image_height);

    assert_raise_kind_of(TypeError) {cam.image_width = "123"}
    assert_raise_kind_of(TypeError) {cam.image_width = :A123}
    assert_raise_kind_of(RangeError) {cam.image_width = 480i}

  ensure
    cam&.close if defined? cam
  end
end
