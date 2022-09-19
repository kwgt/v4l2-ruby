#! /usr/bin/env ruby
# coding: utf-8

require 'test/unit'
require 'v4l2'

using TestUtil

class TestGetDeviceName < Test::Unit::TestCase
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

  test "get device name" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    p cam.name if Config.show_data?

    assert_kind_of(String, cam.name)

  ensure
    cam&.close
  end
end
