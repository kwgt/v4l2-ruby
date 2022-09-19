#! /usr/bin/env ruby
# coding: utf-8

require 'test/unit'
require 'v4l2'

using TestUtil

class TestGetDriverName < Test::Unit::TestCase
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

  test "get driver name" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    cam.bus if Config.show_data?

    assert_kind_of(String, cam.bus)

  ensure
    cam&.close
  end
end
