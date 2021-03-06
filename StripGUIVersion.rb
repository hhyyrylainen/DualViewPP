#!/usr/bin/env ruby
# frozen_string_literal: true

# Strips version requirements from glade files. Sample: <requires lib="gtk+" version="3.20"/>
require 'nokogiri'

Dir.foreach('resources/gui') do |item|
  next if item !~ /\.glade$/

  puts 'Stripping ' + item

  file = 'gui/' + item
  xml = Nokogiri::XML(File.read(file))

  node = xml.at('interface/requires')

  node&.remove

  File.write(file, xml.to_xml)
end
