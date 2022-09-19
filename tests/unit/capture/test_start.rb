#! /usr/bin/env ruby
# coding: utf-8

require 'test/unit'
require 'v4l2'

using TestUtil

class TestStart < Test::Unit::TestCase
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

  test "simple start" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    assert_nothing_raised {
      cam.start
    }

    assert_true(cam.ready?)

  ensure
    cam&.close if defined? cam
  end

  test "with block" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    assert_nothing_raised {
      cam.start {assert_true(cam.ready?)}
    }

    assert_false(cam.ready?)

  ensure
    cam&.close if defined? cam
  end
end
