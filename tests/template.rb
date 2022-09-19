#! /usr/bin/env ruby
# coding: utf-8

require 'test/unit'
require 'v4l2'

using TestUtil

class TestXX < Test::Unit::TestCase
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

  test "smaple" do
    cam = assert_nothing_raised {klass.open(Config.device)}

  ensure
    cam&.close if defined? cam
  end
end
