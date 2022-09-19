require 'test/unit'
require 'v4l2'

module TestUtil
  refine Test::Unit::TestCase do
    def klass
      return Video4Linux2::Camera
    end
  end
end
