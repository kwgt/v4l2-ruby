#! /usr/bin/env ruby
# coding: utf-8

require 'test/unit'
require 'v4l2'

using TestUtil

class TestSimpleCreate < Test::Unit::TestCase
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

  test "simple create" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    assert_respond_to(cam, :close)
    assert_respond_to(cam, :controls)
    assert_respond_to(cam, :support_formats)
    assert_respond_to(cam, :frame_capabilities)
    assert_respond_to(cam, :set_control)
    assert_respond_to(cam, :get_control)
    assert_respond_to(cam, :format=)
    assert_respond_to(cam, :image_width)
    assert_respond_to(cam, :image_width=)
    assert_respond_to(cam, :image_height)
    assert_respond_to(cam, :image_height=)
    assert_respond_to(cam, :framerate)
    assert_respond_to(cam, :framerate=)
    assert_respond_to(cam, :state)
    assert_respond_to(cam, :start)
    assert_respond_to(cam, :stop)
    assert_respond_to(cam, :capture)
    assert_respond_to(cam, :busy?)
    assert_respond_to(cam, :error?)
    assert_respond_to(cam, :name)
    assert_respond_to(cam, :driver)
    assert_respond_to(cam, :bus)

  ensure
    cam&.close
  end
end
