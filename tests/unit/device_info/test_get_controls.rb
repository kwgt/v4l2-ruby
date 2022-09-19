#! /usr/bin/env ruby
# coding: utf-8

require 'test/unit'
require 'v4l2'

using TestUtil

class TestGetControls < Test::Unit::TestCase
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

  test "get controls" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    dat = cam.controls

    pp dat if Config.show_data?

    assert_kind_of(Array, dat)
    assert_true(dat.all? {|item| item.kind_of?(klass::Control)})

  ensure
    cam&.close
  end

  test "atributes" do
    cam = assert_nothing_raised {klass.open(Config.device)}

    cam.controls.each { |item|
      case item
      when klass::IntegerControl
        assert_respond_to(item, :name)
        assert_respond_to(item, :id)
        assert_respond_to(item, :min)
        assert_respond_to(item, :max)
        assert_respond_to(item, :step)
        assert_respond_to(item, :default)

        assert_kind_of(String, item.name)
        assert_kind_of(Integer, item.id)
        assert_kind_of(Integer, item.min)
        assert_kind_of(Integer, item.max)
        assert_kind_of(Integer, item.step)
        assert_kind_of(Integer, item.default)

      when klass::BooleanControl
        assert_respond_to(item, :name)
        assert_respond_to(item, :id)
        assert_respond_to(item, :default)

        assert_kind_of(String, item.name)
        assert_kind_of(Integer, item.id)
        assert_boolean(item.default)

      when klass::MenuControl
        assert_respond_to(item, :name)
        assert_respond_to(item, :id)
        assert_respond_to(item, :items)
        assert_respond_to(item, :default)

        assert_kind_of(String, item.name)
        assert_kind_of(Integer, item.id)
        assert_kind_of(Array, item.items)

        item.items.each {|mi|
          assert_respond_to(mi, :name)
          assert_respond_to(mi, :index)
          assert_kind_of(String, mi.name)
          assert_kind_of(Integer, mi.index)
        }

      else
        add_failure("found unknown controls item")
      end
    }

  ensure
    cam&.close
  end
end
