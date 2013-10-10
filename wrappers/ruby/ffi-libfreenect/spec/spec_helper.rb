SPEC_DIR = File.expand_path(File.dirname(__FILE__))

$LOAD_PATH.unshift(SPEC_DIR)
$LOAD_PATH.unshift(File.join(SPEC_DIR, '..', 'lib'))

require 'freenect'
# require 'rspec'
require 'rspec/core'

RSpec.configure do |config|
end
