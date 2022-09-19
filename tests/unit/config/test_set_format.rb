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

  test "set format from capability" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    assert_nothing_raised {
      cam.support_formats.each {|fmt| cam.format = fmt.fcc}
    }

  ensure
    cam&.close if defined? cam
  end

  test "set format" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    assert_nothing_raised {cam.format = "YUYV"}
    assert_nothing_raised {cam.format = :MJPG}

    assert_raise_kind_of(TypeError) {cam.format = 10}
    assert_raise_kind_of(TypeError) {cam.format = []}
    assert_raise_kind_of(TypeError) {cam.format = {}}

  ensure
    cam&.close if defined? cam
  end
end
