#!/bin/ruby
# Escapes an sql file for use as a C++ header

abort "no target file provided" if ARGV.count < 2

file = File.open(ARGV[0], "r")
targetFile = File.open(ARGV[1], "w")

# Write the beginning part

targetFile.write "#pragma once\n"

targetFile.write "constexpr auto STR_"+
                 "#{File.basename(ARGV[0]).gsub(/[^0-9a-z]/i, '_').upcase} = \n"

# Escape and put the contents of the file
lines = file.read.lines.map(&:chomp).
          map {|i| 
  
  "\"" + i.gsub("\"", "\\\"") + "\\n\""
  
}

targetFile.write lines.join("\n")

targetFile.write ";"




