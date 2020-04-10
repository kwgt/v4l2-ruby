require "bundler/gem_tasks"
require "rake/extensiontask"

task :default => :spec

spec = Gem::Specification.load("v4l2-ruby.gemspec")

# add your default gem packing task
Gem::PackageTask.new(spec) {|pkg|}

# feed the ExtensionTask with your spec
Rake::ExtensionTask.new('v4l2-ruby', spec) { |ext|
  ext.name          = "v4l2"
  ext.ext_dir       = "ext/v4l2"
  ext.cross_compile = true
  ext.lib_dir       = File.join(*["lib", "v4l2", ENV["FAT_DIR"]].compact)
}
