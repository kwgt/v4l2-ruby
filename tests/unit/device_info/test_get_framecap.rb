#! /usr/bin/env ruby
# coding: utf-8

require 'test/unit'
require 'v4l2'

using TestUtil

class TestGetFrameCapability < Test::Unit::TestCase
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

  test "get framecapabilities" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    cam.support_formats.each { |fmt|
      dat = cam.frame_capabilities(fmt.fcc)

      pp dat if Config.show_data?

      assert_kind_of(Array, dat)
      assert_true(dat.all? {|item| item.kind_of?(klass::FrameCapability)})
    }

  ensure
    cam&.close
  end

  test "atributes" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    cam.support_formats.each { |fmt|
      cam.frame_capabilities(fmt.fcc).each { |item|
        assert_respond_to(item, :width)
        assert_respond_to(item, :height)
        assert_respond_to(item, :rate)

        assert_true(item.rate.all? {|e| e.kind_of?(Rational)})
      }
    }

  ensure
    cam&.close
  end
end
