#!/usr/bin/env ruby
# Moves data from sqlite to postgresql. Only works with the signatures right now
require 'optparse'
require 'pg'
require 'sqlite3'

options = {}
OptionParser.new do |opt|
  opt.on('--signatures-db FILE') { |f| options[:signatures] = f }
  opt.on('--postgresql-db DB') { |f| options[:postgredb] = f }
  opt.on('--postgresql-schema SCHEMA') { |f| options[:postgreschema] = f }
  opt.on('--postgresql-user USER') { |f| options[:postgreuser] = f }
end.parse!

if !File.exists? options[:signatures]
  puts "Signatures DB file doesn't exist"
  exit 2
end

options[:postgreschema] ||= "public"

if !options[:postgreuser]
  puts "PostgreSQL user required"
  exit 2
end

if !options[:postgredb]
  puts "PostgreSQL database required"
  exit 2
end


# open sqlite
sqliteDB = SQLite3::Database.new options[:signatures]

version = 0

sqliteDB.execute("SELECT * FROM version") do |row|
  version = row[0]
end

case version
when 1
else
  puts "Unsupported version: #{version}"
  exit 3
end

puts "Exporting data from signatures to postgresql db #{options[:postgredb]}"


begin

  con = PG.connect dbname: options[:postgredb], user: options[:postgreuser]
  puts "Connected to PostgreSQL version: #{con.server_version}"

  # TODO: check also the version in postgresql
  con.exec "SET search_path TO #{options[:postgreschema]}"
  
  # Table setup
  con.exec <<-SQL
  DROP TABLE IF EXISTS version;
  CREATE TABLE version( number INTEGER );
  CREATE TABLE IF NOT EXISTS pictures ( 
      id BIGINT PRIMARY KEY,
      signature BYTEA
  );    
  
  CREATE TABLE IF NOT EXISTS picture_signature_words (
      picture_id BIGINT NOT NULL,
      sig_word BYTEA NOT NULL,
      FOREIGN KEY (picture_id) REFERENCES pictures(id) ON DELETE CASCADE
  );
  
  CREATE INDEX IF NOT EXISTS position_sig ON picture_signature_words(picture_id, sig_word);
SQL

  
  con.prepare 'insert_pic', "INSERT INTO pictures (id, signature) VALUES ($1, $2)"
  con.prepare 'insert_sig', "INSERT INTO picture_signature_words (picture_id, sig_word) " +
                            "VALUES ($1, $2)"

  # Copy data
  con.transaction{|con|
    con.exec_params "INSERT INTO version (number) VALUES ($1)", [version]

    sqliteDB.execute("SELECT id, signature FROM pictures"){|row|
      con.exec_prepared 'insert_pic', [row[0], con.escape_bytea(row[1])]  
    }

    sqliteDB.execute("SELECT picture_id, sig_word FROM picture_signature_words"){|row|
      con.exec_prepared 'insert_sig', [row[0], con.escape_bytea(row[1])]  
    }
  }
  
  puts "Finished without errors"
  
rescue PG::Error => e

  puts e.message

ensure

  con.close if con

end
