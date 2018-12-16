require "bundler/gem_tasks"
require "rake/extensiontask"

task :default => :spec

Rake::ExtensionTask.new("v4l2") do |ext|
  ext.lib_dir = "lib/v4l2"
end
