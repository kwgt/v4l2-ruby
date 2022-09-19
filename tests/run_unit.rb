#! /usr/bin/env ruby
# coding: utf-8

require 'pathname'
require 'test/unit'

require_relative "lib/test_util"
require_relative "lib/config"

raise("target unit is not specified") if ARGV.empty?

ARGV.each { |arg|
  target = Pathname(arg)
  
  case
  when target.file?
    require_relative target

  when target.directory?
    target.find { |file|
      require_relative file if /^test_.*\.rb$/ =~ file.basename.to_s
    }
  end
}
