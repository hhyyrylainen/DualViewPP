#!/bin/sh
# Installs ubuntu packages required to compile DualView++
sudo apt-get install cmake doxygen ruby ruby-dev tar libboost-dev libboost-filesystem-dev libboost-locale-dev libcrypto++-dev libcurl4-openssl-dev libgtk-3-dev libcanberra-gtk3-dev libgtkmm-3.0-dev libmagick++-dev libsqlite3-dev libhtmlcxx-dev libgumbo-dev libxmu-dev
# Make a link to tar
sudo ln -s /bin/tar /usr/bin/tar
# Submodule packages
git submodule update --init --recursive
# Additionally if gtk is an old version nokogiri gem is required to make the .glade files load
