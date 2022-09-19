#! /usr/bin/env ruby
# coding: utf-8

require 'test/unit'
require 'v4l2'

using TestUtil

class TestGetFormats < Test::Unit::TestCase
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

  test "get formats" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    dat = cam.support_formats

    pp dat if Config.show_data?

    assert_kind_of(Array, dat)
    assert_true(dat.all? {|item| item.kind_of?(klass::FormatDescription)})

  ensure
    cam&.close
  end

  test "atributes" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    cam.support_formats.each { |item|
      if item.kind_of?(klass::FormatDescription)
        assert_respond_to(item, :description)
        assert_respond_to(item, :fcc)

        assert_kind_of(String, item.description)
        assert_kind_of(String, item.fcc)
        assert_equal(4, item.fcc.size)

      else
        add_failure("found unknown format item")
      end
    }

  ensure
    cam&.close
  end
end
