#!/usr/bin/env ruby

# aptitude install ruby-bdb

require 'rubygems'
require 'bdb'

db=BDB::Btree.open("keystore.db", "links")
db.keys.each { |k| p [k, db[k]] }
