#! /usr/bin/env ruby
# coding: utf-8

require 'yaml'

module Config
  class << self
    def config
      return @config ||= YAML.load_file("config.yml")
    end
    private :config

    def device
      return config["target_device"]
    end

    def show_data?
      return config["show_data"]
    end
  end
end
